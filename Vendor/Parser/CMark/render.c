// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <stdbool.h>
#include <stdlib.h>

#include "aimarkdown-buffer.h"
#include "aimarkdown-chunk.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-utf8.h"
#include "aimarkdown-render.h"
#include "aimarkdown-node.h"
#include "aimarkdown-syntax_extension.h"

static inline void S_cr(aimarkdown_cmark_renderer *renderer) {
  if (renderer->need_cr < 1) {
    renderer->need_cr = 1;
  }
}

static inline void S_blankline(aimarkdown_cmark_renderer *renderer) {
  if (renderer->need_cr < 2) {
    renderer->need_cr = 2;
  }
}

static void S_out(aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                  const char *source, bool wrap,
                  aimarkdown_cmark_escaping escape) {
  int length = (int)strlen(source);
  unsigned char nextc;
  int32_t c;
  int i = 0;
  int last_nonspace;
  int len;
  aimarkdown_cmark_chunk remainder = aimarkdown_cmark_chunk_literal("");
  int k = renderer->buffer->size - 1;

  aimarkdown_cmark_syntax_extension *ext = node->ancestor_extension;
  if (ext && !ext->commonmark_escape_func)
    ext = NULL;

  wrap = wrap && !renderer->no_linebreaks;

  if (renderer->in_tight_list_item && renderer->need_cr > 1) {
    renderer->need_cr = 1;
  }
  while (renderer->need_cr) {
    if (k < 0 || renderer->buffer->ptr[k] == '\n') {
      k -= 1;
    } else {
      aimarkdown_cmark_strbuf_putc(renderer->buffer, '\n');
      if (renderer->need_cr > 1) {
        aimarkdown_cmark_strbuf_put(renderer->buffer, renderer->prefix->ptr,
                         renderer->prefix->size);
      }
    }
    renderer->column = 0;
    renderer->last_breakable = 0;
    renderer->begin_line = true;
    renderer->begin_content = true;
    renderer->need_cr -= 1;
  }

  while (i < length) {
    if (renderer->begin_line) {
      aimarkdown_cmark_strbuf_put(renderer->buffer, renderer->prefix->ptr,
                       renderer->prefix->size);
      // note: this assumes prefix is ascii:
      renderer->column = renderer->prefix->size;
    }

    len = aimarkdown_cmark_utf8proc_iterate((const uint8_t *)source + i, length - i, &c);
    if (len == -1) { // error condition
      return;        // return without rendering rest of string
    }

    if (ext && ext->commonmark_escape_func(ext, node, c))
      aimarkdown_cmark_strbuf_putc(renderer->buffer, '\\');

    nextc = source[i + len];
    if (c == 32 && wrap) {
      if (!renderer->begin_line) {
        last_nonspace = renderer->buffer->size;
        aimarkdown_cmark_strbuf_putc(renderer->buffer, ' ');
        renderer->column += 1;
        renderer->begin_line = false;
        renderer->begin_content = false;
        // skip following spaces
        while (source[i + 1] == ' ') {
          i++;
        }
        // We don't allow breaks that make a digit the first character
        // because this causes problems with commonmark output.
        if (!aimarkdown_cmark_isdigit(source[i + 1])) {
          renderer->last_breakable = last_nonspace;
        }
      }

    } else if (escape == LITERAL) {
      if (c == 10) {
        aimarkdown_cmark_strbuf_putc(renderer->buffer, '\n');
        renderer->column = 0;
        renderer->begin_line = true;
        renderer->begin_content = true;
        renderer->last_breakable = 0;
      } else {
        aimarkdown_cmark_render_code_point(renderer, c);
        renderer->begin_line = false;
        // we don't set 'begin_content' to false til we've
        // finished parsing a digit.  Reason:  in commonmark
        // we need to escape a potential list marker after
        // a digit:
        renderer->begin_content =
            renderer->begin_content && aimarkdown_cmark_isdigit((char)c) == 1;
      }
    } else {
      (renderer->outc)(renderer, node, escape, c, nextc);
      renderer->begin_line = false;
      renderer->begin_content =
          renderer->begin_content && aimarkdown_cmark_isdigit((char)c) == 1;
    }

    // If adding the character went beyond width, look for an
    // earlier place where the line could be broken:
    if (renderer->width > 0 && renderer->column > renderer->width &&
        !renderer->begin_line && renderer->last_breakable > 0) {

      // copy from last_breakable to remainder
      aimarkdown_cmark_chunk_set_cstr(renderer->mem, &remainder,
                           (char *)renderer->buffer->ptr +
                               renderer->last_breakable + 1);
      // truncate at last_breakable
      aimarkdown_cmark_strbuf_truncate(renderer->buffer, renderer->last_breakable);
      // add newline, prefix, and remainder
      aimarkdown_cmark_strbuf_putc(renderer->buffer, '\n');
      aimarkdown_cmark_strbuf_put(renderer->buffer, renderer->prefix->ptr,
                       renderer->prefix->size);
      aimarkdown_cmark_strbuf_put(renderer->buffer, remainder.data, remainder.len);
      renderer->column = renderer->prefix->size + remainder.len;
      aimarkdown_cmark_chunk_free(renderer->mem, &remainder);
      renderer->last_breakable = 0;
      renderer->begin_line = false;
      renderer->begin_content = false;
    }

    i += len;
  }
}

