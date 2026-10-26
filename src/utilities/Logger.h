#pragma once
#include <QAbstractListModel>
#include <QLoggingCategory>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QFile>
#include <deque>

Q_DECLARE_LOGGING_CATEGORY(lcApp)
Q_DECLARE_LOGGING_CATEGORY(lcPlayback)
Q_DECLARE_LOGGING_CATEGORY(lcVlc)
Q_DECLARE_LOGGING_CATEGORY(lcLibrary)
Q_DECLARE_LOGGING_CATEGORY(lcNet)

namespace lumina {

struct LogEntry {
    qint64 tsMs = 0;
    int level = 0;          // 0 debug, 1 info, 2 warning, 3 error
    QString category;
    QString message;
};

// Process-wide logger: ring buffer for the in-app log viewer + rotating file.
// All text is passed through redactLogLine() so stream credentials never reach disk.
class Logger : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int minLevel READ minLevel WRITE setMinLevel NOTIFY minLevelChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)
public:
    enum Roles { TimeRole = Qt::UserRole + 1, LevelRole, CategoryRole, MessageRole, TextRole };

    static Logger *instance();
    // Installs a Qt message handler that feeds this logger. Call once, early.
    void install(const QString &logDir, int maxFileBytes = 2 * 1024 * 1024);
    void log(int level, const QString &category, const QString &message);

    int minLevel() const { return m_minLevel; }
    void setMinLevel(int l);
    QString logFilePath() const { return m_filePath; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void clear();
    Q_INVOKABLE QString asText() const;

    QList<LogEntry> snapshot() const;

signals:
    void minLevelChanged();

private:
    explicit Logger(QObject *parent = nullptr);
    void appendUi(const LogEntry &e);
    void writeFile(const LogEntry &e);
    void rotateIfNeeded();

    mutable QMutex m_mutex;
    std::deque<LogEntry> m_entries;         // GUI-thread view (model)
    QList<LogEntry> m_all;                  // ring buffer, mutex protected
    int m_minLevel = 1;
    int m_maxEntries = 3000;
    QString m_filePath;
    QFile m_file;
    int m_maxFileBytes = 0;
    qint64 m_fileBytes = 0;
};

} // namespace lumina
