// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "aimarkdown-registry.h"
#include "aimarkdown-node.h"
#include "aimarkdown-houdini.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-buffer.h"

aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_LAST_BLOCK = AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION;
aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_LAST_INLINE = AIMARKDOWN_CMARK_NODE_MATH_INLINE;

int aimarkdown_cmark_version(void) { return AIMARKDOWN_CMARK_GFM_VERSION; }

const char *aimarkdown_cmark_version_string(void) { return AIMARKDOWN_CMARK_GFM_VERSION_STRING; }

static void *xcalloc(size_t nmem, size_t size) {
  void *ptr = calloc(nmem, size);
  if (!ptr) {
    fprintf(stderr, "[cmark] calloc returned null pointer, aborting\n");
    abort();
  }
  return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size);
  if (!new_ptr) {
    fprintf(stderr, "[cmark] realloc returned null pointer, aborting\n");
    abort();
  }
  return new_ptr;
}

static void xfree(void *ptr) {
  free(ptr);
}

aimarkdown_cmark_mem AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR = {xcalloc, xrealloc, xfree};

aimarkdown_cmark_mem *aimarkdown_cmark_get_default_mem_allocator(void) {
  return &AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR;
}

char *aimarkdown_cmark_markdown_to_html(const char *text, size_t len, int options) {
  aimarkdown_cmark_node *doc;
  char *result;

  doc = aimarkdown_cmark_parse_document(text, len, options);

  result = aimarkdown_cmark_render_html(doc, options, NULL);
  aimarkdown_cmark_node_free(doc);

  return result;
}
