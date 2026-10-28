#include "M3u.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QVector>
#include <QByteArray>
#include <QString>

namespace lumina::m3u {

QVector<Entry> parse(const QByteArray &raw, const QString &baseDir)
{
    QVector<Entry> out;
    QByteArray data = raw;
    if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3);           // UTF-8 BOM
    const QString text = QString::fromUtf8(data);
    Entry pending;
    bool havePending = false;
    for (QString line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();                                          // also strips CR
        if (line.isEmpty()) continue;
        if (line.startsWith(QLatin1Char('#'))) {
            if (line.startsWith(QStringLiteral("#EXTINF:"), Qt::CaseInsensitive)) {
                const QString rest = line.mid(8);
                const int comma = rest.indexOf(QLatin1Char(','));
                pending = {};
                pending.durationSec = rest.left(comma < 0 ? rest.size() : comma).toLongLong();
                if (comma >= 0) pending.title = rest.mid(comma + 1).trimmed();
                havePending = true;
            }
            continue;
        }
        Entry e = havePending ? pending : Entry{};
        havePending = false;
        const QUrl url(line);
        if (url.isValid() && !url.scheme().isEmpty() && url.scheme().size() > 1) {   // >1: not a Windows drive letter
            e.location = url.isLocalFile() ? url.toLocalFile() : line;
        } else {
            const QString native = QDir::fromNativeSeparators(line);
            e.location = QDir::isAbsolutePath(native) ? QDir::cleanPath(native)
                                                      : QDir::cleanPath(baseDir + QLatin1Char('/') + native);
        }
        out.push_back(e);
    }
    return out;
}

QVector<Entry> readFile(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return {};
    }
    return parse(f.readAll(), QFileInfo(path).absolutePath());
}

QByteArray serialize(const QVector<Entry> &entries)
{
    QString s = QStringLiteral("#EXTM3U\n");
    for (const Entry &e : entries) {
        if (!e.title.isEmpty() || e.durationSec >= 0)
            s += QStringLiteral("#EXTINF:%1,%2\n").arg(e.durationSec).arg(e.title);
        s += e.location + QLatin1Char('\n');
    }
    return s.toUtf8();
}

bool writeFile(const QString &path, const QVector<Entry> &entries, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(serialize(entries));
    return true;
}

} // namespace lumina::m3u
