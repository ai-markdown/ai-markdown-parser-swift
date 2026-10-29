// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_RENDER_H
#define AIMARKDOWN_CMARK_RENDER_H

#include <stdbool.h>
#include <stdlib.h>

#include "aimarkdown-buffer.h"
#include "aimarkdown-chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { LITERAL, NORMAL, TITLE, URL } aimarkdown_cmark_escaping;

struct aimarkdown_cmark_renderer {
  aimarkdown_cmark_mem *mem;
  aimarkdown_cmark_strbuf *buffer;
  aimarkdown_cmark_strbuf *prefix;
  int column;
  int width;
  int need_cr;
  bufsize_t last_breakable;
  bool begin_line;
  bool begin_content;
  bool no_linebreaks;
  bool in_tight_list_item;
  void (*outc)(struct aimarkdown_cmark_renderer *, aimarkdown_cmark_node *, aimarkdown_cmark_escaping, int32_t, unsigned char);
  void (*cr)(struct aimarkdown_cmark_renderer *);
  void (*blankline)(struct aimarkdown_cmark_renderer *);
  void (*out)(struct aimarkdown_cmark_renderer *, aimarkdown_cmark_node *, const char *, bool, aimarkdown_cmark_escaping);
  unsigned int footnote_ix;
};

typedef struct aimarkdown_cmark_renderer aimarkdown_cmark_renderer;

struct aimarkdown_cmark_html_renderer {
  aimarkdown_cmark_strbuf *html;
  aimarkdown_cmark_node *plain;
  aimarkdown_cmark_llist *filter_extensions;
  unsigned int footnote_ix;
  unsigned int written_footnote_ix;
  void *opaque;
};

typedef struct aimarkdown_cmark_html_renderer aimarkdown_cmark_html_renderer;

void aimarkdown_cmark_render_ascii(aimarkdown_cmark_renderer *renderer, const char *s);

void aimarkdown_cmark_render_code_point(aimarkdown_cmark_renderer *renderer, uint32_t c);

char *aimarkdown_cmark_render(aimarkdown_cmark_mem *mem, aimarkdown_cmark_node *root, int options, int width,
                   void (*outc)(aimarkdown_cmark_renderer *, aimarkdown_cmark_node *,
                                aimarkdown_cmark_escaping, int32_t,
                                unsigned char),
                   int (*render_node)(aimarkdown_cmark_renderer *renderer,
                                      aimarkdown_cmark_node *node,
                                      aimarkdown_cmark_event_type ev_type, int options));

#ifdef __cplusplus
}
#endif

#endif
