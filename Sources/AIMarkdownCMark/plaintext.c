// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <stdbool.h>

#include "aimarkdown-node.h"
#include "aimarkdown-syntax_extension.h"
#include "aimarkdown-render.h"

#define OUT(s, wrap, escaping) renderer->out(renderer, node, s, wrap, escaping)
#define LIT(s) renderer->out(renderer, node, s, false, LITERAL)
#define CR() renderer->cr(renderer)
#define BLANKLINE() renderer->blankline(renderer)
#define LISTMARKER_SIZE 20

// Functions to convert cmark_nodes to plain text strings.

static inline void outc(aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                        aimarkdown_cmark_escaping escape, int32_t c, unsigned char nextc) {
  aimarkdown_cmark_render_code_point(renderer, c);
}

static int S_render_node(aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                         aimarkdown_cmark_event_type ev_type, int options) {
  int list_number;
  aimarkdown_cmark_delim_type list_delim;
  int i;
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  char listmarker[LISTMARKER_SIZE];
  bool first_in_list_item;
  bufsize_t marker_width;
  bool allow_wrap = renderer->width > 0 && !(AIMARKDOWN_CMARK_OPT_NOBREAKS & options) &&
                    !(AIMARKDOWN_CMARK_OPT_HARDBREAKS & options);

  // Don't adjust tight list status til we've started the list.
  // Otherwise we loose the blank line between a paragraph and
  // a following list.
  if (entering) {
    if (node->parent && node->parent->type == AIMARKDOWN_CMARK_NODE_ITEM) {
      renderer->in_tight_list_item = node->parent->parent->as.list.tight;
    }
  } else {
    if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
      renderer->in_tight_list_item =
        node->parent &&
        node->parent->type == AIMARKDOWN_CMARK_NODE_ITEM &&
        node->parent->parent->as.list.tight;
    }
  }

  if (node->extension && node->extension->plaintext_render_func) {
    node->extension->plaintext_render_func(node->extension, renderer, node, ev_type, options);
    return 1;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_DOCUMENT:
    break;

  case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_LIST:
    if (entering) { CR(); } else { BLANKLINE(); }
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_TERM:
    CR();
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION:
    if (entering) {
      CR();
      aimarkdown_cmark_strbuf_puts(renderer->prefix, "    ");
    } else {
      aimarkdown_cmark_strbuf_truncate(renderer->prefix, renderer->prefix->size - 4);
      CR();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_LIST:
    if (!entering && node->next && (node->next->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK ||
                                    node->next->type == AIMARKDOWN_CMARK_NODE_LIST)) {
      CR();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_ITEM:
    if (aimarkdown_cmark_node_get_list_type(node->parent) == AIMARKDOWN_CMARK_BULLET_LIST) {
      marker_width = 4;
    } else {
      list_number = aimarkdown_cmark_node_get_item_index(node);
      list_delim = aimarkdown_cmark_node_get_list_delim(node->parent);
      // we ensure a width of at least 4 so
      // we get nice transition from single digits
      // to double
      snprintf(listmarker, LISTMARKER_SIZE, "%d%s%s", list_number,
               list_delim == AIMARKDOWN_CMARK_PAREN_DELIM ? ")" : ".",
               list_number < 10 ? "  " : " ");
      marker_width = (bufsize_t)strlen(listmarker);
    }
    if (entering) {
      if (aimarkdown_cmark_node_get_list_type(node->parent) == AIMARKDOWN_CMARK_BULLET_LIST) {
        LIT("  - ");
        renderer->begin_content = true;
      } else {
        LIT(listmarker);
        renderer->begin_content = true;
      }
      for (i = marker_width; i--;) {
        aimarkdown_cmark_strbuf_putc(renderer->prefix, ' ');
      }
    } else {
      aimarkdown_cmark_strbuf_truncate(renderer->prefix,
                            renderer->prefix->size - marker_width);
      CR();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_HEADING:
    if (entering) {
      renderer->begin_content = true;
      renderer->no_linebreaks = true;
    } else {
      renderer->no_linebreaks = false;
      BLANKLINE();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
  case AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION:
    break;

  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    first_in_list_item = node->prev == NULL && node->parent &&
                         node->parent->type == AIMARKDOWN_CMARK_NODE_ITEM;

    if (!first_in_list_item) {
      BLANKLINE();
    }
    OUT(aimarkdown_cmark_node_get_literal(node), false, LITERAL);
    BLANKLINE();
    break;

  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
    break;

  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    break;

  case AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK:
    BLANKLINE();
    break;

  case AIMARKDOWN_CMARK_NODE_PARAGRAPH:
    if (!entering) {
      if (node->parent && node->parent->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION && !node->parent->definition.spread) { CR(); }
      else { BLANKLINE(); }
    }
    break;

  case AIMARKDOWN_CMARK_NODE_TEXT:
    OUT(aimarkdown_cmark_node_get_literal(node), allow_wrap, NORMAL);
    break;

  case AIMARKDOWN_CMARK_NODE_LINEBREAK:
    CR();
    break;

  case AIMARKDOWN_CMARK_NODE_SOFTBREAK:
    if (AIMARKDOWN_CMARK_OPT_HARDBREAKS & options) {
      CR();
    } else if (!renderer->no_linebreaks && renderer->width == 0 &&
               !(AIMARKDOWN_CMARK_OPT_HARDBREAKS & options) &&
               !(AIMARKDOWN_CMARK_OPT_NOBREAKS & options)) {
      CR();
    } else {
      OUT(" ", allow_wrap, LITERAL);
    }
    break;

  case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
  case AIMARKDOWN_CMARK_NODE_CODE:
    OUT(aimarkdown_cmark_node_get_literal(node), allow_wrap, LITERAL);
    break;

  case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
    break;

  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
    break;

  case AIMARKDOWN_CMARK_NODE_STRONG:
    break;

  case AIMARKDOWN_CMARK_NODE_EMPH:
    break;

  case AIMARKDOWN_CMARK_NODE_LINK:
    break;

  case AIMARKDOWN_CMARK_NODE_IMAGE:
    break;

  case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
    OUT(aimarkdown_cmark_chunk_to_cstr(renderer->mem, &node->as.literal), false, LITERAL);
    break;

  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE:
    if (entering) {
      LIT("[^");
      OUT(aimarkdown_cmark_chunk_to_cstr(renderer->mem, &node->as.literal), false, LITERAL);
      LIT("]");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
    if (entering) {
      renderer->footnote_ix += 1;
      LIT("[^");
      char n[32];
      snprintf(n, sizeof(n), "%d", renderer->footnote_ix);
      OUT(n, false, LITERAL);
      LIT("]: ");

      aimarkdown_cmark_strbuf_puts(renderer->prefix, "    ");
    } else {
      aimarkdown_cmark_strbuf_truncate(renderer->prefix, renderer->prefix->size - 4);
    }
    break;
  default:
    assert(false);
    break;
  }

  return 1;
}

char *aimarkdown_cmark_render_plaintext(aimarkdown_cmark_node *root, int options, int width) {
  return aimarkdown_cmark_render_plaintext_with_mem(root, options, width, aimarkdown_cmark_node_mem(root));
}

char *aimarkdown_cmark_render_plaintext_with_mem(aimarkdown_cmark_node *root, int options, int width, aimarkdown_cmark_mem *mem) {
  if (options & AIMARKDOWN_CMARK_OPT_HARDBREAKS) {
    // disable breaking on width, since it has
    // a different meaning with OPT_HARDBREAKS
    width = 0;
  }
  return aimarkdown_cmark_render(mem, root, options, width, outc, S_render_node);
}
