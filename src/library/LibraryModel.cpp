#include "LibraryModel.h"
#include "database/Database.h"

#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrent>

namespace lumina {

namespace {

constexpr const char *kCols =
    "m.id,m.path,m.title,m.kind,m.show_name,m.season,m.episode,m.year,m.size,m.mtime,m.duration_ms,m.position_ms,"
    "m.last_played,m.added_at,m.width,m.height,m.fps,m.vcodec,m.acodec,m.audio_tracks,m.sub_tracks,m.play_count,"
    "m.favorite,m.completed,m.missing,m.probed";

QString escapeLike(QString s)
{
    s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    s.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    s.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    return s;
}

QString resolutionLabel(int w, int h)
{
    if (h <= 0) return {};
    if (h >= 2000) return QStringLiteral("4K");
    if (h >= 1400) return QStringLiteral("1440p");
    if (h >= 1000) return QStringLiteral("1080p");
    if (h >= 700) return QStringLiteral("720p");
    if (h >= 560) return QStringLiteral("576p");
    if (h >= 460) return QStringLiteral("480p");
    Q_UNUSED(w);
    return QStringLiteral("%1p").arg(h);
}

QString durationLabel(qint64 ms)
{
    if (ms <= 0) return {};
    const qint64 s = ms / 1000, h = s / 3600, m = (s / 60) % 60;
    if (h > 0) return QStringLiteral("%1h %2m").arg(h).arg(m);
    return m > 0 ? QStringLiteral("%1m").arg(m) : QStringLiteral("%1s").arg(qMax<qint64>(1, s));
}

} // namespace

LibraryModel::LibraryModel(QObject *parent) : QAbstractListModel(parent)
{
    m_watcher = new QFutureWatcher<QVector<MediaItem>>(this);
    connect(m_watcher, &QFutureWatcher<QVector<MediaItem>>::finished, this, [this] {
        // Results of superseded queries are discarded (the watcher is re-armed with the newest future).
        const quint64 gen = m_watcher->property("gen").toULongLong();
        if (gen != m_generation)
            return;
        beginResetModel();
        m_items = m_watcher->result();
        endResetModel();
        m_loading = false;
        emit loadingChanged();
        emit countChanged();
    });
}

int LibraryModel::rowCount(const QModelIndex &p) const { return p.isValid() ? 0 : int(m_items.size()); }

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {{IdRole, "mediaId"}, {PathRole, "path"}, {TitleRole, "title"}, {FileNameRole, "fileName"},
            {KindRole, "kind"}, {ShowNameRole, "showName"}, {SeasonRole, "season"}, {EpisodeRole, "episode"},
            {YearRole, "year"}, {DurationRole, "durationMs"}, {PositionRole, "positionMs"}, {ProgressRole, "progress"},
            {WidthRole, "videoWidth"}, {HeightRole, "videoHeight"}, {ResolutionRole, "resolution"},
            {FavoriteRole, "favorite"}, {CompletedRole, "completed"}, {MissingRole, "missing"}, {SizeRole, "size"},
            {LastPlayedRole, "lastPlayed"}, {AddedAtRole, "addedAt"}, {ThumbnailRole, "thumbnail"},
            {PlayedAtRole, "playedAt"}, {HistoryIdRole, "historyId"}, {PlaylistItemIdRole, "playlistItemId"},
            {DetailRole, "detail"}, {IsStreamRole, "isStream"}, {ProbedRole, "probed"}};
}

QVariant LibraryModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() < 0 || idx.row() >= m_items.size())
        return {};
    const MediaItem &m = m_items.at(idx.row());
    switch (role) {
    case IdRole: return m.id;
    case PathRole: return m.path;
    case TitleRole: return m.title.isEmpty() ? QFileInfo(m.path).completeBaseName() : m.title;
    case FileNameRole: return m.isStream() ? m.path : QFileInfo(m.path).fileName();
    case KindRole: return m.kind;
    case ShowNameRole: return m.showName;
    case SeasonRole: return m.season;
    case EpisodeRole: return m.episode;
    case YearRole: return m.year;
    case DurationRole: return m.durationMs;
    case PositionRole: return m.positionMs;
    case ProgressRole: return m.progress();
    case WidthRole: return m.width;
    case HeightRole: return m.height;
    case ResolutionRole: return resolutionLabel(m.width, m.height);
    case FavoriteRole: return m.favorite;
    case CompletedRole: return m.completed;
    case MissingRole: return m.missing;
    case SizeRole: return m.size;
    case LastPlayedRole: return m.lastPlayed;
    case AddedAtRole: return m.addedAt;
    case ThumbnailRole: return m.isStream() ? QString() : QStringLiteral("image://thumb/%1?v=%2").arg(m.id).arg(m.mtime);
    case PlayedAtRole: return m.playedAt;
    case HistoryIdRole: return m.historyId;
    case PlaylistItemIdRole: return m.playlistItemId;
    case IsStreamRole: return m.isStream();
    case ProbedRole: return m.probed;
    case DetailRole: {
        QStringList parts;
        const QString r = resolutionLabel(m.width, m.height);
        if (!r.isEmpty()) parts << r;
        const QString d = durationLabel(m.durationMs);
        if (!d.isEmpty()) parts << d;
        if (m.missing) parts << tr("File missing");
        return parts.join(QStringLiteral(" · "));
    }
    }
    return {};
}

