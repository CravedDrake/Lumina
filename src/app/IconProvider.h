#pragma once

#include <QQuickImageProvider>
#include <QSize>
#include <QImage>
#include <QByteArray>
#include <QString>

namespace lumina {

/// `image://icon/<name>?color=RRGGBB` — renders resources/icons/<name>.svg (stroke icons using
/// `currentColor`) in the requested colour at the requested size, cached per (name,colour,size).
class IconProvider : public QQuickImageProvider {
public:
    IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    static QByteArray tinted(const QByteArray &svg, const QString &color);
};

} // namespace lumina
