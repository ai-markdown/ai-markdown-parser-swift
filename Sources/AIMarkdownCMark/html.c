// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimarkdown-cmark_ctype.h"
#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-houdini.h"
#include "aimarkdown-scanners.h"
#include "aimarkdown-syntax_extension.h"
#include "aimarkdown-html.h"
#include "aimarkdown-render.h"

// Functions to convert cmark_nodes to HTML strings.

static void escape_html(aimarkdown_cmark_strbuf *dest, const unsigned char *source,
                        bufsize_t length) {
  aimarkdown_houdini_escape_html0(dest, source, length, 0);
}

static void filter_html_block(aimarkdown_cmark_html_renderer *renderer, uint8_t *data, size_t len) {
  aimarkdown_cmark_strbuf *html = renderer->html;
  aimarkdown_cmark_llist *it;
  aimarkdown_cmark_syntax_extension *ext;
  bool filtered;
  uint8_t *match;

  while (len) {
    match = (uint8_t *) memchr(data, '<', len);
    if (!match)
      break;

    if (match != data) {
      aimarkdown_cmark_strbuf_put(html, data, (bufsize_t)(match - data));
      len -= (match - data);
      data = match;
    }

    filtered = false;
    for (it = renderer->filter_extensions; it; it = it->next) {
      ext = ((aimarkdown_cmark_syntax_extension *) it->data);
      if (!ext->html_filter_func(ext, data, len)) {
        filtered = true;
        break;
      }
    }

    if (!filtered) {
      aimarkdown_cmark_strbuf_putc(html, '<');
    } else {
      aimarkdown_cmark_strbuf_puts(html, "&lt;");
    }

    ++data;
    --len;
  }

  if (len)
    aimarkdown_cmark_strbuf_put(html, data, (bufsize_t)len);
}

static bool S_put_footnote_backref(aimarkdown_cmark_html_renderer *renderer, aimarkdown_cmark_strbuf *html, aimarkdown_cmark_node *node) {
  if (renderer->written_footnote_ix >= renderer->footnote_ix)
    return false;
  renderer->written_footnote_ix = renderer->footnote_ix;
  char m[32];
  snprintf(m, sizeof(m), "%d", renderer->written_footnote_ix);

  aimarkdown_cmark_strbuf_puts(html, "<a href=\"#fnref-");
  aimarkdown_houdini_escape_href(html, node->as.literal.data, node->as.literal.len);
  aimarkdown_cmark_strbuf_puts(html, "\" class=\"footnote-backref\" data-footnote-backref data-footnote-backref-idx=\"");
  aimarkdown_cmark_strbuf_puts(html, m);
  aimarkdown_cmark_strbuf_puts(html, "\" aria-label=\"Back to reference ");
  aimarkdown_cmark_strbuf_puts(html, m);
  aimarkdown_cmark_strbuf_puts(html, "\">\xe2\x86\xa9</a>");

  if (node->footnote.def_count > 1)
  {
    for(int i = 2; i <= node->footnote.def_count; i++) {
      char n[32];
      snprintf(n, sizeof(n), "%d", i);

      aimarkdown_cmark_strbuf_puts(html, " <a href=\"#fnref-");
      aimarkdown_houdini_escape_href(html, node->as.literal.data, node->as.literal.len);
      aimarkdown_cmark_strbuf_puts(html, "-");
      aimarkdown_cmark_strbuf_puts(html, n);
      aimarkdown_cmark_strbuf_puts(html, "\" class=\"footnote-backref\" data-footnote-backref data-footnote-backref-idx=\"");
      aimarkdown_cmark_strbuf_puts(html, m);
      aimarkdown_cmark_strbuf_puts(html, "-");
      aimarkdown_cmark_strbuf_puts(html, n);
      aimarkdown_cmark_strbuf_puts(html, "\" aria-label=\"Back to reference ");
      aimarkdown_cmark_strbuf_puts(html, m);
      aimarkdown_cmark_strbuf_puts(html, "-");
      aimarkdown_cmark_strbuf_puts(html, n);
      aimarkdown_cmark_strbuf_puts(html, "\">\xe2\x86\xa9<sup class=\"footnote-ref\">");
      aimarkdown_cmark_strbuf_puts(html, n);
      aimarkdown_cmark_strbuf_puts(html, "</sup></a>");
    }
  }

  return true;
}

