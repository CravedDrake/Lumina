#include "ThumbnailService.h"

#include <QDateTime>
#include <QFileInfo>
#include <QCoreApplication>
#include <QPointer>
#include <QRunnable>
#include <QUrlQuery>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <vlc/vlc.h>

#include "library/LibraryManager.h"
#include "settings/AppSettings.h"
#include "utilities/Logger.h"

namespace lumina {

namespace {

struct GrabState {
    std::mutex mutex;
    std::vector<uchar> buffer;
    int width = 0, height = 0;
    std::atomic<int> frames{0};
    std::atomic<bool> stopped{false};
};

unsigned grabSetup(void **opaque, char *chroma, unsigned *w, unsigned *h, unsigned *pitches, unsigned *lines)
{
    auto *st = static_cast<GrabState *>(*opaque);
    std::memcpy(chroma, "RV32", 4);
    pitches[0] = *w * 4;
    lines[0] = *h;
    std::lock_guard<std::mutex> l(st->mutex);
    st->width = int(*w);
    st->height = int(*h);
    st->buffer.assign(size_t(*w) * *h * 4, 0);
    return 1;
}
void *grabLock(void *opaque, void **planes)
{
    auto *st = static_cast<GrabState *>(opaque);
    st->mutex.lock();                                 // held until unlock: the decoder writes into buffer
    planes[0] = st->buffer.data();
    return nullptr;
}
void grabUnlock(void *opaque, void *, void *const *) { static_cast<GrabState *>(opaque)->mutex.unlock(); }
void grabDisplay(void *opaque, void *) { static_cast<GrabState *>(opaque)->frames.fetch_add(1); }

// libVLC instances are heavy and not needed concurrently: one shared headless instance, serialised use.
std::mutex g_grabMutex;
libvlc_instance_t *g_instance = nullptr;
libvlc_instance_t *instance()
{
    if (!g_instance) {
        const char *args[] = {"--no-audio", "--no-xlib", "--no-video-title-show", "--no-osd", "--no-stats",
                              "--no-snapshot-preview", "--quiet", "--no-spu", "--no-sub-autodetect-file"};
        g_instance = libvlc_new(int(sizeof(args) / sizeof(args[0])), args);
    }
    return g_instance;
}

const QStringList kArtNames = {QStringLiteral("poster"), QStringLiteral("folder"), QStringLiteral("cover"),
                               QStringLiteral("thumb"), QStringLiteral("thumbnail")};
const QStringList kArtExt = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("webp")};

} // namespace

QImage FrameGrabber::grab(const QString &path, qint64 durationMs, qint64 positionMs, int timeoutMs)
{
    std::lock_guard<std::mutex> guard(g_grabMutex);
    libvlc_instance_t *inst = instance();
    if (!inst) return {};
    const QByteArray native = QFile::encodeName(QDir::toNativeSeparators(path));
    libvlc_media_t *media = libvlc_media_new_path(inst, native.constData());
    if (!media) return {};
    libvlc_media_add_option(media, ":no-audio");
    libvlc_media_add_option(media, ":avcodec-hw=none");   // deterministic, and no GPU needed from a background job
    libvlc_media_player_t *mp = libvlc_media_player_new_from_media(media);
    libvlc_media_release(media);
    if (!mp) return {};

    GrabState st;
    libvlc_video_set_format_callbacks(mp, grabSetup, nullptr);
    libvlc_video_set_callbacks(mp, grabLock, grabUnlock, grabDisplay, &st);

    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
    auto sleep = [] { std::this_thread::sleep_for(std::chrono::milliseconds(15)); };

    QImage result;
    if (libvlc_media_player_play(mp) == 0) {
        // wait for first frame
        while (clock::now() < deadline && st.frames.load() < 1) {
            const auto s = libvlc_media_player_get_state(mp);
            if (s == libvlc_Error || s == libvlc_Ended) break;
            sleep();
        }
        if (st.frames.load() >= 1) {
            const qint64 len = durationMs > 0 ? durationMs : libvlc_media_player_get_length(mp);
            qint64 target = positionMs >= 0 ? positionMs : (len > 0 ? std::clamp<qint64>(len / 10, 1000, 120000) : 0);
            if (len > 0 && target > len - 500) target = std::max<qint64>(0, len / 2);
            if (target > 0 && libvlc_media_player_is_seekable(mp)) {
                const int before = st.frames.load();
                libvlc_media_player_set_time(mp, target);
                // wait until the clock reached the target and at least two fresh frames have been produced
                while (clock::now() < deadline) {
                    const qint64 t = libvlc_media_player_get_time(mp);
                    if (st.frames.load() >= before + 2 && t >= target - 300) break;
                    sleep();
                }
            }
            unsigned vw = 0, vh = 0;
            libvlc_video_get_size(mp, 0, &vw, &vh);
            std::lock_guard<std::mutex> l(st.mutex);
            if (st.width > 0 && st.height > 0) {
                QImage full(st.buffer.data(), st.width, st.height, st.width * 4, QImage::Format_RGB32);
                const int cw = vw > 0 ? std::min<int>(int(vw), st.width) : st.width;
                const int ch = vh > 0 ? std::min<int>(int(vh), st.height) : st.height;
                result = full.copy(0, 0, cw, ch);       // deep copy: buffer dies with `st`
            }
        }
    }
    libvlc_media_player_stop(mp);
    libvlc_media_player_release(mp);
    return result;
}

