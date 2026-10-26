#pragma once

#include <QObject>
#include <QImage>
#include <QSize>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace lumina {

/// One decoded RGB frame (libVLC "RV32": B,G,R,X in memory on little-endian).
struct VideoFrame {
    int width = 0;
    int height = 0;
    int pitch = 0;
    std::vector<uchar> data;

    QImage asImage() const { // non-owning view; caller must keep the frame alive
        return QImage(data.data(), width, height, pitch, QImage::Format_RGB32);
    }
};
using VideoFramePtr = std::shared_ptr<VideoFrame>;

/**
 * Receives decoded frames from libVLC's software video callbacks and hands the
 * newest one to any number of consumers (QQuick items, screenshot code).
 *
 * Threading
 *  - lock/unlock/display/format callbacks run on libVLC's video thread.
 *  - takeLatest()/latest() run on any other thread (render thread, GUI thread).
 *  - A triple-buffer scheme (back / ready / in-use) means the decoder never
 *    writes into a buffer a consumer is reading, and the mutex is only held
 *    for pointer swaps, never for pixel copies.
 *  - frameAvailable() is emitted on this object's thread, coalesced so that
 *    at most one notification is pending (no event-queue flooding).
 *
 * The same source can back several VideoItems (main window + picture-in-picture)
 * without creating a second libVLC video output.
 */
class VideoFrameSource : public QObject {
    Q_OBJECT
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(double naturalAspect READ naturalAspect NOTIFY naturalAspectChanged)

public:
    explicit VideoFrameSource(QObject *parent = nullptr);

    // ---- libVLC callbacks (pass `this` as opaque) -------------------------
    static unsigned formatSetup(void **opaque, char *chroma, unsigned *width, unsigned *height,
                                unsigned *pitches, unsigned *lines);
    static void formatCleanup(void *opaque);
    static void *lockCb(void *opaque, void **planes);
    static void unlockCb(void *opaque, void *picture, void *const *planes);
    static void displayCb(void *opaque, void *picture);

    /// Newest frame, or null if none. `isNew` tells whether it differs from the last call.
    VideoFramePtr takeLatest(bool *isNew = nullptr);
    /// Newest frame without consuming the "new" flag (used by screenshots).
    /// Multi-consumer variant: `lastSeq` is the consumer's own bookkeeping, so several items can show the same source.
    VideoFramePtr takeLatestFor(quint64 &lastSeq, bool *isNew);
    VideoFramePtr latest() const;

    QSize videoSize() const;
    bool hasFrame() const { return m_hasFrame.load(); }

    /// Exact visible size of the next media (from a pre-parse). libVLC otherwise hands the format
    /// callback its internal, aligned display size and rescales the picture into it.
    void setSizeHint(const QSize &size) { m_hintW = size.width(); m_hintH = size.height(); }
    /// Display aspect ratio including pixel aspect (SAR); 0 = unknown, use the frame's own ratio.
    void setNaturalAspect(double a);
    double naturalAspect() const { return m_aspect.load(); }

    /// Drop the current picture (on stop / media change) so views go black.
    void clear();

signals:
    void frameAvailable();
    void videoSizeChanged();
    void hasFrameChanged();
    void naturalAspectChanged();

private:
    void notifyGui();
    VideoFramePtr makeFrame(int w, int h) const;

    mutable std::mutex m_mutex;
    VideoFramePtr m_back;
    VideoFramePtr m_ready;
    VideoFramePtr m_inUse;
    bool m_newFrame = false;
    quint64 m_seq = 0;
    int m_width = 0;
    int m_height = 0;

    std::atomic<bool> m_notifyPending{false};
    std::atomic<bool> m_hasFrame{false};
    std::atomic<bool> m_sizeDirty{false};
    std::atomic<int> m_hintW{0};
    std::atomic<int> m_hintH{0};
    std::atomic<double> m_aspect{0.0};
};

} // namespace lumina
