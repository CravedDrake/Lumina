#include "IconProvider.h"

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QSvgRenderer>
#include <QUrlQuery>
#include <QMutexLocker>
#include <QSize>
#include <QImage>
#include <QByteArray>
#include <QRectF>
#include <QString>
#include <QChar>

namespace lumina {

QByteArray IconProvider::tinted(const QByteArray &svg, const QString &color)
{
    QByteArray out = svg;
    out.replace("currentColor", color.toLatin1());
    return out;
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int q = id.indexOf(QLatin1Char('?'));
    const QString name = id.left(q < 0 ? id.size() : q);
    QString color = QStringLiteral("#ffffff");
    if (q >= 0) {
        const QString c = QUrlQuery(id.mid(q + 1)).queryItemValue(QStringLiteral("color"));
        if (!c.isEmpty()) color = c.startsWith(QLatin1Char('#')) ? c : QLatin1Char('#') + c;
    }
    // names are plain identifiers; refuse anything else (no path traversal into the resource tree)
    for (const QChar ch : name)
        if (!(ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_'))) return {};

    static QMutex mutex;
    static QHash<QString, QByteArray> sources;
    QByteArray svg;
    {
        QMutexLocker lock(&mutex);
        auto it = sources.find(name);
        if (it == sources.end()) {
            QFile f(QStringLiteral(":/resources/icons/") + name + QStringLiteral(".svg"));
            it = sources.insert(name, f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
        }
        svg = it.value();
    }
    if (svg.isEmpty()) return {};
    const int s = requestedSize.width() > 0 ? requestedSize.width() : 24;
    const int h = requestedSize.height() > 0 ? requestedSize.height() : s;
    QImage img(s, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QSvgRenderer r(tinted(svg, color));
    if (!r.isValid()) return {};
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p, QRectF(0, 0, s, h));
    p.end();
    if (size) *size = img.size();
    return img;
}

} // namespace lumina
