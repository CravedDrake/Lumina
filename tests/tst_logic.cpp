#include <QtTest>
#include <QTemporaryDir>
#include "history/ResumePolicy.h"
#include "playlists/M3u.h"
#include "settings/AppSettings.h"
#include "subtitles/SubtitleFinder.h"
#include "utilities/MediaNameParser.h"
#include "utilities/MediaFiles.h"

using namespace lumina;

class TstLogic : public QObject {
    Q_OBJECT
private slots:
    void parser_episode_data()
    {
        QTest::addColumn<QString>("path"); QTest::addColumn<QString>("show"); QTest::addColumn<int>("s"); QTest::addColumn<int>("e");
        QTest::newRow("sxe")   << "/tv/Breaking.Bad.S02E05.1080p.BluRay.x264.mkv" << "Breaking Bad" << 2 << 5;
        QTest::newRow("lower") << "/tv/the_office_s03e11.avi" << "the office" << 3 << 11;
        QTest::newRow("nxm")   << "/tv/Seinfeld 4x12 The Contest.mp4" << "Seinfeld" << 4 << 12;
        QTest::newRow("words") << "/tv/Show Season 1 Episode 3.mkv" << "Show" << 1 << 3;
        QTest::newRow("folder")<< "/tv/Dark/Season 2/S02E03.mkv" << "Dark" << 2 << 3;
    }
    void parser_episode()
    {
        QFETCH(QString, path); QFETCH(QString, show); QFETCH(int, s); QFETCH(int, e);
        const auto p = parseMediaName(path);
        QCOMPARE(p.kind, QStringLiteral("episode"));
        QCOMPARE(p.showName, show); QCOMPARE(p.season, s); QCOMPARE(p.episode, e);
    }
    void parser_movie()
    {
        auto p = parseMediaName(QStringLiteral("/m/The.Matrix.1999.1080p.BluRay.x264.mkv"));
        QCOMPARE(p.kind, QStringLiteral("movie")); QCOMPARE(p.year, 1999); QCOMPARE(p.title, QStringLiteral("The Matrix"));
        p = parseMediaName(QStringLiteral("/m/Blade Runner 2049 (2017).mp4"));
        QCOMPARE(p.kind, QStringLiteral("movie")); QCOMPARE(p.year, 2017); QCOMPARE(p.title, QStringLiteral("Blade Runner 2049"));
    }
    void parser_doesNotGuess()
    {
        // 1080p is not an episode marker; a bare year-like title is not a movie; no year => unclassified.
        QCOMPARE(parseMediaName(QStringLiteral("/v/holiday clip 1080p.mp4")).kind, QStringLiteral("video"));
        QCOMPARE(parseMediaName(QStringLiteral("/v/2012.mp4")).kind, QStringLiteral("video"));   // title would be empty
        QCOMPARE(parseMediaName(QStringLiteral("/v/vacation.mov")).kind, QStringLiteral("video"));
        QVERIFY(!parseMediaName(QStringLiteral("/v/___.mp4")).title.isEmpty());
    }
    void resumePolicy()
    {
        using namespace resume;
        QVERIFY(!canResume(2000, 600000));                 // barely started
        QVERIFY(canResume(60000, 600000));
        QVERIFY(!canResume(590000, 600000));               // within the tail
        QVERIFY(!canResume(570000, 600000));               // >95%
        QVERIFY(isCompleted(571000, 600000));
        QVERIFY(!isCompleted(100000, 0));                  // unknown duration is never "completed"
        QVERIFY(canResume(900000, 0));                     // live/unknown length: allow
    }
    void m3u_roundTripAndRelative()
    {
        QTemporaryDir d; QVERIFY(d.isValid());
        const QByteArray text = "\xEF\xBB\xBF#EXTM3U\r\n#EXTINF:12,First \xE2\x80\x94 title\r\nsub/a.mp4\r\n#COMMENT\r\nhttp://example.com/s.m3u8\r\n/abs/b.mkv\r\n";
        auto e = m3u::parse(text, d.path());
        QCOMPARE(e.size(), 3);
        QCOMPARE(e[0].location, d.path() + QStringLiteral("/sub/a.mp4"));
        QCOMPARE(e[0].durationSec, 12);
        QVERIFY(e[0].title.startsWith(QStringLiteral("First")));
        QCOMPARE(e[1].location, QStringLiteral("http://example.com/s.m3u8"));
        QCOMPARE(e[2].location, QStringLiteral("/abs/b.mkv"));
        const QString out = d.filePath(QStringLiteral("o.m3u8"));
        QVERIFY(m3u::writeFile(out, e));
        const auto back = m3u::readFile(out);
        QCOMPARE(back.size(), 3); QCOMPARE(back[0].location, e[0].location); QCOMPARE(back[0].title, e[0].title);
    }
    void settings_validationAndPersistence()
    {
        QTemporaryDir d; const QString ini = d.filePath(QStringLiteral("s.ini"));
        {
            AppSettings s(ini);
            QCOMPARE(s.getInt(QStringLiteral("playback/seekSmall")), 10);
            QVERIFY(s.set(QStringLiteral("playback/seekSmall"), 15));
            QVERIFY(!s.set(QStringLiteral("nope/unknown"), 1));              // unknown key rejected
            QVERIFY(!s.set(QStringLiteral("playback/seekSmall"), QStringLiteral("abc"))); // not convertible
            QCOMPARE(s.getInt(QStringLiteral("playback/seekSmall")), 15);
            QVERIFY(s.set(QStringLiteral("privacy/historyEnabled"), false));
            QVERIFY(!s.getBool(QStringLiteral("privacy/historyEnabled")));
            s.saveWindowState(10, 20, 1200, 700, false);
        }
        AppSettings s2(ini);
        QCOMPARE(s2.getInt(QStringLiteral("playback/seekSmall")), 15);
        QCOMPARE(s2.windowState().value(QStringLiteral("width")).toInt(), 1200);
        s2.resetAll();
        QCOMPARE(s2.getInt(QStringLiteral("playback/seekSmall")), 10);
    }
    void subtitleFinder()
    {
        QTemporaryDir d;
        auto touch = [&](const QString &n) { QFile f(d.filePath(n)); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); };
        touch(QStringLiteral("Movie.mkv")); touch(QStringLiteral("Movie.srt")); touch(QStringLiteral("Movie.en.srt"));
        touch(QStringLiteral("Movie.french.ass")); touch(QStringLiteral("Other.srt")); touch(QStringLiteral("Movie.txt"));
        const auto c = findExternalSubtitles(d.filePath(QStringLiteral("Movie.mkv")));
        QCOMPARE(c.size(), 3);
        QVERIFY(c[0].path.endsWith(QStringLiteral("Movie.srt")));            // exact match ranks first
        QCOMPARE(languageRank(QStringLiteral("en"), {QStringLiteral("eng")}), 0);
        QVERIFY(languageRank(QStringLiteral("french"), {QStringLiteral("eng")}) > 0);
    }
    void fileTypes()
    {
        QVERIFY(media::isVideo(QStringLiteral("/a/B.MKV"))); QVERIFY(media::isAudio(QStringLiteral("x.flac")));
        QVERIFY(media::isSubtitle(QStringLiteral("x.vtt"))); QVERIFY(media::isPlaylist(QStringLiteral("l.m3u8")));
        QVERIFY(!media::isMedia(QStringLiteral("/dir.mp4/readme"))); QVERIFY(!media::isMedia(QStringLiteral("noext")));
    }
};
QTEST_MAIN(TstLogic)
#include "tst_logic.moc"
