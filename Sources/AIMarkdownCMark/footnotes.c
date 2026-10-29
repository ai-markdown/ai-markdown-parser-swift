// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-parser.h"
#include "aimarkdown-footnotes.h"
#include "aimarkdown-inlines.h"
#include "aimarkdown-chunk.h"

static void footnote_free(aimarkdown_cmark_map *map, aimarkdown_cmark_map_entry *_ref) {
  aimarkdown_cmark_footnote *ref = (aimarkdown_cmark_footnote *)_ref;
  aimarkdown_cmark_mem *mem = map->mem;
  if (ref != NULL) {
    mem->free(ref->entry.label);
    if (ref->node)
      aimarkdown_cmark_node_free(ref->node);
    mem->free(ref);
  }
}

void aimarkdown_cmark_footnote_create(aimarkdown_cmark_map *map, aimarkdown_cmark_node *node) {
  aimarkdown_cmark_footnote *ref;
  unsigned char *reflabel = aimarkdown_normalize_map_label(map->mem, &node->as.literal);

  /* empty footnote name, or composed from only whitespace */
  if (reflabel == NULL)
    return;

  assert(map->sorted == NULL);

  ref = (aimarkdown_cmark_footnote *)map->mem->calloc(1, sizeof(*ref));
  ref->entry.label = reflabel;
  ref->node = node;
  ref->entry.age = map->size;
  ref->entry.next = map->refs;

  map->refs = (aimarkdown_cmark_map_entry *)ref;
  map->size++;
}

aimarkdown_cmark_map *aimarkdown_cmark_footnote_map_new(aimarkdown_cmark_mem *mem) {
  return aimarkdown_cmark_map_new(mem, footnote_free);
}

// Before calling `cmark_map_free` on a map with `cmark_footnotes`, first
// unlink all of the footnote nodes before freeing their memory.
//
// Sometimes, two (unused) footnote nodes can end up referencing each other,
// which as they get freed up by calling `cmark_map_free` -> `footnote_free` ->
// etc, can lead to a use-after-free error.
//
// Better to `unlink` every footnote node first, setting their next, prev, and
// parent pointers to NULL, and only then walk thru & free them up.
void aimarkdown_cmark_unlink_footnotes_map(aimarkdown_cmark_map *map) {
  aimarkdown_cmark_map_entry *ref;
  aimarkdown_cmark_map_entry *next;

  ref = map->refs;
  while(ref) {
    next = ref->next;
    if (((aimarkdown_cmark_footnote *)ref)->node) {
      aimarkdown_cmark_node_unlink(((aimarkdown_cmark_footnote *)ref)->node);
    }
    ref = next;
  }
}
