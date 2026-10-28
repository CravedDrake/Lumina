#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>

namespace lumina::m3u {

struct Entry {
    QString location;      // absolute local path or URL
    QString title;         // from #EXTINF, may be empty
    qint64 durationSec = -1;
};

/// Parses M3U / M3U8 text. Relative paths are resolved against `baseDir`. Comments/unknown tags ignored.
QVector<Entry> parse(const QByteArray &data, const QString &baseDir);
QVector<Entry> readFile(const QString &path, QString *error = nullptr);

/// Serialises as extended M3U8 (UTF-8). Paths are written as given (absolute).
QByteArray serialize(const QVector<Entry> &entries);
bool writeFile(const QString &path, const QVector<Entry> &entries, QString *error = nullptr);

} // namespace lumina::m3u
