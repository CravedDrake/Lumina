#include "LibraryManager.h"
#include "database/Database.h"
#include "history/ResumePolicy.h"
#include "playlists/M3u.h"
#include "settings/AppSettings.h"
#include "utilities/MediaFiles.h"
#include "utilities/MediaNameParser.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

namespace lumina {

namespace {

QSqlQuery makeQuery(const QString &sql, const QVariantList &binds = {})
{
    QSqlQuery q(Database::open());
    q.prepare(sql);
    for (const QVariant &b : binds) {
        // A null QString binds as SQL NULL and would violate the NOT NULL text columns: bind "" instead.
        if (b.metaType().id() == QMetaType::QString && b.toString().isNull()) q.addBindValue(QString(QLatin1String("")));
        else q.addBindValue(b);
    }
    return q;
}
bool run(const QString &sql, const QVariantList &binds = {})
{
    QSqlQuery q = makeQuery(sql, binds);
    return q.exec();
}

/// Stable, credential-free key for a URL; a stream's identity must not contain secrets.
QString storableLocation(const QUrl &url)
{
    if (url.isLocalFile())
        return QDir::cleanPath(url.toLocalFile());
    QUrl u = url;
    u.setUserInfo(QString());
    return u.toString(QUrl::FullyEncoded);
}

} // namespace

LibraryManager::LibraryManager(AppSettings *settings, QObject *parent) : QObject(parent), m_settings(settings)
{
    qRegisterMetaType<LibraryScanner::Options>();
    m_scanner = new LibraryScanner;
    m_scanner->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_scanner, &QObject::deleteLater);
    connect(m_scanner, &LibraryScanner::progress, this, [this](const QString &phase, int done, int total) {
        m_phase = phase; m_done = done; m_total = total;
        emit scanProgressChanged();
    }, Qt::QueuedConnection);
    connect(m_scanner, &LibraryScanner::batchCommitted, this, [this] { m_debounce.start(); }, Qt::QueuedConnection);
    connect(m_scanner, &LibraryScanner::finished, this, [this](const QVariantMap &s) {
        m_scanning = false;
        emit scanningChanged();
        bumpRevision();
        emit foldersChanged();
        const QStringList errors = s.value(QStringLiteral("errors")).toStringList();
        for (const QString &e : errors) emit message(e, QStringLiteral("warning"));
        const int changes = s.value(QStringLiteral("added")).toInt() + s.value(QStringLiteral("moved")).toInt()
                            + s.value(QStringLiteral("missing")).toInt();
        if (!s.value(QStringLiteral("cancelled")).toBool() && changes > 0)
            emit message(tr("Library updated: %1 new, %2 moved, %3 missing")
                             .arg(s.value(QStringLiteral("added")).toInt()).arg(s.value(QStringLiteral("moved")).toInt())
                             .arg(s.value(QStringLiteral("missing")).toInt()), QStringLiteral("success"));
        else if (s.value(QStringLiteral("cancelled")).toBool())
            emit message(tr("Scan cancelled. It will pick up where it left off next time."), QStringLiteral("info"));
        emit scanFinished(s);
    }, Qt::QueuedConnection);
    m_thread.setObjectName(QStringLiteral("library-scanner"));
    m_thread.start();

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, &LibraryManager::bumpRevision);
}

LibraryManager::~LibraryManager()
{
    m_scanner->requestCancel();
    m_thread.quit();
    m_thread.wait(15000);
}

void LibraryManager::bumpRevision()
{
    ++m_revision;
    emit dataChanged();
}

