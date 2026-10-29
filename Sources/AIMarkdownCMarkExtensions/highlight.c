// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/*
 * AIMarkdown highlight extension: delimiter plumbing adapted from swift-cmark
 * strikethrough.c (see COPYING). Highlight matching rules
 * ported from ai-markdown remark-mark-highlight 1.0.3, MIT; see
 * AIMarkdown/ThirdPartyLicenses/ai-markdown/LICENSE-remark-mark-highlight.
 */
#include <stdbool.h>

#include "aimarkdown-highlight.h"
#include <aimarkdown-parser.h>
#include <aimarkdown-render.h>

aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_HIGHLIGHT;

// Web classifies the adjacent UTF-16 code unit; supplementary scalars are ordinary.
#include <stdint.h>
#include <aimarkdown-utf8.h>
#include "highlight-unicode.inc"

static int character_class(int32_t code) {
  if (code < 0 || code == 9 || code == 10 || code == 11 || code == 12 ||
      code == 13 || code == 32 || code == 160 || code == 0x1680 ||
      (code >= 0x2000 && code <= 0x200a) || code == 0x2028 || code == 0x2029 ||
      code == 0x202f || code == 0x205f || code == 0x3000 || code == 0xfeff)
    return 1;
  if (code > 0xffff) return 0;
  size_t left = 0, right = sizeof(highlight_punctuation) / sizeof(highlight_punctuation[0]);
  while (left < right) {
    size_t middle = left + (right - left) / 2;
    if (code < highlight_punctuation[middle][0]) right = middle;
    else if (code > highlight_punctuation[middle][1]) left = middle + 1;
    else return 2;
  }
  return 0;
}

static aimarkdown_cmark_node *match(aimarkdown_cmark_syntax_extension *self, aimarkdown_cmark_parser *parser,
                        aimarkdown_cmark_node *parent, unsigned char character,
                        aimarkdown_cmark_inline_parser *inline_parser) {
  if (character != '=') return NULL;
  aimarkdown_cmark_chunk *input = aimarkdown_cmark_inline_parser_get_chunk(inline_parser);
  int start = aimarkdown_cmark_inline_parser_get_offset(inline_parser), end = start;
  while (end < input->len && input->data[end] == '=') end++;
  // Consume the whole run: a long run may not restart at its final two equals.
  aimarkdown_cmark_inline_parser_set_offset(inline_parser, end);
  aimarkdown_cmark_node *res = aimarkdown_cmark_node_new_with_mem(AIMARKDOWN_CMARK_NODE_TEXT, parser->mem);
  aimarkdown_cmark_chunk text = aimarkdown_cmark_chunk_dup(input, start, end - start);
  aimarkdown_cmark_node_set_literal(res, aimarkdown_cmark_chunk_to_cstr(parser->mem, &text));
  aimarkdown_cmark_chunk_free(parser->mem, &text);
  res->start_line = res->end_line = aimarkdown_cmark_inline_parser_get_line(inline_parser);
  res->end_column = aimarkdown_cmark_inline_parser_get_column(inline_parser) - 1;
  res->start_column = res->end_column - (end - start) + 1;
  if (end - start != 2) return res;
  int32_t before = -1, after = -1;
  if (start > 0) {
    int previous = start - 1;
    while (previous > 0 && (input->data[previous] & 0xc0) == 0x80) previous--;
    aimarkdown_cmark_utf8proc_iterate(input->data + previous, start - previous, &before);
  }
  if (end < input->len)
    aimarkdown_cmark_utf8proc_iterate(input->data + end, input->len - end, &after);
  int b = character_class(before), a = character_class(after);
  int open = a == 0 || (a == 2 && b != 0);
  int close = b == 0 || (b == 2 && a != 0);
  if (open || close)
    aimarkdown_cmark_inline_parser_push_delimiter(inline_parser, '=', open, close, res);
  return res;
}

