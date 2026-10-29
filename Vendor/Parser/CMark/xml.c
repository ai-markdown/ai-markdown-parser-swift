// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-node.h"
#include "aimarkdown-buffer.h"
#include "aimarkdown-houdini.h"
#include "aimarkdown-syntax_extension.h"

#define BUFFER_SIZE 100
#define MAX_INDENT 40

// Functions to convert cmark_nodes to XML strings.

static void escape_xml(aimarkdown_cmark_strbuf *dest, const unsigned char *source,
                       bufsize_t length) {
  aimarkdown_houdini_escape_html0(dest, source, length, 0);
}

struct render_state {
  aimarkdown_cmark_strbuf *xml;
  int indent;
};

static inline void indent(struct render_state *state) {
  int i;
  for (i = 0; i < state->indent && i < MAX_INDENT; i++) {
    aimarkdown_cmark_strbuf_putc(state->xml, ' ');
  }
}

static int S_render_node(aimarkdown_cmark_node *node, aimarkdown_cmark_event_type ev_type,
                         struct render_state *state, int options) {
  aimarkdown_cmark_strbuf *xml = state->xml;
  bool literal = false;
  aimarkdown_cmark_delim_type delim;
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  char buffer[BUFFER_SIZE];

  if (entering) {
    indent(state);
    aimarkdown_cmark_strbuf_putc(xml, '<');
    aimarkdown_cmark_strbuf_puts(xml, aimarkdown_cmark_node_get_type_string(node));

    if (options & AIMARKDOWN_CMARK_OPT_SOURCEPOS && node->start_line != 0) {
      snprintf(buffer, BUFFER_SIZE, " sourcepos=\"%d:%d-%d:%d\"",
               node->start_line, node->start_column, node->end_line,
               node->end_column);
      aimarkdown_cmark_strbuf_puts(xml, buffer);
    }

    if (node->extension && node->extension->xml_attr_func) {
      const char* r = node->extension->xml_attr_func(node->extension, node);
      if (r != NULL)
        aimarkdown_cmark_strbuf_puts(xml, r);
    }

    if (node->reference_usage && node->reference_usage->type) {
      aimarkdown_cmark_strbuf_puts(xml, " reference_type=\"");
      aimarkdown_cmark_strbuf_puts(xml, node->reference_usage->type == 3 ? "full" : node->reference_usage->type == 2 ? "collapsed" : "shortcut");
      aimarkdown_cmark_strbuf_puts(xml, "\" reference_source=\"");
      escape_xml(xml, node->reference_usage->source.data, node->reference_usage->source.len);
      aimarkdown_cmark_strbuf_puts(xml, "\" reference_label=\"");
      escape_xml(xml, node->reference_usage->label.data, node->reference_usage->label.len);
      aimarkdown_cmark_strbuf_putc(xml, '"');
    }
    literal = false;

    switch (node->type) {
    case AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION: {
      const char *names[] = {"label", "identifier_source", "destination", "title"};
      const char *values[] = {aimarkdown_cmark_node_get_reference_label(node), aimarkdown_cmark_node_get_reference_identifier_source(node), aimarkdown_cmark_node_get_reference_destination(node), aimarkdown_cmark_node_get_reference_title(node)};
      for (int i = 0; i < 4; i++) if (values[i]) {
        aimarkdown_cmark_strbuf_putc(xml, ' '); aimarkdown_cmark_strbuf_puts(xml, names[i]); aimarkdown_cmark_strbuf_puts(xml, "=\"");
        escape_xml(xml, (const unsigned char *)values[i], strlen(values[i])); aimarkdown_cmark_strbuf_putc(xml, '\"');
      }
      break;
    }
    case AIMARKDOWN_CMARK_NODE_DOCUMENT:
      aimarkdown_cmark_strbuf_puts(xml, " xmlns=\"http://commonmark.org/xml/1.0\"");
      break;
    case AIMARKDOWN_CMARK_NODE_TEXT:
    case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
  case AIMARKDOWN_CMARK_NODE_CODE:
    case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
    case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
      aimarkdown_cmark_strbuf_puts(xml, " xml:space=\"preserve\">");
      escape_xml(xml, node->as.literal.data, node->as.literal.len);
      aimarkdown_cmark_strbuf_puts(xml, "</");
      aimarkdown_cmark_strbuf_puts(xml, aimarkdown_cmark_node_get_type_string(node));
      literal = true;
      break;
    case AIMARKDOWN_CMARK_NODE_LIST:
      switch (aimarkdown_cmark_node_get_list_type(node)) {
      case AIMARKDOWN_CMARK_ORDERED_LIST:
        aimarkdown_cmark_strbuf_puts(xml, " type=\"ordered\"");
        snprintf(buffer, BUFFER_SIZE, " start=\"%d\"",
                 aimarkdown_cmark_node_get_list_start(node));
        aimarkdown_cmark_strbuf_puts(xml, buffer);
        delim = aimarkdown_cmark_node_get_list_delim(node);
        if (delim == AIMARKDOWN_CMARK_PAREN_DELIM) {
          aimarkdown_cmark_strbuf_puts(xml, " delim=\"paren\"");
        } else if (delim == AIMARKDOWN_CMARK_PERIOD_DELIM) {
          aimarkdown_cmark_strbuf_puts(xml, " delim=\"period\"");
        }
        break;
      case AIMARKDOWN_CMARK_BULLET_LIST:
        aimarkdown_cmark_strbuf_puts(xml, " type=\"bullet\"");
        break;
      default:
        break;
      }
      snprintf(buffer, BUFFER_SIZE, " tight=\"%s\"",
               (aimarkdown_cmark_node_get_list_tight(node) ? "true" : "false"));
      aimarkdown_cmark_strbuf_puts(xml, buffer);
      break;
    case AIMARKDOWN_CMARK_NODE_HEADING:
      snprintf(buffer, BUFFER_SIZE, " level=\"%d\"", node->as.heading.level);
      aimarkdown_cmark_strbuf_puts(xml, buffer);
      break;
    case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
      if (node->as.code.info.len > 0) {
        aimarkdown_cmark_strbuf_puts(xml, " info=\"");
        escape_xml(xml, node->as.code.info.data, node->as.code.info.len);
        aimarkdown_cmark_strbuf_putc(xml, '"');
      }
      aimarkdown_cmark_strbuf_puts(xml, " xml:space=\"preserve\">");
      escape_xml(xml, node->as.code.literal.data, node->as.code.literal.len);
      aimarkdown_cmark_strbuf_puts(xml, "</");
      aimarkdown_cmark_strbuf_puts(xml, aimarkdown_cmark_node_get_type_string(node));
      literal = true;
      break;
    case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
      aimarkdown_cmark_strbuf_puts(xml, " on_enter=\"");
      escape_xml(xml, node->as.custom.on_enter.data,
                 node->as.custom.on_enter.len);
      aimarkdown_cmark_strbuf_putc(xml, '"');
      aimarkdown_cmark_strbuf_puts(xml, " on_exit=\"");
      escape_xml(xml, node->as.custom.on_exit.data,
                 node->as.custom.on_exit.len);
      aimarkdown_cmark_strbuf_putc(xml, '"');
      break;
    case AIMARKDOWN_CMARK_NODE_LINK:
    case AIMARKDOWN_CMARK_NODE_IMAGE:
      aimarkdown_cmark_strbuf_puts(xml, " destination=\"");
      escape_xml(xml, node->as.link.url.data, node->as.link.url.len);
      aimarkdown_cmark_strbuf_putc(xml, '"');
      aimarkdown_cmark_strbuf_puts(xml, " title=\"");
      escape_xml(xml, node->as.link.title.data, node->as.link.title.len);
      aimarkdown_cmark_strbuf_putc(xml, '"');
      break;
    case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
      // TODO
      break;
    default:
      break;
    }
    if (node->first_child) {
      state->indent += 2;
    } else if (!literal) {
      aimarkdown_cmark_strbuf_puts(xml, " /");
    }
    aimarkdown_cmark_strbuf_puts(xml, ">\n");

  } else if (node->first_child) {
    state->indent -= 2;
    indent(state);
    aimarkdown_cmark_strbuf_puts(xml, "</");
    aimarkdown_cmark_strbuf_puts(xml, aimarkdown_cmark_node_get_type_string(node));
    aimarkdown_cmark_strbuf_puts(xml, ">\n");
  }

  return 1;
}

char *aimarkdown_cmark_render_xml(aimarkdown_cmark_node *root, int options) {
  return aimarkdown_cmark_render_xml_with_mem(root, options, aimarkdown_cmark_node_mem(root));
}

char *aimarkdown_cmark_render_xml_with_mem(aimarkdown_cmark_node *root, int options, aimarkdown_cmark_mem *mem) {
  char *result;
  aimarkdown_cmark_strbuf xml = AIMARKDOWN_CMARK_BUF_INIT(mem);
  aimarkdown_cmark_event_type ev_type;
  aimarkdown_cmark_node *cur;
  struct render_state state = {&xml, 0};

  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(root);

  aimarkdown_cmark_strbuf_puts(state.xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  aimarkdown_cmark_strbuf_puts(state.xml,
                    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n");
  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    S_render_node(cur, ev_type, &state, options);
  }
  result = (char *)aimarkdown_cmark_strbuf_detach(&xml);

  aimarkdown_cmark_iter_free(iter);
  return result;
}