int LibraryManager::mediaCount() const
{
    QSqlQuery q = makeQuery(QStringLiteral("SELECT COUNT(*) FROM media WHERE kind!='stream' AND missing=0"));
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

// ---------------------------------------------------------------------------------------- folders

QVariantList LibraryManager::folders() const
{
    QVariantList out;
    QSqlQuery q = makeQuery(QStringLiteral(
        "SELECT f.id,f.path,f.last_scan,(SELECT COUNT(*) FROM media m WHERE m.folder_id=f.id AND m.missing=0) FROM folders f ORDER BY f.path"));
    if (q.exec())
        while (q.next())
            out << QVariantMap{{QStringLiteral("id"), q.value(0)}, {QStringLiteral("path"), q.value(1)},
                               {QStringLiteral("lastScan"), q.value(2)}, {QStringLiteral("count"), q.value(3)},
                               {QStringLiteral("accessible"), QFileInfo(q.value(1).toString()).isDir()}};
    return out;
}

bool LibraryManager::addFolder(const QString &pathOrUrl)
{
    const QUrl u(pathOrUrl);
    const QString p = QDir::cleanPath(u.isLocalFile() ? u.toLocalFile() : pathOrUrl);
    const QFileInfo fi(p);
    if (!fi.isDir() || !fi.isReadable()) {
        emit message(tr("That folder cannot be read: %1").arg(p), QStringLiteral("error"));
        return false;
    }
    const QString abs = fi.absoluteFilePath();
    QSqlQuery q = makeQuery(QStringLiteral("INSERT OR IGNORE INTO folders(path,added_at) VALUES(?,?)"),
                            {abs, QDateTime::currentSecsSinceEpoch()});
    q.exec();
    emit foldersChanged();
    startScan({abs});
    return true;
}

void LibraryManager::removeFolder(const QString &path, bool removeMedia)
{
    QSqlQuery q = makeQuery(QStringLiteral("SELECT id FROM folders WHERE path=?"), {QDir::cleanPath(path)});
    if (!(q.exec() && q.next())) return;
    const qint64 id = q.value(0).toLongLong();
    if (removeMedia)
        run(QStringLiteral("DELETE FROM media WHERE folder_id=? AND favorite=0 AND play_count=0"), {id});
    run(QStringLiteral("UPDATE media SET folder_id=NULL WHERE folder_id=?"), {id});   // keeps played/favourite items
    run(QStringLiteral("DELETE FROM folders WHERE id=?"), {id});
    emit foldersChanged();
    bumpRevision();
}

void LibraryManager::rescan()
{
    QStringList paths;
    for (const QVariant &f : folders())
        paths << f.toMap().value(QStringLiteral("path")).toString();
    if (paths.isEmpty()) {
        emit message(tr("Add a library folder first."), QStringLiteral("info"));
        return;
    }
    startScan(paths);
}

void LibraryManager::scanFoldersNow(const QStringList &folders) { startScan(folders); }

void LibraryManager::startScan(const QStringList &folders)
{
    if (m_scanning) {
        emit message(tr("A scan is already running."), QStringLiteral("info"));
        return;
    }
    m_scanning = true;
    m_phase = QStringLiteral("discovering");
    m_done = m_total = 0;
    emit scanningChanged();
    emit scanProgressChanged();
    LibraryScanner::Options opt;
    opt.folders = folders;
    opt.excluded = m_settings->getList(QStringLiteral("library/excluded"));
    opt.removeMissing = m_settings->getBool(QStringLiteral("library/removeMissing"));
    opt.threads = m_settings->getInt(QStringLiteral("performance/scanThreads"));
    QMetaObject::invokeMethod(m_scanner, [this, opt] { m_scanner->scan(opt); }, Qt::QueuedConnection);
}

void LibraryManager::cancelScan() { m_scanner->requestCancel(); }

// ------------------------------------------------------------------------------------ media records

qint64 LibraryManager::registerUrl(const QUrl &url)
{
    const QString loc = storableLocation(url);
    QSqlQuery q = makeQuery(QStringLiteral("SELECT id FROM media WHERE path=?"), {loc});
    if (q.exec() && q.next())
        return q.value(0).toLongLong();

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery ins(Database::open());
    if (url.isLocalFile()) {
        const QFileInfo fi(loc);
        const ParsedName pn = parseMediaName(loc);
        ins.prepare(QStringLiteral("INSERT INTO media(path,title,kind,show_name,season,episode,year,size,mtime,fingerprint,added_at)"
                                   " VALUES(?,?,?,?,?,?,?,?,?,?,?)"));
        auto nn = [](const QString &v) { return v.isNull() ? QString(QLatin1String("")) : v; };   // NOT NULL columns
        ins.addBindValue(loc); ins.addBindValue(nn(pn.title)); ins.addBindValue(nn(pn.kind)); ins.addBindValue(nn(pn.showName));
        ins.addBindValue(pn.season); ins.addBindValue(pn.episode); ins.addBindValue(pn.year);
        ins.addBindValue(fi.size()); ins.addBindValue(fi.lastModified().toSecsSinceEpoch());
        ins.addBindValue(LibraryScanner::fingerprint(loc, fi.size())); ins.addBindValue(now);
    } else {
        QUrl u = url; u.setUserInfo(QString());
        QString title = u.host();
        const QString tail = QFileInfo(u.path()).fileName();
        if (!tail.isEmpty()) title += QStringLiteral(" / ") + tail;
        ins.prepare(QStringLiteral("INSERT INTO media(path,title,kind,added_at,probed) VALUES(?,?,'stream',?,1)"));
        ins.addBindValue(loc); ins.addBindValue(title.isEmpty() ? loc : title); ins.addBindValue(now);
    }
    if (!ins.exec())
        return -1;
    const qint64 id = ins.lastInsertId().toLongLong();
    bumpRevision();
    return id;
}

QVariantMap LibraryManager::mediaInfo(qint64 id) const
{
    QSqlQuery q = makeQuery(QStringLiteral(
        "SELECT path,title,kind,show_name,season,episode,year,size,duration_ms,width,height,fps,vcodec,acodec,audio_tracks,sub_tracks,"
        "favorite,completed,missing,position_ms,last_played,play_count,added_at,kind_manual FROM media WHERE id=?"), {id});
    if (!(q.exec() && q.next())) return {};
    const QStringList keys = {QStringLiteral("path"), QStringLiteral("title"), QStringLiteral("kind"), QStringLiteral("showName"),
        QStringLiteral("season"), QStringLiteral("episode"), QStringLiteral("year"), QStringLiteral("size"), QStringLiteral("durationMs"),
        QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("fps"), QStringLiteral("vcodec"), QStringLiteral("acodec"),
        QStringLiteral("audioTracks"), QStringLiteral("subTracks"), QStringLiteral("favorite"), QStringLiteral("completed"),
        QStringLiteral("missing"), QStringLiteral("positionMs"), QStringLiteral("lastPlayed"), QStringLiteral("playCount"),
        QStringLiteral("addedAt"), QStringLiteral("kindManual")};
    QVariantMap m{{QStringLiteral("id"), id}};
    for (int i = 0; i < keys.size(); ++i) m[keys[i]] = q.value(i);
    return m;
}

QUrl LibraryManager::urlFor(qint64 id) const
{
    const QString p = mediaInfo(id).value(QStringLiteral("path")).toString();
    if (p.isEmpty()) return {};
    const QUrl u(p);
    return (u.isValid() && u.scheme().size() > 1 && !u.isLocalFile()) ? u : QUrl::fromLocalFile(p);
}

bool LibraryManager::setFavorite(qint64 id, bool fav)
{
    const bool ok = run(QStringLiteral("UPDATE media SET favorite=? WHERE id=?"), {fav ? 1 : 0, id});
    if (ok) bumpRevision();
    return ok;
}

bool LibraryManager::removeMedia(qint64 id)
{
    const bool ok = run(QStringLiteral("DELETE FROM media WHERE id=?"), {id});
    if (ok) bumpRevision();
    return ok;
}

bool LibraryManager::correctClassification(qint64 id, const QString &kind, const QString &title, const QString &showName,
                                           int season, int episode)
{
    static const QStringList allowed = {QStringLiteral("movie"), QStringLiteral("episode"), QStringLiteral("video")};
    if (!allowed.contains(kind) || title.trimmed().isEmpty())
        return false;
    // kind_manual=1: later scans must never undo a manual correction.
    const bool ok = run(QStringLiteral("UPDATE media SET kind=?,title=?,show_name=?,season=?,episode=?,kind_manual=1 WHERE id=? AND kind!='stream'"),
                        {kind, title.trimmed(), showName.trimmed(), season, episode, id});
    if (ok) bumpRevision();
    return ok;
}

bool LibraryManager::setMediaDetails(qint64 id, qint64 durationMs, int width, int height, double fps)
{
    return run(QStringLiteral("UPDATE media SET duration_ms=CASE WHEN ?>0 THEN ? ELSE duration_ms END,"
                              "width=CASE WHEN ?>0 THEN ? ELSE width END,height=CASE WHEN ?>0 THEN ? ELSE height END,"
                              "fps=CASE WHEN ?>0 THEN ? ELSE fps END WHERE id=?"),
               {durationMs, durationMs, width, width, height, height, fps, fps, id});
}

// -------------------------------------------------------------------------------------- history

void LibraryManager::recordPlay(qint64 id)
{
    if (!m_settings->getBool(QStringLiteral("privacy/historyEnabled")) || id <= 0)
        return;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    run(QStringLiteral("UPDATE media SET last_played=?,play_count=play_count+1 WHERE id=?"), {now, id});
    // One history row per media per 30 minutes: pausing/seeking/reopening must not spam the list.
    QSqlQuery q = makeQuery(QStringLiteral("SELECT MAX(played_at) FROM history WHERE media_id=?"), {id});
    const bool recent = q.exec() && q.next() && !q.value(0).isNull() && now - q.value(0).toLongLong() < 1800;
    if (!recent)
        run(QStringLiteral("INSERT INTO history(media_id,played_at) VALUES(?,?)"), {id, now});
    const int days = m_settings->getInt(QStringLiteral("privacy/historyDays"));
    if (days > 0) pruneHistory(days);
    bumpRevision();
}

void LibraryManager::saveProgress(qint64 id, qint64 positionMs, qint64 durationMs, bool completed)
{
    if (!m_settings->getBool(QStringLiteral("privacy/historyEnabled")) || id <= 0)
        return;
    const bool done = completed || resume::isCompleted(positionMs, durationMs);
    run(QStringLiteral("UPDATE media SET position_ms=?,completed=?,duration_ms=CASE WHEN ?>0 THEN ? ELSE duration_ms END WHERE id=?"),
        {done ? 0 : positionMs, done ? 1 : 0, durationMs, durationMs, id});
}

QVariantMap LibraryManager::resumeInfo(qint64 id) const
{
    QSqlQuery q = makeQuery(QStringLiteral("SELECT position_ms,duration_ms,completed FROM media WHERE id=?"), {id});
    if (!(q.exec() && q.next())) return {{QStringLiteral("canResume"), false}};
    const qint64 pos = q.value(0).toLongLong(), dur = q.value(1).toLongLong();
    return {{QStringLiteral("positionMs"), pos}, {QStringLiteral("durationMs"), dur},
            {QStringLiteral("canResume"), q.value(2).toInt() == 0 && resume::canResume(pos, dur)}};
}

void LibraryManager::removeFromHistory(qint64 id)
{
    run(QStringLiteral("DELETE FROM history WHERE media_id=?"), {id});
    run(QStringLiteral("UPDATE media SET last_played=0,position_ms=0,completed=0 WHERE id=?"), {id});
    bumpRevision();
}

void LibraryManager::clearHistory()
{
    run(QStringLiteral("DELETE FROM history"));
    run(QStringLiteral("UPDATE media SET last_played=0,position_ms=0,completed=0,play_count=0"));
    bumpRevision();
}

void LibraryManager::pruneHistory(int days)
{
    if (days <= 0) return;
    const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - qint64(days) * 86400;
    run(QStringLiteral("DELETE FROM history WHERE played_at<?"), {cutoff});
    run(QStringLiteral("UPDATE media SET last_played=0,position_ms=0 WHERE last_played>0 AND last_played<?"), {cutoff});
}

// -------------------------------------------------------------------- per-media prefs / bookmarks

QVariantMap LibraryManager::loadPrefs(qint64 id) const
{
    QSqlQuery q = makeQuery(QStringLiteral("SELECT json FROM media_prefs WHERE media_id=?"), {id});
    if (q.exec() && q.next())
        return QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object().toVariantMap();
    return {};
}

void LibraryManager::savePrefs(qint64 id, const QVariantMap &prefs)
{
    if (id <= 0) return;
    const QString json = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(prefs)).toJson(QJsonDocument::Compact));
    run(QStringLiteral("INSERT INTO media_prefs(media_id,json) VALUES(?,?) ON CONFLICT(media_id) DO UPDATE SET json=excluded.json"),
        {id, json});
}