static delimiter *insert(aimarkdown_cmark_syntax_extension *self, aimarkdown_cmark_parser *parser,
                         aimarkdown_cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  aimarkdown_cmark_node *highlight;
  aimarkdown_cmark_node *tmp, *next;
  delimiter *delim, *tmp_delim;
  delimiter *res = closer->next;

  highlight = opener->inl_text;

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len)
    goto done;

  if (!aimarkdown_cmark_node_set_type(highlight, AIMARKDOWN_CMARK_NODE_HIGHLIGHT))
    goto done;

  aimarkdown_cmark_node_set_syntax_extension(highlight, self);

  tmp = aimarkdown_cmark_node_next(opener->inl_text);

  while (tmp) {
    if (tmp == closer->inl_text)
      break;
    next = aimarkdown_cmark_node_next(tmp);
    aimarkdown_cmark_node_append_child(highlight, tmp);
    tmp = next;
  }

  highlight->end_line = closer->inl_text->end_line;
  highlight->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
  aimarkdown_cmark_node_free(closer->inl_text);

done:
  delim = closer;
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    aimarkdown_cmark_inline_parser_remove_delimiter(inline_parser, delim);
    delim = tmp_delim;
  }

  aimarkdown_cmark_inline_parser_remove_delimiter(inline_parser, opener);

  return res;
}

static const char *get_type_string(aimarkdown_cmark_syntax_extension *extension,
                                   aimarkdown_cmark_node *node) {
  return node->type == AIMARKDOWN_CMARK_NODE_HIGHLIGHT ? "highlight" : "<unknown>";
}

static int can_contain(aimarkdown_cmark_syntax_extension *extension, aimarkdown_cmark_node *node,
                       aimarkdown_cmark_node_type child_type) {
  if (node->type != AIMARKDOWN_CMARK_NODE_HIGHLIGHT)
    return false;

  return AIMARKDOWN_CMARK_NODE_TYPE_INLINE_P(child_type);
}

static void commonmark_render(aimarkdown_cmark_syntax_extension *extension,
                              aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                              aimarkdown_cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "==", false, LITERAL);
}

static void html_render(aimarkdown_cmark_syntax_extension *extension,
                        aimarkdown_cmark_html_renderer *renderer, aimarkdown_cmark_node *node,
                        aimarkdown_cmark_event_type ev_type, int options) {
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  if (entering) {
    aimarkdown_cmark_strbuf_puts(renderer->html, "<mark>");
  } else {
    aimarkdown_cmark_strbuf_puts(renderer->html, "</mark>");
  }
}

static void plaintext_render(aimarkdown_cmark_syntax_extension *extension,
                             aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                             aimarkdown_cmark_event_type ev_type, int options) {
  /* Plain text traverses children without adding highlight delimiters. */
}

aimarkdown_cmark_syntax_extension *aimarkdown_create_highlight_extension(void) {
  aimarkdown_cmark_syntax_extension *ext = aimarkdown_cmark_syntax_extension_new("highlight");
  aimarkdown_cmark_llist *special_chars = NULL;

  aimarkdown_cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  aimarkdown_cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  aimarkdown_cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  aimarkdown_cmark_syntax_extension_set_html_render_func(ext, html_render);
  aimarkdown_cmark_syntax_extension_set_plaintext_render_func(ext, plaintext_render);
  // These text formatters retain children without claiming native mark styling.
  aimarkdown_cmark_syntax_extension_set_latex_render_func(ext, plaintext_render);
  aimarkdown_cmark_syntax_extension_set_man_render_func(ext, plaintext_render);
  AIMARKDOWN_CMARK_NODE_HIGHLIGHT = aimarkdown_cmark_syntax_extension_add_node(1);

  aimarkdown_cmark_syntax_extension_set_match_inline_func(ext, match);
  aimarkdown_cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  aimarkdown_cmark_mem *mem = aimarkdown_cmark_get_default_mem_allocator();
  special_chars = aimarkdown_cmark_llist_append(mem, special_chars, (void *)'=');
  aimarkdown_cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  aimarkdown_cmark_syntax_extension_set_emphasis(ext, 1);

  return ext;
}
