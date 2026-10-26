#include "VideoFrameSource.h"

#include <QMetaObject>
#include <cstring>

namespace lumina {

VideoFrameSource::VideoFrameSource(QObject *parent) : QObject(parent) {}

VideoFramePtr VideoFrameSource::makeFrame(int w, int h) const
{
    auto f = std::make_shared<VideoFrame>();
    f->width = w;
    f->height = h;
    f->pitch = w * 4;
    f->data.assign(static_cast<size_t>(f->pitch) * static_cast<size_t>(h), 0);
    return f;
}

unsigned VideoFrameSource::formatSetup(void **opaque, char *chroma, unsigned *width, unsigned *height,
                                       unsigned *pitches, unsigned *lines)
{
    auto *self = static_cast<VideoFrameSource *>(*opaque);
    std::memcpy(chroma, "RV32", 4);
    if (self->m_hintW > 0 && self->m_hintH > 0) {   // render at the true visible size: no rescale
        *width = static_cast<unsigned>(self->m_hintW.load());
        *height = static_cast<unsigned>(self->m_hintH.load());
    }
    pitches[0] = *width * 4;
    lines[0] = *height;

    {
        std::lock_guard<std::mutex> lock(self->m_mutex);
        const int w = static_cast<int>(*width);
        const int h = static_cast<int>(*height);
        self->m_back = self->makeFrame(w, h);
        self->m_ready = self->makeFrame(w, h);
        // m_inUse may still be held by a consumer; it is replaced lazily in takeLatest().
        self->m_newFrame = false;
        if (self->m_width != w || self->m_height != h) {
            self->m_width = w;
            self->m_height = h;
            self->m_sizeDirty = true;
        }
    }
    self->notifyGui();
    return 1; // number of picture buffers
}

void VideoFrameSource::formatCleanup(void *) {}

void *VideoFrameSource::lockCb(void *opaque, void **planes)
{
    auto *self = static_cast<VideoFrameSource *>(opaque);
    // Only the video thread touches m_back between lock and display.
    planes[0] = self->m_back->data.data();
    return nullptr;
}

void VideoFrameSource::unlockCb(void *, void *, void *const *) {}

void VideoFrameSource::displayCb(void *opaque, void *)
{
    auto *self = static_cast<VideoFrameSource *>(opaque);
    {
        std::lock_guard<std::mutex> lock(self->m_mutex);
        std::swap(self->m_back, self->m_ready);
        self->m_newFrame = true;
    }
    self->m_hasFrame = true;
    self->notifyGui();
}

void VideoFrameSource::notifyGui()
{
    if (m_notifyPending.exchange(true))
        return; // a notification is already queued
    QMetaObject::invokeMethod(
        this,
        [this] {
            m_notifyPending = false;
            if (m_sizeDirty.exchange(false))
                emit videoSizeChanged();
            emit hasFrameChanged();
            emit frameAvailable();
        },
        Qt::QueuedConnection);
}

VideoFramePtr VideoFrameSource::takeLatest(bool *isNew)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_newFrame) {
        std::swap(m_inUse, m_ready);
        m_newFrame = false;
        if (isNew) *isNew = true;
        // After the swap m_ready holds the previously displayed frame (or null on first use);
        // make sure the decoder always has two valid buffers to rotate through.
        // A buffer from before a resolution change must never go back to the decoder.
        if (m_width > 0 && (!m_ready || m_ready->width != m_width || m_ready->height != m_height))
            m_ready = makeFrame(m_width, m_height);
    } else if (isNew) {
        *isNew = false;
    }
    return m_inUse;
}

VideoFramePtr VideoFrameSource::takeLatestFor(quint64 &lastSeq, bool *isNew)
{
    bool fresh = false;
    VideoFramePtr f = takeLatest(&fresh);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (fresh) ++m_seq;
    if (isNew) *isNew = (lastSeq != m_seq);
    lastSeq = m_seq;
    return f;
}

VideoFramePtr VideoFrameSource::latest() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // The newest decoded picture is m_ready if flagged new, else m_inUse.
    return m_newFrame ? m_ready : m_inUse;
}

QSize VideoFrameSource::videoSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return QSize(m_width, m_height);
}

void VideoFrameSource::setNaturalAspect(double a)
{
    if (qFuzzyCompare(m_aspect.load() + 1.0, a + 1.0))
        return;
    m_aspect = a;
    emit naturalAspectChanged();
}

void VideoFrameSource::clear()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_inUse.reset();
        m_newFrame = false;
        if (m_ready) std::fill(m_ready->data.begin(), m_ready->data.end(), 0);
    }
    m_hasFrame = false;
    notifyGui();
}

} // namespace lumina