QVariantList LibraryManager::bookmarks(qint64 mediaId) const
{
    QVariantList out;
    QSqlQuery q = makeQuery(QStringLiteral("SELECT id,position_ms,note FROM bookmarks WHERE media_id=? ORDER BY position_ms"), {mediaId});
    if (q.exec())
        while (q.next())
            out << QVariantMap{{QStringLiteral("id"), q.value(0)}, {QStringLiteral("positionMs"), q.value(1)}, {QStringLiteral("note"), q.value(2)}};
    return out;
}

qint64 LibraryManager::addBookmark(qint64 mediaId, qint64 positionMs, const QString &note)
{
    QSqlQuery q = makeQuery(QStringLiteral("INSERT INTO bookmarks(media_id,position_ms,note,created_at) VALUES(?,?,?,?)"),
                            {mediaId, positionMs, note, QDateTime::currentSecsSinceEpoch()});
    return q.exec() ? q.lastInsertId().toLongLong() : -1;
}
void LibraryManager::removeBookmark(qint64 id) { run(QStringLiteral("DELETE FROM bookmarks WHERE id=?"), {id}); }
void LibraryManager::updateBookmarkNote(qint64 id, const QString &note) { run(QStringLiteral("UPDATE bookmarks SET note=? WHERE id=?"), {note, id}); }

