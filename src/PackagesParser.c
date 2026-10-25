#include "PackagesParser.h"
#include "Database.h"
#include <lzma.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUF_SIZE   (64 * 1024)
#define FIELD_MAX  1024
#define BATCH_SIZE 500

typedef struct {
    char package[256];
    char name[512];
    char version[128];
    char section[128];
    char depends[1024];
    char filename[512];
    char sha256[128];
    char description[2048];
} PkgEntry;

static void reset_entry(PkgEntry *e) {
    memset(e, 0, sizeof(PkgEntry));
}

// Zero-copy field extraction: no strdup, no NSString
static void extract_field(const char *line, size_t len,
                          const char *prefix, char *out, size_t out_sz) {
    size_t plen = strlen(prefix);
    if (len <= plen) return;
    if (memcmp(line, prefix, plen) != 0) return;

    // Пропускаем пробел после ":"
    const char *val = line + plen;
    size_t vlen = len - plen;
    while (vlen > 0 && *val == ' ') { val++; vlen--; }

    // Убираем trailing \r\n
    while (vlen > 0 && (val[vlen-1] == '\r' || val[vlen-1] == '\n')) vlen--;

    if (vlen >= out_sz) vlen = out_sz - 1;
    memcpy(out, val, vlen);
    out[vlen] = '\0';
}

int parse_packages_xz(const char *xz_path) {
    FILE *f = fopen(xz_path, "rb");
    if (!f) return -1;

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
    if (ret != LZMA_OK) { fclose(f); return -1; }

    uint8_t in_buf[BUF_SIZE];
    uint8_t out_buf[BUF_SIZE];

    // Line buffer for decompressed text
    char line[FIELD_MAX];
    size_t line_len = 0;

    PkgEntry entry;
    reset_entry(&entry);
    int count = 0;
    int batch = 0;

    db_begin_bulk_insert();

    strm.next_in = NULL;
    strm.avail_in = 0;
    strm.next_out = out_buf;
    strm.avail_out = BUF_SIZE;

    lzma_action action = LZMA_RUN;
    int eof = 0;

    while (!eof || strm.avail_out == 0) {
        if (strm.avail_in == 0 && !eof) {
            size_t n = fread(in_buf, 1, BUF_SIZE, f);
            strm.next_in = in_buf;
            strm.avail_in = n;
            if (n == 0) { eof = 1; action = LZMA_FINISH; }
        }

        strm.next_out = out_buf;
        strm.avail_out = BUF_SIZE;

        ret = lzma_code(&strm, action);
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) break;

        size_t produced = BUF_SIZE - strm.avail_out;

        // Process decompressed bytes line by line
        for (size_t i = 0; i < produced; i++) {
            char c = (char)out_buf[i];
            if (c == '\n') {
                line[line_len] = '\0';

                if (line_len == 0 || (line_len == 1 && line[0] == '\r')) {
                    // Empty line = end of package block
                    if (entry.package[0] != '\0') {
                        db_insert_package(
                            entry.package, entry.name, entry.version,
                            entry.section, entry.depends, entry.filename,
                            entry.sha256, entry.description);
                        count++;
                        batch++;
                        if (batch >= BATCH_SIZE) {
                            db_end_bulk_insert();
                            db_begin_bulk_insert();
                            batch = 0;
                        }
                    }
                    reset_entry(&entry);
                } else {
                    extract_field(line, line_len, "Package:",     entry.package,     sizeof(entry.package));
                    extract_field(line, line_len, "Name:",        entry.name,        sizeof(entry.name));
                    extract_field(line, line_len, "Version:",     entry.version,     sizeof(entry.version));
                    extract_field(line, line_len, "Section:",     entry.section,     sizeof(entry.section));
                    extract_field(line, line_len, "Depends:",     entry.depends,     sizeof(entry.depends));
                    extract_field(line, line_len, "Filename:",    entry.filename,    sizeof(entry.filename));
                    extract_field(line, line_len, "SHA256:",      entry.sha256,      sizeof(entry.sha256));
                    extract_field(line, line_len, "Description:", entry.description, sizeof(entry.description));
                }
                line_len = 0;
            } else if (line_len < FIELD_MAX - 1) {
                line[line_len++] = c;
            }
        }

        if (ret == LZMA_STREAM_END) break;
    }

    // Flush last entry
    if (entry.package[0] != '\0') {
        db_insert_package(
            entry.package, entry.name, entry.version,
            entry.section, entry.depends, entry.filename,
            entry.sha256, entry.description);
        count++;
    }

    db_end_bulk_insert();
    lzma_end(&strm);
    fclose(f);
    return count;
}
