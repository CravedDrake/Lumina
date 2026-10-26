#include "QueueModel.h"

#include <QFileInfo>

namespace lumina {

QueueModel::QueueModel(QObject *parent) : QAbstractListModel(parent) {}

int QueueModel::rowCount(const QModelIndex &p) const { return p.isValid() ? 0 : int(m_items.size()); }

QHash<int, QByteArray> QueueModel::roleNames() const
{
    return {{MediaIdRole, "mediaId"}, {TitleRole, "title"}, {UrlRole, "url"}, {IsCurrentRole, "isCurrent"},
            {IsStreamRole, "isStream"}, {MissingRole, "missing"}, {DurationRole, "durationMs"}, {IsPastRole, "isPast"}};
}

QVariant QueueModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() < 0 || idx.row() >= m_items.size()) return {};
    const QueueEntry &e = m_items.at(idx.row());
    switch (role) {
    case MediaIdRole: return e.mediaId;
    case TitleRole: return e.title;
    case UrlRole: return e.url;
    case IsCurrentRole: return idx.row() == m_current;
    case IsStreamRole: return e.isStream;
    case MissingRole: return e.url.isLocalFile() && !QFileInfo::exists(e.url.toLocalFile());
    case DurationRole: return e.durationMs;
    case IsPastRole: return idx.row() < m_current;
    }
    return {};
}

void QueueModel::setEntries(const QVector<QueueEntry> &e, int current)
{
    beginResetModel();
    m_items = e;
    m_current = (current >= 0 && current < m_items.size()) ? current : (m_items.isEmpty() ? -1 : 0);
    endResetModel();
    emit countChanged();
    emit currentIndexChanged();
}

void QueueModel::append(const QVector<QueueEntry> &e)
{
    if (e.isEmpty()) return;
    beginInsertRows({}, int(m_items.size()), int(m_items.size() + e.size() - 1));
    m_items += e;
    endInsertRows();
    if (m_current < 0) { m_current = 0; emit currentIndexChanged(); }
    emit countChanged();
}

void QueueModel::insertNext(const QVector<QueueEntry> &e)
{
    if (e.isEmpty()) return;
    const int at = m_current + 1;
    beginInsertRows({}, at, at + int(e.size()) - 1);
    for (int i = 0; i < e.size(); ++i) m_items.insert(at + i, e.at(i));
    endInsertRows();
    emit countChanged();
}

void QueueModel::setCurrent(int i)
{
    if (i == m_current || (i >= m_items.size())) return;
    const int old = m_current;
    m_current = i;
    if (!m_items.isEmpty()) {
        // every row's "isPast" may change: refresh the whole range (queues are small)
        emit dataChanged(index(0), index(int(m_items.size()) - 1), {IsCurrentRole, IsPastRole});
    }
    Q_UNUSED(old)
    emit currentIndexChanged();
}

int QueueModel::indexOfMedia(qint64 id) const
{
    for (int i = 0; i < m_items.size(); ++i) if (m_items.at(i).mediaId == id) return i;
    return -1;
}

bool QueueModel::removeAt(int i)
{
    if (!isValid(i)) return false;
    beginRemoveRows({}, i, i);
    m_items.removeAt(i);
    endRemoveRows();
    if (i < m_current) { --m_current; emit currentIndexChanged(); }
    else if (i == m_current) { /* current media keeps playing but is no longer in the list */ m_current = -1; emit currentIndexChanged(); }
    emit countChanged();
    return true;
}

bool QueueModel::move(int from, int to)
{
    if (!isValid(from) || !isValid(to) || from == to) return false;
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_items.move(from, to);
    endMoveRows();
    if (m_current == from) m_current = to;
    else if (from < m_current && to >= m_current) --m_current;
    else if (from > m_current && to <= m_current) ++m_current;
    emit currentIndexChanged();
    if (!m_items.isEmpty()) emit dataChanged(index(0), index(int(m_items.size()) - 1), {IsCurrentRole, IsPastRole});
    return true;
}

void QueueModel::clearExceptCurrent()
{
    if (m_current < 0 || m_current >= m_items.size()) { setEntries({}, -1); return; }
    const QueueEntry keep = m_items.at(m_current);
    setEntries({keep}, 0);
}

} // namespace lumina
