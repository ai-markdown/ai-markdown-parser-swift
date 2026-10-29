// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <stdbool.h>

#include "aimarkdown-tasklist.h"
#include <aimarkdown-parser.h>
#include <aimarkdown-render.h>
#include <aimarkdown-html.h>
#include "aimarkdown-ext_scanners.h"

typedef enum {
  AIMARKDOWN_CMARK_TASKLIST_NOCHECKED,
  AIMARKDOWN_CMARK_TASKLIST_CHECKED,
} aimarkdown_cmark_tasklist_type;

// Local constants
static const char *TYPE_STRING = "tasklist";

static const char *get_type_string(aimarkdown_cmark_syntax_extension *extension, aimarkdown_cmark_node *node) {
  return TYPE_STRING;
}


// Return 1 if state was set, 0 otherwise
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_gfm_extensions_set_tasklist_item_checked(aimarkdown_cmark_node *node, bool is_checked) {
  // The node has to exist, and be an extension, and actually be the right type in order to get the value.
  if (!node || !node->extension || strcmp(aimarkdown_cmark_node_get_type_string(node), TYPE_STRING))
    return 0;

  node->as.list.checked = is_checked;
  return 1;
}

AIMARKDOWN_CMARK_GFM_EXPORT
bool aimarkdown_cmark_gfm_extensions_get_tasklist_item_checked(aimarkdown_cmark_node *node) {
  if (!node || !node->extension || strcmp(aimarkdown_cmark_node_get_type_string(node), TYPE_STRING))
    return false;

  if (node->as.list.checked) {
    return true;
  }
  else {
    return false;
  }
}

static bool parse_node_item_prefix(aimarkdown_cmark_parser *parser, const char *input,
                                   aimarkdown_cmark_node *container) {
  bool res = false;

  if (parser->indent >=
      container->as.list.marker_offset + container->as.list.padding) {
    aimarkdown_cmark_parser_advance_offset(parser, input, container->as.list.marker_offset +
                                        container->as.list.padding,
                     true);
    res = true;
  } else if (parser->blank && container->first_child != NULL) {
    // if container->first_child is NULL, then the opening line
    // of the list item was blank after the list marker; in this
    // case, we are done with the list item.
    aimarkdown_cmark_parser_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                     false);
    res = true;
  }
  return res;
}

static int matches(aimarkdown_cmark_syntax_extension *self, aimarkdown_cmark_parser *parser,
                   unsigned char *input, int len,
                   aimarkdown_cmark_node *parent_container) {
  return parse_node_item_prefix(parser, (const char*)input, parent_container);
}

static int can_contain(aimarkdown_cmark_syntax_extension *extension, aimarkdown_cmark_node *node,
                       aimarkdown_cmark_node_type child_type) {
  return (node->type == AIMARKDOWN_CMARK_NODE_ITEM) ? 1 : 0;
}

static aimarkdown_cmark_node *open_tasklist_item(aimarkdown_cmark_syntax_extension *self,
                                      int indented, aimarkdown_cmark_parser *parser,
                                      aimarkdown_cmark_node *parent_container,
                                      unsigned char *input, int len) {
  aimarkdown_cmark_node_type node_type = aimarkdown_cmark_node_get_type(parent_container);
  if (node_type != AIMARKDOWN_CMARK_NODE_ITEM) {
    return NULL;
  }

  bool web_mode = (parser->options & (AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS | AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES | AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES)) != 0;
  if (web_mode) {
    // The full first paragraph is needed to distinguish EOL from paragraph EOF.
    // Do not strip a checkbox before flow definitions have been recognized.
    if (!indented && !parent_container->first_child) parent_container->task_extension = self;
    return NULL;
  }
  if (!scan_tasklist(input, len, 0)) return NULL;
  aimarkdown_cmark_node_set_syntax_extension(parent_container, self);
  aimarkdown_cmark_parser_advance_offset(parser, (char *)input, 3, false);
  parent_container->as.list.checked = (strstr((char*)input, "[x]") || strstr((char*)input, "[X]"));

  return NULL;
}

static void commonmark_render(aimarkdown_cmark_syntax_extension *extension,
                              aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                              aimarkdown_cmark_event_type ev_type, int options) {
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  if (entering) {
    renderer->cr(renderer);
    if (node->as.list.checked) {
      renderer->out(renderer, node, "- [x] ", false, LITERAL);
    } else {
      renderer->out(renderer, node, "- [ ] ", false, LITERAL);
    }
    aimarkdown_cmark_strbuf_puts(renderer->prefix, "  ");
  } else {
    aimarkdown_cmark_strbuf_truncate(renderer->prefix, renderer->prefix->size - 2);
    renderer->cr(renderer);
  }
}

static void html_render(aimarkdown_cmark_syntax_extension *extension,
                        aimarkdown_cmark_html_renderer *renderer, aimarkdown_cmark_node *node,
                        aimarkdown_cmark_event_type ev_type, int options) {
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  if (entering) {
    aimarkdown_cmark_html_render_cr(renderer->html);
    aimarkdown_cmark_strbuf_puts(renderer->html, "<li");
    aimarkdown_cmark_html_render_sourcepos(node, renderer->html, options);
    aimarkdown_cmark_strbuf_putc(renderer->html, '>');
    if (node->as.list.checked) {
      aimarkdown_cmark_strbuf_puts(renderer->html, "<input type=\"checkbox\" checked=\"\" disabled=\"\" /> ");
    } else {
      aimarkdown_cmark_strbuf_puts(renderer->html, "<input type=\"checkbox\" disabled=\"\" /> ");
    }
  } else {
    aimarkdown_cmark_strbuf_puts(renderer->html, "</li>\n");
  }
}

static const char *xml_attr(aimarkdown_cmark_syntax_extension *extension,
                            aimarkdown_cmark_node *node) {
  if (node->as.list.checked) {
    return " completed=\"true\"";
  } else {
    return " completed=\"false\"";
  }
}

aimarkdown_cmark_syntax_extension *aimarkdown_create_tasklist_extension(void) {
  aimarkdown_cmark_syntax_extension *ext = aimarkdown_cmark_syntax_extension_new("tasklist");

  aimarkdown_cmark_syntax_extension_set_match_block_func(ext, matches);
  aimarkdown_cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  aimarkdown_cmark_syntax_extension_set_open_block_func(ext, open_tasklist_item);
  aimarkdown_cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  aimarkdown_cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  aimarkdown_cmark_syntax_extension_set_plaintext_render_func(ext, commonmark_render);
  aimarkdown_cmark_syntax_extension_set_html_render_func(ext, html_render);
  aimarkdown_cmark_syntax_extension_set_xml_attr_func(ext, xml_attr);

  return ext;
}
