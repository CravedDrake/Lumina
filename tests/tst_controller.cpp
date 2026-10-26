// Controller tests: the REAL PlaybackController, REAL libVLC, REAL generated media.
#include <cmath>
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "database/Database.h"
#include "library/LibraryManager.h"
#include "playback/PlaybackController.h"
#include "playback/VideoFrameSource.h"
#include "settings/AppSettings.h"

using namespace lumina;

static QString media(const char *n) { return QStringLiteral(LUMINA_TESTDATA "/") + QString::fromLatin1(n); }
static bool haveMedia() { return QFileInfo::exists(media("sample.mp4")); }

static double meanLuma(const QImage &img)
{
    double sum = 0; qint64 n = 0;
    for (int y = 0; y < img.height(); y += 4)
        for (int x = 0; x < img.width(); x += 4) { sum += qGray(img.pixel(x, y)); ++n; }
    return n ? sum / n : 0;
}

struct FrameStats { double luma = 0, contrast = 0, sat = 0, r = 0, g = 0, b = 0; };
static FrameStats frameStats(const QImage &img)
{
    FrameStats f; qint64 n = 0; double sq = 0;
    for (int y = 0; y < img.height(); y += 4)
        for (int x = 0; x < img.width(); x += 4) {
            const QColor c = img.pixelColor(x, y);
            const double l = qGray(img.pixel(x, y));
            f.luma += l; sq += l * l; f.r += c.red(); f.g += c.green(); f.b += c.blue();
            f.sat += c.hsvSaturationF(); ++n;
        }
    if (!n) return f;
    f.luma /= n; f.r /= n; f.g /= n; f.b /= n; f.sat /= n;
    f.contrast = std::sqrt(std::max(0.0, sq / n - f.luma * f.luma));
    return f;
}
// bounding box height of bright pixels (subtitle text) on a black frame
static int brightRows(const QImage &img, int *firstRow = nullptr)
{
    int first = -1, last = -1;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qGray(img.pixel(x, y)) > 128) { if (first < 0) first = y; last = y; break; }
    if (firstRow) *firstRow = first;
    return first < 0 ? 0 : last - first + 1;
}

class TstController : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    AppSettings *m_settings = nullptr;
    LibraryManager *m_lib = nullptr;
    PlaybackController *p = nullptr;

    void openAndPlay(const QString &path)
    {
        QVERIFY(p->open({path}, 0));
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(p->frameSource()->hasFrame(), 5000);
    }
