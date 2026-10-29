// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_GFM_TABLE_H
#define AIMARKDOWN_CMARK_GFM_TABLE_H

#include "aimarkdown-cmark-gfm-core-extensions.h"


extern aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_TABLE, AIMARKDOWN_CMARK_NODE_TABLE_ROW,
    AIMARKDOWN_CMARK_NODE_TABLE_CELL;

aimarkdown_cmark_syntax_extension *aimarkdown_create_table_extension(void);

#endif