// ----------------------------------------------------------------------------------- playlists

QVariantList LibraryManager::playlists() const
{
    QVariantList out;
    QSqlQuery q = makeQuery(QStringLiteral(
        "SELECT p.id,p.name,(SELECT COUNT(*) FROM playlist_items i WHERE i.playlist_id=p.id) FROM playlists p ORDER BY p.name COLLATE NOCASE"));
    if (q.exec())
        while (q.next())
            out << QVariantMap{{QStringLiteral("id"), q.value(0)}, {QStringLiteral("name"), q.value(1)}, {QStringLiteral("count"), q.value(2)}};
    return out;
}

qint64 LibraryManager::createPlaylist(const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty()) return -1;
    QSqlQuery q = makeQuery(QStringLiteral("INSERT INTO playlists(name,created_at) VALUES(?,?)"), {n, QDateTime::currentSecsSinceEpoch()});
    if (!q.exec()) return -1;
    emit playlistsChanged();
    return q.lastInsertId().toLongLong();
}

void LibraryManager::renamePlaylist(qint64 id, const QString &name)
{
    if (name.trimmed().isEmpty()) return;
    run(QStringLiteral("UPDATE playlists SET name=? WHERE id=?"), {name.trimmed(), id});
    emit playlistsChanged();
}