void LibraryModel::setView(const QString &v) { if (v != m_view) { m_view = v; emit viewChanged(); scheduleRefresh(); } }
void LibraryModel::setViewArg(qint64 a) { if (a != m_viewArg) { m_viewArg = a; emit viewArgChanged(); scheduleRefresh(); } }
void LibraryModel::setSearch(const QString &s) { if (s != m_search) { m_search = s; emit searchChanged(); scheduleRefresh(); } }
void LibraryModel::setSortKey(const QString &k) { if (k != m_sortKey) { m_sortKey = k; emit sortChanged(); scheduleRefresh(); } }
void LibraryModel::setSortDescending(bool d) { if (d != m_desc) { m_desc = d; emit sortChanged(); scheduleRefresh(); } }
void LibraryModel::setLimit(int l) { if (l != m_limit) { m_limit = l; emit limitChanged(); scheduleRefresh(); } }

void LibraryModel::setActive(bool a)
{
    if (a == m_active) return;
    m_active = a;
    emit activeChanged();
    if (a && m_dirtyWhileInactive)
        refresh();
}

void LibraryModel::scheduleRefresh()
{
    // Coalesce property changes made in the same event-loop turn (QML sets view+arg+limit together).
    QMetaObject::invokeMethod(this, [this] { refresh(); }, Qt::QueuedConnection);
}

void LibraryModel::refresh()
{
    if (!m_active) { m_dirtyWhileInactive = true; return; }
    m_dirtyWhileInactive = false;
    const quint64 gen = ++m_generation;
    if (!m_loading) { m_loading = true; emit loadingChanged(); }
    const QString view = m_view, search = m_search, sort = m_sortKey;
    const qint64 arg = m_viewArg;
    const bool desc = m_desc;
    const int limit = m_limit;
    m_watcher->setProperty("gen", gen);
    m_watcher->setFuture(QtConcurrent::run([=] { return query(view, arg, search, sort, desc, limit); }));
}

qint64 LibraryModel::idAt(int row) const { return row >= 0 && row < m_items.size() ? m_items.at(row).id : -1; }
QVariantMap LibraryModel::get(int row) const { return row >= 0 && row < m_items.size() ? m_items.at(row).toMap() : QVariantMap{}; }

QVariantList LibraryModel::allIds() const
{
    QVariantList l;
    for (const auto &i : m_items) l << i.id;
    return l;
}