private slots:
    void initTestCase()
    {
        qRegisterMetaType<lumina::VlcEngine::State>();
        QVERIFY(m_dir.isValid());
        QVERIFY(Database::initialize(m_dir.filePath(QStringLiteral("lib.db"))));
        m_settings = new AppSettings(m_dir.filePath(QStringLiteral("s.ini")), this);
        m_settings->set(QStringLiteral("playback/resume"), QStringLiteral("never"));
        m_settings->set(QStringLiteral("screenshots/dir"), m_dir.filePath(QStringLiteral("shots")));
        m_lib = new LibraryManager(m_settings, this);
        p = new PlaybackController(m_settings, m_lib, this);
        QVERIFY(p->engine()->isValid());
    }
    void init() { if (haveMedia()) {} p->closeMedia(); p->clearLoop(); p->setRate(1.0); p->setRepeatMode(QStringLiteral("off")); p->setShuffle(false); }
    void cleanupTestCase() { p->shutdown(); delete p; delete m_lib; Database::closeAll(); }

    void playPauseSeek()
    {
        if (!haveMedia()) QSKIP("no media");
        openAndPlay(media("sample.mp4"));
        QTRY_VERIFY_WITH_TIMEOUT(p->positionMs() > 500, 5000);
        QVERIFY(qAbs(p->durationMs() - 20000) < 400);
        p->pause();
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("paused"), 5000);
        const qint64 held = p->positionMs();
        QTest::qWait(600);
        QVERIFY(qAbs(p->positionMs() - held) < 200);
        p->seekTo(10000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(p->positionMs() - 10000) < 800, 5000);
        p->seekFraction(0.25);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(p->positionMs() - 5000) < 800, 5000);
        p->play();
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 5000);
        p->seekRelative(-100000);                       // clamps at the start, no crash
        QTRY_VERIFY_WITH_TIMEOUT(p->positionMs() < 1500, 5000);
    }
    void volumeAndSpeed()
    {
        if (!haveMedia()) QSKIP("no media");
        openAndPlay(media("sample.mp4"));
        p->setVolume(30); QCOMPARE(p->volume(), 30);
        p->volumeStep(5); QCOMPARE(p->volume(), 35);
        p->volumeStep(-500); QCOMPARE(p->volume(), 0);
        p->setVolume(500); QCOMPARE(p->volume(), 200);   // libVLC allows software amplification up to 200 %
        p->setMuted(true); QVERIFY(p->muted()); p->toggleMute(); QVERIFY(!p->muted());
        p->setRate(2.0); QCOMPARE(p->rate(), 2.0);
        const qint64 a = p->positionMs(); QTest::qWait(1500);
        QVERIFY2(p->positionMs() - a > 2200, "2x speed should advance ~3 s in 1.5 s");
        p->setRate(100.0); QVERIFY(p->rate() <= 4.0);
        p->setRate(0.01); QVERIFY(p->rate() >= 0.25);
        p->resetSpeed(); QCOMPARE(p->rate(), 1.0);
    }
    void frameStepAdvancesOneFrame()
    {
        if (!haveMedia()) QSKIP("no media");
        openAndPlay(media("sample.mp4"));
        p->pause();
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("paused"), 5000);
        const QImage before = p->frameSource()->latest()->asImage().copy();
        p->frameStep();
        QTest::qWait(400);
        const QImage after = p->frameSource()->latest()->asImage().copy();
        QVERIFY2(before != after, "frame step must show a different decoded frame");
        QCOMPARE(p->stateName(), QStringLiteral("paused"));
    }
    void queueNextPreviousAndEnd()
    {
        if (!haveMedia()) QSKIP("no media");
        QVERIFY(p->open({media("sample.mp4"), media("black.mp4")}, 0));
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        QCOMPARE(p->queue()->currentIndex(), 0);
        QVERIFY(p->canNext()); QVERIFY(!p->canPrevious());
        QVERIFY(p->next());
        QTRY_COMPARE_WITH_TIMEOUT(p->queue()->currentIndex(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        QVERIFY(!p->canNext());
        QSignalSpy ended(p, &PlaybackController::playbackEnded);
        p->seekTo(p->durationMs() - 700);
        QTRY_VERIFY_WITH_TIMEOUT(ended.count() == 1, 10000);
        QVERIFY(!p->isPlaying());
    }
    void autoAdvanceAndRepeat()
    {
        if (!haveMedia()) QSKIP("no media");
        QVERIFY(p->open({media("black.mp4"), media("sample.mp4")}, 0));
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        p->seekTo(11000);
        QTRY_COMPARE_WITH_TIMEOUT(p->queue()->currentIndex(), 1, 15000);   // auto-advanced
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);

        p->setRepeatMode(QStringLiteral("one"));
        QVERIFY(p->open({media("black.mp4")}, 0));
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        p->seekTo(11000);
        QTRY_VERIFY_WITH_TIMEOUT(p->positionMs() < 3000 && p->stateName() == QLatin1String("playing"), 15000);   // looped
    }
    void abLoopStaysInsideRange()
    {
        if (!haveMedia()) QSKIP("no media");
        openAndPlay(media("sample.mp4"));
        p->setLoop(2000, 4000);
        p->seekTo(2100);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(p->positionMs() - 2100) < 600, 5000);
        qint64 maxSeen = 0; int wraps = 0; qint64 prev = p->positionMs();
        QElapsedTimer t; t.start();
        while (t.elapsed() < 7000) {
            QTest::qWait(25);
            const qint64 now = p->positionMs();
            maxSeen = qMax(maxSeen, now);
            if (now + 500 < prev) ++wraps;
            prev = now;
        }
        QVERIFY2(maxSeen < 4600, qPrintable(QStringLiteral("position escaped the loop: %1").arg(maxSeen)));
        QVERIFY2(wraps >= 2, qPrintable(QStringLiteral("loop wrapped %1 times").arg(wraps)));
        p->clearLoop();
        QCOMPARE(p->loopA(), qint64(-1));
    }
    void errorsDoNotBreakThePlayer()
    {
        if (!haveMedia()) QSKIP("no media");
        p->open({media("notmedia.mp4")}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(p->stateName() == QLatin1String("error") || !p->errorText().isEmpty() || p->stateName() == QLatin1String("stopped"), 15000);
        p->open({QStringLiteral("/definitely/not/here.mkv")}, 0);
        QTest::qWait(500);
        openAndPlay(media("sample.mp4"));                     // still fully usable
        QVERIFY(p->errorText().isEmpty());
    }
    void rapidSwitchingIsSafe()
    {
        if (!haveMedia()) QSKIP("no media");
        for (int i = 0; i < 25; ++i) {
            p->open({media(i % 2 ? "sample.mp4" : "black.mp4")}, 0);
            QTest::qWait(30 + (i % 4) * 25);
            if (i % 5 == 0) p->seekTo(1000);
            if (i % 7 == 0) p->closeMedia();
        }
        openAndPlay(media("sample.mp4"));
        QCOMPARE(p->title(), QStringLiteral("sample"));
    }
    void screenshotsAreRealFramesAndNeverOverwrite()
    {
        if (!haveMedia()) QSKIP("no media");
        openAndPlay(media("sample.mp4"));
        p->pause();
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("paused"), 5000);
        const QString a = p->takeScreenshot();
        const QString b = p->takeScreenshot();
        QVERIFY(!a.isEmpty() && !b.isEmpty());
        QVERIFY(a != b);
        QImage img(a);
        QCOMPARE(img.size(), QSize(640, 360));
        QVERIFY(meanLuma(img) > 30);                         // colourful test pattern, not black/blank
        QVERIFY(img.pixelColor(10, 100) != img.pixelColor(600, 100));
    }
    void bookmarksPersist()
    {
        if (!haveMedia()) QSKIP("no media");
        const qint64 id = m_lib->registerUrl(QUrl::fromLocalFile(media("sample.mp4")));
        QVERIFY(id > 0);
        QVERIFY(p->openMediaIds({id}, 0));
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        p->seekTo(6000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(p->positionMs() - 6000) < 800, 5000);
        p->addBookmark(QStringLiteral("note"));
        QCOMPARE(p->bookmarks().size(), 1);
        const QVariantMap bm = p->bookmarks().first().toMap();
        QVERIFY(qAbs(bm.value(QStringLiteral("positionMs")).toLongLong() - 6000) < 1000);
        p->seekTo(0);
        p->seekToBookmark(bm.value(QStringLiteral("id")).toLongLong());
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(p->positionMs() - 6000) < 1000, 5000);
        p->removeBookmark(bm.value(QStringLiteral("id")).toLongLong());
        QCOMPARE(p->bookmarks().size(), 0);
    }
    void resumeAlwaysAskAndNever()
    {
        if (!haveMedia()) QSKIP("no media");
        const qint64 id = m_lib->registerUrl(QUrl::fromLocalFile(media("long.mp4")));
        QVERIFY(p->openMediaIds({id}, 0));
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        p->seekTo(40000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(p->positionMs() - 40000) < 800, 5000);
        p->savePosition();
        p->closeMedia();
        qInfo() << "stored resume info" << m_lib->resumeInfo(id);

        m_settings->set(QStringLiteral("playback/resume"), QStringLiteral("always"));
        QVERIFY(p->openMediaIds({id}, 0));
        QTRY_VERIFY_WITH_TIMEOUT(p->stateName() == QLatin1String("playing") && p->positionMs() > 39000, 10000);
        p->savePosition(); p->closeMedia();

        m_settings->set(QStringLiteral("playback/resume"), QStringLiteral("ask"));
        QVERIFY(p->openMediaIds({id}, 0));
        QTRY_VERIFY_WITH_TIMEOUT(!p->resumePrompt().isEmpty(), 10000);
        QVERIFY(qAbs(p->resumePrompt().value(QStringLiteral("positionMs")).toLongLong() - 40000) < 1500);
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("paused"), 10000);      // waits for the answer
        p->acceptResume();
        QTRY_VERIFY_WITH_TIMEOUT(p->stateName() == QLatin1String("playing") && p->positionMs() > 39000, 10000);
        QVERIFY(p->resumePrompt().isEmpty());
        p->closeMedia();

        QVERIFY(p->openMediaIds({id}, 0));
        QTRY_VERIFY_WITH_TIMEOUT(!p->resumePrompt().isEmpty(), 10000);
        p->declineResume();
        QTRY_VERIFY_WITH_TIMEOUT(p->stateName() == QLatin1String("playing") && p->positionMs() < 3000, 10000);
        m_settings->set(QStringLiteral("playback/resume"), QStringLiteral("never"));
    }
    void tracksAndChaptersFromContainer()
    {
        if (!QFileInfo::exists(media("multi.mkv"))) QSKIP("multi.mkv not generated");
        m_settings->set(QStringLiteral("subtitles/autoEnable"), false);
        openAndPlay(media("multi.mkv"));
        QTRY_VERIFY_WITH_TIMEOUT(p->audioTracks().size() >= 2, 8000);
        QTRY_COMPARE_WITH_TIMEOUT(p->chapters().size(), 3, 8000);
        QCOMPARE(p->chapters().at(1).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Middle"));
        // audio track switching
        int other = -1;
        for (const QVariant &t : p->audioTracks()) { const int id = t.toMap().value(QStringLiteral("id")).toInt(); if (id >= 0 && id != p->audioTrack()) other = id; }
        QVERIFY(other >= 0);
        p->setAudioTrack(other);
        QTRY_COMPARE_WITH_TIMEOUT(p->audioTrack(), other, 5000);
        p->cycleAudioTrack();
        QTRY_VERIFY_WITH_TIMEOUT(p->audioTrack() != other, 5000);
        // embedded subtitle track present and selectable
        int subId = -1;
        QTRY_VERIFY_WITH_TIMEOUT([&] { for (const QVariant &t : p->subtitleTracks()) { const int id = t.toMap().value(QStringLiteral("id")).toInt(); if (id >= 0) { subId = id; return true; } } return false; }(), 8000);
        p->setSubtitleTrack(subId);
        QTRY_COMPARE_WITH_TIMEOUT(p->subtitleTrack(), subId, 5000);
        p->setSubtitleTrack(-1);
        QTRY_COMPARE_WITH_TIMEOUT(p->subtitleTrack(), -1, 5000);
        // chapters
        p->setChapter(2);
        QTRY_VERIFY_WITH_TIMEOUT(p->positionMs() > 39000, 8000);
        p->previousChapter();
        QTRY_VERIFY_WITH_TIMEOUT(p->positionMs() > 19000 && p->positionMs() < 41000, 8000);
        p->nextChapter();
        QTRY_VERIFY_WITH_TIMEOUT(p->positionMs() > 39000, 8000);
        // delays
        p->setAudioDelayMs(250); QCOMPARE(p->audioDelayMs(), qint64(250));
        p->adjustSubtitleDelay(-100); QCOMPARE(p->subtitleDelayMs(), qint64(-100));
        p->setSubtitleDelayMs(0); p->setAudioDelayMs(0);
        m_settings->set(QStringLiteral("subtitles/autoEnable"), true);
    }
    void videoAdjustmentChangesPixels()
    {
        if (!haveMedia()) QSKIP("no media");
        openAndPlay(media("sample.mp4"));
        p->pause();
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("paused"), 5000);
        p->seekTo(5000); QTest::qWait(800);
        const double base = meanLuma(p->frameSource()->latest()->asImage());
        p->setAdjustValue(QStringLiteral("brightness"), 1.8);
        QTest::qWait(300); p->seekTo(5000); QTest::qWait(800);
        const double bright = meanLuma(p->frameSource()->latest()->asImage());
        p->resetAdjust();
        qInfo() << "brightness mean luma" << base << "->" << bright;
        QVERIFY2(bright > base + 3, "brightness adjustment must visibly change the decoded frame");
    }
    void videoAdjustmentsAllMeasured()
    {
        if (!haveMedia()) QSKIP("no media");
        openAndPlay(media("sample.mp4"));
        p->pause();
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("paused"), 5000);
        auto grab = [&] { p->seekTo(5000); QTest::qWait(900); return frameStats(p->frameSource()->latest()->asImage()); };
        const FrameStats base = grab();
        p->setAdjustValue(QStringLiteral("contrast"), 2.0);   const FrameStats con = grab(); p->resetAdjust();
        p->setAdjustValue(QStringLiteral("saturation"), 0.0); const FrameStats sat = grab(); p->resetAdjust();
        p->setAdjustValue(QStringLiteral("gamma"), 3.0);      const FrameStats gam = grab(); p->resetAdjust();
        p->setAdjustValue(QStringLiteral("hue"), 120.0);      const FrameStats hue = grab(); p->resetAdjust();
        const FrameStats reset = grab();
        qInfo() << "base" << base.luma << base.contrast << base.sat << "| contrast" << con.contrast << "| sat0" << sat.sat
                << "| gamma" << gam.luma << "| hue rgb" << base.r << base.g << base.b << "->" << hue.r << hue.g << hue.b;
        QVERIFY2(qAbs(con.contrast - base.contrast) > 3, "contrast");
        QVERIFY2(sat.sat < base.sat * 0.5, "saturation 0 must desaturate");
        QVERIFY2(qAbs(gam.luma - base.luma) > 3, "gamma");
        QVERIFY2(qAbs(hue.r - base.r) + qAbs(hue.g - base.g) + qAbs(hue.b - base.b) > 6, "hue");
        QVERIFY2(qAbs(reset.luma - base.luma) < 2, "reset restores the image");
    }
    void subtitlesDiscoveredRenderedAndStyled()
    {
        if (!haveMedia()) QSKIP("no media");
        QDir().mkpath(m_dir.filePath(QStringLiteral("sub")));
        const QString v = m_dir.filePath(QStringLiteral("sub/movie.mp4"));
        QVERIFY(QFile::copy(media("black.mp4"), v));
        QVERIFY(QFile::copy(media("black.srt"), m_dir.filePath(QStringLiteral("sub/movie.en.srt"))));
        m_settings->set(QStringLiteral("subtitles/autoEnable"), true);
        QVERIFY(p->open({v}, 0));
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(p->externalSubtitles().size(), 1, 5000);
        p->seekTo(4000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(p->positionMs() - 4000) < 700, 5000);
        auto textRows = [&](int *first = nullptr) { return brightRows(p->frameSource()->latest()->asImage(), first); };
        int row = -1;
        int h18 = 0;
        QTRY_VERIFY_WITH_TIMEOUT((h18 = textRows(&row)) > 4, 8000);          // subtitle text appears on black
        QVERIFY2(row > 360 / 2, "subtitle should sit in the lower half");
        // turn off -> gone
        p->setSubtitleTrack(-1);
        QTRY_COMPARE_WITH_TIMEOUT(textRows(), 0, 6000);
        // back on, then larger style
        p->loadSubtitleFile(v.left(v.size() - 4) + QStringLiteral(".en.srt"));
        QTRY_VERIFY_WITH_TIMEOUT(textRows() > 4, 8000);
        m_settings->set(QStringLiteral("subtitles/size"), 36);
        p->applySubtitleStyle();
        QTRY_COMPARE_WITH_TIMEOUT(p->stateName(), QStringLiteral("playing"), 10000);
        QTest::qWait(2500);                                     // let the re-opened media render its first subtitle frames
        int h36 = 0;
        QTRY_VERIFY_WITH_TIMEOUT((h36 = textRows()) > 4, 12000);
        qInfo() << "subtitle text height size18=" << h18 << "size36=" << h36;
        QVERIFY2(h36 > h18 * 1.4, "doubling the size setting must visibly enlarge the text");
        m_settings->set(QStringLiteral("subtitles/size"), 18);
    }
};

QTEST_MAIN(TstController)
#include "tst_controller.moc"
