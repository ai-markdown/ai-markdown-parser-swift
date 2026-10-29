// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_REFERENCES_H
#define AIMARKDOWN_CMARK_REFERENCES_H

#include <stdbool.h>

#include "aimarkdown-map.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aimarkdown_cmark_reference {
  aimarkdown_cmark_map_entry entry;
  bool is_attributes_reference;
  aimarkdown_cmark_chunk url;
  aimarkdown_cmark_chunk title;
  aimarkdown_cmark_chunk attributes;
};

typedef struct aimarkdown_cmark_reference aimarkdown_cmark_reference;

void aimarkdown_cmark_reference_create(aimarkdown_cmark_map *map, aimarkdown_cmark_chunk *label,
                            aimarkdown_cmark_chunk *url, aimarkdown_cmark_chunk *title);
void aimarkdown_cmark_reference_create_attributes(aimarkdown_cmark_map *map, aimarkdown_cmark_chunk *label,
                                       aimarkdown_cmark_chunk *attributes);
aimarkdown_cmark_map *aimarkdown_cmark_reference_map_new(aimarkdown_cmark_mem *mem);

#ifdef __cplusplus
}
#endif

#endif
