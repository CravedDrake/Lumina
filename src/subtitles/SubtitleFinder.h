#pragma once

#include <QString>
#include <QVector>

namespace lumina {

struct SubtitleCandidate {
    QString path;
    QString language;   // guessed from the file name ("en", "eng", "english"...), may be empty
    int score = 0;      // higher = better match to the video's file name
};

/// Finds subtitle files that very likely belong to `videoPath`: same base name in the same
/// folder (or in Subs/Subtitles sub-folders), optionally followed by a language tag.
QVector<SubtitleCandidate> findExternalSubtitles(const QString &videoPath);

/// Order candidates by the user's preferred language list (ISO 639-2/1 codes or English names).
int languageRank(const QString &guessedLanguage, const QStringList &preferred);

} // namespace lumina
