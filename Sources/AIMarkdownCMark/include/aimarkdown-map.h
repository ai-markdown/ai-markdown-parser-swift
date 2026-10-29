// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_MAP_H
#define AIMARKDOWN_CMARK_MAP_H

#include "aimarkdown-chunk.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *aimarkdown_cmark_reference_key(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *label);


struct aimarkdown_cmark_map_entry {
  struct aimarkdown_cmark_map_entry *next;
  unsigned char *label;
  size_t age;
  size_t size;
};

typedef struct aimarkdown_cmark_map_entry aimarkdown_cmark_map_entry;

struct aimarkdown_cmark_map;

typedef void (*aimarkdown_cmark_map_free_f)(struct aimarkdown_cmark_map *, aimarkdown_cmark_map_entry *);

struct aimarkdown_cmark_map {
  aimarkdown_cmark_mem *mem;
  bool web_reference_labels;
  bool web_character_references;
  aimarkdown_cmark_map_entry *refs;
  aimarkdown_cmark_map_entry **sorted;
  size_t size;
  size_t ref_size;
  size_t max_ref_size;
  aimarkdown_cmark_map_free_f free;
};

typedef struct aimarkdown_cmark_map aimarkdown_cmark_map;

unsigned char *aimarkdown_normalize_map_label(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *ref);
aimarkdown_cmark_map *aimarkdown_cmark_map_new(aimarkdown_cmark_mem *mem, aimarkdown_cmark_map_free_f free);
void aimarkdown_cmark_map_free(aimarkdown_cmark_map *map);
aimarkdown_cmark_map_entry *aimarkdown_cmark_map_lookup(aimarkdown_cmark_map *map, aimarkdown_cmark_chunk *label);

#ifdef __cplusplus
}
#endif

#endif
