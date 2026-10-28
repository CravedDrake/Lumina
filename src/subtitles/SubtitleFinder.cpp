#include "SubtitleFinder.h"
#include "utilities/MediaFiles.h"

#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QVector>
#include <QStringList>
#include <QString>
#include <algorithm>

namespace lumina {

namespace {
const QMap<QString, QString> &languageAliases()
{
    static const QMap<QString, QString> m = {
        {QStringLiteral("en"), QStringLiteral("eng")},  {QStringLiteral("eng"), QStringLiteral("eng")},
        {QStringLiteral("english"), QStringLiteral("eng")}, {QStringLiteral("es"), QStringLiteral("spa")},
        {QStringLiteral("spa"), QStringLiteral("spa")}, {QStringLiteral("spanish"), QStringLiteral("spa")},
        {QStringLiteral("fr"), QStringLiteral("fra")},  {QStringLiteral("fra"), QStringLiteral("fra")},
        {QStringLiteral("fre"), QStringLiteral("fra")}, {QStringLiteral("french"), QStringLiteral("fra")},
        {QStringLiteral("de"), QStringLiteral("deu")},  {QStringLiteral("deu"), QStringLiteral("deu")},
        {QStringLiteral("ger"), QStringLiteral("deu")}, {QStringLiteral("german"), QStringLiteral("deu")},
        {QStringLiteral("it"), QStringLiteral("ita")},  {QStringLiteral("ita"), QStringLiteral("ita")},
        {QStringLiteral("pt"), QStringLiteral("por")},  {QStringLiteral("por"), QStringLiteral("por")},
        {QStringLiteral("ru"), QStringLiteral("rus")},  {QStringLiteral("rus"), QStringLiteral("rus")},
        {QStringLiteral("ja"), QStringLiteral("jpn")},  {QStringLiteral("jpn"), QStringLiteral("jpn")},
        {QStringLiteral("zh"), QStringLiteral("zho")},  {QStringLiteral("zho"), QStringLiteral("zho")},
        {QStringLiteral("chi"), QStringLiteral("zho")}, {QStringLiteral("ko"), QStringLiteral("kor")},
        {QStringLiteral("kor"), QStringLiteral("kor")}, {QStringLiteral("hi"), QStringLiteral("hin")},
        {QStringLiteral("hin"), QStringLiteral("hin")}, {QStringLiteral("ne"), QStringLiteral("nep")},
        {QStringLiteral("nep"), QStringLiteral("nep")}, {QStringLiteral("ar"), QStringLiteral("ara")},
        {QStringLiteral("ara"), QStringLiteral("ara")},
    };
    return m;
}
QString canonical(const QString &s) { return languageAliases().value(s.toLower().trimmed(), s.toLower().trimmed()); }
} // namespace

int languageRank(const QString &guessed, const QStringList &preferred)
{
    if (guessed.isEmpty())
        return preferred.size() + 1;           // unknown language ranks after every preferred one
    const QString g = canonical(guessed);
    for (int i = 0; i < preferred.size(); ++i)
        if (canonical(preferred[i]) == g)
            return i;
    return preferred.size();
}

QVector<SubtitleCandidate> findExternalSubtitles(const QString &videoPath)
{
    QVector<SubtitleCandidate> out;
    const QFileInfo video(videoPath);
    if (!video.exists())
        return out;
    const QString base = video.completeBaseName().toLower();

    QStringList dirs{video.absolutePath()};
    for (const char *sub : {"Subs", "Subtitles", "subs", "subtitles"}) {
        const QString d = video.absolutePath() + QLatin1Char('/') + QString::fromLatin1(sub);
        if (QDir(d).exists() && !dirs.contains(d))
            dirs << d;
    }
    for (const QString &d : dirs) {
        const QFileInfoList files = QDir(d).entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
        for (const QFileInfo &f : files) {
            if (!media::isSubtitle(f.fileName()))
                continue;
            const QString sbase = f.completeBaseName().toLower();
            if (!sbase.startsWith(base))
                continue;
            SubtitleCandidate c;
            c.path = f.absoluteFilePath();
            const QString tail = sbase.mid(base.size());                 // e.g. ".en" or ".english.forced"
            const QStringList tokens = tail.split(QRegularExpression(QStringLiteral("[._\\- ]")), Qt::SkipEmptyParts);
            for (const QString &t : tokens)
                if (languageAliases().contains(t)) { c.language = t; break; }
            c.score = (tail.isEmpty() ? 100 : 80) + (d == video.absolutePath() ? 5 : 0);
            out.push_back(c);
        }
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.score > b.score; });
    return out;
}

} // namespace lumina
