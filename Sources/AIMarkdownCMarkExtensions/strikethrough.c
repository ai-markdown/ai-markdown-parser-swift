// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <stdbool.h>

#include "aimarkdown-strikethrough.h"
#include <aimarkdown-parser.h>
#include <aimarkdown-render.h>

aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_STRIKETHROUGH;

static void token_span(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *node, int start, int end) {
  if (!(parser->options & AIMARKDOWN_CMARK_OPT_WEB_POSITIONS)) return;
  node->web_subject_start = start;
  node->web_subject_end = end;
  node->web_position_flags = AIMARKDOWN_CMARK_WEB_POSITION_SUBJECT;
}

static aimarkdown_cmark_node *match(aimarkdown_cmark_syntax_extension *self, aimarkdown_cmark_parser *parser,
                         aimarkdown_cmark_node *parent, unsigned char character,
                         aimarkdown_cmark_inline_parser *inline_parser) {
  aimarkdown_cmark_node *res = NULL;
  int left_flanking, right_flanking, punct_before, punct_after, delims;
  char buffer[101];
  int start = aimarkdown_cmark_inline_parser_get_offset(inline_parser);

  if (character != '~')
    return NULL;

  if (parser->options & AIMARKDOWN_CMARK_OPT_WEB_EMPHASIS) {
    aimarkdown_cmark_chunk *input = aimarkdown_cmark_inline_parser_get_chunk(inline_parser);
    int end = start;
    while (end < input->len && input->data[end] == '~') end++;
    // Web rejects the complete run when a third tilde is present: do not restart
    // at the final one/two markers after cmark's fixed scan buffer boundary.
    if (end - start > 2) {
      aimarkdown_cmark_inline_parser_set_offset(inline_parser, end);
      res = aimarkdown_cmark_node_new_with_mem(AIMARKDOWN_CMARK_NODE_TEXT, parser->mem);
      aimarkdown_cmark_chunk text = aimarkdown_cmark_chunk_dup(input, start, end - start);
      aimarkdown_cmark_node_set_literal(res, aimarkdown_cmark_chunk_to_cstr(parser->mem, &text));
      aimarkdown_cmark_chunk_free(parser->mem, &text);
      res->start_line = res->end_line = aimarkdown_cmark_inline_parser_get_line(inline_parser);
      res->end_column = aimarkdown_cmark_inline_parser_get_column(inline_parser) - 1;
      res->start_column = res->end_column - (end - start) + 1;
      token_span(parser, res, start, end);
      return res;
    }
  }

  delims = aimarkdown_cmark_inline_parser_scan_delimiters(
      inline_parser, sizeof(buffer) - 1, '~',
      &left_flanking,
      &right_flanking, &punct_before, &punct_after);

  memset(buffer, '~', delims);
  buffer[delims] = 0;

  res = aimarkdown_cmark_node_new_with_mem(AIMARKDOWN_CMARK_NODE_TEXT, parser->mem);
  aimarkdown_cmark_node_set_literal(res, buffer);
  res->start_line = res->end_line = aimarkdown_cmark_inline_parser_get_line(inline_parser);
  res->start_column = aimarkdown_cmark_inline_parser_get_column(inline_parser) - delims;
  token_span(parser, res, start, start + delims);

  if ((left_flanking || right_flanking) &&
      (delims == 2 || (!(parser->options & AIMARKDOWN_CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE) && delims == 1))) {
    aimarkdown_cmark_inline_parser_push_delimiter(inline_parser, character, left_flanking,
                                       right_flanking, res);
  }

  return res;
}

