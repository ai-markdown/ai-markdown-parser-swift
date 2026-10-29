// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_GFM_CORE_EXTENSIONS_H
#define AIMARKDOWN_CMARK_GFM_CORE_EXTENSIONS_H

#include "aimarkdown-cmark-gfm-extension_api.h"
#include "aimarkdown-export.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_gfm_core_extensions_ensure_registered(void);

AIMARKDOWN_CMARK_GFM_EXPORT
uint16_t aimarkdown_cmark_gfm_extensions_get_table_columns(aimarkdown_cmark_node *node);

/** Sets the number of columns for the table, returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_set_table_columns(aimarkdown_cmark_node *node, uint16_t n_columns);

AIMARKDOWN_CMARK_GFM_EXPORT
uint8_t *aimarkdown_cmark_gfm_extensions_get_table_alignments(aimarkdown_cmark_node *node);

/** Sets the alignments for the table, returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_set_table_alignments(aimarkdown_cmark_node *node, uint16_t ncols, uint8_t *alignments);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_get_table_row_is_header(aimarkdown_cmark_node *node);

/** Sets the column span for the table cell, returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_set_table_cell_colspan(aimarkdown_cmark_node *node, unsigned colspan);

/** Sets the row span for the table cell, returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_set_table_cell_rowspan(aimarkdown_cmark_node *node, unsigned rowspan);

/**
 Gets the column span for the table cell, returning \c UINT_MAX on error.

 A value of 0 indicates that the cell is a "filler" cell, intended to be overlapped with a previous
 cell with a span > 1.

 Column span is only parsed when \c CMARK_OPT_TABLE_SPANS is set.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
unsigned aimarkdown_cmark_gfm_extensions_get_table_cell_colspan(aimarkdown_cmark_node *node);

/**
 Gets the row span for the table cell, returning \c UINT_MAX on error.

 A value of 0 indicates that the cell is a "filler" cell, intended to be overlapped with a previous
 cell with a span > 1.

 Row span is only parsed when \c CMARK_OPT_TABLE_SPANS is set.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
unsigned aimarkdown_cmark_gfm_extensions_get_table_cell_rowspan(aimarkdown_cmark_node *node);

/** Sets whether the node is a table header row, returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_set_table_row_is_header(aimarkdown_cmark_node *node, int is_header);

AIMARKDOWN_CMARK_GFM_EXPORT
bool aimarkdown_cmark_gfm_extensions_get_tasklist_item_checked(aimarkdown_cmark_node *node);
/* For backwards compatibility */
#define aimarkdown_cmark_gfm_extensions_tasklist_is_checked aimarkdown_cmark_gfm_extensions_get_tasklist_item_checked

/** Sets whether a tasklist item is "checked" (completed), returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_set_tasklist_item_checked(aimarkdown_cmark_node *node, bool is_checked);

#ifdef __cplusplus
}
#endif

#endif
