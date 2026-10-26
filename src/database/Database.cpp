#include "Database.h"
#include <QCoreApplication>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMutex>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

Q_LOGGING_CATEGORY(lcDb, "lumina.db")

namespace lumina {

namespace {

QMutex g_mutex;
QString g_path;

// Ordered migrations. NEVER edit an existing entry once released; append a new one.
const char *const kMigrations[] = {
    // ---- v1: core schema ---------------------------------------------------------------
    R"SQL(
    CREATE TABLE folders(
        id INTEGER PRIMARY KEY,
        path TEXT NOT NULL UNIQUE,
        added_at INTEGER NOT NULL,
        last_scan INTEGER NOT NULL DEFAULT 0
    );
    CREATE TABLE media(
        id INTEGER PRIMARY KEY,
        path TEXT NOT NULL UNIQUE,                 -- file path, or URL (credentials are never stored)
        folder_id INTEGER REFERENCES folders(id) ON DELETE SET NULL,
        title TEXT NOT NULL DEFAULT '',
        kind TEXT NOT NULL DEFAULT 'video',        -- movie | episode | video | stream
        kind_manual INTEGER NOT NULL DEFAULT 0,    -- 1 = user-corrected; scans must not override
        show_name TEXT NOT NULL DEFAULT '',
        season INTEGER NOT NULL DEFAULT 0,
        episode INTEGER NOT NULL DEFAULT 0,
        year INTEGER NOT NULL DEFAULT 0,
        size INTEGER NOT NULL DEFAULT 0,
        mtime INTEGER NOT NULL DEFAULT 0,
        fingerprint TEXT NOT NULL DEFAULT '',      -- stable id used to follow moved/renamed files
        duration_ms INTEGER NOT NULL DEFAULT 0,
        width INTEGER NOT NULL DEFAULT 0,
        height INTEGER NOT NULL DEFAULT 0,
        fps REAL NOT NULL DEFAULT 0,
        vcodec TEXT NOT NULL DEFAULT '',
        acodec TEXT NOT NULL DEFAULT '',
        audio_tracks INTEGER NOT NULL DEFAULT 0,
        sub_tracks INTEGER NOT NULL DEFAULT 0,
        added_at INTEGER NOT NULL DEFAULT 0,
        missing INTEGER NOT NULL DEFAULT 0,
        probed INTEGER NOT NULL DEFAULT 0,         -- 0 = metadata still to be read
        position_ms INTEGER NOT NULL DEFAULT 0,
        completed INTEGER NOT NULL DEFAULT 0,
        last_played INTEGER NOT NULL DEFAULT 0,
        play_count INTEGER NOT NULL DEFAULT 0,
        favorite INTEGER NOT NULL DEFAULT 0
    );
    CREATE INDEX idx_media_folder ON media(folder_id);
    CREATE INDEX idx_media_fingerprint ON media(fingerprint);
    CREATE INDEX idx_media_last_played ON media(last_played);
    CREATE INDEX idx_media_kind ON media(kind);
    CREATE TABLE history(
        id INTEGER PRIMARY KEY,
        media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,
        played_at INTEGER NOT NULL
    );
    CREATE INDEX idx_history_played ON history(played_at);
    CREATE TABLE playlists(
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        created_at INTEGER NOT NULL
    );
    CREATE TABLE playlist_items(
        id INTEGER PRIMARY KEY,
        playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
        media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,
        position INTEGER NOT NULL
    );
    CREATE INDEX idx_pl_items ON playlist_items(playlist_id, position);
    CREATE TABLE bookmarks(
        id INTEGER PRIMARY KEY,
        media_id INTEGER NOT NULL REFERENCES media(id) ON DELETE CASCADE,
        position_ms INTEGER NOT NULL,
        note TEXT NOT NULL DEFAULT '',
        created_at INTEGER NOT NULL
    );
    CREATE INDEX idx_bookmarks_media ON bookmarks(media_id);
    CREATE TABLE media_prefs(
        media_id INTEGER PRIMARY KEY REFERENCES media(id) ON DELETE CASCADE,
        json TEXT NOT NULL
    );
    )SQL",
    // ---- v2: stream metadata, artwork cache reference ----------------------------------
    R"SQL(
    ALTER TABLE media ADD COLUMN stream_favorite INTEGER NOT NULL DEFAULT 0;
    ALTER TABLE media ADD COLUMN thumb_key TEXT NOT NULL DEFAULT '';
    CREATE INDEX idx_media_missing ON media(missing);
    )SQL",
};
constexpr int kMigrationCount = int(sizeof(kMigrations) / sizeof(kMigrations[0]));
static_assert(kMigrationCount == Database::kSchemaVersion, "kSchemaVersion must equal the number of migrations");

// Per-thread connection bookkeeping. QSqlDatabase connections must never cross threads.
struct ConnectionHolder {
    QString name;
    QString boundPath;
    void release()
    {
        if (!name.isEmpty()) {
            {
                QSqlDatabase db = QSqlDatabase::database(name, false);
                if (db.isOpen())
                    db.close();
            }
            QSqlDatabase::removeDatabase(name);
            name.clear();
        }
    }
    // Pool threads can outlive QCoreApplication (their TLS is destroyed during process exit, after Qt SQL's
    // global registry is gone). Touching QSqlDatabase then crashes, and the OS reclaims the handle anyway.
    ~ConnectionHolder() { if (QCoreApplication::instance()) release(); }
};
thread_local ConnectionHolder t_holder;

