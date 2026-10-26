#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace lumina::media {

inline const QSet<QString> &videoExtensions()
{
    static const QSet<QString> s = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"), QStringLiteral("mov"), QStringLiteral("wmv"),
        QStringLiteral("flv"), QStringLiteral("webm"), QStringLiteral("m4v"), QStringLiteral("mpg"), QStringLiteral("mpeg"),
        QStringLiteral("ts"), QStringLiteral("m2ts"), QStringLiteral("mts"), QStringLiteral("3gp"), QStringLiteral("ogv"),
        QStringLiteral("vob"), QStringLiteral("divx"), QStringLiteral("rm"), QStringLiteral("rmvb"), QStringLiteral("asf")};
    return s;
}
inline const QSet<QString> &audioExtensions()
{
    static const QSet<QString> s = {QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav"), QStringLiteral("ogg"),
                                    QStringLiteral("opus"), QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("wma"),
                                    QStringLiteral("mka")};
    return s;
}
inline const QSet<QString> &subtitleExtensions()
{
    static const QSet<QString> s = {QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"), QStringLiteral("vtt"),
                                    QStringLiteral("sub"), QStringLiteral("smi")};
    return s;
}
inline const QSet<QString> &playlistExtensions()
{
    static const QSet<QString> s = {QStringLiteral("m3u"), QStringLiteral("m3u8")};
    return s;
}

inline QString suffixOf(const QString &path)
{
    const int dot = path.lastIndexOf(QLatin1Char('.'));
    const int slash = qMax(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
    return dot > slash ? path.mid(dot + 1).toLower() : QString();
}
inline bool isVideo(const QString &p) { return videoExtensions().contains(suffixOf(p)); }
inline bool isAudio(const QString &p) { return audioExtensions().contains(suffixOf(p)); }
inline bool isMedia(const QString &p) { return isVideo(p) || isAudio(p); }
inline bool isSubtitle(const QString &p) { return subtitleExtensions().contains(suffixOf(p)); }
inline bool isPlaylist(const QString &p) { return playlistExtensions().contains(suffixOf(p)); }

inline QStringList mediaNameFilters()
{
    QStringList f;
    for (const auto &e : videoExtensions()) f << QStringLiteral("*.") + e;
    for (const auto &e : audioExtensions()) f << QStringLiteral("*.") + e;
    return f;
}

} // namespace lumina::media
