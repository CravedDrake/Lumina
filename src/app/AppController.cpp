#include "AppController.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QSysInfo>
#include <QDateTime>
#include <QStandardPaths>
#include <QUrl>
#include <QVariant>
#include <QStringList>
#include <QObject>
#include <QString>

#include "library/LibraryManager.h"
#include "playback/PlaybackController.h"
#include "settings/AppSettings.h"
#include "thumbnails/ThumbnailService.h"
#include "utilities/Logger.h"
#include "utilities/MediaFiles.h"
#include "utilities/Redact.h"
#include "utilities/TimeFormat.h"

namespace lumina {

AppController::AppController(AppSettings *settings, LibraryManager *library, PlaybackController *player,
                             ThumbnailService *thumbs, const QString &dataDir, const QString &cacheDir, QObject *parent)
    : QObject(parent), m_settings(settings), m_library(library), m_player(player), m_thumbs(thumbs),
      m_dataDir(dataDir), m_cacheDir(cacheDir)
{
}

QString AppController::version() const { return QStringLiteral(LUMINA_VERSION); }
QString AppController::vlcVersion() const { return VlcEngine::versionString(); }
QString AppController::qtVersion() const { return QString::fromLatin1(qVersion()); }
QString AppController::platform() const { return QSysInfo::prettyProductName(); }

QStringList AppController::mediaFilters() const
{
    QStringList v, a;
    for (const auto &e : media::videoExtensions()) v << QStringLiteral("*.") + e;
    for (const auto &e : media::audioExtensions()) a << QStringLiteral("*.") + e;
    v.sort(); a.sort();
    return {QObject::tr("Media files (%1)").arg((v + a).join(QLatin1Char(' '))),
            QObject::tr("Video (%1)").arg(v.join(QLatin1Char(' '))),
            QObject::tr("Audio (%1)").arg(a.join(QLatin1Char(' '))),
            QObject::tr("All files (*)")};
}
QStringList AppController::subtitleFilters() const
{
    QStringList s;
    for (const auto &e : media::subtitleExtensions()) s << QStringLiteral("*.") + e;
    s.sort();
    return {QObject::tr("Subtitles (%1)").arg(s.join(QLatin1Char(' '))), QObject::tr("All files (*)")};
}
QStringList AppController::playlistFilters() const
{
    return {QObject::tr("Playlists (*.m3u *.m3u8)"), QObject::tr("All files (*)")};
}
QString AppController::moviesFolder() const
{
    const QString m = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return m.isEmpty() ? QDir::homePath() : m;
}
qint64 AppController::thumbnailCacheBytes() const { return m_thumbs ? m_thumbs->cacheBytes() : 0; }

QString AppController::toLocalPath(const QUrl &url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QVariantList AppController::classifyUrls(const QVariantList &urls) const
{
    QVariantList out;
    for (const QVariant &v : urls) {
        const QUrl u = v.toUrl();
        const QString loc = u.isLocalFile() ? u.toLocalFile() : u.toString();
        QString kind = QStringLiteral("other");
        if (u.isLocalFile()) {
            const QFileInfo fi(loc);
            if (fi.isDir()) kind = QStringLiteral("folder");
            else if (media::isPlaylist(loc)) kind = QStringLiteral("playlist");
            else if (media::isSubtitle(loc)) kind = QStringLiteral("subtitle");
            else if (media::isMedia(loc)) kind = QStringLiteral("media");
        } else if (!u.scheme().isEmpty()) {
            kind = QStringLiteral("stream");
        }
        out << QVariantMap{{QStringLiteral("location"), loc}, {QStringLiteral("kind"), kind}};
    }
    return out;
}

void AppController::copyText(const QString &text)
{
    if (auto *cb = QGuiApplication::clipboard()) cb->setText(text);
}

bool AppController::revealInFileManager(const QString &path)
{
    // No portable "select this file" API; open the containing folder.
    return openFolderInFileManager(QFileInfo(path).absolutePath());
}
bool AppController::openFolderInFileManager(const QString &path)
{
    if (!QFileInfo(path).isDir()) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
bool AppController::fileExists(const QString &path) const { return QFileInfo::exists(path); }

QString AppController::safeUrl(const QString &url) const { return redactUrl(url); }
QString AppController::formatTime(qint64 ms) const { return lumina::formatTime(ms); }
qint64 AppController::parseTime(const QString &text) const { return lumina::parseTime(text); }
QString AppController::formatBytes(qint64 bytes) const { return lumina::formatBytes(bytes); }
QString AppController::formatDate(qint64 secs) const
{
    if (secs <= 0) return QString();
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(secs), QLocale::ShortFormat);
}

QString AppController::diagnostics() const
{
    QStringList l;
    l << QStringLiteral("Lumina %1").arg(version());
    l << QStringLiteral("libVLC %1").arg(vlcVersion());
    l << QStringLiteral("Qt %1 (%2)").arg(qtVersion(), QGuiApplication::platformName());
    l << QStringLiteral("OS: %1 / %2").arg(platform(), QSysInfo::currentCpuArchitecture());
    l << QStringLiteral("Settings: %1").arg(m_settings->fileName());
    l << QStringLiteral("Database: %1").arg(m_library->databasePath());
    l << QStringLiteral("Cache: %1 (%2)").arg(m_cacheDir, lumina::formatBytes(thumbnailCacheBytes()));
    l << QStringLiteral("Media in library: %1").arg(m_library->mediaCount());
    l << QStringLiteral("Hardware decoding preference: %1").arg(m_settings->getString(QStringLiteral("performance/hwDecoding")));
    return l.join(QLatin1Char('\n'));
}

void AppController::clearThumbnailCache()
{
    if (m_thumbs) m_thumbs->clearCache();
    emit cacheChanged();
    emit message(tr("Thumbnail cache cleared."), QStringLiteral("success"));
}

void AppController::clearAllPrivateData()
{
    m_library->clearHistory();
    forgetStreams();
    clearThumbnailCache();
    Logger::instance()->clear();
    emit message(tr("History, recent streams, thumbnails and the in-app log were cleared."), QStringLiteral("success"));
}

QVariantList AppController::recentStreams() const
{
    QVariantList out;
    for (const QString &u : m_settings->getList(QStringLiteral("privacy/streamHistory"))) out << u;
    return out;
}
void AppController::rememberStream(const QString &url)
{
    if (!m_settings->getBool(QStringLiteral("privacy/historyEnabled"))) return;
    if (urlHasCredentials(url)) return;                    // never persist credentials
    QStringList l = m_settings->getList(QStringLiteral("privacy/streamHistory"));
    l.removeAll(url);
    l.prepend(url);
    const int cap = qMax(0, m_settings->getInt(QStringLiteral("privacy/recentStreams")));
    while (l.size() > cap) l.removeLast();
    m_settings->set(QStringLiteral("privacy/streamHistory"), l);
}
void AppController::forgetStreams() { m_settings->set(QStringLiteral("privacy/streamHistory"), QStringList()); }

void AppController::quit() { emit quitRequested(); }

} // namespace lumina
