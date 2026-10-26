#pragma once

#include <QSqlDatabase>
#include <QString>

namespace lumina {

/**
 * SQLite access with a versioned schema.
 *
 *  - open() returns a connection for the *calling thread* (QSqlDatabase connections must not cross
 *    threads), creating it on first use. Every thread uses WAL + foreign keys + a busy timeout.
 *  - The schema is upgraded with ordered migrations tracked by `PRAGMA user_version`; each migration
 *    runs in its own transaction, so a failure leaves the previous version intact.
 *  - If the file is corrupt (cannot be opened, or fails `PRAGMA quick_check`) it is moved aside to
 *    "<name>.corrupt-<timestamp>" and a fresh database is created, so the app always starts.
 */
class Database {
public:
    static constexpr int kSchemaVersion = 2;

    /// Set once at startup (or in tests). Opens/migrates the database file and returns true on success.
    static bool initialize(const QString &path, QString *errorOut = nullptr, bool *recreatedOut = nullptr);
    static QString path();
    static QSqlDatabase open();            // per-thread connection
    static void closeAll();                // call before deleting the file in tests
    static int schemaVersion();

    /// Exposed for tests: apply migrations from `from` to the newest version on a raw connection.
    static bool migrate(QSqlDatabase &db, int from, QString *errorOut = nullptr);

private:
    static bool configure(QSqlDatabase &db);
};

} // namespace lumina
