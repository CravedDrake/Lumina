#pragma once
#include <QString>
#include <QtGlobal>

namespace lumina {

// 3723000 -> "1:02:03"; 65000 -> "1:05"; negative/unknown -> "--:--"
QString formatTime(qint64 ms, bool showUnknown = true);
// Parses "90", "1:30", "01:02:03", "1h2m3s", "1:30.5" into milliseconds; -1 on failure.
qint64 parseTime(const QString &text);
// 1536 -> "1.5 KB"
QString formatBytes(qint64 bytes);
QString formatBitrate(qint64 bitsPerSecond);

} // namespace lumina
