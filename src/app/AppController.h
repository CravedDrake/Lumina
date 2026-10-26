#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace lumina {

class AppSettings;
class LibraryManager;
class PlaybackController;
class ThumbnailService;

/// Small QML-facing facade for things that are neither playback nor library: dialogs' helper data,
/// clipboard, opening folders in the file manager, diagnostics and privacy actions.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString vlcVersion READ vlcVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)
    Q_PROPERTY(QStringList mediaFilters READ mediaFilters CONSTANT)
    Q_PROPERTY(QStringList subtitleFilters READ subtitleFilters CONSTANT)
    Q_PROPERTY(QStringList playlistFilters READ playlistFilters CONSTANT)
    Q_PROPERTY(QString moviesFolder READ moviesFolder CONSTANT)
    Q_PROPERTY(qint64 thumbnailCacheBytes READ thumbnailCacheBytes NOTIFY cacheChanged)

public:
    AppController(AppSettings *settings, LibraryManager *library, PlaybackController *player, ThumbnailService *thumbs,
                  const QString &dataDir, const QString &cacheDir, QObject *parent = nullptr);

    QString version() const;
    QString vlcVersion() const;
    QString qtVersion() const;
    QString platform() const;
    QString dataDir() const { return m_dataDir; }
    QString cacheDir() const { return m_cacheDir; }
    QStringList mediaFilters() const;
    QStringList subtitleFilters() const;
    QStringList playlistFilters() const;
    QString moviesFolder() const;
    qint64 thumbnailCacheBytes() const;

    Q_INVOKABLE QString toLocalPath(const QUrl &url) const;
    Q_INVOKABLE QVariantList classifyUrls(const QVariantList &urls) const;      // [{location, kind: media|subtitle|playlist|folder|other}]
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE bool revealInFileManager(const QString &path);
    Q_INVOKABLE bool openFolderInFileManager(const QString &path);
    Q_INVOKABLE bool fileExists(const QString &path) const;
    Q_INVOKABLE QString diagnostics() const;
    Q_INVOKABLE QString safeUrl(const QString &url) const;                      // credentials stripped, for display
    Q_INVOKABLE QString formatTime(qint64 ms) const;
    Q_INVOKABLE qint64 parseTime(const QString &text) const;
    Q_INVOKABLE QString formatBytes(qint64 bytes) const;
    Q_INVOKABLE QString formatDate(qint64 secsSinceEpoch) const;
    Q_INVOKABLE void clearThumbnailCache();
    Q_INVOKABLE void clearAllPrivateData();                                      // history + recent streams + thumbnails
    Q_INVOKABLE QVariantList recentStreams() const;
    Q_INVOKABLE void rememberStream(const QString &url);                         // only URLs without credentials
    Q_INVOKABLE void forgetStreams();
    Q_INVOKABLE void quit();

signals:
    void cacheChanged();
    void message(const QString &text, const QString &level);
    void quitRequested();

private:
    AppSettings *m_settings;
    LibraryManager *m_library;
    PlaybackController *m_player;
    ThumbnailService *m_thumbs;
    QString m_dataDir, m_cacheDir;
};

} // namespace lumina