QVector<MediaItem> LibraryModel::query(const QString &view, qint64 arg, const QString &search, const QString &sortKey,
                                       bool desc, int limit)
{
    QString from = QStringLiteral("FROM media m");
    QString extraCols = QStringLiteral(",0,0,0");     // played_at, history_id, playlist_item_id
    QStringList where;
    QString order;
    QVariantList binds;

    if (view == QLatin1String("all")) { where << QStringLiteral("m.kind!='stream' AND m.missing=0"); order = QStringLiteral("m.title COLLATE NOCASE"); }
    else if (view == QLatin1String("movies")) { where << QStringLiteral("m.kind='movie' AND m.missing=0"); order = QStringLiteral("m.title COLLATE NOCASE"); }
    else if (view == QLatin1String("tv")) { where << QStringLiteral("m.kind='episode' AND m.missing=0"); order = QStringLiteral("m.show_name COLLATE NOCASE,m.season,m.episode,m.title"); }
    else if (view == QLatin1String("recent")) { where << QStringLiteral("m.last_played>0 AND m.missing=0"); order = QStringLiteral("m.last_played DESC"); }
    else if (view == QLatin1String("continue")) { where << QStringLiteral("m.position_ms>5000 AND m.completed=0 AND m.missing=0 AND m.kind!='stream'"); order = QStringLiteral("m.last_played DESC"); }
    else if (view == QLatin1String("favorites")) { where << QStringLiteral("m.favorite=1"); order = QStringLiteral("m.title COLLATE NOCASE"); }
    else if (view == QLatin1String("added")) { where << QStringLiteral("m.kind!='stream' AND m.missing=0"); order = QStringLiteral("m.added_at DESC,m.id DESC"); }
    else if (view == QLatin1String("streams")) { where << QStringLiteral("m.kind='stream'"); order = QStringLiteral("m.last_played DESC,m.id DESC"); }
    else if (view == QLatin1String("history")) {
        from = QStringLiteral("FROM history h JOIN media m ON m.id=h.media_id");
        extraCols = QStringLiteral(",h.played_at,h.id,0");
        order = QStringLiteral("h.played_at DESC,h.id DESC");
    } else if (view == QLatin1String("playlist")) {
        from = QStringLiteral("FROM playlist_items p JOIN media m ON m.id=p.media_id");
        extraCols = QStringLiteral(",0,0,p.id");
        where << QStringLiteral("p.playlist_id=?");
        binds << arg;
        order = QStringLiteral("p.position,p.id");
    } else { where << QStringLiteral("m.kind!='stream'"); order = QStringLiteral("m.title COLLATE NOCASE"); }   // "all"

    if (!search.trimmed().isEmpty()) {
        where << QStringLiteral("(m.title LIKE ? ESCAPE '\\' OR m.path LIKE ? ESCAPE '\\' OR m.show_name LIKE ? ESCAPE '\\')");
        const QString like = QLatin1Char('%') + escapeLike(search.trimmed()) + QLatin1Char('%');
        binds << like << like << like;
    }
    static const QHash<QString, QString> sortable = {
        {QStringLiteral("title"), QStringLiteral("m.title COLLATE NOCASE")}, {QStringLiteral("added"), QStringLiteral("m.added_at")},
        {QStringLiteral("played"), QStringLiteral("m.last_played")}, {QStringLiteral("duration"), QStringLiteral("m.duration_ms")},
        {QStringLiteral("size"), QStringLiteral("m.size")}, {QStringLiteral("resolution"), QStringLiteral("m.height")}};
    if (sortable.contains(sortKey))
        order = sortable.value(sortKey) + (desc ? QStringLiteral(" DESC") : QStringLiteral(" ASC")) + QStringLiteral(",m.id");

    QString sql = QStringLiteral("SELECT %1%2 %3").arg(QLatin1String(kCols), extraCols, from);
    if (!where.isEmpty()) sql += QStringLiteral(" WHERE ") + where.join(QStringLiteral(" AND "));
    sql += QStringLiteral(" ORDER BY ") + order;
    if (limit > 0) sql += QStringLiteral(" LIMIT %1").arg(limit);

    QVector<MediaItem> out;
    QSqlDatabase db = Database::open();
    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &b : binds) q.addBindValue(b);
    if (!q.exec())
        return out;
    while (q.next()) {
        MediaItem m;
        int i = 0;
        m.id = q.value(i++).toLongLong(); m.path = q.value(i++).toString(); m.title = q.value(i++).toString();
        m.kind = q.value(i++).toString(); m.showName = q.value(i++).toString(); m.season = q.value(i++).toInt();
        m.episode = q.value(i++).toInt(); m.year = q.value(i++).toInt(); m.size = q.value(i++).toLongLong();
        m.mtime = q.value(i++).toLongLong(); m.durationMs = q.value(i++).toLongLong(); m.positionMs = q.value(i++).toLongLong();
        m.lastPlayed = q.value(i++).toLongLong(); m.addedAt = q.value(i++).toLongLong(); m.width = q.value(i++).toInt();
        m.height = q.value(i++).toInt(); m.fps = q.value(i++).toDouble(); m.vcodec = q.value(i++).toString();
        m.acodec = q.value(i++).toString(); m.audioTracks = q.value(i++).toInt(); m.subTracks = q.value(i++).toInt();
        m.playCount = q.value(i++).toInt(); m.favorite = q.value(i++).toBool(); m.completed = q.value(i++).toBool();
        m.missing = q.value(i++).toBool(); m.probed = q.value(i++).toBool();
        m.playedAt = q.value(i++).toLongLong(); m.historyId = q.value(i++).toLongLong(); m.playlistItemId = q.value(i++).toLongLong();
        out.push_back(m);
    }
    return out;
}

} // namespace lumina
