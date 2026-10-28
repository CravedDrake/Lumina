#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QKeySequence>
#include <memory>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QJsonValue>
#include <QByteArray>
#include <QStringList>
#include <QCoreApplication>
#include <QFont>
#include <QObject>
#include <QString>
#include <QLatin1String>

#include "app/AppController.h"
#include "app/IconProvider.h"
#include "database/Database.h"
#include "library/LibraryManager.h"
#include "library/LibraryModel.h"
#include "playback/PlaybackController.h"
#include "playback/QueueModel.h"
#include "playback/VideoFrameSource.h"
#include "playback/VideoItem.h"
#include "settings/AppSettings.h"
#include "shortcuts/ShortcutManager.h"
#include "thumbnails/ThumbnailService.h"
#include "utilities/Logger.h"

using namespace lumina;

int main(int argc, char *argv[])
{
    QGuiApplication::setOrganizationName(QStringLiteral("Lumina"));
    QGuiApplication::setApplicationName(QStringLiteral("Lumina"));
    QGuiApplication::setApplicationVersion(QStringLiteral(LUMINA_VERSION));
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("Lumina media player"));
    cli.addHelpOption();
    cli.addVersionOption();
    cli.addPositionalArgument(QStringLiteral("media"), QStringLiteral("Files, folders, playlists or network URLs to play."), QStringLiteral("[media...]"));
    QCommandLineOption fullscreenOpt(QStringLiteral("fullscreen"), QStringLiteral("Start in fullscreen."));
    cli.addOption(fullscreenOpt);
    QCommandLineOption shotOpt(QStringLiteral("ui-screenshot"), QStringLiteral("(testing) save a screenshot of the window after loading and exit."), QStringLiteral("file"));
    QCommandLineOption shotDelayOpt(QStringLiteral("ui-screenshot-delay"), QStringLiteral("(testing) delay in ms before the screenshot."), QStringLiteral("ms"), QStringLiteral("1500"));
    QCommandLineOption pageOpt(QStringLiteral("page"), QStringLiteral("(testing) start on this page."), QStringLiteral("page"));
    QCommandLineOption scanOpt(QStringLiteral("add-library-folder"), QStringLiteral("Add a folder to the library and scan it."), QStringLiteral("folder"));
    cli.addOption(scanOpt);
    QCommandLineOption selfTestOpt(QStringLiteral("ui-selftest"), QStringLiteral("(testing) drive the real window with key events and verify the results."));
    cli.addOption(selfTestOpt);
    cli.addOption(shotOpt);
    cli.addOption(shotDelayOpt);
    QCommandLineOption panelOpt(QStringLiteral("panel"), QStringLiteral("(testing) open this player panel."), QStringLiteral("name"));
    QCommandLineOption catOpt(QStringLiteral("settings-category"), QStringLiteral("(testing) settings category index."), QStringLiteral("n"));
    cli.addOption(pageOpt);
    cli.addOption(panelOpt);
    cli.addOption(catOpt);
    cli.process(app);

    // ------------------------------------------------------------------ storage locations
    const QString dataOverride = qEnvironmentVariable("LUMINA_DATA_DIR");
    QString dataDir = dataOverride.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : dataOverride;
    QString cacheDir = dataOverride.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                                              : QDir(dataOverride).filePath(QStringLiteral("cache"));
    QDir().mkpath(dataDir);
    QDir().mkpath(cacheDir);

    // ------------------------------------------------------------------ single instance
    // A second launch hands its media arguments to the running instance and exits.
    // Skipped for the testing options so tests never talk to a real session.
    const bool testing = cli.isSet(QStringLiteral("ui-selftest")) || cli.isSet(QStringLiteral("ui-screenshot"));
    const QString serverName = QStringLiteral("lumina-") + QString::fromLatin1(
        QCryptographicHash::hash(QDir(dataDir).absolutePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
    auto resolveArgs = [](const QStringList &in) {
        QVariantList locs;
        for (const QString &a : in) {
            const QUrl u = QUrl::fromUserInput(a, QDir::currentPath(), QUrl::AssumeLocalFile);
            locs << (u.isLocalFile() ? u.toLocalFile() : u.toString());
        }
        return locs;
    };
    if (!testing) {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            const QJsonArray arr = QJsonArray::fromVariantList(resolveArgs(cli.positionalArguments()));
            probe.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
            probe.flush();
            probe.waitForBytesWritten(1000);
            probe.disconnectFromServer();
            return 0;
        }
    }
    if (!dataOverride.isEmpty()) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dataDir);
    }

    Logger::instance()->install(QDir(dataDir).filePath(QStringLiteral("logs")));
    qCInfo(lcApp) << "Lumina" << LUMINA_VERSION << "starting; data dir" << dataDir;

    QString dbError;
    bool recreated = false;
    if (!Database::initialize(QDir(dataDir).filePath(QStringLiteral("library.db")), &dbError, &recreated)) {
        qCCritical(lcApp) << "Database unavailable:" << dbError;
        // The player still works for direct playback; library features report errors instead of crashing.
    }

    for (const char *f : {"Regular", "Medium", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(QStringLiteral(":/resources/fonts/Inter-") + QString::fromLatin1(f) + QStringLiteral(".otf"));
    QFont base(QStringLiteral("Inter"));
    base.setPixelSize(14);
    QGuiApplication::setFont(base);

    // ------------------------------------------------------------------ services
    AppSettings settings;
    LibraryManager library(&settings);
    PlaybackController player(&settings, &library);
    ShortcutManager shortcuts(&settings);
    auto *thumbs = new ThumbnailService(&settings, &library, QDir(cacheDir).filePath(QStringLiteral("thumbnails")));
    AppController appController(&settings, &library, &player, thumbs, dataDir, cacheDir);

    if (recreated)
        QTimer::singleShot(1500, &library, [&library] {
            emit library.message(QObject::tr("The library database was damaged and has been reset. A backup copy was kept."), QStringLiteral("warning"));
        });

    qmlRegisterType<VideoItem>("Lumina.Core", 1, 0, "VideoItem");
    qmlRegisterType<LibraryModel>("Lumina.Core", 1, 0, "LibraryModel");
    qmlRegisterUncreatableType<QueueModel>("Lumina.Core", 1, 0, "QueueModel", QStringLiteral("owned by Player"));
    qmlRegisterUncreatableType<VideoFrameSource>("Lumina.Core", 1, 0, "VideoFrameSource", QStringLiteral("owned by Player"));

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("thumb"), thumbs);     // engine takes ownership
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("Player"), &player);
    ctx->setContextProperty(QStringLiteral("Library"), &library);
    ctx->setContextProperty(QStringLiteral("Settings"), &settings);
    ctx->setContextProperty(QStringLiteral("Shortcuts"), &shortcuts);
    ctx->setContextProperty(QStringLiteral("App"), &appController);
    ctx->setContextProperty(QStringLiteral("AppLog"), Logger::instance());
    ctx->setContextProperty(QStringLiteral("StartupPage"), cli.value(pageOpt));
    ctx->setContextProperty(QStringLiteral("StartupPanel"), cli.value(panelOpt));
    ctx->setContextProperty(QStringLiteral("StartupCategory"), cli.value(catOpt).toInt());
    ctx->setContextProperty(QStringLiteral("StartupOpen"), !cli.positionalArguments().isEmpty());
    ctx->setContextProperty(QStringLiteral("StartFullscreen"), cli.isSet(fullscreenOpt));

    QObject::connect(&appController, &AppController::quitRequested, &app, &QGuiApplication::quit);
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &player, &PlaybackController::shutdown);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/Lumina/UI/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 2;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window) {
        window->installEventFilter(&shortcuts);
    }

    if (settings.getBool(QStringLiteral("library/autoScan")))
        QTimer::singleShot(800, &library, [&library] { if (!library.folders().isEmpty()) library.rescan(); });

    for (const QString &f : cli.values(scanOpt)) library.addFolder(QDir(f).absolutePath());

    const QStringList args = cli.positionalArguments();
    if (!args.isEmpty()) {
        const QVariantList locs = resolveArgs(args);
        QTimer::singleShot(0, &player, [&player, locs] { player.open(locs, 0); });
    }

    QLocalServer instanceServer;
    if (!testing) {
        QLocalServer::removeServer(serverName);          // stale socket from a crashed run
        if (instanceServer.listen(serverName)) {
            instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
            QObject::connect(&instanceServer, &QLocalServer::newConnection, &app, [&instanceServer, &player, window] {
                while (QLocalSocket *c = instanceServer.nextPendingConnection()) {
                    QObject::connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
                    auto handle = [c, &player] {
                        if (!c->bytesAvailable()) return;
                        static constexpr qint64 kMax = 1 << 20;                 // untrusted input: bounded, parsed as JSON only
                        const QByteArray data = c->read(kMax);
                        const QJsonDocument d = QJsonDocument::fromJson(data);
                        QVariantList locs;
                        if (d.isArray()) for (const QJsonValue &v : d.array()) if (v.isString()) locs << v.toString();
                        if (!locs.isEmpty()) player.open(locs, 0);
                    };
                    QObject::connect(c, &QLocalSocket::readyRead, c, handle);
                    handle();
                }
                if (window) {
                    window->show();
                    if (window->windowStates() & Qt::WindowMinimized) window->showNormal();
                    window->raise();
                    window->requestActivate();
                }
            });
        } else qCWarning(lcApp) << "single-instance server unavailable:" << instanceServer.errorString();
    }

    if (cli.isSet(selfTestOpt) && window) {
        settings.set(QStringLiteral("screenshots/dir"), QDir(dataDir).filePath(QStringLiteral("shots")));
        settings.set(QStringLiteral("playback/resume"), QStringLiteral("never"));
        auto *st = new QTimer(&app);
        st->setInterval(150);
        struct Ctx { int step = 0; int waited = 0; qint64 mark = 0; int ok = 0, bad = 0; int vol = 0; };
        auto ctx = std::make_shared<Ctx>();
        auto key = [window](int k, Qt::KeyboardModifiers m = Qt::NoModifier) {
            QKeyEvent press(QEvent::KeyPress, k, m, QKeySequence(k).toString());
            QKeyEvent release(QEvent::KeyRelease, k, m);
            QCoreApplication::sendEvent(window, &press);
            QCoreApplication::sendEvent(window, &release);
        };
        auto check = [ctx](bool cond, const char *what) {
            qInfo().noquote() << (cond ? "SELFTEST PASS" : "SELFTEST FAIL") << what;
            (cond ? ctx->ok : ctx->bad)++;
        };
        QObject::connect(st, &QTimer::timeout, &app, [&, ctx, key, check, window, st] {
            auto wait = [&](int ticks) { if (++ctx->waited < ticks) return true; ctx->waited = 0; return false; };
            switch (ctx->step) {
            case 0: if (player.isPlaying() && player.positionMs() > 500) { check(true, "media playing in real window"); ++ctx->step; } else if (wait(100)) break; else { check(false, "media never started"); ctx->step = 99; } break;
            case 1: key(Qt::Key_Space); ++ctx->step; break;
            case 2: if (wait(6)) break; check(player.stateName() == QLatin1String("paused"), "Space pauses"); key(Qt::Key_Space); ++ctx->step; break;
            case 3: if (wait(6)) break; check(player.isPlaying(), "Space resumes"); ctx->mark = player.positionMs(); key(Qt::Key_Right); ++ctx->step; break;
            case 4: if (wait(10)) break; check(player.positionMs() - ctx->mark > 7000, "Right seeks forward"); ctx->vol = player.volume(); key(Qt::Key_Up); ++ctx->step; break;
            case 5: if (wait(3)) break; check(player.volume() == ctx->vol + 5, "Up raises volume"); key(Qt::Key_M); ++ctx->step; break;
            case 6: if (wait(3)) break; check(player.muted(), "M mutes"); key(Qt::Key_M); ++ctx->step; break;
            case 7: if (wait(3)) break; check(!player.muted(), "M unmutes"); key(Qt::Key_Q); ++ctx->step; break;
            case 8: if (wait(4)) break; check(window->property("debugPanel").toString() == QLatin1String("queue"), "Q opens queue panel"); key(Qt::Key_Escape); ++ctx->step; break;
            case 9: if (wait(4)) break; check(window->property("debugPanel").toString().isEmpty(), "Esc closes the panel"); key(Qt::Key_BracketRight); ++ctx->step; break;
            case 10: if (wait(3)) break; check(player.rate() > 1.0, "] speeds up"); key(Qt::Key_Backspace); ++ctx->step; break;
            case 11: if (wait(3)) break; check(qFuzzyCompare(player.rate(), 1.0), "Backspace resets speed"); key(Qt::Key_L); ++ctx->step; break;
            case 12: if (wait(8)) break; check(player.loopA() >= 0, "L sets loop start"); key(Qt::Key_L); ++ctx->step; break;
            case 13: if (wait(3)) break; check(player.loopB() >= 0, "L sets loop end"); key(Qt::Key_L); ++ctx->step; break;
            case 14: if (wait(3)) break; check(player.loopA() < 0, "L clears loop"); key(Qt::Key_C); ++ctx->step; break;
            case 15: if (wait(10)) break; check(!settings.getString(QStringLiteral("screenshots/dir")).isEmpty() && QDir(settings.getString(QStringLiteral("screenshots/dir"))).entryList(QDir::Files).size() >= 1, "C saves a screenshot"); key(Qt::Key_X); ++ctx->step; break;
            case 16: if (wait(6)) break; check(!player.isPlaying() && !window->property("debugPlayerVisible").toBool(), "X stops and leaves the player"); ++ctx->step; break;
            default:
                qInfo().noquote() << QStringLiteral("SELFTEST DONE passed=%1 failed=%2").arg(ctx->ok).arg(ctx->bad);
                st->stop();
                QCoreApplication::exit(ctx->bad ? 1 : 0);
            }
        });
        st->start();
    }

    if (cli.isSet(shotOpt) && window) {
        const QString out = cli.value(shotOpt);
        QTimer::singleShot(cli.value(shotDelayOpt).toInt(), &app, [window, out] {
            window->grabWindow().save(out);
            QGuiApplication::quit();
        });
    }

    const int rc = app.exec();
    if (window) window->removeEventFilter(&shortcuts);
    QThreadPool::globalInstance()->waitForDone(3000);
    return rc;
}