// Assumes no newlines, assumes ascii content:
void aimarkdown_cmark_render_ascii(aimarkdown_cmark_renderer *renderer, const char *s) {
  int origsize = renderer->buffer->size;
  aimarkdown_cmark_strbuf_puts(renderer->buffer, s);
  renderer->column += renderer->buffer->size - origsize;
}

void aimarkdown_cmark_render_code_point(aimarkdown_cmark_renderer *renderer, uint32_t c) {
  aimarkdown_cmark_utf8proc_encode_char(c, renderer->buffer);
  renderer->column += 1;
}

char *aimarkdown_cmark_render(aimarkdown_cmark_mem *mem, aimarkdown_cmark_node *root, int options, int width,
                   void (*outc)(aimarkdown_cmark_renderer *, aimarkdown_cmark_node *,
                                aimarkdown_cmark_escaping, int32_t,
                                unsigned char),
                   int (*render_node)(aimarkdown_cmark_renderer *renderer,
                                      aimarkdown_cmark_node *node,
                                      aimarkdown_cmark_event_type ev_type, int options)) {
  aimarkdown_cmark_strbuf pref = AIMARKDOWN_CMARK_BUF_INIT(mem);
  aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(mem);
  aimarkdown_cmark_node *cur;
  aimarkdown_cmark_event_type ev_type;
  char *result;
  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(root);

  aimarkdown_cmark_renderer renderer = {mem,   &buf, &pref, 0,           width,
                             0,     0,    true,  true,        false,
                             false, outc, S_cr,  S_blankline, S_out,
                             0};

  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    if (cur->extension) {
      cur->ancestor_extension = cur->extension;
    } else if (cur->parent) {
      cur->ancestor_extension = cur->parent->ancestor_extension;
    }
    if (cur->type == AIMARKDOWN_CMARK_NODE_ITEM) {
      // Calculate the list item's index, for the benefit of output formats
      // like commonmark and plaintext.
      if (cur->prev) {
        aimarkdown_cmark_node_set_item_index(cur, 1 + aimarkdown_cmark_node_get_item_index(cur->prev));
      } else {
        aimarkdown_cmark_node_set_item_index(cur, aimarkdown_cmark_node_get_list_start(cur->parent));
      }
    }
    if (!render_node(&renderer, cur, ev_type, options)) {
      // a false value causes us to skip processing
      // the node's contents.  this is used for
      // autolinks.
      aimarkdown_cmark_iter_reset(iter, cur, AIMARKDOWN_CMARK_EVENT_EXIT);
    }
  }

  // ensure final newline
  if (renderer.buffer->size == 0 || renderer.buffer->ptr[renderer.buffer->size - 1] != '\n') {
    aimarkdown_cmark_strbuf_putc(renderer.buffer, '\n');
  }

  result = (char *)aimarkdown_cmark_strbuf_detach(renderer.buffer);

  aimarkdown_cmark_iter_free(iter);
  aimarkdown_cmark_strbuf_free(renderer.prefix);
  aimarkdown_cmark_strbuf_free(renderer.buffer);

  return result;
}
