// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_GFM_STRIKETHROUGH_H
#define AIMARKDOWN_CMARK_GFM_STRIKETHROUGH_H

#include "aimarkdown-cmark-gfm-core-extensions.h"

extern aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_STRIKETHROUGH;
aimarkdown_cmark_syntax_extension *aimarkdown_create_strikethrough_extension(void);

#endif
