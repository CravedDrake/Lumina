// Engine tests: these drive the REAL libVLC against REAL generated media.
#include <QtTest>

#include "playback/VideoFrameSource.h"
#include "playback/VlcEngine.h"

#include <QFileInfo>
#include <QSignalSpy>

using lumina::VlcEngine;
using State = lumina::VlcEngine::State;

static QStringList testArgs() { return {QStringLiteral("--aout=dummy"), QStringLiteral("--no-audio-time-stretch")}; }
static QUrl media(const char *name) { return QUrl::fromLocalFile(QStringLiteral(LUMINA_TESTDATA "/") + QString::fromLatin1(name)); }
static bool haveMedia() { return QFileInfo::exists(QStringLiteral(LUMINA_TESTDATA "/sample.mp4")); }

class TstEngine : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qRegisterMetaType<lumina::VlcEngine::State>(); }

    void createsAndReportsVersion()
    {
        VlcEngine e(testArgs());
        QVERIFY(e.isValid());
        qInfo() << "libVLC" << VlcEngine::versionString();
        QVERIFY(VlcEngine::versionString().startsWith(QLatin1String("3.")));
    }

    void playsLocalFileAndDeliversRealFrames()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        VlcEngine e(testArgs());
        QVERIFY(e.load(media("sample.mp4")));
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), State::Playing, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(e.frameSource()->hasFrame(), 5000);
        QCOMPARE(e.frameSource()->videoSize(), QSize(640, 360));   // exact visible size, no rescale

        // Frames must be genuinely decoded video: testsrc2 is colourful and changes over time.
        auto f1 = e.frameSource()->latest();
        QVERIFY(f1);
        const QImage img1 = f1->asImage().copy();
        QSet<QRgb> colours;
        for (int y = 0; y < img1.height(); y += 8)
            for (int x = 0; x < img1.width(); x += 8)
                colours.insert(img1.pixel(x, y));
        QVERIFY2(colours.size() > 8, "frame looks blank / simulated");
        QTest::qWait(700);
        const QImage img2 = e.frameSource()->latest()->asImage().copy();
        QVERIFY2(img1 != img2, "video frames are not advancing");

        QTRY_VERIFY_WITH_TIMEOUT(e.length() > 19000 && e.length() < 21000, 5000);
        QVERIFY(e.isSeekable());
    }

    void pauseResumeSeekRateVolume()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        VlcEngine e(testArgs());
        QSignalSpy times(&e, &VlcEngine::timeChanged);
        e.load(media("sample.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), State::Playing, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(times.count() > 2, 5000);   // event-driven time updates, no polling

        e.pause();
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), State::Paused, 5000);
        const qint64 t0 = e.time();
        QTest::qWait(600);
        QVERIFY2(qAbs(e.time() - t0) < 150, "time kept advancing while paused");
        e.play();
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), State::Playing, 5000);

        e.setTime(10000);
        QTRY_VERIFY_WITH_TIMEOUT(e.time() > 9500 && e.time() < 13000, 5000);
        e.setPosition(0.25);
        QTRY_VERIFY_WITH_TIMEOUT(e.time() > 4000 && e.time() < 8000, 5000);

        QVERIFY(e.setRate(2.0f));
        QVERIFY(e.setRate(0.25f));
        QVERIFY(e.setRate(1.0f));
        e.setVolume(55);  QCOMPARE(e.volume(), 55);
        e.setVolume(500); QCOMPARE(e.volume(), 200);  // clamped
        e.setMuted(true); QVERIFY(e.isMuted());
        QVERIFY(!e.audioTracks().isEmpty());
        QVERIFY(e.setAudioDelayMs(150));
        QVERIFY(e.setSubtitleDelayMs(-250));
        QVERIFY(e.setAdjust(QStringLiteral("brightness"), 1.3f));
        QVERIFY(!e.setAdjust(QStringLiteral("bogus"), 1.0f));
        e.resetAdjust();
    }

    void speedActuallyChangesPlaybackRate()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        VlcEngine e(testArgs());
        e.load(media("sample.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), State::Playing, 10000);
        QTest::qWait(500);
        QVERIFY(e.setRate(2.0f));
        QTest::qWait(300);
        const qint64 a = e.time();
        QTest::qWait(2000);
        const qint64 advanced = e.time() - a;
        QVERIFY2(advanced > 3000, qPrintable(QStringLiteral("2x advanced only %1 ms in 2 s").arg(advanced)));
    }

    void invalidAndMissingMediaReportErrors()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        {
            VlcEngine e(testArgs());
            QSignalSpy err(&e, &VlcEngine::errorOccurred);
            e.load(media("notmedia.mp4"));
            QTRY_VERIFY_WITH_TIMEOUT(err.count() >= 1 || e.state() == State::Error || e.state() == State::Stopped, 15000);
            QVERIFY2(e.state() == State::Error || e.state() == State::Stopped || e.state() == State::Ended,
                     "invalid file must not look like it is playing");
        }
        {
            VlcEngine e(testArgs());
            QSignalSpy err(&e, &VlcEngine::errorOccurred);
            e.load(QUrl::fromLocalFile(QStringLiteral("/nonexistent/path/nothing.mkv")));
            QTRY_VERIFY_WITH_TIMEOUT(err.count() >= 1 || e.state() == State::Error, 15000);
        }
    }

    void endOfStreamIsReported()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        VlcEngine e(testArgs());
        QSignalSpy ended(&e, &VlcEngine::endReached);
        e.load(media("sample.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), State::Playing, 10000);
        e.setTime(e.length() - 1200);
        QTRY_VERIFY_WITH_TIMEOUT(ended.count() == 1, 15000);
        QCOMPARE(e.state(), State::Ended);
    }

    void rapidMediaSwitchingIsSafe()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        VlcEngine e(testArgs());
        for (int i = 0; i < 25; ++i) {
            e.load(media(i % 2 ? "sample.mp4" : "black.mp4"));
            QTest::qWait(i % 3 ? 5 : 40);
        }
        e.load(media("sample.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(e.state(), State::Playing, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(e.frameSource()->hasFrame(), 5000);
        QCOMPARE(e.frameSource()->videoSize(), QSize(640, 360));   // exact visible size, no rescale
    }

    void destroyingEngineWhilePlayingIsSafe()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        for (int i = 0; i < 3; ++i) {
            auto *e = new VlcEngine(testArgs());
            e->load(media("sample.mp4"));
            QTRY_COMPARE_WITH_TIMEOUT(e->state(), State::Playing, 10000);
            delete e;                      // must not crash, hang or leave queued events dangling
            QCoreApplication::processEvents();
        }
    }

    void screenshotSourceIsRealFrame()
    {
        if (!haveMedia()) QSKIP("ffmpeg test media not available");
        VlcEngine e(testArgs());
        e.load(media("sample.mp4"));
        QTRY_VERIFY_WITH_TIMEOUT(e.frameSource()->hasFrame(), 10000);
        const auto f = e.frameSource()->latest();
        QVERIFY(f);
        QImage img = f->asImage().copy();
        QCOMPARE(img.size(), QSize(640, 360));
        QVERIFY(!img.isNull());
    }
};

QTEST_MAIN(TstEngine)
#include "tst_engine.moc"
