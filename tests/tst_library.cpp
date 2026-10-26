#include <QtTest>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include "database/Database.h"
#include "library/LibraryManager.h"
#include "library/LibraryModel.h"
#include "settings/AppSettings.h"

using namespace lumina;

static void touch(const QString &p, const QByteArray &content = "data")
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(content);
}

class TstLibrary : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    AppSettings *m_settings = nullptr;
    LibraryManager *m_lib = nullptr;

    void scanAndWait(const QStringList &folders)
    {
        QSignalSpy done(m_lib, &LibraryManager::scanFinished);
        m_lib->scanFoldersNow(folders);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 60000);
    }
    int count(const QString &sql)
    {
        QSqlQuery q(Database::open()); q.exec(sql); return q.next() ? q.value(0).toInt() : -1;
    }
private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("LUMINA_NO_PROBE", "1");
        QVERIFY(Database::initialize(m_dir.filePath(QStringLiteral("lib.db"))));
        m_settings = new AppSettings(m_dir.filePath(QStringLiteral("s.ini")), this);
        m_lib = new LibraryManager(m_settings, this);
    }
    void cleanupTestCase() { delete m_lib; Database::closeAll(); }

    void schemaIsCurrent() { QCOMPARE(Database::schemaVersion(), Database::kSchemaVersion); }

    void scanFindsClassifiesAndIgnores()
    {
        const QString root = m_dir.filePath(QStringLiteral("media"));
        touch(root + QStringLiteral("/Movies/Heat.1995.mkv"), "aaaaaaaaaaaaaaaa");
        touch(root + QStringLiteral("/TV/Show.S01E01.mkv"), "bbbbbbbbbbbbbbbb");
        touch(root + QStringLiteral("/TV/Show.S01E02.mkv"), "cccccccccccccccc");
        touch(root + QStringLiteral("/home video.mp4"), "dddddddddddddddd");
        touch(root + QStringLiteral("/readme.txt")); touch(root + QStringLiteral("/Movies/poster.jpg"));
        scanAndWait({root});
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media")), 4);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media WHERE kind='movie'")), 1);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media WHERE kind='episode'")), 2);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media WHERE kind='video'")), 1);
        // rescan is idempotent: no duplicates
        scanAndWait({root});
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media")), 4);
    }
    void manualCorrectionSurvivesRescan()
    {
        QSqlQuery q(Database::open()); q.exec(QStringLiteral("SELECT id FROM media WHERE title LIKE 'home video%'")); QVERIFY(q.next());
        const qint64 id = q.value(0).toLongLong(); q.finish();   // release the read snapshot (WAL)
        QVERIFY(m_lib->correctClassification(id, QStringLiteral("movie"), QStringLiteral("Home Video"), QString(), 0, 0));
        QVERIFY(!m_lib->correctClassification(id, QStringLiteral("bogus"), QStringLiteral("x"), QString(), 0, 0));
        scanAndWait({m_dir.filePath(QStringLiteral("media"))});
        QCOMPARE(m_lib->mediaInfo(id).value(QStringLiteral("kind")).toString(), QStringLiteral("movie"));
        QCOMPARE(m_lib->mediaInfo(id).value(QStringLiteral("title")).toString(), QStringLiteral("Home Video"));
    }
    void missingFilesAreFlaggedNotDeleted()
    {
        QVERIFY(QFile::remove(m_dir.filePath(QStringLiteral("media/TV/Show.S01E02.mkv"))));
        scanAndWait({m_dir.filePath(QStringLiteral("media"))});
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media WHERE missing=1")), 1);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media")), 4);
    }
    void movedFileKeepsHistoryAndFavourite()
    {
        QSqlQuery q(Database::open()); q.exec(QStringLiteral("SELECT id FROM media WHERE title LIKE 'Heat%'")); QVERIFY(q.next());
        const qint64 id = q.value(0).toLongLong(); q.finish();   // release the read snapshot (WAL)
        m_lib->setFavorite(id, true); m_lib->recordPlay(id); m_lib->saveProgress(id, 40000, 100000, false);
        QVERIFY(QDir().mkpath(m_dir.filePath(QStringLiteral("media/Archive"))));
        QVERIFY(QFile::rename(m_dir.filePath(QStringLiteral("media/Movies/Heat.1995.mkv")),
                              m_dir.filePath(QStringLiteral("media/Archive/Heat (1995) renamed.mkv"))));
        scanAndWait({m_dir.filePath(QStringLiteral("media"))});
        const auto m = m_lib->mediaInfo(id);
        QVERIFY(m.value(QStringLiteral("path")).toString().endsWith(QStringLiteral("renamed.mkv")));
        QVERIFY(m.value(QStringLiteral("favorite")).toBool());
        QCOMPARE(m.value(QStringLiteral("positionMs")).toLongLong(), 40000);
        QCOMPARE(m.value(QStringLiteral("missing")).toInt(), 0);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media WHERE title LIKE 'Heat%'")), 1);
    }
    void inaccessibleFolderDoesNotMarkMissing()
    {
        const int before = count(QStringLiteral("SELECT COUNT(*) FROM media WHERE missing=1"));
        QSignalSpy msg(m_lib, &LibraryManager::message);
        scanAndWait({m_dir.filePath(QStringLiteral("media")), m_dir.filePath(QStringLiteral("no/such/drive"))});
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media WHERE missing=1")), before);
        QVERIFY(msg.count() >= 1);
    }
    void resumeAndCompletion()
    {
        const qint64 id = m_lib->registerUrl(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("media/home video.mp4"))));
        QVERIFY(id > 0);
        m_lib->saveProgress(id, 3000, 600000, false);
        QVERIFY(!m_lib->resumeInfo(id).value(QStringLiteral("canResume")).toBool());     // too early
        m_lib->saveProgress(id, 120000, 600000, false);
        QVERIFY(m_lib->resumeInfo(id).value(QStringLiteral("canResume")).toBool());
        m_lib->saveProgress(id, 590000, 600000, false);                                   // near the end
        const auto r = m_lib->resumeInfo(id);
        QVERIFY(!r.value(QStringLiteral("canResume")).toBool());
        QVERIFY(m_lib->mediaInfo(id).value(QStringLiteral("completed")).toBool());
        QCOMPARE(m_lib->mediaInfo(id).value(QStringLiteral("positionMs")).toLongLong(), 0);
    }
    void historyRetentionAndPrivacy()
    {
        const qint64 id = m_lib->registerUrl(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("media/home video.mp4"))));
        m_lib->clearHistory();
        m_lib->recordPlay(id); m_lib->recordPlay(id);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM history")), 1);                  // de-duplicated within 30 min
        m_settings->set(QStringLiteral("privacy/historyEnabled"), false);
        m_lib->clearHistory(); m_lib->recordPlay(id); m_lib->saveProgress(id, 100000, 600000, false);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM history")), 0);                  // history off: nothing stored
        m_settings->set(QStringLiteral("privacy/historyEnabled"), true);
    }
    void streamCredentialsNeverStored()
    {
        const qint64 id = m_lib->registerUrl(QUrl(QStringLiteral("rtsp://alice:s3cret@cam.example.com/live?x=1")));
        QVERIFY(id > 0);
        const QString p = m_lib->mediaInfo(id).value(QStringLiteral("path")).toString();
        QVERIFY2(!p.contains(QStringLiteral("s3cret")) && !p.contains(QStringLiteral("alice")), qPrintable(p));
        QCOMPARE(m_lib->mediaInfo(id).value(QStringLiteral("kind")).toString(), QStringLiteral("stream"));
    }
    void playlistsAndModel()
    {
        const qint64 pl = m_lib->createPlaylist(QStringLiteral("Weekend"));
        QVERIFY(pl > 0); QVERIFY(m_lib->createPlaylist(QStringLiteral("  ")) < 0);
        QSqlQuery q(Database::open()); q.exec(QStringLiteral("SELECT id FROM media WHERE kind!='stream' ORDER BY id")); QVariantList ids;
        while (q.next()) ids << q.value(0);
        m_lib->addToPlaylist(pl, ids);
        QCOMPARE(m_lib->playlistMediaIds(pl).size(), ids.size());
        m_lib->moveInPlaylist(pl, 0, ids.size() - 1);
        QCOMPARE(m_lib->playlistMediaIds(pl).last(), ids.first());
        const QString out = m_dir.filePath(QStringLiteral("export.m3u8"));
        QVERIFY(m_lib->exportPlaylistFile(pl, out));
        const qint64 pl2 = m_lib->importPlaylistFile(out);
        QCOMPARE(m_lib->playlistMediaIds(pl2).size(), ids.size());
        // deleting a playlist must not delete media
        const int mediaBefore = count(QStringLiteral("SELECT COUNT(*) FROM media"));
        m_lib->deletePlaylist(pl);
        QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM media")), mediaBefore);

        // model: searching with LIKE wildcards must be literal
        const auto all = LibraryModel::query(QStringLiteral("all"), 0, QString(), QString(), false, 0);
        QVERIFY(all.size() >= 3);
        QCOMPARE(LibraryModel::query(QStringLiteral("all"), 0, QStringLiteral("%"), QString(), false, 0).size(), 0);
        QCOMPARE(LibraryModel::query(QStringLiteral("all"), 0, QStringLiteral("show"), QString(), false, 0).size(), 1);
        QCOMPARE(LibraryModel::query(QStringLiteral("playlist"), pl2, QString(), QString(), false, 0).size(), ids.size());
        QCOMPARE(LibraryModel::query(QStringLiteral("all"), 0, QString(), QStringLiteral("duration; DROP TABLE media"), false, 0).size(), all.size()); // sort key is whitelisted
    }
    void asyncModelDropsStaleResults()
    {
        LibraryModel model; QSignalSpy counted(&model, &LibraryModel::countChanged);
        model.setSearch(QStringLiteral("zzzz-nothing")); model.setSearch(QStringLiteral("show")); model.setSearch(QString());
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading() && counted.count() >= 1, 5000);
        QTest::qWait(300);
        QCOMPARE(model.count(), LibraryModel::query(QStringLiteral("all"), 0, QString(), QString(), false, 0).size());
    }
    void largeLibraryStaysResponsive()
    {
        QSqlDatabase db = Database::open(); db.transaction();
        QSqlQuery ins(db); ins.prepare(QStringLiteral("INSERT INTO media(path,title,kind,added_at,probed) VALUES(?,?, 'video', 1, 1)"));
        for (int i = 0; i < 30000; ++i) { ins.addBindValue(QStringLiteral("/big/f%1.mkv").arg(i)); ins.addBindValue(QStringLiteral("File %1").arg(i)); ins.exec(); }
        db.commit();
        QElapsedTimer t; t.start();
        const auto r = LibraryModel::query(QStringLiteral("all"), 0, QStringLiteral("File 29999"), QString(), false, 0);
        QCOMPARE(r.size(), 1); QVERIFY2(t.elapsed() < 1500, qPrintable(QStringLiteral("search took %1ms").arg(t.elapsed())));
        t.restart();
        const auto page = LibraryModel::query(QStringLiteral("added"), 0, QString(), QString(), false, 24);
        QCOMPARE(page.size(), 24); QVERIFY(t.elapsed() < 500);
    }
};
QTEST_MAIN(TstLibrary)
#include "tst_library.moc"
