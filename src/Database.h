#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>

int  db_open(const char *path);
void db_close(void);

// Keyset-пагинация. last_rowid=0 для первой страницы.
// callback вызывается для каждой строки в main-thread-safe манере.
typedef void (*pkg_callback_t)(sqlite3_int64 rowid,
                               const char *package,
                               const char *name,
                               const char *version,
                               void *ctx);

int db_get_packages(sqlite3_int64 last_rowid,
                    pkg_callback_t cb, void *ctx);

int db_search_packages(const char *query, int limit,
                       pkg_callback_t cb, void *ctx);

// Массовая вставка. Вызывать в background thread.
int db_begin_bulk_insert(void);
int db_insert_package(const char *package, const char *name,
                      const char *version, const char *section,
                      const char *depends, const char *filename,
                      const char *sha256, const char *description);
int db_end_bulk_insert(void);

#endif
