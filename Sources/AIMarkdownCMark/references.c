// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <stdbool.h>

#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-parser.h"
#include "aimarkdown-references.h"
#include "aimarkdown-inlines.h"
#include "aimarkdown-chunk.h"

static void reference_free(aimarkdown_cmark_map *map, aimarkdown_cmark_map_entry *_ref) {
  aimarkdown_cmark_reference *ref = (aimarkdown_cmark_reference *)_ref;
  aimarkdown_cmark_mem *mem = map->mem;
  if (ref != NULL) {
    mem->free(ref->entry.label);
    aimarkdown_cmark_chunk_free(mem, &ref->url);
    aimarkdown_cmark_chunk_free(mem, &ref->title);
    aimarkdown_cmark_chunk_free(mem, &ref->attributes);
    mem->free(ref);
  }
}

void aimarkdown_cmark_reference_create(aimarkdown_cmark_map *map, aimarkdown_cmark_chunk *label,
                            aimarkdown_cmark_chunk *url, aimarkdown_cmark_chunk *title) {
  aimarkdown_cmark_reference *ref;
  unsigned char *reflabel = map->web_reference_labels ? aimarkdown_cmark_reference_key(map->mem, label) : aimarkdown_normalize_map_label(map->mem, label);

  /* empty reference name, or composed from only whitespace */
  if (reflabel == NULL)
    return;

  assert(map->sorted == NULL);

  ref = (aimarkdown_cmark_reference *)map->mem->calloc(1, sizeof(*ref));
  ref->entry.label = reflabel;
  ref->is_attributes_reference = false;
  ref->url = map->web_character_references ? aimarkdown_cmark_clean_url_web(map->mem, url) : aimarkdown_cmark_clean_url(map->mem, url);
  ref->title = map->web_character_references ? aimarkdown_cmark_clean_title_web(map->mem, title) : aimarkdown_cmark_clean_title(map->mem, title);
  ref->attributes = aimarkdown_cmark_chunk_literal("");
  ref->entry.age = map->size;
  ref->entry.next = map->refs;
  ref->entry.size = ref->url.len + ref->title.len;

  map->refs = (aimarkdown_cmark_map_entry *)ref;
  map->size++;
}

void aimarkdown_cmark_reference_create_attributes(aimarkdown_cmark_map *map, aimarkdown_cmark_chunk *label,
                                       aimarkdown_cmark_chunk *attributes) {
  aimarkdown_cmark_reference *ref;
  unsigned char *reflabel = aimarkdown_normalize_map_label(map->mem, label);

  /* empty reference name, or composed from only whitespace */
  if (reflabel == NULL)
    return;

  assert(map->sorted == NULL);

  ref = (aimarkdown_cmark_reference *)map->mem->calloc(1, sizeof(*ref));
  ref->entry.label = reflabel;
  ref->is_attributes_reference = true;
  ref->url = aimarkdown_cmark_chunk_literal("");
  ref->title = aimarkdown_cmark_chunk_literal("");
  ref->attributes = aimarkdown_cmark_clean_attributes(map->mem, attributes);
  ref->entry.age = map->size;
  ref->entry.next = map->refs;
  
  map->refs = (aimarkdown_cmark_map_entry *)ref;
  map->size++;
}

aimarkdown_cmark_map *aimarkdown_cmark_reference_map_new(aimarkdown_cmark_mem *mem) {
  return aimarkdown_cmark_map_new(mem, reference_free);
}