void LibraryManager::deletePlaylist(qint64 id)
{
    run(QStringLiteral("DELETE FROM playlists WHERE id=?"), {id});
    emit playlistsChanged();
}

void LibraryManager::addToPlaylist(qint64 playlistId, const QVariantList &mediaIds)
{
    QSqlDatabase db = Database::open();
    db.transaction();
    QSqlQuery pos(db);
    pos.prepare(QStringLiteral("SELECT COALESCE(MAX(position),-1) FROM playlist_items WHERE playlist_id=?"));
    pos.addBindValue(playlistId);
    int next = pos.exec() && pos.next() ? pos.value(0).toInt() + 1 : 0;
    for (const QVariant &m : mediaIds) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("INSERT INTO playlist_items(playlist_id,media_id,position) VALUES(?,?,?)"));
        q.addBindValue(playlistId); q.addBindValue(m.toLongLong()); q.addBindValue(next++);
        q.exec();
    }
    db.commit();
    emit playlistsChanged();
    bumpRevision();
}

void LibraryManager::removeFromPlaylist(qint64 itemId)
{
    run(QStringLiteral("DELETE FROM playlist_items WHERE id=?"), {itemId});
    emit playlistsChanged();
    bumpRevision();
}

void LibraryManager::moveInPlaylist(qint64 playlistId, int from, int to)
{
    QSqlQuery q = makeQuery(QStringLiteral("SELECT id FROM playlist_items WHERE playlist_id=? ORDER BY position,id"), {playlistId});
    QList<qint64> ids;
    if (q.exec()) while (q.next()) ids << q.value(0).toLongLong();
    if (from < 0 || from >= ids.size() || to < 0 || to >= ids.size() || from == to) return;
    ids.move(from, to);
    QSqlDatabase db = Database::open();
    db.transaction();
    for (int i = 0; i < ids.size(); ++i) {
        QSqlQuery u(db);
        u.prepare(QStringLiteral("UPDATE playlist_items SET position=? WHERE id=?"));
        u.addBindValue(i); u.addBindValue(ids[i]); u.exec();
    }
    db.commit();
    bumpRevision();
}

