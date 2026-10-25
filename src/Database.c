#include "Database.h"
#include <stdio.h>
#include <string.h>

static sqlite3      *g_db         = NULL;
static sqlite3_stmt *g_stmt_page  = NULL;
static sqlite3_stmt *g_stmt_search = NULL;
static sqlite3_stmt *g_stmt_insert = NULL;

/* ───────── Schema ───────── */
static int db_init_schema(void) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS package ("
        "  rowid INTEGER PRIMARY KEY,"
        "  package TEXT NOT NULL,"
        "  name TEXT,"
        "  version TEXT NOT NULL,"
        "  section TEXT,"
        "  depends TEXT,"
        "  filename TEXT,"
        "  sha256 TEXT,"
        "  description TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_pkg_name ON package(name);"
        "CREATE INDEX IF NOT EXISTS idx_pkg_pkg  ON package(package);";

    char *err = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[Dash] schema: %s\n", err);
        sqlite3_free(err);
    }
    return rc;
}

/* ───────── Open / Close ───────── */
int db_open(const char *path) {
    int rc = sqlite3_open_v2(path, &g_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL);
    if (rc != SQLITE_OK) return rc;

    // iPhone 6 (1GB RAM): mmap=0 чтобы избежать jetsam
    sqlite3_exec(g_db,
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "PRAGMA cache_size=-4000;"
        "PRAGMA mmap_size=0;"
        "PRAGMA temp_store=MEMORY;",
        NULL, NULL, NULL);

    return db_init_schema();
}

void db_close(void) {
    if (g_stmt_page)   { sqlite3_finalize(g_stmt_page);   g_stmt_page = NULL; }
    if (g_stmt_search) { sqlite3_finalize(g_stmt_search); g_stmt_search = NULL; }
    if (g_stmt_insert) { sqlite3_finalize(g_stmt_insert); g_stmt_insert = NULL; }
    if (g_db)          { sqlite3_close(g_db);             g_db = NULL; }
}

/* ───────── Pagination (keyset, NOT offset) ───────── */
int db_get_packages(sqlite3_int64 last_rowid,
                    pkg_callback_t cb, void *ctx) {
    if (!g_stmt_page) {
        const char *sql =
            "SELECT rowid, package, name, version FROM package "
            "WHERE rowid > ? ORDER BY rowid LIMIT 200";
        sqlite3_prepare_v2(g_db, sql, -1, &g_stmt_page, NULL);
    }

    sqlite3_reset(g_stmt_page);
    sqlite3_bind_int64(g_stmt_page, 1, last_rowid);

    int count = 0;
    while (sqlite3_step(g_stmt_page) == SQLITE_ROW) {
        cb(sqlite3_column_int64(g_stmt_page, 0),
           (const char *)sqlite3_column_text(g_stmt_page, 1),
           (const char *)sqlite3_column_text(g_stmt_page, 2),
           (const char *)sqlite3_column_text(g_stmt_page, 3),
           ctx);
        count++;
    }
    return count;
}

/* ───────── Search ───────── */
int db_search_packages(const char *query, int limit,
                       pkg_callback_t cb, void *ctx) {
    if (!g_stmt_search) {
        const char *sql =
            "SELECT rowid, package, name, version FROM package "
            "WHERE name LIKE ? ESCAPE '\\' "
            "ORDER BY name LIMIT ?";
        sqlite3_prepare_v2(g_db, sql, -1, &g_stmt_search, NULL);
    }

    // Оборачиваем запрос в %...%
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%%%s%%", query);

    sqlite3_reset(g_stmt_search);
    sqlite3_bind_text(g_stmt_search, 1, pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(g_stmt_search, 2, limit);

    int count = 0;
    while (sqlite3_step(g_stmt_search) == SQLITE_ROW) {
        cb(sqlite3_column_int64(g_stmt_search, 0),
           (const char *)sqlite3_column_text(g_stmt_search, 1),
           (const char *)sqlite3_column_text(g_stmt_search, 2),
           (const char *)sqlite3_column_text(g_stmt_search, 3),
           ctx);
        count++;
    }
    return count;
}

/* ───────── Bulk Insert ───────── */
int db_begin_bulk_insert(void) {
    sqlite3_exec(g_db,
        "BEGIN TRANSACTION;"
        "PRAGMA synchronous=OFF;",   // безопасно внутри транзакции
        NULL, NULL, NULL);

    if (!g_stmt_insert) {
        const char *sql =
            "INSERT OR REPLACE INTO package "
            "(package,name,version,section,depends,filename,sha256,description) "
            "VALUES (?,?,?,?,?,?,?,?)";
        sqlite3_prepare_v2(g_db, sql, -1, &g_stmt_insert, NULL);
    }
    return SQLITE_OK;
}

int db_insert_package(const char *package, const char *name,
                      const char *version, const char *section,
                      const char *depends, const char *filename,
                      const char *sha256, const char *description) {
    sqlite3_reset(g_stmt_insert);
    sqlite3_bind_text(g_stmt_insert, 1, package,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g_stmt_insert, 2, name,        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g_stmt_insert, 3, version,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g_stmt_insert, 4, section,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g_stmt_insert, 5, depends,     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g_stmt_insert, 6, filename,    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g_stmt_insert, 7, sha256,      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g_stmt_insert, 8, description, -1, SQLITE_TRANSIENT);
    return sqlite3_step(g_stmt_insert) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
}

int db_end_bulk_insert(void) {
    sqlite3_exec(g_db,
        "END TRANSACTION;"
        "PRAGMA synchronous=NORMAL;",
        NULL, NULL, NULL);
    return SQLITE_OK;
}
