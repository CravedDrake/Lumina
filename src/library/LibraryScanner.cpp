#include <QSet>
#include "LibraryScanner.h"
#include "MediaProbe.h"
#include "database/Database.h"
#include "utilities/MediaFiles.h"
#include "utilities/MediaNameParser.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QThreadPool>
#include <QDir>
#include <QSqlDatabase>
#include <QVector>
#include <QByteArray>
#include <QStringList>
#include <QString>

#include <memory>

Q_LOGGING_CATEGORY(lcScan, "lumina.scan")

namespace lumina {

namespace {
struct ExistingRow {
    qint64 id = 0;
    qint64 size = 0;
    qint64 mtime = 0;
    bool missing = false;
};
struct DiskFile {
    QString path;
    qint64 size = 0;
    qint64 mtime = 0;
};
} // namespace

QString LibraryScanner::fingerprint(const QString &path, qint64 size)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(QByteArray::number(size));
    constexpr qint64 kChunk = 64 * 1024;
    h.addData(f.read(kChunk));
    if (size > 2 * kChunk && f.seek(size - kChunk))
        h.addData(f.read(kChunk));
    return QString::fromLatin1(h.result().toHex());
}

void LibraryScanner::scan(const Options &opt)
{
    m_cancel = false;
    QVariantMap summary{{QStringLiteral("added"), 0}, {QStringLiteral("moved"), 0}, {QStringLiteral("missing"), 0},
                        {QStringLiteral("probed"), 0}, {QStringLiteral("errors"), QStringList()},
                        {QStringLiteral("cancelled"), false}};
    QStringList errors;
    int added = 0, moved = 0, missingCount = 0;

    QSqlDatabase db = Database::open();
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    auto isExcluded = [&](const QString &p) {
        for (const QString &ex : opt.excluded) {
            const QString e = QDir::cleanPath(ex);
            if (p == e || p.startsWith(e + QLatin1Char('/')))
                return true;
        }
        return false;
    };

    for (const QString &folderPath : opt.folders) {
        if (m_cancel) break;
        const QFileInfo fi(folderPath);
        if (!fi.exists() || !fi.isDir() || !fi.isReadable()) {
            errors << tr("Folder is not accessible: %1").arg(folderPath);
            continue;                       // an unreachable drive must not mark its files as missing
        }
        const QString root = QDir::cleanPath(fi.absoluteFilePath());

        // ---- folder row ------------------------------------------------------------
        qint64 folderId = 0;
        {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT id FROM folders WHERE path=?"));
            q.addBindValue(root);
            if (q.exec() && q.next()) folderId = q.value(0).toLongLong();
            else {
                QSqlQuery ins(db);
                ins.prepare(QStringLiteral("INSERT INTO folders(path,added_at) VALUES(?,?)"));
                ins.addBindValue(root); ins.addBindValue(now);
                if (!ins.exec()) { errors << ins.lastError().text(); continue; }
                folderId = ins.lastInsertId().toLongLong();
            }
        }

        // ---- discover files --------------------------------------------------------
        emit progress(QStringLiteral("discovering"), 0, 0);
        QVector<DiskFile> files;
        // AllDirs: otherwise the name filter is applied to directory names too and sub-folders are never entered.
        QDirIterator it(root, media::mediaNameFilters(), QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext() && !m_cancel) {
            const QString p = it.next();
            const QFileInfo f = it.fileInfo();
            if (f.isDir() || isExcluded(p)) continue;
            if (!media::mediaNameFilters().isEmpty() && !media::isMedia(p)) continue;
            files.push_back({p, f.size(), f.lastModified().toSecsSinceEpoch()});
            if (files.size() % 250 == 0)
                emit progress(QStringLiteral("discovering"), int(files.size()), 0);
        }
        if (m_cancel) break;

        // ---- existing rows ---------------------------------------------------------
        QHash<QString, ExistingRow> existing;
        {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT id,path,size,mtime,missing FROM media WHERE folder_id=?"));
            q.addBindValue(folderId);
            q.exec();
            while (q.next())
                existing.insert(q.value(1).toString(), {q.value(0).toLongLong(), q.value(2).toLongLong(),
                                                          q.value(3).toLongLong(), q.value(4).toInt() != 0});
        }
        QHash<QString, bool> seen;
        QSet<qint64> movedIds;   // rows whose file now lives at a new path: must not be flagged missing
        // Candidates for "this file was moved": rows whose file no longer exists.
        QHash<QString, QVector<qint64>> orphansByFingerprint;
        {
            QSqlQuery q(db);
            q.exec(QStringLiteral("SELECT id,path,fingerprint FROM media WHERE kind!='stream' AND fingerprint!=''"));
            while (q.next()) {
                const QString p = q.value(1).toString();
                if (!QFileInfo::exists(p))
                    orphansByFingerprint[q.value(2).toString()].push_back(q.value(0).toLongLong());
            }
        }

        // ---- synchronise ----------------------------------------------------------
        int done = 0, batch = 0;
        db.transaction();
        for (const DiskFile &df : files) {
            if (m_cancel) break;
            seen.insert(df.path, true);
            auto ex = existing.constFind(df.path);
            QSqlQuery q(db);
            if (ex != existing.constEnd()) {
                if (ex->size != df.size || ex->mtime != df.mtime || ex->missing) {
                    q.prepare(QStringLiteral("UPDATE media SET size=?,mtime=?,fingerprint=?,probed=CASE WHEN ?=1 THEN 0 ELSE probed END,missing=0 WHERE id=?"));
                    q.addBindValue(df.size); q.addBindValue(df.mtime);
                    q.addBindValue(fingerprint(df.path, df.size));
                    q.addBindValue(ex->size != df.size || ex->mtime != df.mtime ? 1 : 0);
                    q.addBindValue(ex->id);
                    q.exec();
                }
            } else {
                const QString fp = fingerprint(df.path, df.size);
                qint64 movedId = 0;
                auto orphan = orphansByFingerprint.find(fp);
                if (!fp.isEmpty() && orphan != orphansByFingerprint.end() && !orphan->isEmpty()) {
                    movedId = orphan->takeFirst();
                    QSqlQuery chk(db);
                    chk.prepare(QStringLiteral("SELECT size FROM media WHERE id=?"));
                    chk.addBindValue(movedId);
                    if (!(chk.exec() && chk.next() && chk.value(0).toLongLong() == df.size))
                        movedId = 0;
                }
                if (movedId) {   // same content under a new path: keep history, favourites, resume point
                    const ParsedName pn = parseMediaName(df.path);
                    q.prepare(QStringLiteral("UPDATE media SET path=?,folder_id=?,mtime=?,missing=0,"
                                             "title=CASE WHEN kind_manual=1 THEN title ELSE ? END WHERE id=?"));
                    q.addBindValue(df.path); q.addBindValue(folderId); q.addBindValue(df.mtime);
                    q.addBindValue(pn.title); q.addBindValue(movedId);
                    if (q.exec()) { ++moved; movedIds.insert(movedId); }
                } else {
                    const ParsedName pn = parseMediaName(df.path);
                    q.prepare(QStringLiteral(
                        "INSERT INTO media(path,folder_id,title,kind,show_name,season,episode,year,size,mtime,"
                        "fingerprint,added_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(path) DO NOTHING"));
                    q.addBindValue(df.path); q.addBindValue(folderId); q.addBindValue(QString(pn.title.isNull() ? QStringLiteral("") : pn.title));
                    q.addBindValue(pn.kind); q.addBindValue(QString(pn.showName.isNull() ? QStringLiteral("") : pn.showName)); q.addBindValue(pn.season);
                    q.addBindValue(pn.episode); q.addBindValue(pn.year); q.addBindValue(df.size);
                    q.addBindValue(df.mtime); q.addBindValue(fp); q.addBindValue(now);
                    if (!q.exec()) errors << q.lastError().text();
                    else if (q.numRowsAffected() > 0) ++added;
                }
            }
            ++done;
            if (++batch >= 400) {            // bounded transactions: cancel/crash loses at most one batch
                db.commit(); db.transaction(); batch = 0;
                emit progress(QStringLiteral("indexing"), done, int(files.size()));
                emit batchCommitted();
            }
        }
        db.commit();
        emit progress(QStringLiteral("indexing"), done, int(files.size()));

        // ---- files that disappeared -------------------------------------------------
        if (!m_cancel) {
            db.transaction();
            for (auto e = existing.constBegin(); e != existing.constEnd(); ++e) {
                if (seen.contains(e.key()) || isExcluded(e.key()) || movedIds.contains(e->id)) continue;
                QSqlQuery q(db);
                if (opt.removeMissing)
                    q.prepare(QStringLiteral("DELETE FROM media WHERE id=? AND favorite=0"));
                else
                    q.prepare(QStringLiteral("UPDATE media SET missing=1 WHERE id=?"));
                q.addBindValue(e->id);
                q.exec();
                ++missingCount;
            }
            QSqlQuery upd(db);
            upd.prepare(QStringLiteral("UPDATE folders SET last_scan=? WHERE id=?"));
            upd.addBindValue(QDateTime::currentSecsSinceEpoch()); upd.addBindValue(folderId);
            upd.exec();
            db.commit();
        }
        emit batchCommitted();
    }

    // ---- metadata (libVLC parser), N probe instances in parallel ------------------------------
    int probed = 0;
    if (opt.probeMetadata && !m_cancel) {
        struct Todo { qint64 id; QString path; };
        QVector<Todo> todo;
        {
            QSqlQuery q(db);
            q.exec(QStringLiteral("SELECT id,path FROM media WHERE probed=0 AND missing=0 AND kind!='stream'"));
            while (q.next()) todo.push_back({q.value(0).toLongLong(), q.value(1).toString()});
        }
        std::atomic<int> counter{0};
        const int total = int(todo.size());
        if (total > 0) {
            QThreadPool pool;
            pool.setMaxThreadCount(qBound(1, opt.threads, 8));
            pool.setExpiryTimeout(0);       // let threads (and their libVLC instances) exit when idle
            for (const Todo &t : todo) {
                pool.start([this, t, &counter, total] {
                    if (m_cancel) return;
                    thread_local std::unique_ptr<MediaProbe> probe;
                    if (!probe) probe = std::make_unique<MediaProbe>();
                    const ProbeResult r = probe->probe(t.path);
                    QSqlDatabase wdb = Database::open();
                    QSqlQuery q(wdb);
                    q.prepare(QStringLiteral("UPDATE media SET duration_ms=?,width=?,height=?,fps=?,vcodec=?,acodec=?,"
                                             "audio_tracks=?,sub_tracks=?,probed=1 WHERE id=?"));
                    q.addBindValue(r.durationMs); q.addBindValue(r.width); q.addBindValue(r.height);
                    q.addBindValue(r.fps); q.addBindValue(r.videoCodec); q.addBindValue(r.audioCodec);
                    q.addBindValue(r.audioTracks); q.addBindValue(r.subtitleTracks); q.addBindValue(t.id);
                    q.exec();       // unreadable files are marked probed=1 too (no endless re-probing)
                    const int n = ++counter;
                    if (n % 10 == 0 || n == total) {
                        emit progress(QStringLiteral("reading metadata"), n, total);
                        emit batchCommitted();
                    }
                });
            }
            pool.waitForDone();
        }
        probed = counter.load();
    }

    summary[QStringLiteral("added")] = added;
    summary[QStringLiteral("moved")] = moved;
    summary[QStringLiteral("missing")] = missingCount;
    summary[QStringLiteral("probed")] = probed;
    summary[QStringLiteral("errors")] = errors;
    summary[QStringLiteral("cancelled")] = m_cancel.load();
    emit finished(summary);
}

} // namespace lumina