QVariantList LibraryManager::playlistMediaIds(qint64 playlistId) const
{
    QVariantList out;
    QSqlQuery q = makeQuery(QStringLiteral("SELECT media_id FROM playlist_items WHERE playlist_id=? ORDER BY position,id"), {playlistId});
    if (q.exec()) while (q.next()) out << q.value(0);
    return out;
}

qint64 LibraryManager::importPlaylistFile(const QString &path)
{
    QString err;
    const auto entries = m3u::readFile(path, &err);
    if (entries.isEmpty()) {
        emit message(err.isEmpty() ? tr("The playlist contains no entries.") : tr("Cannot read playlist: %1").arg(err), QStringLiteral("error"));
        return -1;
    }
    const qint64 pl = createPlaylist(QFileInfo(path).completeBaseName());
    if (pl < 0) return -1;
    QVariantList ids;
    int missing = 0;
    for (const auto &e : entries) {
        const bool isUrl = QUrl(e.location).scheme().size() > 1 && !QUrl(e.location).isLocalFile();
        if (!isUrl && !QFileInfo::exists(e.location)) ++missing;     // kept, shown as missing - not silently dropped
        const qint64 id = registerUrl(isUrl ? QUrl(e.location) : QUrl::fromLocalFile(e.location));
        if (id > 0) ids << id;
    }
    addToPlaylist(pl, ids);
    if (missing) emit message(tr("%n playlist item(s) point to files that do not exist.", nullptr, missing), QStringLiteral("warning"));
    return pl;
}

bool LibraryManager::exportPlaylistFile(qint64 playlistId, const QString &path)
{
    QVector<m3u::Entry> entries;
    for (const QVariant &v : playlistMediaIds(playlistId)) {
        const QVariantMap m = mediaInfo(v.toLongLong());
        if (m.isEmpty()) continue;
        entries.push_back({m.value(QStringLiteral("path")).toString(), m.value(QStringLiteral("title")).toString(),
                           m.value(QStringLiteral("durationMs")).toLongLong() / 1000});
    }
    QString err;
    if (!m3u::writeFile(path, entries, &err)) {
        emit message(tr("Cannot write playlist: %1").arg(err), QStringLiteral("error"));
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------------- maintenance

void LibraryManager::clearLibrary()
{
    run(QStringLiteral("DELETE FROM media"));
    run(QStringLiteral("DELETE FROM folders"));
    emit foldersChanged();
    emit playlistsChanged();
    bumpRevision();
}

QString LibraryManager::databasePath() const { return Database::path(); }

} // namespace lumina
