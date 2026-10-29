// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include "aimarkdown-chunk.h"
#include "aimarkdown-cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

bufsize_t aimarkdown__ext_scan_at(bufsize_t (*scanner)(const unsigned char *),
                       unsigned char *ptr, int len, bufsize_t offset);
bufsize_t aimarkdown__scan_table_start(const unsigned char *p);
bufsize_t aimarkdown__scan_table_cell(const unsigned char *p);
bufsize_t aimarkdown__scan_table_cell_end(const unsigned char *p);
bufsize_t aimarkdown__scan_table_row_end(const unsigned char *p);
bufsize_t aimarkdown__scan_tasklist(const unsigned char *p);

#define scan_table_start(c, l, n) aimarkdown__ext_scan_at(&aimarkdown__scan_table_start, c, l, n)
#define scan_table_cell(c, l, n) aimarkdown__ext_scan_at(&aimarkdown__scan_table_cell, c, l, n)
#define scan_table_cell_end(c, l, n) aimarkdown__ext_scan_at(&aimarkdown__scan_table_cell_end, c, l, n)
#define scan_table_row_end(c, l, n) aimarkdown__ext_scan_at(&aimarkdown__scan_table_row_end, c, l, n)
#define scan_tasklist(c, l, n) aimarkdown__ext_scan_at(&aimarkdown__scan_tasklist, c, l, n)

#ifdef __cplusplus
}
#endif
