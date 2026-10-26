#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>

namespace lumina {

struct QueueEntry {
    qint64 mediaId = 0;
    QUrl url;
    QString title;
    bool isStream = false;
    qint64 durationMs = 0;
};

/// The play queue shown in the player's side panel. Pure data + list-model plumbing (no playback logic),
/// so it can be unit-tested without libVLC.
class QueueModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
public:
    enum Roles { MediaIdRole = Qt::UserRole + 1, TitleRole, UrlRole, IsCurrentRole, IsStreamRole, MissingRole, DurationRole, IsPastRole };

    explicit QueueModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_items.size()); }
    int currentIndex() const { return m_current; }
    const QueueEntry &at(int i) const { return m_items.at(i); }
    bool isValid(int i) const { return i >= 0 && i < m_items.size(); }

    void setEntries(const QVector<QueueEntry> &e, int current);
    void append(const QVector<QueueEntry> &e);
    void insertNext(const QVector<QueueEntry> &e);   // right after the current item
    void setCurrent(int i);
    int indexOfMedia(qint64 id) const;

    Q_INVOKABLE bool removeAt(int i);        // adjusts the current index; removing the current item keeps playing it
    Q_INVOKABLE bool move(int from, int to);
    Q_INVOKABLE void clearExceptCurrent();
    QVector<QueueEntry> entries() const { return m_items; }

signals:
    void countChanged();
    void currentIndexChanged();

private:
    QVector<QueueEntry> m_items;
    int m_current = -1;
};

} // namespace lumina