static delimiter *insert(aimarkdown_cmark_syntax_extension *self, aimarkdown_cmark_parser *parser,
                         aimarkdown_cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  aimarkdown_cmark_node *strikethrough;
  aimarkdown_cmark_node *tmp, *next;
  delimiter *delim, *tmp_delim;
  delimiter *res = closer->next;

  strikethrough = opener->inl_text;

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len)
    goto done;

  bool positions = (opener->inl_text->web_position_flags & AIMARKDOWN_CMARK_WEB_POSITION_SUBJECT) &&
      (closer->inl_text->web_position_flags & AIMARKDOWN_CMARK_WEB_POSITION_SUBJECT);
  int start = opener->inl_text->web_subject_start;
  int end = closer->inl_text->web_subject_end;
  if (!aimarkdown_cmark_node_set_type(strikethrough, AIMARKDOWN_CMARK_NODE_STRIKETHROUGH))
    goto done;

  aimarkdown_cmark_node_set_syntax_extension(strikethrough, self);
  if (positions) token_span(parser, strikethrough, start, end);

  tmp = aimarkdown_cmark_node_next(opener->inl_text);

  while (tmp) {
    if (tmp == closer->inl_text)
      break;
    next = aimarkdown_cmark_node_next(tmp);
    aimarkdown_cmark_node_append_child(strikethrough, tmp);
    tmp = next;
  }

  strikethrough->end_column = closer->inl_text->start_column + closer->inl_text->as.literal.len - 1;
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
  return node->type == AIMARKDOWN_CMARK_NODE_STRIKETHROUGH ? "strikethrough" : "<unknown>";
}

static int can_contain(aimarkdown_cmark_syntax_extension *extension, aimarkdown_cmark_node *node,
                       aimarkdown_cmark_node_type child_type) {
  if (node->type != AIMARKDOWN_CMARK_NODE_STRIKETHROUGH)
    return false;

  return AIMARKDOWN_CMARK_NODE_TYPE_INLINE_P(child_type);
}

static void commonmark_render(aimarkdown_cmark_syntax_extension *extension,
                              aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                              aimarkdown_cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "~~", false, LITERAL);
}

static void latex_render(aimarkdown_cmark_syntax_extension *extension,
                         aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                         aimarkdown_cmark_event_type ev_type, int options) {
  // requires \usepackage{ulem}
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  if (entering) {
    renderer->out(renderer, node, "\\sout{", false, LITERAL);
  } else {
    renderer->out(renderer, node, "}", false, LITERAL);
  }
}

static void man_render(aimarkdown_cmark_syntax_extension *extension,
                       aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                       aimarkdown_cmark_event_type ev_type, int options) {
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  if (entering) {
    renderer->cr(renderer);
    renderer->out(renderer, node, ".ST \"", false, LITERAL);
  } else {
    renderer->out(renderer, node, "\"", false, LITERAL);
    renderer->cr(renderer);
  }
}

static void html_render(aimarkdown_cmark_syntax_extension *extension,
                        aimarkdown_cmark_html_renderer *renderer, aimarkdown_cmark_node *node,
                        aimarkdown_cmark_event_type ev_type, int options) {
  bool entering = (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER);
  if (entering) {
    aimarkdown_cmark_strbuf_puts(renderer->html, "<del>");
  } else {
    aimarkdown_cmark_strbuf_puts(renderer->html, "</del>");
  }
}

static void plaintext_render(aimarkdown_cmark_syntax_extension *extension,
                             aimarkdown_cmark_renderer *renderer, aimarkdown_cmark_node *node,
                             aimarkdown_cmark_event_type ev_type, int options) {
  renderer->out(renderer, node, "~", false, LITERAL);
}

aimarkdown_cmark_syntax_extension *aimarkdown_create_strikethrough_extension(void) {
  aimarkdown_cmark_syntax_extension *ext = aimarkdown_cmark_syntax_extension_new("strikethrough");
  aimarkdown_cmark_llist *special_chars = NULL;

  aimarkdown_cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  aimarkdown_cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  aimarkdown_cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  aimarkdown_cmark_syntax_extension_set_latex_render_func(ext, latex_render);
  aimarkdown_cmark_syntax_extension_set_man_render_func(ext, man_render);
  aimarkdown_cmark_syntax_extension_set_html_render_func(ext, html_render);
  aimarkdown_cmark_syntax_extension_set_plaintext_render_func(ext, plaintext_render);
  AIMARKDOWN_CMARK_NODE_STRIKETHROUGH = aimarkdown_cmark_syntax_extension_add_node(1);

  aimarkdown_cmark_syntax_extension_set_match_inline_func(ext, match);
  aimarkdown_cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  aimarkdown_cmark_mem *mem = aimarkdown_cmark_get_default_mem_allocator();
  special_chars = aimarkdown_cmark_llist_append(mem, special_chars, (void *)'~');
  aimarkdown_cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  aimarkdown_cmark_syntax_extension_set_emphasis(ext, 1);

  return ext;
}
