#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVector>
#include <QHash>
#include <QVariant>
#include <QByteArray>
#include <QModelIndex>
#include <QObject>
#include <QString>

#include "MediaItem.h"

namespace lumina {

/**
 * List model over a query of the media table. Queries run on a worker thread and the result is swapped
 * in on the GUI thread; a generation counter drops results of superseded queries, so typing in the
 * search box or flipping views quickly can never show stale rows.
 *
 * views: all | movies | tv | recent | continue | favorites | added | history | streams | playlist
 */
class LibraryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(qint64 viewArg READ viewArg WRITE setViewArg NOTIFY viewArgChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortChanged)
    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1, PathRole, TitleRole, FileNameRole, KindRole, ShowNameRole, SeasonRole, EpisodeRole,
        YearRole, DurationRole, PositionRole, ProgressRole, WidthRole, HeightRole, ResolutionRole, FavoriteRole,
        CompletedRole, MissingRole, SizeRole, LastPlayedRole, AddedAtRole, ThumbnailRole, PlayedAtRole, HistoryIdRole,
        PlaylistItemIdRole, DetailRole, IsStreamRole, ProbedRole
    };
    explicit LibraryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString view() const { return m_view; }
    void setView(const QString &v);
    qint64 viewArg() const { return m_viewArg; }
    void setViewArg(qint64 a);
    QString search() const { return m_search; }
    void setSearch(const QString &s);
    QString sortKey() const { return m_sortKey; }
    void setSortKey(const QString &k);
    bool sortDescending() const { return m_desc; }
    void setSortDescending(bool d);
    int limit() const { return m_limit; }
    void setLimit(int l);
    int count() const { return int(m_items.size()); }
    bool loading() const { return m_loading; }
    bool active() const { return m_active; }
    void setActive(bool a);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE qint64 idAt(int row) const;
    Q_INVOKABLE QVariantMap get(int row) const;
    /// All ids currently listed (used to queue a whole view).
    Q_INVOKABLE QVariantList allIds() const;

    /// Synchronous query used by tests and by queue building.
    static QVector<MediaItem> query(const QString &view, qint64 viewArg, const QString &search, const QString &sortKey,
                                    bool desc, int limit);

signals:
    void viewChanged();
    void viewArgChanged();
    void searchChanged();
    void sortChanged();
    void limitChanged();
    void countChanged();
    void loadingChanged();
    void activeChanged();

private:
    void scheduleRefresh();
    QString m_view = QStringLiteral("all");
    qint64 m_viewArg = 0;
    QString m_search;
    QString m_sortKey;           // empty = the view's natural order
    bool m_desc = false;
    int m_limit = 0;
    bool m_loading = false;
    bool m_active = true;
    bool m_dirtyWhileInactive = false;
    quint64 m_generation = 0;
    QVector<MediaItem> m_items;
    QFutureWatcher<QVector<MediaItem>> *m_watcher = nullptr;
};

} // namespace lumina
