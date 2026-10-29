// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include "aimarkdown-map.h"
#include "aimarkdown-utf8.h"
#include "aimarkdown-parser.h"

// normalize map label:  collapse internal whitespace to single space,
// remove leading/trailing whitespace, case fold
// Return NULL if the label is actually empty (i.e. composed solely from
// whitespace)
unsigned char *aimarkdown_normalize_map_label(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *ref) {
  aimarkdown_cmark_strbuf normalized = AIMARKDOWN_CMARK_BUF_INIT(mem);
  unsigned char *result;

  if (ref == NULL)
    return NULL;

  if (ref->len == 0)
    return NULL;

  aimarkdown_cmark_utf8proc_case_fold(&normalized, ref->data, ref->len);
  aimarkdown_cmark_strbuf_trim(&normalized);
  aimarkdown_cmark_strbuf_normalize_whitespace(&normalized);

  result = aimarkdown_cmark_strbuf_detach(&normalized);
  assert(result);

  if (result[0] == '\0') {
    mem->free(result);
    return NULL;
  }

  return result;
}

static int
labelcmp(const unsigned char *a, const unsigned char *b) {
  return strcmp((const char *)a, (const char *)b);
}

static int
refcmp(const void *p1, const void *p2) {
  aimarkdown_cmark_map_entry *r1 = *(aimarkdown_cmark_map_entry **)p1;
  aimarkdown_cmark_map_entry *r2 = *(aimarkdown_cmark_map_entry **)p2;
  int res = labelcmp(r1->label, r2->label);
  return res ? res : ((int)r1->age - (int)r2->age);
}

static int
refsearch(const void *label, const void *p2) {
  aimarkdown_cmark_map_entry *ref = *(aimarkdown_cmark_map_entry **)p2;
  return labelcmp((const unsigned char *)label, ref->label);
}

static void sort_map(aimarkdown_cmark_map *map) {
  size_t i = 0, last = 0, size = map->size;
  aimarkdown_cmark_map_entry *r = map->refs, **sorted = NULL;

  sorted = (aimarkdown_cmark_map_entry **)map->mem->calloc(size, sizeof(aimarkdown_cmark_map_entry *));
  while (r) {
    sorted[i++] = r;
    r = r->next;
  }

  qsort(sorted, size, sizeof(aimarkdown_cmark_map_entry *), refcmp);

  for (i = 1; i < size; i++) {
    if (labelcmp(sorted[i]->label, sorted[last]->label) != 0)
      sorted[++last] = sorted[i];
  }

  map->sorted = sorted;
  map->size = last + 1;
}

aimarkdown_cmark_map_entry *aimarkdown_cmark_map_lookup(aimarkdown_cmark_map *map, aimarkdown_cmark_chunk *label) {
  aimarkdown_cmark_map_entry **ref = NULL;
  aimarkdown_cmark_map_entry *r = NULL;
  unsigned char *norm;

  if (map == NULL || !map->size)
    return NULL;

  if (label->len < 1 || (!map->web_reference_labels && label->len > MAX_LINK_LABEL_LENGTH))
    return NULL;

  if (map == NULL || !map->size)
    return NULL;

  norm = map->web_reference_labels ? aimarkdown_cmark_reference_key(map->mem, label) : aimarkdown_normalize_map_label(map->mem, label);
  if (norm == NULL)
    return NULL;

  if (!map->sorted)
    sort_map(map);

  ref = (aimarkdown_cmark_map_entry **)bsearch(norm, map->sorted, map->size, sizeof(aimarkdown_cmark_map_entry *), refsearch);
  map->mem->free(norm);

  if (ref != NULL) {
    r = ref[0];
    /* Check for expansion limit */
    if (!map->web_reference_labels && r->size > map->max_ref_size - map->ref_size)
      return NULL;
    if (!map->web_reference_labels) map->ref_size += r->size;
  }

  return r;
}

void aimarkdown_cmark_map_free(aimarkdown_cmark_map *map) {
  aimarkdown_cmark_map_entry *ref;

  if (map == NULL)
    return;

  ref = map->refs;
  while (ref) {
    aimarkdown_cmark_map_entry *next = ref->next;
    map->free(map, ref);
    ref = next;
  }

  map->mem->free(map->sorted);
  map->mem->free(map);
}

aimarkdown_cmark_map *aimarkdown_cmark_map_new(aimarkdown_cmark_mem *mem, aimarkdown_cmark_map_free_f free) {
  aimarkdown_cmark_map *map = (aimarkdown_cmark_map *)mem->calloc(1, sizeof(aimarkdown_cmark_map));
  map->mem = mem;
  map->free = free;
  map->max_ref_size = UINT_MAX;
  return map;
}
