// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef TASKLIST_H
#define TASKLIST_H

#include "aimarkdown-cmark-gfm-core-extensions.h"

aimarkdown_cmark_syntax_extension *aimarkdown_create_tasklist_extension(void);

#endif
