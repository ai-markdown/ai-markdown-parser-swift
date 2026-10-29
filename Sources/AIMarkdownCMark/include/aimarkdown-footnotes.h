// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_FOOTNOTES_H
#define AIMARKDOWN_CMARK_FOOTNOTES_H

#include "aimarkdown-map.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aimarkdown_cmark_footnote {
  aimarkdown_cmark_map_entry entry;
  aimarkdown_cmark_node *node;
  unsigned int ix;
};

typedef struct aimarkdown_cmark_footnote aimarkdown_cmark_footnote;

void aimarkdown_cmark_footnote_create(aimarkdown_cmark_map *map, aimarkdown_cmark_node *node);
aimarkdown_cmark_map *aimarkdown_cmark_footnote_map_new(aimarkdown_cmark_mem *mem);

void aimarkdown_cmark_unlink_footnotes_map(aimarkdown_cmark_map *map);

#ifdef __cplusplus
}
#endif

#endif
