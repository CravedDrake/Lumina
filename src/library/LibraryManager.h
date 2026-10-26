#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "LibraryScanner.h"

namespace lumina {

class AppSettings;

/// QML-facing facade ("Library"): folders, scanning, history, favourites, per-media preferences,
/// bookmarks and playlists. All SQL for these lives here (or in LibraryModel for list queries).
class LibraryManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString scanPhase READ scanPhase NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanDone READ scanDone NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanProgressChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(int revision READ revision NOTIFY dataChanged)
    Q_PROPERTY(int mediaCount READ mediaCount NOTIFY dataChanged)

public:
    explicit LibraryManager(AppSettings *settings, QObject *parent = nullptr);
    ~LibraryManager() override;

    bool scanning() const { return m_scanning; }
    QString scanPhase() const { return m_phase; }
    int scanDone() const { return m_done; }
    int scanTotal() const { return m_total; }
    QVariantList folders() const;
    QVariantList playlists() const;
    int revision() const { return m_revision; }
    int mediaCount() const;

    // ---- folders & scanning
    Q_INVOKABLE bool addFolder(const QString &pathOrUrl);
    Q_INVOKABLE void removeFolder(const QString &path, bool removeMedia = true);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void cancelScan();
    Q_INVOKABLE void scanFoldersNow(const QStringList &folders);   // used by tests

    // ---- media records
    /// Returns the id for a file/URL, creating a record if needed (credentials are stripped from URLs).
    Q_INVOKABLE qint64 registerUrl(const QUrl &url);
    Q_INVOKABLE QVariantMap mediaInfo(qint64 id) const;
    Q_INVOKABLE QUrl urlFor(qint64 id) const;
    Q_INVOKABLE bool setFavorite(qint64 id, bool fav);
    Q_INVOKABLE bool removeMedia(qint64 id);                       // removes the record only, never the file
    Q_INVOKABLE bool correctClassification(qint64 id, const QString &kind, const QString &title,
                                           const QString &showName, int season, int episode);
    Q_INVOKABLE bool setMediaDetails(qint64 id, qint64 durationMs, int width, int height, double fps);

    // ---- history & resume
    Q_INVOKABLE void recordPlay(qint64 id);
    Q_INVOKABLE void saveProgress(qint64 id, qint64 positionMs, qint64 durationMs, bool completed);
    Q_INVOKABLE QVariantMap resumeInfo(qint64 id) const;           // {positionMs, durationMs, canResume}
    Q_INVOKABLE void removeFromHistory(qint64 id);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void pruneHistory(int olderThanDays);

    // ---- per-media preferences / bookmarks
    Q_INVOKABLE QVariantMap loadPrefs(qint64 id) const;
    Q_INVOKABLE void savePrefs(qint64 id, const QVariantMap &prefs);
    Q_INVOKABLE QVariantList bookmarks(qint64 mediaId) const;
    Q_INVOKABLE qint64 addBookmark(qint64 mediaId, qint64 positionMs, const QString &note);
    Q_INVOKABLE void removeBookmark(qint64 bookmarkId);
    Q_INVOKABLE void updateBookmarkNote(qint64 bookmarkId, const QString &note);

    // ---- playlists
    Q_INVOKABLE qint64 createPlaylist(const QString &name);
    Q_INVOKABLE void renamePlaylist(qint64 id, const QString &name);
    Q_INVOKABLE void deletePlaylist(qint64 id);
    Q_INVOKABLE void addToPlaylist(qint64 playlistId, const QVariantList &mediaIds);
    Q_INVOKABLE void removeFromPlaylist(qint64 playlistItemId);
    Q_INVOKABLE void moveInPlaylist(qint64 playlistId, int from, int to);
    Q_INVOKABLE QVariantList playlistMediaIds(qint64 playlistId) const;
    Q_INVOKABLE qint64 importPlaylistFile(const QString &path);    // returns playlist id or -1
    Q_INVOKABLE bool exportPlaylistFile(qint64 playlistId, const QString &path);

    // ---- maintenance
    Q_INVOKABLE void clearLibrary();
    Q_INVOKABLE QString databasePath() const;

signals:
    void scanningChanged();
    void scanProgressChanged();
    void scanFinished(const QVariantMap &summary);
    void foldersChanged();
    void playlistsChanged();
    void dataChanged();
    void message(const QString &text, const QString &level);   // level: info|success|warning|error

private:
    void bumpRevision();
    void startScan(const QStringList &folders);

    AppSettings *m_settings;
    QThread m_thread;
    LibraryScanner *m_scanner;
    QTimer m_debounce;
    bool m_scanning = false;
    QString m_phase;
    int m_done = 0, m_total = 0;
    int m_revision = 0;
};

} // namespace lumina
