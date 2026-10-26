#include "MediaNameParser.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace lumina {

namespace {

const QRegularExpression &releaseTagRe()
{
    static const QRegularExpression re(
        QStringLiteral("\\b(480p|576p|720p|1080p|1440p|2160p|4k|x264|x265|h\\.?264|h\\.?265|hevc|blu-?ray|brrip|bdrip|web-?rip|"
                       "web-?dl|hdtv|dvdrip|hdrip|remux|aac|ac3|dts|proper|repack|extended|unrated|10bit)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

QString stripAtTag(const QString &s)
{
    const auto m = releaseTagRe().match(s);
    return m.hasMatch() ? s.left(m.capturedStart()) : s;
}

} // namespace

QString cleanTitle(QString s)
{
    static const QRegularExpression brackets(QStringLiteral("\\[[^\\]]*\\]|\\{[^}]*\\}"));
    s.remove(brackets);
    s.replace(QLatin1Char('.'), QLatin1Char(' '));
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    s.replace(spaces, QStringLiteral(" "));
    s = s.trimmed();
    while (s.endsWith(QLatin1Char('-')) || s.endsWith(QLatin1Char('(')) || s.endsWith(QLatin1Char(' ')))
        s.chop(1);
    while (s.startsWith(QLatin1Char('-')) || s.startsWith(QLatin1Char(' ')))
        s.remove(0, 1);
    return s;
}

ParsedName parseMediaName(const QString &path)
{
    ParsedName out;
    const QFileInfo fi(path);
    QString base = fi.completeBaseName();
    out.title = cleanTitle(stripAtTag(base));
    if (out.title.isEmpty())
        out.title = cleanTitle(base);
    if (out.title.isEmpty())
        out.title = base;

    static const QRegularExpression sxe(QStringLiteral("(?<![A-Za-z0-9])S(\\d{1,2})[ ._-]?E(\\d{1,3})(?![0-9])"),
                                        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression nxm(QStringLiteral("(?<![A-Za-z0-9])(\\d{1,2})x(\\d{2,3})(?![0-9pP])"));
    static const QRegularExpression words(QStringLiteral("Season[ ._-]?(\\d{1,2})[ ._-]*Episode[ ._-]?(\\d{1,3})"),
                                          QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch m = sxe.match(base);
    if (!m.hasMatch()) m = nxm.match(base);
    if (!m.hasMatch()) m = words.match(base);
    if (m.hasMatch()) {
        out.kind = QStringLiteral("episode");
        out.season = m.captured(1).toInt();
        out.episode = m.captured(2).toInt();
        out.showName = cleanTitle(base.left(m.capturedStart()));
        if (out.showName.isEmpty()) {                       // fall back to the folder name
            QFileInfo dir(fi.absolutePath());
            static const QRegularExpression seasonDir(QStringLiteral("^(season|series|s)[ ._-]?\\d+$"),
                                                      QRegularExpression::CaseInsensitiveOption);
            if (seasonDir.match(dir.fileName()).hasMatch())
                dir = QFileInfo(dir.absolutePath());
            out.showName = cleanTitle(dir.fileName());
        }
        const QString rest = cleanTitle(stripAtTag(base.mid(m.capturedEnd())));
        out.title = QStringLiteral("%1 – S%2E%3")
                        .arg(out.showName.isEmpty() ? QStringLiteral("Episode") : out.showName)
                        .arg(out.season, 2, 10, QLatin1Char('0'))
                        .arg(out.episode, 2, 10, QLatin1Char('0'));
        if (!rest.isEmpty())
            out.title += QStringLiteral(" – ") + rest;
        return out;
    }

    // Movie: "Title (2019)" / "Title.2019.1080p" - needs a plausible year after a non-empty title.
    static const QRegularExpression yearRe(QStringLiteral("(?<![0-9])(19[2-9]\\d|20[0-4]\\d)(?![0-9])"));
    // Take the LAST plausible year that follows a non-empty title: "Blade Runner 2049 (2017)" -> 2017.
    auto it = yearRe.globalMatch(base);
    QRegularExpressionMatch best;
    while (it.hasNext()) {
        const auto ym = it.next();
        if (!cleanTitle(base.left(ym.capturedStart())).isEmpty()) best = ym;
    }
    if (best.hasMatch()) {
        out.kind = QStringLiteral("movie");
        out.year = best.captured(1).toInt();
        out.title = cleanTitle(base.left(best.capturedStart()));
        return out;
    }
    return out;   // unclassified
}

} // namespace lumina
