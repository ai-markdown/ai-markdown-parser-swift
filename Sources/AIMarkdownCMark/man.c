// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-node.h"
#include "aimarkdown-buffer.h"
#include "aimarkdown-utf8.h"
#include "aimarkdown-render.h"
#include "aimarkdown-syntax_extension.h"

#define OUT(s, wrap, escaping) renderer->out(renderer, node, s, wrap, escaping)
#define LIT(s) renderer->out(renderer, node, s, false, LITERAL)
#define CR() renderer->cr(renderer)
#define BLANKLINE() renderer->blankline(renderer)
#define LIST_NUMBER_SIZE 20

// Functions to convert cmark_nodes to groff man strings.
static void S_outc(aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node, 
                   aimarkdown_cmark_escaping escape, int32_t c,
                   unsigned char nextc) {
  (void)(nextc);

  if (escape == LITERAL) {
    aimarkdown_cmark_render_code_point(renderer, c);
    return;
  }

  switch (c) {
  case 46:
    if (renderer->begin_line) {
      aimarkdown_cmark_render_ascii(renderer, "\\&.");
    } else {
      aimarkdown_cmark_render_code_point(renderer, c);
    }
    break;
  case 39:
    if (renderer->begin_line) {
      aimarkdown_cmark_render_ascii(renderer, "\\&'");
    } else {
      aimarkdown_cmark_render_code_point(renderer, c);
    }
    break;
  case 45:
    aimarkdown_cmark_render_ascii(renderer, "\\-");
    break;
  case 92:
    aimarkdown_cmark_render_ascii(renderer, "\\e");
    break;
  case 8216: // left single quote
    aimarkdown_cmark_render_ascii(renderer, "\\[oq]");
    break;
  case 8217: // right single quote
    aimarkdown_cmark_render_ascii(renderer, "\\[cq]");
    break;
  case 8220: // left double quote
    aimarkdown_cmark_render_ascii(renderer, "\\[lq]");
    break;
  case 8221: // right double quote
    aimarkdown_cmark_render_ascii(renderer, "\\[rq]");
    break;
  case 8212: // em dash
    aimarkdown_cmark_render_ascii(renderer, "\\[em]");
    break;
  case 8211: // en dash
    aimarkdown_cmark_render_ascii(renderer, "\\[en]");
    break;
  default:
    aimarkdown_cmark_render_code_point(renderer, c);
  }
}

