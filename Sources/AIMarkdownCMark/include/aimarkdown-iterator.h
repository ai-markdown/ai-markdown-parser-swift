// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_ITERATOR_H
#define AIMARKDOWN_CMARK_ITERATOR_H

#include "aimarkdown-cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  aimarkdown_cmark_event_type ev_type;
  aimarkdown_cmark_node *node;
} aimarkdown_cmark_iter_state;

struct aimarkdown_cmark_iter {
  aimarkdown_cmark_mem *mem;
  aimarkdown_cmark_node *root;
  aimarkdown_cmark_iter_state cur;
  aimarkdown_cmark_iter_state next;
};

#ifdef __cplusplus
}
#endif

#endif
