#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <atomic>

namespace lumina {

/**
 * Walks library folders, synchronises the `media` table, then reads metadata for new/changed files.
 * Lives on its own thread (see LibraryManager). Every phase checks the cancel flag, and the database
 * is only ever changed in small transactions, so cancelling or crashing mid-scan leaves a consistent
 * library; the next scan resumes with whatever is still marked `probed = 0`.
 *
 * Moved / renamed files are recognised by fingerprint (size + head/tail hash), so their history,
 * favourites and resume positions follow them instead of being orphaned.
 */
class LibraryScanner : public QObject {
    Q_OBJECT
public:
    struct Options {
        QStringList folders;
        QStringList excluded;
        bool removeMissing = false;
        int threads = 2;
        bool probeMetadata = true;   // tests can disable libVLC probing
    };

    explicit LibraryScanner(QObject *parent = nullptr) : QObject(parent) {}
    void requestCancel() { m_cancel = true; }

    static QString fingerprint(const QString &path, qint64 size);

public slots:
    void scan(const lumina::LibraryScanner::Options &options);

signals:
    void progress(const QString &phase, int done, int total);
    void batchCommitted();                       // data changed; UI may refresh (debounced there)
    void finished(const QVariantMap &summary);   // keys: added, moved, missing, probed, errors(list), cancelled

private:
    std::atomic<bool> m_cancel{false};
};

} // namespace lumina

Q_DECLARE_METATYPE(lumina::LibraryScanner::Options)
