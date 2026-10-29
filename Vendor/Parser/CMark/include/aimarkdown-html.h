// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_HTML_H
#define AIMARKDOWN_CMARK_HTML_H

#include "aimarkdown-buffer.h"
#include "aimarkdown-node.h"

inline
static void aimarkdown_cmark_html_render_cr(aimarkdown_cmark_strbuf *html) {
  if (html->size && html->ptr[html->size - 1] != '\n')
    aimarkdown_cmark_strbuf_putc(html, '\n');
}

#define BUFFER_SIZE 100

inline 
static void aimarkdown_cmark_html_render_sourcepos(aimarkdown_cmark_node *node, aimarkdown_cmark_strbuf *html, int options) {
  char buffer[BUFFER_SIZE];
  if (AIMARKDOWN_CMARK_OPT_SOURCEPOS & options) {
    snprintf(buffer, BUFFER_SIZE, " data-sourcepos=\"%d:%d-%d:%d\"",
             aimarkdown_cmark_node_get_start_line(node), aimarkdown_cmark_node_get_start_column(node),
             aimarkdown_cmark_node_get_end_line(node), aimarkdown_cmark_node_get_end_column(node));
    aimarkdown_cmark_strbuf_puts(html, buffer);
  }
}


#endif