static int S_render_node(aimarkdown_cmark_html_renderer *renderer, aimarkdown_cmark_node *node,
                         aimarkdown_cmark_event_type ev_type, int options) {
  aimarkdown_cmark_node *parent;
  aimarkdown_cmark_node *grandparent;
  aimarkdown_cmark_strbuf *html = renderer->html;
  aimarkdown_cmark_llist *it;
  aimarkdown_cmark_syntax_extension *ext;
  char start_heading[] = "<h0";
  char end_heading[] = "</h0";
  bool tight;
  bool filtered;
  char buffer[BUFFER_SIZE];

  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);

  if (renderer->plain == node) { // back at original node
    renderer->plain = NULL;
  }

  if (renderer->plain != NULL) {
    switch (node->type) {
    case AIMARKDOWN_CMARK_NODE_TEXT:
    case AIMARKDOWN_CMARK_NODE_CODE:
    case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
      escape_html(html, node->as.literal.data, node->as.literal.len);
      break;

    case AIMARKDOWN_CMARK_NODE_LINEBREAK:
    case AIMARKDOWN_CMARK_NODE_SOFTBREAK:
      aimarkdown_cmark_strbuf_putc(html, ' ');
      break;

    default:
      break;
    }
    return 1;
  }

  // A tight paragraph ends in semantic text (possibly already LF), while
  // block renderers end in structural LF. The dd separator must not collapse
  // a paragraph hardbreak into the surrounding block separator.
  if (entering && node->parent && node->parent->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION &&
      !node->parent->definition.spread && node->prev && node->prev->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH)
    aimarkdown_cmark_strbuf_putc(html, '\n');

  if (node->extension && node->extension->html_render_func) {
    node->extension->html_render_func(node->extension, renderer, node, ev_type, options);
    return 1;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_DOCUMENT:
    break;

  case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
    if (entering) {
      aimarkdown_cmark_html_render_cr(html);
      aimarkdown_cmark_strbuf_puts(html, "<blockquote");
      aimarkdown_cmark_html_render_sourcepos(node, html, options);
      aimarkdown_cmark_strbuf_puts(html, ">\n");
    } else {
      aimarkdown_cmark_html_render_cr(html);
      aimarkdown_cmark_strbuf_puts(html, "</blockquote>\n");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_LIST:
    if (entering) {
      aimarkdown_cmark_html_render_cr(html);
      aimarkdown_cmark_strbuf_puts(html, "<dl");
      aimarkdown_cmark_html_render_sourcepos(node, html, options);
      aimarkdown_cmark_strbuf_putc(html, '>');
      if (node->first_child) aimarkdown_cmark_strbuf_putc(html, '\n');
    } else {
      aimarkdown_cmark_strbuf_puts(html, "</dl>\n");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_TERM:
    if (entering) {
      aimarkdown_cmark_html_render_cr(html);
      aimarkdown_cmark_strbuf_puts(html, "<dt");
      aimarkdown_cmark_html_render_sourcepos(node, html, options);
      aimarkdown_cmark_strbuf_putc(html, '>');
    } else {
      aimarkdown_cmark_strbuf_puts(html, "</dt>\n");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION:
    if (entering) {
      aimarkdown_cmark_html_render_cr(html);
      aimarkdown_cmark_strbuf_puts(html, "<dd");
      aimarkdown_cmark_html_render_sourcepos(node, html, options);
      aimarkdown_cmark_strbuf_putc(html, '>');
    } else {
      if (node->last_child && !node->definition.spread && node->last_child->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH) aimarkdown_cmark_strbuf_putc(html, '\n');
      else if (node->first_child) aimarkdown_cmark_html_render_cr(html);
      aimarkdown_cmark_strbuf_puts(html, "</dd>\n");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_LIST: {
    aimarkdown_cmark_list_type list_type = node->as.list.list_type;
    int start = node->as.list.start;

    if (entering) {
      aimarkdown_cmark_html_render_cr(html);
      if (list_type == AIMARKDOWN_CMARK_BULLET_LIST) {
        aimarkdown_cmark_strbuf_puts(html, "<ul");
        aimarkdown_cmark_html_render_sourcepos(node, html, options);
        aimarkdown_cmark_strbuf_puts(html, ">\n");
      } else if (start == 1) {
        aimarkdown_cmark_strbuf_puts(html, "<ol");
        aimarkdown_cmark_html_render_sourcepos(node, html, options);
        aimarkdown_cmark_strbuf_puts(html, ">\n");
      } else {
        snprintf(buffer, BUFFER_SIZE, "<ol start=\"%d\"", start);
        aimarkdown_cmark_strbuf_puts(html, buffer);
        aimarkdown_cmark_html_render_sourcepos(node, html, options);
        aimarkdown_cmark_strbuf_puts(html, ">\n");
      }
    } else {
      aimarkdown_cmark_strbuf_puts(html,
                        list_type == AIMARKDOWN_CMARK_BULLET_LIST ? "</ul>\n" : "</ol>\n");
    }
    break;
  }

  case AIMARKDOWN_CMARK_NODE_ITEM:
    if (entering) {
      aimarkdown_cmark_html_render_cr(html);
      aimarkdown_cmark_strbuf_puts(html, "<li");
      aimarkdown_cmark_html_render_sourcepos(node, html, options);
      aimarkdown_cmark_strbuf_putc(html, '>');
    } else {
      aimarkdown_cmark_strbuf_puts(html, "</li>\n");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_HEADING:
    if (entering) {
      aimarkdown_cmark_html_render_cr(html);
      start_heading[2] = (char)('0' + node->as.heading.level);
      aimarkdown_cmark_strbuf_puts(html, start_heading);
      aimarkdown_cmark_html_render_sourcepos(node, html, options);
      aimarkdown_cmark_strbuf_putc(html, '>');
    } else {
      end_heading[3] = (char)('0' + node->as.heading.level);
      aimarkdown_cmark_strbuf_puts(html, end_heading);
      aimarkdown_cmark_strbuf_puts(html, ">\n");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
    aimarkdown_cmark_html_render_cr(html);
    aimarkdown_cmark_strbuf_puts(html, "<pre><code class=\"language-math math-display\">");
    escape_html(html, node->as.code.literal.data, node->as.code.literal.len);
    aimarkdown_cmark_strbuf_puts(html, "</code></pre>\n");
    break;
  case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
    aimarkdown_cmark_strbuf_puts(html, "<code class=\"language-math math-inline\">");
    escape_html(html, node->as.literal.data, node->as.literal.len);
    aimarkdown_cmark_strbuf_puts(html, "</code>");
    break;

  case AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION:
    break;

  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    aimarkdown_cmark_html_render_cr(html);

    if (node->as.code.info.len == 0) {
      aimarkdown_cmark_strbuf_puts(html, "<pre");
      aimarkdown_cmark_html_render_sourcepos(node, html, options);
      aimarkdown_cmark_strbuf_puts(html, "><code>");
    } else {
      bufsize_t first_tag = 0;
      while (first_tag < node->as.code.info.len &&
             !aimarkdown_cmark_isspace(node->as.code.info.data[first_tag])) {
        first_tag += 1;
      }

      if (options & AIMARKDOWN_CMARK_OPT_GITHUB_PRE_LANG) {
        aimarkdown_cmark_strbuf_puts(html, "<pre");
        aimarkdown_cmark_html_render_sourcepos(node, html, options);
        aimarkdown_cmark_strbuf_puts(html, " lang=\"");
        escape_html(html, node->as.code.info.data, first_tag);
        if (first_tag < node->as.code.info.len && (options & AIMARKDOWN_CMARK_OPT_FULL_INFO_STRING)) {
          aimarkdown_cmark_strbuf_puts(html, "\" data-meta=\"");
          escape_html(html, node->as.code.info.data + first_tag + 1, node->as.code.info.len - first_tag - 1);
        }
        aimarkdown_cmark_strbuf_puts(html, "\"><code>");
      } else {
        aimarkdown_cmark_strbuf_puts(html, "<pre");
        aimarkdown_cmark_html_render_sourcepos(node, html, options);
        aimarkdown_cmark_strbuf_puts(html, "><code class=\"language-");
        escape_html(html, node->as.code.info.data, first_tag);
        if (first_tag < node->as.code.info.len && (options & AIMARKDOWN_CMARK_OPT_FULL_INFO_STRING)) {
          aimarkdown_cmark_strbuf_puts(html, "\" data-meta=\"");
          escape_html(html, node->as.code.info.data + first_tag + 1, node->as.code.info.len - first_tag - 1);
        }
        aimarkdown_cmark_strbuf_puts(html, "\">");
      }
    }

    escape_html(html, node->as.code.literal.data, node->as.code.literal.len);
    aimarkdown_cmark_strbuf_puts(html, "</code></pre>\n");
    break;

  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
    aimarkdown_cmark_html_render_cr(html);
    if (!(options & AIMARKDOWN_CMARK_OPT_UNSAFE)) {
      aimarkdown_cmark_strbuf_puts(html, "<!-- raw HTML omitted -->");
    } else if (renderer->filter_extensions) {
      filter_html_block(renderer, node->as.literal.data, node->as.literal.len);
    } else {
      aimarkdown_cmark_strbuf_put(html, node->as.literal.data, node->as.literal.len);
    }
    aimarkdown_cmark_html_render_cr(html);
    break;

  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    aimarkdown_cmark_html_render_cr(html);
    if (entering) {
      aimarkdown_cmark_strbuf_put(html, node->as.custom.on_enter.data,
                       node->as.custom.on_enter.len);
    } else {
      aimarkdown_cmark_strbuf_put(html, node->as.custom.on_exit.data,
                       node->as.custom.on_exit.len);
    }
    aimarkdown_cmark_html_render_cr(html);
    break;

  case AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK:
    aimarkdown_cmark_html_render_cr(html);
    aimarkdown_cmark_strbuf_puts(html, "<hr");
    aimarkdown_cmark_html_render_sourcepos(node, html, options);
    aimarkdown_cmark_strbuf_puts(html, " />\n");
    break;

  case AIMARKDOWN_CMARK_NODE_PARAGRAPH:
    parent = aimarkdown_cmark_node_parent(node);
    grandparent = aimarkdown_cmark_node_parent(parent);
    if (parent && parent->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION) {
      tight = !parent->definition.spread;
      if (tight && entering && node->prev) aimarkdown_cmark_html_render_cr(html);
    } else if (grandparent != NULL && grandparent->type == AIMARKDOWN_CMARK_NODE_LIST) {
      tight = grandparent->as.list.tight;
    } else {
      tight = false;
    }
    if (!tight) {
      if (entering) {
        aimarkdown_cmark_html_render_cr(html);
        aimarkdown_cmark_strbuf_puts(html, "<p");
        aimarkdown_cmark_html_render_sourcepos(node, html, options);
        aimarkdown_cmark_strbuf_putc(html, '>');
      } else {
        if (parent->type == AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION && node->next == NULL) {
          aimarkdown_cmark_strbuf_putc(html, ' ');
          S_put_footnote_backref(renderer, html, parent);
        }
        aimarkdown_cmark_strbuf_puts(html, "</p>\n");
      }
    }
    break;

  case AIMARKDOWN_CMARK_NODE_TEXT:
    escape_html(html, node->as.literal.data, node->as.literal.len);
    break;

  case AIMARKDOWN_CMARK_NODE_LINEBREAK:
    aimarkdown_cmark_strbuf_puts(html, "<br />\n");
    break;

  case AIMARKDOWN_CMARK_NODE_SOFTBREAK:
    if (options & AIMARKDOWN_CMARK_OPT_HARDBREAKS) {
      aimarkdown_cmark_strbuf_puts(html, "<br />\n");
    } else if (options & AIMARKDOWN_CMARK_OPT_NOBREAKS) {
      aimarkdown_cmark_strbuf_putc(html, ' ');
    } else {
      aimarkdown_cmark_strbuf_putc(html, '\n');
    }
    break;

  case AIMARKDOWN_CMARK_NODE_CODE:
    aimarkdown_cmark_strbuf_puts(html, "<code>");
    escape_html(html, node->as.literal.data, node->as.literal.len);
    aimarkdown_cmark_strbuf_puts(html, "</code>");
    break;

  case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
    if (!(options & AIMARKDOWN_CMARK_OPT_UNSAFE)) {
      aimarkdown_cmark_strbuf_puts(html, "<!-- raw HTML omitted -->");
    } else {
      filtered = false;
      for (it = renderer->filter_extensions; it; it = it->next) {
        ext = (aimarkdown_cmark_syntax_extension *) it->data;
        if (!ext->html_filter_func(ext, node->as.literal.data, node->as.literal.len)) {
          filtered = true;
          break;
        }
      }
      if (!filtered) {
        aimarkdown_cmark_strbuf_put(html, node->as.literal.data, node->as.literal.len);
      } else {
        aimarkdown_cmark_strbuf_puts(html, "&lt;");
        aimarkdown_cmark_strbuf_put(html, node->as.literal.data + 1, node->as.literal.len - 1);
      }
    }
    break;

  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
    if (entering) {
      aimarkdown_cmark_strbuf_put(html, node->as.custom.on_enter.data,
                       node->as.custom.on_enter.len);
    } else {
      aimarkdown_cmark_strbuf_put(html, node->as.custom.on_exit.data,
                       node->as.custom.on_exit.len);
    }
    break;

  case AIMARKDOWN_CMARK_NODE_STRONG:
    if (node->parent == NULL || node->parent->type != AIMARKDOWN_CMARK_NODE_STRONG) {
      if (entering) {
        aimarkdown_cmark_strbuf_puts(html, "<strong>");
      } else {
        aimarkdown_cmark_strbuf_puts(html, "</strong>");
      }
    }
    break;

  case AIMARKDOWN_CMARK_NODE_EMPH:
    if (entering) {
      aimarkdown_cmark_strbuf_puts(html, "<em>");
    } else {
      aimarkdown_cmark_strbuf_puts(html, "</em>");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_LINK:
    if (entering) {
      aimarkdown_cmark_strbuf_puts(html, "<a href=\"");
      if ((options & AIMARKDOWN_CMARK_OPT_UNSAFE) ||
            !(scan_dangerous_url(&node->as.link.url, 0))) {
        aimarkdown_houdini_escape_href(html, node->as.link.url.data,
                            node->as.link.url.len);
      }
      if (node->as.link.title.len) {
        aimarkdown_cmark_strbuf_puts(html, "\" title=\"");
        escape_html(html, node->as.link.title.data, node->as.link.title.len);
      }
      aimarkdown_cmark_strbuf_puts(html, "\">");
    } else {
      aimarkdown_cmark_strbuf_puts(html, "</a>");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_IMAGE:
    if (entering) {
      aimarkdown_cmark_strbuf_puts(html, "<img src=\"");
      if ((options & AIMARKDOWN_CMARK_OPT_UNSAFE) ||
            !(scan_dangerous_url(&node->as.link.url, 0))) {
        aimarkdown_houdini_escape_href(html, node->as.link.url.data,
                            node->as.link.url.len);
      }
      aimarkdown_cmark_strbuf_puts(html, "\" alt=\"");
      renderer->plain = node;
    } else {
      if (node->as.link.title.len) {
        aimarkdown_cmark_strbuf_puts(html, "\" title=\"");
        escape_html(html, node->as.link.title.data, node->as.link.title.len);
      }

      aimarkdown_cmark_strbuf_puts(html, "\" />");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
    // TODO: Output span, attributes potentially controlling class/id here. For now just output the main string.
    /*
    if (entering) {
      cmark_strbuf_puts(html, "<span __attributes=\"");
      cmark_strbuf_put(html, node->as.attribute.attributes.data, node->as.attribute.attributes.len);
      cmark_strbuf_puts(html, "\">");
    } else {
      cmark_strbuf_puts(html, "</span>");
    }
    */
    break;

  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
    if (entering) {
      if (renderer->footnote_ix == 0) {
        aimarkdown_cmark_strbuf_puts(html, "<section class=\"footnotes\" data-footnotes>\n<ol>\n");
      }
      ++renderer->footnote_ix;

      aimarkdown_cmark_strbuf_puts(html, "<li id=\"fn-");
      aimarkdown_houdini_escape_href(html, node->as.literal.data, node->as.literal.len);
      aimarkdown_cmark_strbuf_puts(html, "\">\n");
    } else {
      if (S_put_footnote_backref(renderer, html, node)) {
        aimarkdown_cmark_strbuf_putc(html, '\n');
      }
      aimarkdown_cmark_strbuf_puts(html, "</li>\n");
    }
    break;

  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE:
    if (entering) {
      aimarkdown_cmark_strbuf_puts(html, "<sup class=\"footnote-ref\"><a href=\"#fn-");
      aimarkdown_houdini_escape_href(html, node->parent_footnote_def->as.literal.data, node->parent_footnote_def->as.literal.len);
      aimarkdown_cmark_strbuf_puts(html, "\" id=\"fnref-");
      aimarkdown_houdini_escape_href(html, node->parent_footnote_def->as.literal.data, node->parent_footnote_def->as.literal.len);

      if (node->footnote.ref_ix > 1) {
        char n[32];
        snprintf(n, sizeof(n), "%d", node->footnote.ref_ix);
        aimarkdown_cmark_strbuf_puts(html, "-");
        aimarkdown_cmark_strbuf_puts(html, n);
      }

      aimarkdown_cmark_strbuf_puts(html, "\" data-footnote-ref>");
      aimarkdown_houdini_escape_href(html, node->as.literal.data, node->as.literal.len);
      aimarkdown_cmark_strbuf_puts(html, "</a></sup>");
    }
    break;

  default:
    assert(false);
    break;
  }

  return 1;
}

char *aimarkdown_cmark_render_html(aimarkdown_cmark_node *root, int options, aimarkdown_cmark_llist *extensions) {
  return aimarkdown_cmark_render_html_with_mem(root, options, extensions, aimarkdown_cmark_node_mem(root));
}

char *aimarkdown_cmark_render_html_with_mem(aimarkdown_cmark_node *root, int options, aimarkdown_cmark_llist *extensions, aimarkdown_cmark_mem *mem) {
  char *result;
  aimarkdown_cmark_strbuf html = AIMARKDOWN_CMARK_BUF_INIT(mem);
  aimarkdown_cmark_event_type ev_type;
  aimarkdown_cmark_node *cur;
  aimarkdown_cmark_html_renderer renderer = {&html, NULL, NULL, 0, 0, NULL};
  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(root);

  for (; extensions; extensions = extensions->next)
    if (((aimarkdown_cmark_syntax_extension *) extensions->data)->html_filter_func)
      renderer.filter_extensions = aimarkdown_cmark_llist_append(
          mem,
          renderer.filter_extensions,
          (aimarkdown_cmark_syntax_extension *) extensions->data);

  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    S_render_node(&renderer, cur, ev_type, options);
  }

  if (renderer.footnote_ix) {
    aimarkdown_cmark_strbuf_puts(&html, "</ol>\n</section>\n");
  }

  result = (char *)aimarkdown_cmark_strbuf_detach(&html);

  aimarkdown_cmark_llist_free(mem, renderer.filter_extensions);

  aimarkdown_cmark_iter_free(iter);
  return result;
}
