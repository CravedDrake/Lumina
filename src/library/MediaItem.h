#pragma once

#include <QString>
#include <QVariantMap>

namespace lumina {

struct MediaItem {
    qint64 id = 0;
    QString path;
    QString title;
    QString kind;               // movie | episode | video | stream
    QString showName;
    int season = 0, episode = 0, year = 0;
    qint64 size = 0, mtime = 0;
    qint64 durationMs = 0, positionMs = 0;
    qint64 lastPlayed = 0, addedAt = 0;
    int width = 0, height = 0;
    double fps = 0;
    QString vcodec, acodec;
    int audioTracks = 0, subTracks = 0, playCount = 0;
    bool favorite = false, completed = false, missing = false, probed = false;
    qint64 playedAt = 0;        // history view only
    qint64 historyId = 0;       // history view only
    qint64 playlistItemId = 0;  // playlist view only

    bool isStream() const { return kind == QLatin1String("stream"); }
    double progress() const
    {
        if (completed) return 1.0;
        return durationMs > 0 ? qBound(0.0, double(positionMs) / double(durationMs), 1.0) : 0.0;
    }
    QVariantMap toMap() const;
};

} // namespace lumina