// ------------------------------------------------------------------------------------------------

namespace {
class ThumbResponse : public QQuickImageResponse {
public:
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(m_image); }
    void cancel() override { m_cancelled = true; }
    bool cancelled() const { return m_cancelled; }
    void complete(const QImage &img) { m_image = img; emit finished(); }
private:
    QImage m_image;
    std::atomic<bool> m_cancelled{false};
};

// The job holds only a guarded pointer to the response: QML may destroy the response at any time.
class ThumbJob : public QRunnable {
public:
    ThumbJob(ThumbnailService *svc, QPointer<ThumbResponse> r, qint64 id, QString path, qint64 dur, qint64 mtime, bool enabled, int maxW, qint64 seekMs = -1)
        : m_svc(svc), m_resp(std::move(r)), m_id(id), m_path(std::move(path)), m_dur(dur), m_mtime(mtime), m_enabled(enabled), m_maxW(maxW), m_seekMs(seekMs) {}
    void run() override
    {
        QImage img;
        if (m_resp && !m_resp->cancelled()) {
            img = m_seekMs >= 0 ? m_svc->seekFrame(m_id, m_path, m_dur, m_seekMs) : m_svc->load(m_id, m_path, m_dur, m_mtime, m_enabled);
            if (!img.isNull() && m_maxW > 0) img = ThumbnailService::scaled(img, m_maxW);
        }
        QPointer<ThumbResponse> resp = m_resp;
        QMetaObject::invokeMethod(qApp, [resp, img] { if (resp) resp->complete(img); }, Qt::QueuedConnection);
    }
private:
    ThumbnailService *m_svc;
    QPointer<ThumbResponse> m_resp;
    qint64 m_id;
    QString m_path;
    qint64 m_dur, m_mtime;
    bool m_enabled;
    int m_maxW;
    qint64 m_seekMs;
};
} // namespace

ThumbnailService::ThumbnailService(AppSettings *settings, LibraryManager *library, const QString &cacheDir)
    : m_settings(settings), m_library(library), m_dir(cacheDir)
{
    m_dir.mkpath(QStringLiteral("."));
    m_pool.setMaxThreadCount(1);
    m_seekPool.setMaxThreadCount(1);
    trim(qint64(m_settings->getInt(QStringLiteral("performance/thumbCacheMB"))) * 1024 * 1024);
}

QQuickImageResponse *ThumbnailService::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // id = "seek/<mediaId>/<positionMs>"  (seek-bar preview; local files only, opt-in setting)
    if (id.startsWith(QLatin1String("seek/"))) {
        const QStringList parts = id.split(QLatin1Char('/'));
        auto *r = new ThumbResponse;
        const qint64 mid = parts.value(1).toLongLong();
        const qint64 pos = parts.value(2).toLongLong();
        const QVariantMap info = m_library->mediaInfo(mid);
        const QString path = info.value(QStringLiteral("path")).toString();
        if (!m_settings->getBool(QStringLiteral("performance/seekPreview")) || path.isEmpty() || !QFileInfo::exists(path)) {
            QMetaObject::invokeMethod(r, [r] { r->complete(QImage()); }, Qt::QueuedConnection);
            return r;
        }
        m_seekPool.start(new ThumbJob(this, QPointer<ThumbResponse>(r), mid, path, info.value(QStringLiteral("durationMs")).toLongLong(),
                                      0, true, requestedSize.width() > 0 ? requestedSize.width() : 240, qMax<qint64>(0, pos)));
        return r;
    }
    // id = "<mediaId>?v=<mtime>"
    const int q = id.indexOf(QLatin1Char('?'));
    const qint64 mediaId = id.left(q < 0 ? id.size() : q).toLongLong();
    const QVariantMap info = m_library->mediaInfo(mediaId);
    auto *r = new ThumbResponse;
    const QString path = info.value(QStringLiteral("path")).toString();
    const qint64 mtime = QFileInfo(path).lastModified().toSecsSinceEpoch();
    m_pool.start(new ThumbJob(this, QPointer<ThumbResponse>(r), mediaId, path,
                              info.value(QStringLiteral("durationMs")).toLongLong(), mtime,
                              m_settings->getBool(QStringLiteral("performance/thumbnails")),
                              requestedSize.width() > 0 ? requestedSize.width() : 0));
    return r;
}