static int S_render_node(aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                         aimarkdown_cmark_event_type ev_type, int options) {
  int list_number;
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  bool allow_wrap = renderer->width > 0 && !(AIMARKDOWN_CMARK_OPT_NOBREAKS & options);

  if (node->extension && node->extension->man_render_func) {
    node->extension->man_render_func(node->extension, renderer, node, ev_type, options);
    return 1;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_DOCUMENT:
    if (entering) {
      /* Define a strikethrough macro */
      /* Commenting out because this makes tests fail
      LIT(".de ST");
      CR();
      LIT(".nr ww \\w'\\\\$1'");
      CR();
      LIT("\\Z@\\v'-.25m'\\l'\\\\n[ww]u'@\\\\$1");
      CR();
      LIT("..");
      CR();
      */
    }
    break;

  case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
    if (entering) {
      CR();
      LIT(".RS");
      CR();
    } else {
      CR();
      LIT(".RE");
      CR();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_LIST:
    CR(); LIT(entering ? ".RS" : ".RE"); CR();
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_TERM:
    if (entering) { CR(); LIT(".TP"); CR(); LIT("\\f[B]"); }
    else { LIT("\\f[]"); CR(); }
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION:
    if (entering && node->prev && node->prev->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION) {
      CR(); LIT(".sp"); CR();
    }
    if (!entering) CR();
    break;

  case AIMARKDOWN_CMARK_NODE_LIST:
    break;

  case AIMARKDOWN_CMARK_NODE_ITEM:
    if (entering) {
      CR();
      LIT(".IP ");
      if (aimarkdown_cmark_node_get_list_type(node->parent) == AIMARKDOWN_CMARK_BULLET_LIST) {
        LIT("\\[bu] 2");
      } else {
        list_number = aimarkdown_cmark_node_get_item_index(node);
        char list_number_s[LIST_NUMBER_SIZE];
        snprintf(list_number_s, LIST_NUMBER_SIZE, "\"%d.\" 4", list_number);
        LIT(list_number_s);
      }
      CR();
    } else {
      CR();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_HEADING:
    if (entering) {
      CR();
      LIT(aimarkdown_cmark_node_get_heading_level(node) == 1 ? ".SH" : ".SS");
      CR();
    } else {
      CR();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
  case AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION:
    break;

  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    CR();
    LIT(".IP\n.nf\n\\f[C]\n");
    OUT(aimarkdown_cmark_node_get_literal(node), false, NORMAL);
    CR();
    LIT("\\f[]\n.fi");
    CR();
    break;

  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
    break;

  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    CR();
    OUT(entering ? aimarkdown_cmark_node_get_on_enter(node) : aimarkdown_cmark_node_get_on_exit(node),
        false, LITERAL);
    CR();
    break;

  case AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK:
    CR();
    LIT(".PP\n  *  *  *  *  *");
    CR();
    break;

  case AIMARKDOWN_CMARK_NODE_PARAGRAPH:
    if (entering) {
      // no blank line if first paragraph in list:
      if (node->parent && (node->parent->type == AIMARKDOWN_CMARK_NODE_ITEM || node->parent->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION) &&
          node->prev == NULL) {
        // no blank line or .PP
      } else {
        CR();
        LIT(".PP");
        CR();
      }
    } else {
      CR();
    }
    break;

  case AIMARKDOWN_CMARK_NODE_TEXT:
    OUT(aimarkdown_cmark_node_get_literal(node), allow_wrap, NORMAL);
    break;

  case AIMARKDOWN_CMARK_NODE_LINEBREAK:
    LIT(".PD 0\n.P\n.PD");
    CR();
    break;

  case AIMARKDOWN_CMARK_NODE_SOFTBREAK:
    if (options & AIMARKDOWN_CMARK_OPT_HARDBREAKS) {
      LIT(".PD 0\n.P\n.PD");
      CR();
    } else if (renderer->width == 0 && !(AIMARKDOWN_CMARK_OPT_NOBREAKS & options)) {
      CR();
    } else {
      OUT(" ", allow_wrap, LITERAL);
    }
    break;

  case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
  case AIMARKDOWN_CMARK_NODE_CODE:
    LIT("\\f[C]");
    OUT(aimarkdown_cmark_node_get_literal(node), allow_wrap, NORMAL);
    LIT("\\f[]");
    break;

  case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
    break;

  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
    OUT(entering ? aimarkdown_cmark_node_get_on_enter(node) : aimarkdown_cmark_node_get_on_exit(node),
        false, LITERAL);
    break;

  case AIMARKDOWN_CMARK_NODE_STRONG:
    if (node->parent == NULL || node->parent->type != AIMARKDOWN_CMARK_NODE_STRONG) {
      if (entering) {
        LIT("\\f[B]");
      } else {
        LIT("\\f[]");
      }
    }
    break;

  case AIMARKDOWN_CMARK_NODE_EMPH:
    if (entering) {
      LIT("\\f[I]");
    } else {
      LIT("\\f[]");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_LINK:
    if (!entering) {
      LIT(" (");
      OUT(aimarkdown_cmark_node_get_url(node), allow_wrap, URL);
      LIT(")");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_IMAGE:
    if (entering) {
      LIT("[IMAGE: ");
    } else {
      LIT("]");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
    /* Attribute containers have no literal. Render their children only. */
    break;

  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE:
    // TODO
    break;

  default:
    assert(false);
    break;
  }

  return 1;
}

char *aimarkdown_cmark_render_man(aimarkdown_cmark_node *root, int options, int width) {
  return aimarkdown_cmark_render_man_with_mem(root, options, width, aimarkdown_cmark_node_mem(root));
}

char *aimarkdown_cmark_render_man_with_mem(aimarkdown_cmark_node *root, int options, int width, aimarkdown_cmark_mem *mem) {
  return aimarkdown_cmark_render(mem, root, options, width, S_outc, S_render_node);
}
