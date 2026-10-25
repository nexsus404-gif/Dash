#ifndef PACKAGES_PARSER_H
#define PACKAGES_PARSER_H

// Парсит Packages.xz напрямую в SQLite.
// Zero-copy: никаких NSString/NSScanner.
// Возвращает количество распарсенных пакетов или -1 при ошибке.
int parse_packages_xz(const char *xz_path);

#endif
