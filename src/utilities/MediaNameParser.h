#pragma once

#include <QString>

namespace lumina {

struct ParsedName {
    QString kind = QStringLiteral("video");   // movie | episode | video (unclassified)
    QString title;
    QString showName;
    int season = 0;
    int episode = 0;
    int year = 0;
};

/// Derives a display title and a *conservative* classification from a file path.
/// "Show.Name.S02E05.1080p.mkv" -> episode; "Some.Movie.2019.BluRay.mkv" -> movie;
/// anything ambiguous stays "video" - the user can correct it manually, we never guess.
ParsedName parseMediaName(const QString &path);

QString cleanTitle(QString s);

} // namespace lumina
