#pragma once

#include <QDir>
#include <QCache>
#include <QImage>
#include <QMutex>
#include <QQuickAsyncImageProvider>
#include <QThreadPool>

namespace lumina {

class AppSettings;
class LibraryManager;

/// Grabs one decoded frame of a video with a private, headless libVLC instance. Blocking; call from a worker.
class FrameGrabber {
public:
    /// positionMs < 0 picks ~10 % of the duration. Returns a null image on failure.
    static QImage grab(const QString &path, qint64 durationMs, qint64 positionMs = -1, int timeoutMs = 9000);
};

/**
 * `image://thumb/<mediaId>?v=<mtime>` provider. Order of preference: cached thumbnail, sidecar artwork
 * (poster/folder/cover.jpg|png next to the file), a frame grabbed from the video. Results are cached as
 * JPEG in the cache directory, whose size is capped (oldest first). Generation is serialised on one worker
 * thread so scanning a large library never starves playback of CPU.
 */
class ThumbnailService : public QQuickAsyncImageProvider {
public:
    ThumbnailService(AppSettings *settings, LibraryManager *library, const QString &cacheDir);
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    QString cacheDir() const { return m_dir.absolutePath(); }
    /// Delete every cached thumbnail.
    void clearCache();
    qint64 cacheBytes() const;
    /// Remove oldest files until the cache fits `maxBytes`.
    void trim(qint64 maxBytes);
    static QString sidecarArtwork(const QString &mediaPath);
    static QImage scaled(const QImage &src, int maxWidth);

    /// Preview frame for the seek bar: decoded in a private libVLC player at `positionMs` (rounded to 5 s buckets), memory-cached.
    QImage seekFrame(qint64 id, const QString &path, qint64 durationMs, qint64 positionMs);

    QImage load(qint64 id, const QString &path, qint64 durationMs, qint64 mtime, bool enabled);

private:
    AppSettings *m_settings;
    LibraryManager *m_library;
    QDir m_dir;
    QThreadPool m_pool;
    QThreadPool m_seekPool;
    QCache<QString, QImage> m_seekCache{120};
    mutable QMutex m_mutex;
    int m_generated = 0;
};

} // namespace lumina