QString connectionName()
{
    return QStringLiteral("lumina-%1-%2").arg(reinterpret_cast<quintptr>(QThread::currentThread()), 0, 16)
        .arg(QUuid::createUuid().toString(QUuid::Id128).left(6));
}

bool execScript(QSqlDatabase &db, const QString &script, QString *err)
{
    // QSqlQuery executes one statement at a time.
    // Strip "-- comments" first: they may legitimately contain ';'.
    QString cleaned;
    for (const QString &line : script.split(QLatin1Char('\n'))) {
        const int c = line.indexOf(QLatin1String("--"));
        cleaned += (c >= 0 ? line.left(c) : line) + QLatin1Char('\n');
    }
    const QStringList statements = cleaned.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    QSqlQuery q(db);
    for (const QString &stmt : statements) {
        if (stmt.trimmed().isEmpty())
            continue;
        if (!q.exec(stmt)) {
            if (err) *err = q.lastError().text();
            return false;
        }
    }
    return true;
}

} // namespace

bool Database::configure(QSqlDatabase &db)
{
    QSqlQuery q(db);
    return q.exec(QStringLiteral("PRAGMA foreign_keys=ON")) && q.exec(QStringLiteral("PRAGMA busy_timeout=5000")) &&
           q.exec(QStringLiteral("PRAGMA journal_mode=WAL")) && q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
}

int Database::schemaVersion()
{
    QSqlQuery q(open());
    if (q.exec(QStringLiteral("PRAGMA user_version")) && q.next())
        return q.value(0).toInt();
    return -1;
}

bool Database::migrate(QSqlDatabase &db, int from, QString *errorOut)
{
    for (int v = from; v < kMigrationCount; ++v) {
        if (!db.transaction()) {
            if (errorOut) *errorOut = db.lastError().text();
            return false;
        }
        QString err;
        if (!execScript(db, QString::fromUtf8(kMigrations[v]), &err) ||
            !QSqlQuery(db).exec(QStringLiteral("PRAGMA user_version=%1").arg(v + 1))) {
            db.rollback();
            if (errorOut) *errorOut = QStringLiteral("migration to v%1 failed: %2").arg(v + 1).arg(err);
            qCCritical(lcDb) << "migration to v" << (v + 1) << "failed:" << err;
            return false;
        }
        if (!db.commit()) {
            if (errorOut) *errorOut = db.lastError().text();
            return false;
        }
    }
    return true;
}

bool Database::initialize(const QString &path, QString *errorOut, bool *recreatedOut)
{
    closeAll();
    if (recreatedOut) *recreatedOut = false;
    QDir().mkpath(QFileInfo(path).absolutePath());

    auto tryOpen = [&](QString *err) -> bool {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("lumina-init"));
        db.setDatabaseName(path);
        bool ok = db.open() && configure(db);
        if (ok) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("PRAGMA quick_check")) && q.next() &&
                 q.value(0).toString() == QLatin1String("ok");
            if (!ok && err) *err = QStringLiteral("integrity check failed");
        } else if (err) {
            *err = db.lastError().text();
        }
        if (ok) {
            int ver = 0;
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("PRAGMA user_version")) && q.next())
                ver = q.value(0).toInt();
            if (ver > kSchemaVersion) {
                ok = false;
                if (err) *err = QStringLiteral("database was created by a newer version (schema %1)").arg(ver);
            } else {
                ok = migrate(db, ver, err);
            }
        }
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("lumina-init"));
        return ok;
    };

    QString err;
    bool ok = tryOpen(&err);
    if (!ok && QFile::exists(path)) {
        // Never lose the user's file silently: keep it next to the new one.
        const QString aside = path + QStringLiteral(".corrupt-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        qCWarning(lcDb) << "database unusable (" << err << "); moving to" << aside;
        QFile::remove(path + QStringLiteral("-wal"));
        QFile::remove(path + QStringLiteral("-shm"));
        if (QFile::rename(path, aside)) {
            if (recreatedOut) *recreatedOut = true;
            QString err2;
            ok = tryOpen(&err2);
            if (!ok) err = err2;
        }
    }
    if (errorOut) *errorOut = ok ? QString() : err;
    if (ok) {
        QMutexLocker lock(&g_mutex);
        g_path = path;
    }
    return ok;
}

QString Database::path()
{
    QMutexLocker lock(&g_mutex);
    return g_path;
}

QSqlDatabase Database::open()
{
    const QString p = path();
    if (!t_holder.name.isEmpty() && t_holder.boundPath == p) {
        QSqlDatabase db = QSqlDatabase::database(t_holder.name, true);
        if (db.isValid() && db.isOpen())
            return db;
    }
    t_holder.release();
    t_holder.name = connectionName();
    t_holder.boundPath = p;
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), t_holder.name);
    db.setDatabaseName(p);
    if (!db.open())
        qCCritical(lcDb) << "cannot open database:" << db.lastError().text();
    else
        configure(db);
    return db;
}

void Database::closeAll()
{
    // Only the calling thread's connection can be closed safely; worker threads release theirs
    // when they exit (ConnectionHolder destructor).
    t_holder.release();
}

} // namespace lumina
