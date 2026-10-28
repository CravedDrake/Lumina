#include "TimeFormat.h"
#include <QRegularExpression>
#include <QString>
#include <cmath>

namespace lumina {

QString formatTime(qint64 ms, bool showUnknown)
{
    if (ms < 0) return showUnknown ? QStringLiteral("--:--") : QStringLiteral("0:00");
    const qint64 total = ms / 1000;
    const qint64 h = total / 3600, m = (total / 60) % 60, s = total % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

qint64 parseTime(const QString &textIn)
{
    const QString text = textIn.trimmed().toLower();
    if (text.isEmpty()) return -1;
    static const QRegularExpression hms(QStringLiteral("^(?:(\\d+)h)?(?:(\\d+)m)?(?:(\\d+(?:\\.\\d+)?)s)?$"));
    static const QRegularExpression colon(QStringLiteral("^(?:(\\d+):)?(\\d+):(\\d+(?:\\.\\d+)?)$"));
    static const QRegularExpression plain(QStringLiteral("^\\d+(?:\\.\\d+)?$"));
    if (plain.match(text).hasMatch())
        return qint64(std::llround(text.toDouble() * 1000.0));
    auto c = colon.match(text);
    if (c.hasMatch()) {
        const double sec = c.captured(3).toDouble();
        const int mn = c.captured(2).toInt();
        if (mn >= 60 && !c.captured(1).isEmpty()) return -1;
        if (sec >= 60.0 && (!c.captured(1).isEmpty() || mn >= 60)) return -1;
        return qint64(std::llround((c.captured(1).toLongLong() * 3600 + mn * 60 + sec) * 1000.0));
    }
    auto h = hms.match(text);
    if (h.hasMatch() && (h.hasCaptured(1) || h.hasCaptured(2) || h.hasCaptured(3)) &&
        !(h.captured(1).isEmpty() && h.captured(2).isEmpty() && h.captured(3).isEmpty()))
        return qint64(std::llround((h.captured(1).toLongLong() * 3600 + h.captured(2).toLongLong() * 60 +
                                    h.captured(3).toDouble()) * 1000.0));
    return -1;
}

QString formatBytes(qint64 b)
{
    if (b < 0) return QStringLiteral("—");
    static const QString u[] = {QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB"), QStringLiteral("TB")};
    double v = double(b);
    int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return i == 0 ? QStringLiteral("%1 B").arg(b) : QStringLiteral("%1 %2").arg(v, 0, 'f', v >= 100 ? 0 : 1).arg(u[i]);
}

QString formatBitrate(qint64 bps)
{
    if (bps <= 0) return QStringLiteral("—");
    if (bps >= 1000000) return QStringLiteral("%1 Mb/s").arg(bps / 1e6, 0, 'f', 1);
    return QStringLiteral("%1 kb/s").arg(bps / 1000);
}

} // namespace lumina
