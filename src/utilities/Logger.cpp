#include "Logger.h"
#include "Redact.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QThread>
#include <QTextStream>
#include <cstdio>

Q_LOGGING_CATEGORY(lcApp, "lumina.app")
Q_LOGGING_CATEGORY(lcPlayback, "lumina.playback")
Q_LOGGING_CATEGORY(lcVlc, "lumina.vlc")
Q_LOGGING_CATEGORY(lcLibrary, "lumina.library")
Q_LOGGING_CATEGORY(lcNet, "lumina.net")

namespace lumina {

static QtMessageHandler s_prevHandler = nullptr;

static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    int level = 1;
    switch (type) {
    case QtDebugMsg: level = 0; break;
    case QtInfoMsg: level = 1; break;
    case QtWarningMsg: level = 2; break;
    case QtCriticalMsg:
    case QtFatalMsg: level = 3; break;
    }
    const QString cat = ctx.category && QByteArray(ctx.category) != "default" ? QString::fromLatin1(ctx.category) : QStringLiteral("qt");
    Logger::instance()->log(level, cat, msg);
    if (s_prevHandler && level >= 2) {
        // keep Qt's default stderr output for warnings and errors (already redacted by us would be nicer,
        // but Qt warnings never contain stream URLs)
        s_prevHandler(type, ctx, msg);
    }
}

Logger::Logger(QObject *parent) : QAbstractListModel(parent) {}

Logger *Logger::instance()
{
    static Logger *inst = new Logger(); // intentionally leaked: must outlive every other object
    return inst;
}

void Logger::install(const QString &logDir, int maxFileBytes)
{
    QDir().mkpath(logDir);
    m_filePath = QDir(logDir).filePath(QStringLiteral("lumina.log"));
    m_maxFileBytes = maxFileBytes;
    {
        QMutexLocker l(&m_mutex);
        m_file.setFileName(m_filePath);
        m_file.open(QIODevice::Append | QIODevice::Text);
        m_fileBytes = m_file.size();
    }
    if (!s_prevHandler)
        s_prevHandler = qInstallMessageHandler(messageHandler);
}

void Logger::setMinLevel(int l)
{
    if (l == m_minLevel) return;
    m_minLevel = qBound(0, l, 3);
    emit minLevelChanged();
}

void Logger::rotateIfNeeded()
{
    if (m_maxFileBytes <= 0 || m_fileBytes < m_maxFileBytes || !m_file.isOpen()) return;
    m_file.close();
    const QString old = m_filePath + QStringLiteral(".1");
    QFile::remove(old);
    QFile::rename(m_filePath, old);
    m_file.setFileName(m_filePath);
    m_file.open(QIODevice::Append | QIODevice::Text);
    m_fileBytes = 0;
}

void Logger::writeFile(const LogEntry &e)
{
    if (!m_file.isOpen()) return;
    static const char *names[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
    const QByteArray line = QDateTime::fromMSecsSinceEpoch(e.tsMs).toString(Qt::ISODateWithMs).toUtf8() + ' ' +
                            names[qBound(0, e.level, 3)] + ' ' + e.category.toUtf8() + ": " + e.message.toUtf8() + '\n';
    m_file.write(line);
    m_file.flush();
    m_fileBytes += line.size();
    rotateIfNeeded();
}

void Logger::log(int level, const QString &category, const QString &messageIn)
{
    LogEntry e;
    e.tsMs = QDateTime::currentMSecsSinceEpoch();
    e.level = level;
    e.category = category;
    e.message = redactLogLine(messageIn);
    {
        QMutexLocker l(&m_mutex);
        m_all.append(e);
        if (m_all.size() > m_maxEntries) m_all.removeFirst();
        writeFile(e);
    }
    if (level < m_minLevel) return;
    if (QThread::currentThread() == thread())
        appendUi(e);
    else
        QMetaObject::invokeMethod(this, [this, e] { appendUi(e); }, Qt::QueuedConnection);
}

void Logger::appendUi(const LogEntry &e)
{
    if (int(m_entries.size()) >= m_maxEntries) {
        beginRemoveRows({}, 0, 0);
        m_entries.pop_front();
        endRemoveRows();
    }
    beginInsertRows({}, int(m_entries.size()), int(m_entries.size()));
    m_entries.push_back(e);
    endInsertRows();
}

int Logger::rowCount(const QModelIndex &p) const { return p.isValid() ? 0 : int(m_entries.size()); }

QVariant Logger::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= int(m_entries.size())) return {};
    const LogEntry &e = m_entries[size_t(index.row())];
    switch (role) {
    case TimeRole: return QDateTime::fromMSecsSinceEpoch(e.tsMs).toString(QStringLiteral("HH:mm:ss.zzz"));
    case LevelRole: return e.level;
    case CategoryRole: return e.category;
    case MessageRole: return e.message;
    case TextRole: return QStringLiteral("%1 %2: %3").arg(QDateTime::fromMSecsSinceEpoch(e.tsMs).toString(QStringLiteral("HH:mm:ss.zzz")), e.category, e.message);
    }
    return {};
}

QHash<int, QByteArray> Logger::roleNames() const
{
    return {{TimeRole, "time"}, {LevelRole, "level"}, {CategoryRole, "category"}, {MessageRole, "message"}, {TextRole, "text"}};
}

void Logger::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

QList<LogEntry> Logger::snapshot() const
{
    QMutexLocker l(&m_mutex);
    return m_all;
}

QString Logger::asText() const
{
    QString out;
    for (const auto &e : snapshot())
        out += QStringLiteral("%1 [%2] %3: %4\n")
                   .arg(QDateTime::fromMSecsSinceEpoch(e.tsMs).toString(Qt::ISODateWithMs))
                   .arg(e.level).arg(e.category, e.message);
    return out;
}

} // namespace lumina
