// Pure-logic tests for the supporting modules (no media needed).
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include "playback/QueueModel.h"
#include "playback/VideoItem.h"
#include <QElapsedTimer>
#include "playlists/M3u.h"
#include "settings/AppSettings.h"
#include "shortcuts/ShortcutManager.h"
#include "thumbnails/ThumbnailService.h"
#include "utilities/Redact.h"
#include "utilities/TimeFormat.h"

using namespace lumina;

class TstSupport : public QObject {
    Q_OBJECT
private slots:
    void redactionHidesCredentials()
    {
        const QString r = redactUrl(QStringLiteral("rtsp://alice:s3cret@cam.local:554/stream?token=abc"));
        QVERIFY(!r.contains(QLatin1String("s3cret")));
        QVERIFY(!r.contains(QLatin1String("alice")));
        QVERIFY(r.contains(QLatin1String("cam.local")));
        const QString line = redactLogLine(QStringLiteral("opening http://bob:pw123@host/a.mp4 and https://ok.example/x"));
        QVERIFY(!line.contains(QLatin1String("pw123")) && !line.contains(QLatin1String("bob")));
        QVERIFY(line.contains(QLatin1String("ok.example")));
        QVERIFY(urlHasCredentials(QStringLiteral("ftp://u:p@h/x")));
        QVERIFY(!urlHasCredentials(QStringLiteral("https://h/x")));
        QVERIFY(!urlHasCredentials(QStringLiteral("/home/me/video.mp4")));
    }
    void timeFormatting()
    {
        QCOMPARE(formatTime(0), QStringLiteral("0:00"));
        QCOMPARE(formatTime(65000), QStringLiteral("1:05"));
        QCOMPARE(formatTime(3723000), QStringLiteral("1:02:03"));
        QCOMPARE(parseTime(QStringLiteral("90")), qint64(90000));
        QCOMPARE(parseTime(QStringLiteral("1:30")), qint64(90000));
        QCOMPARE(parseTime(QStringLiteral("01:02:03")), qint64(3723000));
        QCOMPARE(parseTime(QStringLiteral("1h2m3s")), qint64(3723000));
        QCOMPARE(parseTime(QStringLiteral("banana")), qint64(-1));
        QCOMPARE(parseTime(QStringLiteral("")), qint64(-1));
        QVERIFY(formatBytes(1536).contains(QLatin1String("1.5")));
    }
    void m3uRoundTripAndRelativePaths()
    {
        QTemporaryDir d; QVERIFY(d.isValid());
        QVector<m3u::Entry> in = {{d.filePath(QStringLiteral("a b.mp4")), QStringLiteral("Title, with comma"), 90},
                                  {QStringLiteral("http://example.com/s.m3u8"), QString(), -1}};
        const QString path = d.filePath(QStringLiteral("list.m3u8"));
        QVERIFY(m3u::writeFile(path, in));
        const auto out = m3u::readFile(path);
        QCOMPARE(out.size(), 2);
        QCOMPARE(out[0].location, in[0].location);
        QCOMPARE(out[0].title, in[0].title);
        QCOMPARE(out[1].location, in[1].location);
        const auto rel = m3u::parse("#EXTM3U\n#EXTINF:10,Rel\nsub/clip.mkv\n\n# comment\n", QStringLiteral("/base"));
        QCOMPARE(rel.size(), 1);
        QCOMPARE(rel[0].location, QStringLiteral("/base/sub/clip.mkv"));
        QVERIFY(m3u::parse("garbage \0 binary", QStringLiteral("/b")).size() <= 1);
        QString err;
        QVERIFY(m3u::readFile(d.filePath(QStringLiteral("missing.m3u")), &err).isEmpty());
    }
    void aspectFitGeometry()
    {
        // 16:9 video in a 1000x1000 item: letter-boxed, width limited
        QSizeF s = VideoItem::fit({1000, 1000}, 16.0 / 9.0, QStringLiteral("auto"), 0);
        QVERIFY(qAbs(s.width() - 1000) < 0.5 && qAbs(s.height() - 562.5) < 0.5);
        // forced 4:3
        s = VideoItem::fit({1000, 1000}, 16.0 / 9.0, QStringLiteral("4:3"), 0);
        QVERIFY(qAbs(s.width() / s.height() - 4.0 / 3.0) < 0.01);
        // fill crops (covers the item)
        s = VideoItem::fit({1000, 500}, 1.0, QStringLiteral("fill"), 0);
        QVERIFY(s.width() >= 1000 - 0.5 && s.height() >= 500 - 0.5);
        // stretch ignores aspect
        s = VideoItem::fit({800, 300}, 2.0, QStringLiteral("stretch"), 0);
        QCOMPARE(s, QSizeF(800, 300));
        // 90° rotation swaps the content aspect
        s = VideoItem::fit({1000, 1000}, 2.0, QStringLiteral("auto"), 90);
        QVERIFY(qAbs(s.height() - 1000) < 0.5 && qAbs(s.width() - 500) < 0.5);
        QCOMPARE(VideoItem::parseRatio(QStringLiteral("21:9")), 21.0 / 9.0);
        QCOMPARE(VideoItem::parseRatio(QStringLiteral("auto")), 0.0);
    }
    void queueModelOperations()
    {
        QueueModel q;
        auto mk = [](int i) { QueueEntry e; e.mediaId = i; e.title = QString::number(i); e.url = QUrl(QStringLiteral("file:///%1.mp4").arg(i)); return e; };
        q.setEntries({mk(1), mk(2), mk(3), mk(4)}, 1);
        QCOMPARE(q.count(), 4); QCOMPARE(q.currentIndex(), 1);
        q.move(3, 0);                                   // current index follows its item
        QCOMPARE(q.at(q.currentIndex()).mediaId, qint64(2));
        QVERIFY(q.removeAt(0));
        QCOMPARE(q.at(q.currentIndex()).mediaId, qint64(2));
        q.insertNext({mk(9)});
        QCOMPARE(q.at(q.currentIndex() + 1).mediaId, qint64(9));
        q.append({mk(10)});
        QCOMPARE(q.at(q.count() - 1).mediaId, qint64(10));
        QVERIFY(!q.removeAt(99));
        q.clearExceptCurrent();
        QCOMPARE(q.count(), 1); QCOMPARE(q.at(0).mediaId, qint64(2));
    }
    void defaultShortcutsHaveNoConflicts() { QVERIFY2(ShortcutManager::defaultConflicts().isEmpty(), qPrintable(ShortcutManager::defaultConflicts().join(QLatin1Char(';')))); }
    void shortcutRebindingRejectsConflictsAndPersists()
    {
        QTemporaryDir d; AppSettings s(d.filePath(QStringLiteral("s.ini")));
        ShortcutManager m(&s);
        QCOMPARE(m.keysText(QStringLiteral("playPause")), QStringLiteral("Space"));
        QVERIFY(!m.setBinding(QStringLiteral("stop"), QStringLiteral("Space")).isEmpty());          // conflict message
        QVERIFY(m.setBinding(QStringLiteral("stop"), QStringLiteral("Ctrl+Shift+X")).isEmpty());
        QCOMPARE(m.keysText(QStringLiteral("stop")), QStringLiteral("Ctrl+Shift+X"));
        {
            ShortcutManager m2(&s);                                                              // persisted
            QCOMPARE(m2.keysText(QStringLiteral("stop")), QStringLiteral("Ctrl+Shift+X"));
        }
        m.resetBinding(QStringLiteral("stop"));
        QCOMPARE(m.keysText(QStringLiteral("stop")), QStringLiteral("X"));
    }
    void settingsValidateTypesAndReset()
    {
        QTemporaryDir d; AppSettings s(d.filePath(QStringLiteral("s.ini")));
        QVERIFY(!s.set(QStringLiteral("no/such/key"), 1));
        QVERIFY(s.set(QStringLiteral("audio/volume"), 55));
        QCOMPARE(s.getInt(QStringLiteral("audio/volume")), 55);
        { AppSettings again(d.filePath(QStringLiteral("s.ini"))); QCOMPARE(again.getInt(QStringLiteral("audio/volume")), 55); }
        s.resetAll();
        QCOMPARE(s.getInt(QStringLiteral("audio/volume")), 80);
        QVERIFY(s.defaultValues().value(QStringLiteral("privacy/historyEnabled")).toBool());                 // history on by default
        QVERIFY(!s.defaultValues().contains(QStringLiteral("privacy/onlineServices")));                      // no online features exist
    }
    void thumbnailsFromRealVideoAndCacheTrim()
    {
        const QString v = QStringLiteral(LUMINA_TESTDATA "/sample.mp4");
        if (!QFileInfo::exists(v)) QSKIP("no media");
        const QImage a = FrameGrabber::grab(v, 20000);
        QVERIFY(!a.isNull());
        QCOMPARE(a.size(), QSize(640, 360));
        const QImage b = FrameGrabber::grab(v, 20000, 15000);
        QVERIFY(!b.isNull());
        QVERIFY2(a != b, "different positions must give different frames");
        QVERIFY(FrameGrabber::grab(QStringLiteral(LUMINA_TESTDATA "/notmedia.mp4"), 0, -1, 4000).isNull());
        QVERIFY(FrameGrabber::grab(QStringLiteral("/nonexistent.mkv"), 0, -1, 3000).isNull());
    }
    void seekPreviewFramesAndCache()
    {
        const QString v = QStringLiteral(LUMINA_TESTDATA "/sample.mp4");
        if (!QFileInfo::exists(v)) QSKIP("no media");
        QTemporaryDir d;
        AppSettings s(d.filePath(QStringLiteral("s.ini")));
        ThumbnailService svc(&s, nullptr, d.filePath(QStringLiteral("thumbs")));
        const QImage a = svc.seekFrame(1, v, 20000, 2000);
        const QImage b = svc.seekFrame(1, v, 20000, 16000);
        QVERIFY(!a.isNull() && !b.isNull());
        QVERIFY(a.width() <= 240);
        QVERIFY2(a != b, "different positions give different preview frames");
        QElapsedTimer t; t.start();
        const QImage a2 = svc.seekFrame(1, v, 20000, 3000);     // same 5 s bucket -> served from cache
        QVERIFY2(t.elapsed() < 100, "cached bucket must not decode again");
        QCOMPARE(a2, a);
        QVERIFY(!s.getBool(QStringLiteral("performance/seekPreview")));   // off by default
    }
    void sidecarArtworkDiscovery()
    {
        QTemporaryDir d; QDir dir(d.path());
        QFile f(dir.filePath(QStringLiteral("Film.mkv"))); QVERIFY(f.open(QIODevice::WriteOnly)); f.close();
        QVERIFY(ThumbnailService::sidecarArtwork(dir.filePath(QStringLiteral("Film.mkv"))).isEmpty());
        QImage img(8, 8, QImage::Format_RGB32); img.fill(Qt::red);
        QVERIFY(img.save(dir.filePath(QStringLiteral("poster.jpg"))));
        QVERIFY(ThumbnailService::sidecarArtwork(dir.filePath(QStringLiteral("Film.mkv"))).endsWith(QLatin1String("poster.jpg")));
        QVERIFY(img.save(dir.filePath(QStringLiteral("Film.png"))));
        QVERIFY(ThumbnailService::sidecarArtwork(dir.filePath(QStringLiteral("Film.mkv"))).endsWith(QLatin1String("Film.png")));   // same-name wins
    }
};

QTEST_MAIN(TstSupport)
#include "tst_support.moc"
