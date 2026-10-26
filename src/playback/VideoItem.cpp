#include "VideoItem.h"

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGTransformNode>
#include <QMatrix4x4>
#include <QRegularExpression>
#include <cmath>

namespace lumina {

namespace {
struct Node : QSGTransformNode {
    QSGImageNode *image = nullptr;
};
} // namespace

VideoItem::VideoItem(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setClip(true);
}

VideoItem::~VideoItem() = default;

void VideoItem::setSource(VideoFrameSource *s)
{
    if (s == m_source) return;
    QObject::disconnect(m_frameConn);
    QObject::disconnect(m_hasFrameConn);
    m_source = s;
    m_lastFrame.reset();
    if (m_source) {
        m_frameConn = connect(m_source, &VideoFrameSource::frameAvailable, this, [this] { update(); });
        connect(m_source, &VideoFrameSource::videoSizeChanged, this, [this] { emit layoutChanged(); update(); });
        connect(m_source, &VideoFrameSource::naturalAspectChanged, this, [this] { emit layoutChanged(); update(); });
        m_hasFrameConn = connect(m_source, &VideoFrameSource::hasFrameChanged, this, [this] { emit hasFrameChanged(); update(); });
    }
    emit sourceChanged();
    emit hasFrameChanged();
    update();
}

#define LUMINA_SETTER(Name, member, type)                  \
    void VideoItem::Name(type v)                           \
    {                                                      \
        if (member == v) return;                           \
        member = v;                                        \
        emit layoutChanged();                              \
        update();                                          \
    }
LUMINA_SETTER(setAspectMode, m_aspectMode, const QString &)
LUMINA_SETTER(setPanX, m_panX, double)
LUMINA_SETTER(setPanY, m_panY, double)
LUMINA_SETTER(setFlipHorizontal, m_flipH, bool)
#undef LUMINA_SETTER

void VideoItem::setZoom(double z)
{
    z = qBound(0.25, z, 8.0);
    if (qFuzzyCompare(z, m_zoom)) return;
    m_zoom = z;
    emit layoutChanged();
    update();
}

void VideoItem::setVideoRotation(int r)
{
    r = ((r % 360) + 360) % 360;
    r = (r / 90) * 90;
    if (r == m_rotation) return;
    m_rotation = r;
    emit layoutChanged();
    update();
}

double VideoItem::parseRatio(const QString &mode)
{
    static const QRegularExpression re(QStringLiteral("^(\\d+(?:\\.\\d+)?):(\\d+(?:\\.\\d+)?)$"));
    const auto m = re.match(mode);
    if (!m.hasMatch()) return 0.0;
    const double a = m.captured(1).toDouble(), b = m.captured(2).toDouble();
    return b > 0 ? a / b : 0.0;
}

QSizeF VideoItem::fit(const QSizeF &item, double contentAspect, const QString &mode, int rotation)
{
    if (item.width() <= 0 || item.height() <= 0) return {};
    const bool quarter = (rotation / 90) % 2 != 0;
    if (mode == QLatin1String("stretch")) return item;
    double a = parseRatio(mode);
    if (a <= 0) a = contentAspect > 0 ? contentAspect : 16.0 / 9.0;
    if (quarter) a = 1.0 / a;                        // aspect of the picture as it will appear on screen
    const double itemA = item.width() / item.height();
    const bool cover = mode == QLatin1String("fill");
    double w, h;
    if ((itemA > a) != cover) { h = item.height(); w = h * a; }   // letter-box (or cover when `cover`)
    else { w = item.width(); h = w / a; }
    return {w, h};
}

QRectF VideoItem::videoRect() const
{
    double a = 0;
    if (m_source) {
        a = m_source->naturalAspect();
        if (a <= 0) { const QSize s = m_source->videoSize(); if (s.height() > 0) a = double(s.width()) / s.height(); }
    }
    const QSizeF f = fit(size(), a, m_aspectMode, m_rotation);
    return QRectF((width() - f.width()) / 2, (height() - f.height()) / 2, f.width(), f.height());
}

QSGNode *VideoItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *root = static_cast<Node *>(old);
    if (!m_source || width() <= 0 || height() <= 0 || !window()) {
        delete root;
        return nullptr;
    }

    bool isNew = false;
    VideoFramePtr frame = m_source->takeLatestFor(m_lastSeq, &isNew);
    if (isNew && frame) m_lastFrame = frame;
    if (!m_lastFrame) {                       // nothing decoded yet: draw nothing (parent supplies the black backdrop)
        delete root;
        return nullptr;
    }

    if (!root) {
        root = new Node;
        root->image = window()->createImageNode();
        root->image->setFiltering(QSGTexture::Linear);
        root->image->setOwnsTexture(true);
        root->appendChildNode(root->image);
    }

    if (isNew || !root->image->texture()) {
        // The QImage borrows the frame's pixels; the cleanup callback keeps the frame alive until Qt is done with it.
        auto *keep = new VideoFramePtr(m_lastFrame);
        QImage img(m_lastFrame->data.data(), m_lastFrame->width, m_lastFrame->height, m_lastFrame->pitch,
                   QImage::Format_RGB32, [](void *p) { delete static_cast<VideoFramePtr *>(p); }, keep);
        QSGTexture *tex = window()->createTextureFromImage(img, QQuickWindow::TextureIsOpaque);
        root->image->setTexture(tex);   // takes ownership, deletes the previous texture
    }

    double a = m_source->naturalAspect();
    if (a <= 0 && m_lastFrame->height > 0) a = double(m_lastFrame->width) / m_lastFrame->height;
    const QSizeF screen = fit(size(), a, m_aspectMode, m_rotation);
    const bool quarter = (m_rotation / 90) % 2 != 0;
    const QSizeF pre = quarter ? QSizeF(screen.height(), screen.width()) : screen;   // size before rotation
    root->image->setRect(QRectF(-pre.width() / 2, -pre.height() / 2, pre.width(), pre.height()));
    root->image->setTextureCoordinatesTransform(m_flipH ? QSGImageNode::MirrorHorizontally : QSGImageNode::NoTransform);

    QMatrix4x4 m;
    m.translate(float(width() / 2 + m_panX * width()), float(height() / 2 + m_panY * height()));
    m.rotate(float(m_rotation), 0, 0, 1);
    m.scale(float(m_zoom), float(m_zoom));
    root->setMatrix(m);
    return root;
}

void VideoItem::releaseResources()
{
    m_lastFrame.reset();
    QQuickItem::releaseResources();
}

} // namespace lumina
