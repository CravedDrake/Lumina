#include "MediaItem.h"

namespace lumina {

QVariantMap MediaItem::toMap() const
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("path"), path}, {QStringLiteral("title"), title},
            {QStringLiteral("kind"), kind}, {QStringLiteral("showName"), showName}, {QStringLiteral("season"), season},
            {QStringLiteral("episode"), episode}, {QStringLiteral("year"), year}, {QStringLiteral("size"), size},
            {QStringLiteral("durationMs"), durationMs}, {QStringLiteral("positionMs"), positionMs},
            {QStringLiteral("width"), width}, {QStringLiteral("height"), height}, {QStringLiteral("fps"), fps},
            {QStringLiteral("vcodec"), vcodec}, {QStringLiteral("acodec"), acodec},
            {QStringLiteral("audioTracks"), audioTracks}, {QStringLiteral("subTracks"), subTracks},
            {QStringLiteral("favorite"), favorite}, {QStringLiteral("completed"), completed},
            {QStringLiteral("missing"), missing}, {QStringLiteral("lastPlayed"), lastPlayed},
            {QStringLiteral("addedAt"), addedAt}, {QStringLiteral("playCount"), playCount},
            {QStringLiteral("probed"), probed}};
}

} // namespace lumina
