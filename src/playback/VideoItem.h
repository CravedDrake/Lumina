#pragma once

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QQuickItem>
#include <QRectF>
#include <QSizeF>
#include <QMetaObject>
#include <QString>

#include "VideoFrameSource.h"

namespace lumina {

/**
 * Scene-graph item that shows the newest frame of a VideoFrameSource.
 *
 * Several items may observe the same source (main player + picture-in-picture window): there is only
 * ever one libVLC video output, so the surface can be handed over without restarting playback.
 *
 * Layout modes (`aspectMode`): "auto" (the media's own display aspect, letter-boxed), a ratio such as
 * "16:9" / "4:3" / "21:9" / "1:1" / "16:10" (forced), "fill" (cover, crops) and "stretch" (ignores aspect).
 * Zoom/pan/rotation are applied on top. Nothing is distorted unless "stretch" or a forced ratio is chosen.
 */
class VideoItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(lumina::VideoFrameSource *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString aspectMode READ aspectMode WRITE setAspectMode NOTIFY layoutChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY layoutChanged)
    Q_PROPERTY(double panX READ panX WRITE setPanX NOTIFY layoutChanged)
    Q_PROPERTY(double panY READ panY WRITE setPanY NOTIFY layoutChanged)
    Q_PROPERTY(int videoRotation READ videoRotation WRITE setVideoRotation NOTIFY layoutChanged)
    Q_PROPERTY(bool flipHorizontal READ flipHorizontal WRITE setFlipHorizontal NOTIFY layoutChanged)
    Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)   // 0 = every frame; small previews use ~10
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(QRectF videoRect READ videoRect NOTIFY layoutChanged)
public:
    explicit VideoItem(QQuickItem *parent = nullptr);
    ~VideoItem() override;

    VideoFrameSource *source() const { return m_source; }
    void setSource(VideoFrameSource *s);
    QString aspectMode() const { return m_aspectMode; }
    void setAspectMode(const QString &m);
    double zoom() const { return m_zoom; }
    void setZoom(double z);
    double panX() const { return m_panX; }
    void setPanX(double v);
    double panY() const { return m_panY; }
    void setPanY(double v);
    int videoRotation() const { return m_rotation; }
    void setVideoRotation(int r);
    bool flipHorizontal() const { return m_flipH; }
    void setFlipHorizontal(bool f);
    bool hasFrame() const { return m_source && m_source->hasFrame(); }
    int maxFps() const { return m_maxFps; }
    void setMaxFps(int fps);
    QRectF videoRect() const;   // where the picture lands inside the item (item coordinates, before pan/zoom)

    /// Pure geometry helper (unit-tested): size of the picture inside an item of size `item`.
    static QSizeF fit(const QSizeF &item, double contentAspect, const QString &mode, int rotation);
    /// Parses "16:9" -> 1.7777; returns 0 for non-ratio modes.
    static double parseRatio(const QString &mode);

signals:
    void sourceChanged();
    void layoutChanged();
    void hasFrameChanged();
    void maxFpsChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void releaseResources() override;

private:
    QPointer<VideoFrameSource> m_source;
    QString m_aspectMode = QStringLiteral("auto");
    double m_zoom = 1.0, m_panX = 0.0, m_panY = 0.0;
    int m_rotation = 0;
    bool m_flipH = false;
    void requestUpdate();               // frame arrived: repaint only if somebody can see it
    int m_maxFps = 0;
    QElapsedTimer m_throttle;
    QTimer m_trailing;                  // delivers the last skipped frame of a throttled item
    VideoFramePtr m_lastFrame;
    quint64 m_lastSeq = 0;
    QMetaObject::Connection m_frameConn, m_hasFrameConn;
};

} // namespace lumina