QImage ThumbnailService::seekFrame(qint64 id, const QString &path, qint64 durationMs, qint64 positionMs)
{
    const qint64 bucket = (positionMs / 5000) * 5000;
    const QString key = QString::number(id) + QLatin1Char('/') + QString::number(bucket);
    {
        QMutexLocker l(&m_mutex);
        if (const QImage *c = m_seekCache.object(key)) return *c;
    }
    QImage img = FrameGrabber::grab(path, durationMs, qMax<qint64>(bucket, 1), 6000);
    if (img.isNull()) return img;
    img = scaled(img, 240);
    QMutexLocker l(&m_mutex);
    m_seekCache.insert(key, new QImage(img));
    return img;
}

QImage ThumbnailService::scaled(const QImage &src, int maxWidth)
{
    if (src.isNull() || src.width() <= maxWidth) return src;
    return src.scaledToWidth(maxWidth, Qt::SmoothTransformation);
}

QString ThumbnailService::sidecarArtwork(const QString &mediaPath)
{
    const QFileInfo fi(mediaPath);
    const QDir dir = fi.dir();
    const QString base = fi.completeBaseName();
    for (const QString &ext : kArtExt) {
        const QString same = dir.filePath(base + QLatin1Char('.') + ext);
        if (QFileInfo::exists(same)) return same;
        const QString thumb = dir.filePath(base + QStringLiteral("-thumb.") + ext);
        if (QFileInfo::exists(thumb)) return thumb;
    }
    for (const QString &n : kArtNames)
        for (const QString &ext : kArtExt) {
            const QString p = dir.filePath(n + QLatin1Char('.') + ext);
            if (QFileInfo::exists(p)) return p;
        }
    return {};
}

QImage ThumbnailService::load(qint64 id, const QString &path, qint64 durationMs, qint64 mtime, bool enabled)
{
    if (id <= 0 || path.isEmpty()) return {};
    const QString cached = m_dir.filePath(QStringLiteral("%1-%2.jpg").arg(id).arg(mtime));
    if (QFileInfo::exists(cached)) {
        QImage img(cached);
        if (!img.isNull()) return img;
    }
    if (!QFileInfo::exists(path)) return {};
    QImage img;
    if (m_settings->getBool(QStringLiteral("library/localArtwork"))) {
        const QString art = sidecarArtwork(path);
        if (!art.isEmpty()) img = QImage(art);
    }
    if (img.isNull() && enabled) img = FrameGrabber::grab(path, durationMs);
    if (img.isNull()) return {};
    img = scaled(img.convertToFormat(QImage::Format_RGB32), 480);
    {
        QMutexLocker lock(&m_mutex);
        // remove stale versions of this item's thumbnail
        const auto old = m_dir.entryList({QStringLiteral("%1-*.jpg").arg(id)}, QDir::Files);
        for (const QString &o : old) m_dir.remove(o);
        img.save(cached, "JPG", 82);
        if (++m_generated % 50 == 0)
            trim(qint64(m_settings->getInt(QStringLiteral("performance/thumbCacheMB"))) * 1024 * 1024);
    }
    return img;
}

qint64 ThumbnailService::cacheBytes() const
{
    qint64 total = 0;
    for (const QFileInfo &fi : m_dir.entryInfoList(QDir::Files)) total += fi.size();
    return total;
}

void ThumbnailService::trim(qint64 maxBytes)
{
    auto files = m_dir.entryInfoList(QDir::Files, QDir::Time | QDir::Reversed);   // oldest first
    qint64 total = 0;
    for (const auto &f : files) total += f.size();
    for (const auto &f : files) {
        if (total <= maxBytes) break;
        if (QFile::remove(f.absoluteFilePath())) total -= f.size();
    }
}

void ThumbnailService::clearCache()
{
    QMutexLocker lock(&m_mutex);
    for (const QString &f : m_dir.entryList(QDir::Files)) m_dir.remove(f);
}

} // namespace lumina
