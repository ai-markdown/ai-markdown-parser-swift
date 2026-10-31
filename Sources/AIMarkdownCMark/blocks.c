// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/**
 * Block parsing implementation.
 *
 * For a high-level overview of the block parsing process,
 * see http://spec.commonmark.org/0.24/#phase-1-block-structure
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "aimarkdown-cmark_ctype.h"
#include "aimarkdown-syntax_extension.h"
#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-parser.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-node.h"
#include "aimarkdown-references.h"
#include "aimarkdown-utf8.h"
#include "aimarkdown-scanners.h"
#include "aimarkdown-inlines.h"
#include "aimarkdown-houdini.h"
#include "aimarkdown-string_token.h"
#include "aimarkdown-buffer.h"
#include "aimarkdown-footnotes.h"

#define CODE_INDENT 4
#define TAB_STOP 4

/**
 * Very deeply nested lists can cause quadratic performance issues.
 * This constant is used in open_new_blocks() to limit the nesting
 * depth. It is unlikely that a non-contrived markdown document will
 * be nested this deeply.
 */
#define MAX_LIST_DEPTH 100

#ifndef MIN
#define MIN(x, y) ((x < y) ? x : y)
#endif

#define peek_at(i, n) (i)->data[n]

static bool S_last_line_blank(const aimarkdown_cmark_node *node) {
  return (node->flags & AIMARKDOWN_CMARK_NODE__LAST_LINE_BLANK) != 0;
}

static bool S_last_line_checked(const aimarkdown_cmark_node *node) {
  return (node->flags & AIMARKDOWN_CMARK_NODE__LAST_LINE_CHECKED) != 0;
}

static inline aimarkdown_cmark_node_type S_type(const aimarkdown_cmark_node *node) {
  return (aimarkdown_cmark_node_type)node->type;
}

static void S_set_last_line_blank(aimarkdown_cmark_node *node, bool is_blank) {
  if (is_blank)
    node->flags |= AIMARKDOWN_CMARK_NODE__LAST_LINE_BLANK;
  else
    node->flags &= ~AIMARKDOWN_CMARK_NODE__LAST_LINE_BLANK;
}

static void S_set_last_line_checked(aimarkdown_cmark_node *node) {
  node->flags |= AIMARKDOWN_CMARK_NODE__LAST_LINE_CHECKED;
}

static inline bool S_is_line_end_char(char c) {
  return (c == '\n' || c == '\r');
}

static inline bool S_is_space_or_tab(char c) {
  return (c == ' ' || c == '\t');
}

static void S_parser_feed(aimarkdown_cmark_parser *parser, const unsigned char *buffer,
                          size_t len, bool eof);

static void S_process_line(aimarkdown_cmark_parser *parser, const unsigned char *buffer,
                           bufsize_t bytes, bool ensureEndsInNewline);

static aimarkdown_cmark_node *make_block(aimarkdown_cmark_mem *mem, aimarkdown_cmark_node_type tag,
                              int start_line, int start_column) {
  aimarkdown_cmark_node *e;

  e = (aimarkdown_cmark_node *)mem->calloc(1, sizeof(*e));
  aimarkdown_cmark_strbuf_init(mem, &e->content, 32);
  e->type = (uint16_t)tag;
  e->flags = AIMARKDOWN_CMARK_NODE__OPEN;
  e->start_line = start_line;
  e->start_column = start_column;
  e->end_line = start_line;

  return e;
}

// Create a root document node.
static aimarkdown_cmark_node *make_document(aimarkdown_cmark_mem *mem) {
  aimarkdown_cmark_node *e = make_block(mem, AIMARKDOWN_CMARK_NODE_DOCUMENT, 1, 1);
  return e;
}

int aimarkdown_cmark_parser_attach_syntax_extension(aimarkdown_cmark_parser *parser,
                                         aimarkdown_cmark_syntax_extension *extension) {
  parser->syntax_extensions = aimarkdown_cmark_llist_append(parser->mem, parser->syntax_extensions, extension);
  if (extension->match_inline || extension->insert_inline_from_delim) {
    if (!parser->inline_syntax_extensions) {
      // if we're loading an inline extension into this parser for the first time,
      // allocate new buffers for the inline parser character arrays
      parser->skip_chars = (int8_t *)parser->mem->calloc(sizeof(int8_t), 256);
      aimarkdown_cmark_set_default_skip_chars(&parser->skip_chars, true);

      parser->special_chars = (int8_t *)parser->mem->calloc(sizeof(int8_t), 256);
      aimarkdown_cmark_set_default_special_chars(&parser->special_chars, true);
    }

    parser->inline_syntax_extensions = aimarkdown_cmark_llist_append(
      parser->mem, parser->inline_syntax_extensions, extension);
  }

  return 1;
}

static void aimarkdown_cmark_parser_dispose(aimarkdown_cmark_parser *parser) {
  aimarkdown_cmark_list_spread_free(parser);
  if (parser->root)
    aimarkdown_cmark_node_free(parser->root);

  if (parser->refmap)
    aimarkdown_cmark_map_free(parser->refmap);
}

static void aimarkdown_cmark_parser_reset(aimarkdown_cmark_parser *parser) {
  aimarkdown_cmark_llist *saved_exts = parser->syntax_extensions;
  aimarkdown_cmark_llist *saved_inline_exts = parser->inline_syntax_extensions;
  int saved_options = parser->options;
  aimarkdown_cmark_mem *saved_mem = parser->mem;
  int8_t *saved_specials = parser->special_chars;
  int8_t *saved_skips = parser->skip_chars;

  aimarkdown_cmark_parser_dispose(parser);

  memset(parser, 0, sizeof(aimarkdown_cmark_parser));
  parser->mem = saved_mem;

  aimarkdown_cmark_strbuf_init(parser->mem, &parser->curline, 256);
  aimarkdown_cmark_strbuf_init(parser->mem, &parser->linebuf, 0);

  aimarkdown_cmark_node *document = make_document(parser->mem);

  if (saved_options & AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES) saved_options |= AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS;
  parser->refmap = aimarkdown_cmark_reference_map_new(parser->mem);
  parser->refmap->web_reference_labels = (saved_options & AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES) != 0;
  parser->refmap->web_character_references = (saved_options & AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES) != 0;
  parser->root = document;
  parser->current = document;

  parser->syntax_extensions = saved_exts;
  parser->inline_syntax_extensions = saved_inline_exts;
  parser->options = saved_options;

  parser->special_chars = saved_specials;
  parser->skip_chars = saved_skips;
}

aimarkdown_cmark_parser *aimarkdown_cmark_parser_new_with_mem(int options, aimarkdown_cmark_mem *mem) {
  aimarkdown_cmark_parser *parser = (aimarkdown_cmark_parser *)mem->calloc(1, sizeof(aimarkdown_cmark_parser));
  parser->mem = mem;
  parser->options = options;
  aimarkdown_cmark_set_default_skip_chars(&parser->skip_chars, false);
  aimarkdown_cmark_set_default_special_chars(&parser->special_chars, false);
  aimarkdown_cmark_parser_reset(parser);
  return parser;
}

aimarkdown_cmark_parser *aimarkdown_cmark_parser_new(int options) {
  extern aimarkdown_cmark_mem AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR;
  return aimarkdown_cmark_parser_new_with_mem(options, &AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR);
}

void aimarkdown_cmark_parser_free(aimarkdown_cmark_parser *parser) {
  aimarkdown_cmark_mem *mem = parser->mem;

  // If any inline syntax extensions were added, free the memory allocated for the special-chars arrays
  if (parser->inline_syntax_extensions) {
    mem->free(parser->special_chars);
    mem->free(parser->skip_chars);
  }

  aimarkdown_cmark_parser_dispose(parser);
  aimarkdown_cmark_strbuf_free(&parser->curline);
  aimarkdown_cmark_strbuf_free(&parser->linebuf);
  aimarkdown_cmark_llist_free(parser->mem, parser->syntax_extensions);
  aimarkdown_cmark_llist_free(parser->mem, parser->inline_syntax_extensions);
  mem->free(parser);
}

static aimarkdown_cmark_node *finalize(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *b);

// Returns true if line has only space characters, else false.
static bool is_blank(aimarkdown_cmark_strbuf *s, bufsize_t offset) {
  while (offset < s->size) {
    switch (s->ptr[offset]) {
    case '\r':
    case '\n':
      return true;
    case ' ':
      offset++;
      break;
    case '\t':
      offset++;
      break;
    default:
      return false;
    }
  }

  return true;
}

static inline bool accepts_lines(aimarkdown_cmark_node_type block_type) {
  return (block_type == AIMARKDOWN_CMARK_NODE_PARAGRAPH ||
          block_type == AIMARKDOWN_CMARK_NODE_HEADING ||
          block_type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK || block_type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK);
}

static inline bool contains_inlines(aimarkdown_cmark_node *node) {
  if (node->extension && node->extension->contains_inlines_func) {
    return node->extension->contains_inlines_func(node->extension, node) != 0;
  }

  return (node->type == AIMARKDOWN_CMARK_NODE_DEFINITION_TERM ||
          node->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH ||
          node->type == AIMARKDOWN_CMARK_NODE_HEADING);
}

static void S_advance_offset(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input, bufsize_t count, bool columns);

/* A final continuation can consist entirely of consumed container/fence
 * prefixes. Its synthetic LF is not mathFlow data. Opening/closing fences,
 * real line endings and partial tabs are separate parser events. */
static bool math_prefix_only_eof(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *node,
                                  bufsize_t physical_length) {
  return node->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK && node == parser->current &&
      node->start_line < parser->line_number && !node->math_closing_fence &&
      parser->definition_synthetic_ending && parser->math_current_ending == 0 &&
      !parser->partially_consumed_tab && parser->offset > 0 &&
      parser->offset == physical_length;
}

static void add_line(aimarkdown_cmark_node *node, aimarkdown_cmark_chunk *ch, aimarkdown_cmark_parser *parser) {
  int chars_to_tab;
  int i;
  bufsize_t code_source_start = node->content.size;
  assert(node->flags & AIMARKDOWN_CMARK_NODE__OPEN);
  bufsize_t physical_length = ch->len;
  if (physical_length && ch->data[physical_length - 1] == '\n') physical_length--;
  if (math_prefix_only_eof(parser, node, physical_length)) return;
  if ((parser->options & AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS) && node->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH &&
      node->content.size > 0 && parser->column > 0 && node->parent &&
      (node->parent->type == AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE || node->parent->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION)) {
    node->definition.flow_offset = node->content.size;
    node->definition.flow_line = parser->line_number;
    node->definition.flow_column = parser->offset + (parser->partially_consumed_tab ? 2 : 1);
  }
  if (parser->partially_consumed_tab) {
    parser->offset += 1; // skip over tab
    // add space characters:
    chars_to_tab = TAB_STOP - (parser->column % TAB_STOP);
    for (i = 0; i < chars_to_tab; i++) {
      aimarkdown_cmark_strbuf_putc(&node->content, ' ');
    }
  }
  if ((parser->options & (AIMARKDOWN_CMARK_OPT_MATH | AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS | AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES | AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES | AIMARKDOWN_CMARK_OPT_CODE_METADATA | AIMARKDOWN_CMARK_OPT_SOFTBREAK_METADATA | AIMARKDOWN_CMARK_OPT_WEB_POSITIONS)) &&
      (node->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH || node->type == AIMARKDOWN_CMARK_NODE_HEADING)) {
    if (node->math_line_count == node->math_line_capacity) {
      node->math_line_capacity = node->math_line_capacity ? node->math_line_capacity * 2 : 8;
      node->math_lines = parser->mem->realloc(node->math_lines, node->math_line_capacity * sizeof(aimarkdown_cmark_math_line));
    }
    aimarkdown_cmark_math_line *line = &node->math_lines[node->math_line_count++];
    line->offset = node->content.size;
    line->line = parser->line_number;
    line->column = parser->offset + 1;
    line->ending = parser->math_current_ending;
    int prefix = parser->math_inline_prefix;
    line->visual_column = aimarkdown_cmark_reference_column(1, ch->data, parser->offset);
    line->prefix_visual_column = aimarkdown_cmark_reference_column(1, ch->data, parser->offset - prefix);
    line->prefix = aimarkdown_cmark_chunk_dup(ch, parser->offset - prefix, prefix);
    // Own the prefix: the parser's current-line storage is reused next line.
    aimarkdown_cmark_chunk_to_cstr(parser->mem, &line->prefix);
    parser->math_eol_target = node;
  }
  aimarkdown_cmark_strbuf_put(&node->content, ch->data + parser->offset,
                   ch->len - parser->offset);
  if (node->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK && node->content.size && parser->math_current_ending != 0) {
    aimarkdown_cmark_strbuf_truncate(&node->content, node->content.size - 1);
    if (parser->math_current_ending != 10) aimarkdown_cmark_strbuf_putc(&node->content, '\r');
    if (parser->math_current_ending != 13) aimarkdown_cmark_strbuf_putc(&node->content, '\n');
    parser->math_eol_target = node;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK && node->code_metadata) {
    aimarkdown_cmark_code_metadata *data = node->code_metadata;
    aimarkdown_cmark_strbuf line = AIMARKDOWN_CMARK_BUF_INIT(parser->mem);
    if (parser->blank && parser->code_footnote_blank_offset >= 0) {
      /* GFM footnote blankLine is a check, not an indent token. Use the real
       * blank-line bytes before the legacy footnote prefix consumed them. */
      aimarkdown_cmark_parser cursor = *parser;
      cursor.offset = parser->code_footnote_blank_offset;
      cursor.column = parser->code_footnote_blank_column;
      cursor.partially_consumed_tab = parser->code_footnote_blank_partial;
      /* Replay only the real open-container whitespace prefixes. GFM
       * footnote blankLine is non-consuming; list/definition/code prefixes
       * still consume their token widths, including virtual tab spaces. */
      size_t count = 0;
      aimarkdown_cmark_node *owner = node;
      while (owner && owner != parser->code_footnote_blank_container) { count++; owner = owner->parent; }
      if (owner) {
        aimarkdown_cmark_node **path = parser->mem->calloc(count, sizeof(aimarkdown_cmark_node *));
        owner = node;
        for (size_t j = count; j > 0; j--) { path[j - 1] = owner; owner = owner->parent; }
        for (size_t j = 0; j < count; j++) {
          int width = 0;
          if (path[j]->type == AIMARKDOWN_CMARK_NODE_ITEM) width = path[j]->as.list.marker_offset + path[j]->as.list.padding;
          else if (path[j]->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION) width = path[j]->definition.padding;
          else if (path[j] == node) width = data->kind == 2 ? CODE_INDENT : node->as.code.fence_offset;
          int consumed = 0;
          while (consumed < width && S_is_space_or_tab(peek_at(ch, cursor.offset))) { S_advance_offset(&cursor, ch, 1, true); consumed++; }
          if (path[j] == node && data->kind == 2) data->indented_qualified = consumed == CODE_INDENT;
        }
        parser->mem->free(path);
      }
      if (cursor.partially_consumed_tab) {
        cursor.offset++;
        for (int j = TAB_STOP - (cursor.column % TAB_STOP); j > 0; j--) aimarkdown_cmark_strbuf_putc(&line, ' ');
      }
      aimarkdown_cmark_strbuf_put(&line, ch->data + cursor.offset, ch->len - cursor.offset);
    } else aimarkdown_cmark_strbuf_put(&line, node->content.ptr + code_source_start, node->content.size - code_source_start);
    if (line.size && line.ptr[line.size - 1] == '\n') aimarkdown_cmark_strbuf_truncate(&line, line.size - 1);
    aimarkdown_cmark_strbuf_put(&data->source, line.ptr, line.size);
    if (data->kind == 2 && data->indented_qualified) {
      aimarkdown_cmark_strbuf_put(&data->compiled, data->pending.ptr, data->pending.size);
      aimarkdown_cmark_strbuf_clear(&data->pending);
      aimarkdown_cmark_strbuf_put(&data->compiled, line.ptr, line.size);
    }
    if (parser->math_current_ending == 13 || parser->math_current_ending == 0x0d0a) {
      aimarkdown_cmark_strbuf_putc(&data->source, '\r');
      if (data->kind == 2) aimarkdown_cmark_strbuf_putc(&data->pending, '\r');
    }
    if (parser->math_current_ending == 10 || parser->math_current_ending == 0x0d0a) {
      aimarkdown_cmark_strbuf_putc(&data->source, '\n');
      if (data->kind == 2) aimarkdown_cmark_strbuf_putc(&data->pending, '\n');
    }
    aimarkdown_cmark_strbuf_free(&line);
    parser->math_eol_target = node;
  }
}

static void remove_trailing_blank_lines(aimarkdown_cmark_strbuf *ln) {
  bufsize_t i;
  unsigned char c;

  for (i = ln->size - 1; i >= 0; --i) {
    c = ln->ptr[i];

    if (c != ' ' && c != '\t' && !S_is_line_end_char(c))
      break;
  }

  if (i < 0) {
    aimarkdown_cmark_strbuf_clear(ln);
    return;
  }

  for (; i < ln->size; ++i) {
    c = ln->ptr[i];

    if (!S_is_line_end_char(c))
      continue;

    aimarkdown_cmark_strbuf_truncate(ln, i);
    break;
  }
}

// Check to see if a node ends with a blank line, descending
// if needed into lists and sublists.
static bool S_ends_with_blank_line(aimarkdown_cmark_node *node) {
  if (S_last_line_checked(node)) {
    return(S_last_line_blank(node));
  } else if ((S_type(node) == AIMARKDOWN_CMARK_NODE_LIST ||
              S_type(node) == AIMARKDOWN_CMARK_NODE_ITEM) && node->last_child) {
    S_set_last_line_checked(node);
    return(S_ends_with_blank_line(node->last_child));
  } else {
    S_set_last_line_checked(node);
    return (S_last_line_blank(node));
  }
}

// returns true if content remains after link defs are resolved.
static bool resolve_reference_link_definitions(
		aimarkdown_cmark_parser *parser,
                aimarkdown_cmark_node *b) {
  bufsize_t pos;
  aimarkdown_cmark_strbuf *node_content = &b->content;
  aimarkdown_cmark_chunk chunk = {node_content->ptr, node_content->size, 0};
  for (;;) {
    aimarkdown_cmark_node *definition = NULL;
    bufsize_t indent = 0;
    if (parser->options & AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS) {
      while (indent < chunk.len && (chunk.data[indent] == ' ' || chunk.data[indent] == '\t')) indent++;
    }
    if (chunk.len > indent && chunk.data[indent] == '[') {
      aimarkdown_cmark_chunk candidate = {chunk.data + indent, chunk.len - indent, 0};
      pos = (parser->options & AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS) ?
          aimarkdown_cmark_parse_reference_definition(parser->mem, &candidate, parser->refmap, b,
                                          node_content->size - chunk.len + indent, &definition) :
          aimarkdown_cmark_parse_reference_inline(parser->mem, &chunk, parser->refmap);
      if (pos) pos += indent;
    } else if (!(parser->options & AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES) && chunk.len > 1 && chunk.data[0] == '^' && chunk.data[1] == '[') {
      pos = aimarkdown_cmark_parse_reference_attributes_inline(parser->mem, &chunk, parser->refmap);
    } else pos = 0;
    if (!pos) break;
    if (definition && !aimarkdown_cmark_node_insert_before(b, definition)) aimarkdown_cmark_node_free(definition);
    chunk.data += pos;
    chunk.len -= pos;
  }
  if ((parser->options & AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS) && node_content->size > chunk.len) {
    // Remaining flow content starts a new logical paragraph after the definitions.
    while (chunk.len && (chunk.data[0] == ' ' || chunk.data[0] == '\t')) { chunk.data++; chunk.len--; }
  }
  if ((parser->options & (AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS | AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS)) && node_content->size > chunk.len) {
    for (bufsize_t i = 0; i < node_content->size - chunk.len; i++) {
      if (node_content->ptr[i] == '\n' || (node_content->ptr[i] == '\r' && (i + 1 == node_content->size - chunk.len || node_content->ptr[i + 1] != '\n'))) b->start_line++;
    }
    b->start_column = 1;
  }
  bufsize_t dropped = node_content->size - chunk.len;
  if (dropped && b->math_line_count) {
    size_t out = 0;
    for (size_t i = 0; i < b->math_line_count; i++) {
      aimarkdown_cmark_math_line row = b->math_lines[i];
      bufsize_t next = i + 1 < b->math_line_count ? b->math_lines[i + 1].offset : node_content->size;
      if (next <= dropped) { aimarkdown_cmark_chunk_free(parser->mem, &row.prefix); continue; }
      if (row.offset < dropped) {
        row.visual_column = aimarkdown_cmark_reference_column(row.visual_column, node_content->ptr + row.offset, dropped - row.offset);
        row.prefix_visual_column = row.visual_column;
        row.column += dropped - row.offset;
        row.offset = 0;
        aimarkdown_cmark_chunk_free(parser->mem, &row.prefix);
        row.prefix = aimarkdown_cmark_chunk_literal("");
      } else row.offset -= dropped;
      b->math_lines[out++] = row;
    }
    b->math_line_count = out;
    if ((parser->options & AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS) && out) {
      b->start_line = b->math_lines[0].line; b->start_column = b->math_lines[0].column;
    }
  }
  if (dropped) b->reference_cached_valid = false;
  aimarkdown_cmark_strbuf_drop(node_content, dropped);
  return !is_blank(&b->content, 0);
}

// Table conversion must preserve completed reference definitions before changing paragraph type.
bool aimarkdown_cmark_resolve_reference_definitions(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *node) {
  return resolve_reference_link_definitions(parser, node);
}

static aimarkdown_cmark_node *finalize(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *b) {
  bufsize_t pos;
  aimarkdown_cmark_node *item;
  aimarkdown_cmark_node *subitem;
  aimarkdown_cmark_node *parent;
  bool has_content;

  parent = b->parent;
  assert(b->flags &
         AIMARKDOWN_CMARK_NODE__OPEN); // shouldn't call finalize on closed blocks
  b->flags &= ~AIMARKDOWN_CMARK_NODE__OPEN;
  aimarkdown_cmark_list_spread_close(parser, b);

  if (S_type(b) == AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION &&
      (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS)) {
    // S_process_line has recorded the actual boundary of this definition's last nonblank content line.
  } else if (parser->curline.size == 0) {
    // end of input - line number has not been incremented
    b->end_line = parser->line_number;
    b->end_column = parser->last_line_length;
  } else if (S_type(b) == AIMARKDOWN_CMARK_NODE_DOCUMENT ||
             (S_type(b) == AIMARKDOWN_CMARK_NODE_CODE_BLOCK && b->as.code.fenced) ||
             (S_type(b) == AIMARKDOWN_CMARK_NODE_MATH_BLOCK && b->math_closing_fence) ||
             (S_type(b) == AIMARKDOWN_CMARK_NODE_HEADING && b->as.heading.setext) ||
             (parser->line_number - 1 < b->end_line)) {
    b->end_line = parser->line_number;
    b->end_column = parser->curline.size;
    if (b->end_column && parser->curline.ptr[b->end_column - 1] == '\n')
      b->end_column -= 1;
    if (b->end_column && parser->curline.ptr[b->end_column - 1] == '\r')
      b->end_column -= 1;
  } else {
    b->end_line = parser->line_number - 1;
    b->end_column = parser->last_line_length;
  }

  if ((b->type == AIMARKDOWN_CMARK_NODE_DEFINITION_LIST || b->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION) && parser->curline.size > 0) {
    b->end_line = parser->line_number;
    b->end_column = parser->definition_line_offset;
  } else if ((b->type == AIMARKDOWN_CMARK_NODE_DEFINITION_LIST || b->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION) && !parser->definition_synthetic_ending) {
    b->end_line = parser->line_number + 1;
    b->end_column = 0;
  }
  aimarkdown_cmark_strbuf *node_content = &b->content;

  if (S_type(b) == AIMARKDOWN_CMARK_NODE_MATH_BLOCK && parser->curline.size == 0 &&
      !b->math_closing_fence && parser->math_last_line_ending) {
    b->end_line = parser->line_number + 1;
    b->end_column = 0; // exclusive EOF is column 1 of the following line
  }

  if (S_type(b) == AIMARKDOWN_CMARK_NODE_MATH_BLOCK && !b->math_closing_fence) {
    // A following list item/list is a non-lazy container boundary. The newline
    // belongs to math, but none of that next line's quote/list prefixes do.
    for (aimarkdown_cmark_node *owner = b->parent; owner; owner = owner->parent) {
      aimarkdown_cmark_node *next = owner->next;
      if (next && next->start_line == parser->line_number &&
          ((owner->type == AIMARKDOWN_CMARK_NODE_ITEM && next->type == AIMARKDOWN_CMARK_NODE_ITEM) ||
           (owner->type == AIMARKDOWN_CMARK_NODE_LIST && next->type == AIMARKDOWN_CMARK_NODE_LIST))) {
        b->end_line = parser->line_number; b->end_column = 0; break;
      }
    }
  }

  switch (S_type(b)) {
  case AIMARKDOWN_CMARK_NODE_PARAGRAPH:
  {
    has_content = b->definition.no_reference || resolve_reference_link_definitions(parser, b);
    if (!has_content && (parser->options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) == 0) {
      // remove blank node (former reference def)
      aimarkdown_cmark_node_free(b);
    }
    break;
  }

  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK: {
    // First content line is the meta string. Keep trailing spaces: unlike code info,
    // math meta is not trimmed by mdast-util-math.
    for (pos = 0; pos < node_content->size && !S_is_line_end_char(node_content->ptr[pos]); pos++) {}
    aimarkdown_cmark_strbuf meta = AIMARKDOWN_CMARK_BUF_INIT(parser->mem);
    aimarkdown_cmark_decode_string_token(&meta, node_content->ptr, pos);
    b->as.code.info = aimarkdown_cmark_chunk_buf_detach(&meta);
    if (pos < node_content->size && node_content->ptr[pos] == '\r') pos++;
    if (pos < node_content->size && node_content->ptr[pos] == '\n') pos++;
    aimarkdown_cmark_strbuf_drop(node_content, pos);
    // A container ending before an ordinary flow line does not give that
    // boundary's line ending to mathFlow. A new document container (a list
    // item, list, or block quote) does. Keep EOF and explicit fences distinct,
    // and remove exactly one boundary EOL, not all trailing blank lines or
    // whitespace. Source coordinates above still describe the original bytes.
    bool container_prefix = false;
    if (!b->math_closing_fence && parser->curline.size > 0) {
      for (aimarkdown_cmark_node *owner = b->parent; owner; owner = owner->parent) {
        aimarkdown_cmark_node *next = owner->next;
        if (next && next->start_line == parser->line_number &&
            (next->type == AIMARKDOWN_CMARK_NODE_ITEM || next->type == AIMARKDOWN_CMARK_NODE_LIST ||
             next->type == AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE)) {
          container_prefix = true;
          break;
        }
      }
      if (!container_prefix) {
        if (node_content->size && node_content->ptr[node_content->size - 1] == '\n') {
          aimarkdown_cmark_strbuf_truncate(node_content, node_content->size - 1);
          if (node_content->size && node_content->ptr[node_content->size - 1] == '\r')
            aimarkdown_cmark_strbuf_truncate(node_content, node_content->size - 1);
        } else if (node_content->size && node_content->ptr[node_content->size - 1] == '\r') {
          aimarkdown_cmark_strbuf_truncate(node_content, node_content->size - 1);
        }
      }
    }
    // Math value excludes only its last source line ending, retaining internal raw EOLs.
    if (node_content->size && node_content->ptr[node_content->size - 1] == '\n') {
      aimarkdown_cmark_strbuf_truncate(node_content, node_content->size - 1);
      if (node_content->size && node_content->ptr[node_content->size - 1] == '\r') aimarkdown_cmark_strbuf_truncate(node_content, node_content->size - 1);
    } else if (node_content->size && node_content->ptr[node_content->size - 1] == '\r') aimarkdown_cmark_strbuf_truncate(node_content, node_content->size - 1);
    b->as.code.literal = aimarkdown_cmark_chunk_buf_detach(node_content);
    break;
  }

  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    if (b->code_metadata && b->code_metadata->list_boundary) {
      bool container_prefix = false;
      /* New document containers contribute the boundary EOL to codeFlow;
       * ordinary flow exits before it. The successor AST is now available. */
      for (aimarkdown_cmark_node *owner = b->parent; owner; owner = owner->parent) {
        aimarkdown_cmark_node *next = owner->next;
        if (next && next->start_line == parser->line_number &&
            (next->type == AIMARKDOWN_CMARK_NODE_ITEM || next->type == AIMARKDOWN_CMARK_NODE_LIST ||
             next->type == AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE)) {
          container_prefix = true;
          break;
        }
      }
      if (!container_prefix) b->code_metadata->boundary_trim = true;
    }
    aimarkdown_cmark_code_metadata_finish(b);
    if (!b->as.code.fenced) { // indented code
      remove_trailing_blank_lines(node_content);
      aimarkdown_cmark_strbuf_putc(node_content, '\n');
    } else {
      // first line of contents becomes info
      for (pos = 0; pos < node_content->size; ++pos) {
        if (S_is_line_end_char(node_content->ptr[pos]))
          break;
      }
      assert(pos < node_content->size);

      aimarkdown_cmark_strbuf tmp = AIMARKDOWN_CMARK_BUF_INIT(parser->mem);
      if (parser->options & AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES) {
        bufsize_t start = 0, end = pos;
        while (start < end && (node_content->ptr[start] == ' ' || node_content->ptr[start] == '\t')) start++;
        while (end > start && (node_content->ptr[end - 1] == ' ' || node_content->ptr[end - 1] == '\t')) end--;
        aimarkdown_cmark_decode_string_token(&tmp, node_content->ptr + start, end - start);
      } else {
        aimarkdown_houdini_unescape_html_f(&tmp, node_content->ptr, pos);
        aimarkdown_cmark_strbuf_trim(&tmp);
        aimarkdown_cmark_strbuf_unescape(&tmp);
      }
      b->as.code.info = aimarkdown_cmark_chunk_buf_detach(&tmp);

      if (node_content->ptr[pos] == '\r')
        pos += 1;
      if (node_content->ptr[pos] == '\n')
        pos += 1;
      aimarkdown_cmark_strbuf_drop(node_content, pos);
    }
    b->as.code.literal = aimarkdown_cmark_chunk_buf_detach(node_content);
    break;

  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
    b->as.literal = aimarkdown_cmark_chunk_buf_detach(node_content);
    break;

  case AIMARKDOWN_CMARK_NODE_LIST:      // determine tight/loose status
    b->as.list.tight = true; // tight by default
    item = b->first_child;

    while (item) {
      // check for non-final non-empty list item ending with blank line:
      if (S_last_line_blank(item) && item->next) {
        b->as.list.tight = false;
        break;
      }
      // recurse into children of list item, to see if there are
      // spaces between them:
      subitem = item->first_child;
      while (subitem) {
        if ((item->next || subitem->next) &&
            S_ends_with_blank_line(subitem)) {
          b->as.list.tight = false;
          break;
        }
        subitem = subitem->next;
      }
      if (!(b->as.list.tight)) {
        break;
      }
      item = item->next;
    }

    break;

  default:
    break;
  }

  return parent;
}

// Add a node as child of another.  Return pointer to child.
static aimarkdown_cmark_node *add_child(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *parent,
                             aimarkdown_cmark_node_type block_type, int start_column) {
  assert(parent);

  // if 'parent' isn't the kind of node that can accept this child,
  // then back up til we hit a node that can.
  while (!aimarkdown_cmark_node_can_contain_type(parent, block_type)) {
    parent = finalize(parser, parent);
  }

  aimarkdown_cmark_node *child =
      make_block(parser->mem, block_type, parser->line_number, start_column);
  child->parent = parent;

  if (parent->last_child) {
    parent->last_child->next = child;
    child->prev = parent->last_child;
  } else {
    parent->first_child = child;
    child->prev = NULL;
  }
  parent->last_child = child;
  return child;
}

void aimarkdown_cmark_manage_extensions_special_characters(aimarkdown_cmark_parser *parser, int add) {
  aimarkdown_cmark_llist *tmp_ext;

  for (tmp_ext = parser->inline_syntax_extensions; tmp_ext; tmp_ext=tmp_ext->next) {
    aimarkdown_cmark_syntax_extension *ext = (aimarkdown_cmark_syntax_extension *) tmp_ext->data;
    aimarkdown_cmark_llist *tmp_char;
    for (tmp_char = ext->special_inline_chars; tmp_char; tmp_char=tmp_char->next) {
      unsigned char c = (unsigned char)(size_t)tmp_char->data;
      if (add)
        aimarkdown_cmark_inlines_add_special_character(parser, c, ext->emphasis);
      else
        aimarkdown_cmark_inlines_remove_special_character(parser, c, ext->emphasis);
    }
  }
}

// Walk through node and all children, recursively, parsing
// string content into inline content where appropriate.
static void process_inlines(aimarkdown_cmark_parser *parser,
                            aimarkdown_cmark_map *refmap, int options) {
  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(parser->root);
  aimarkdown_cmark_node *cur;
  aimarkdown_cmark_event_type ev_type;

  aimarkdown_cmark_manage_extensions_special_characters(parser, true);

  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    if (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER) {
      if (contains_inlines(cur)) {
        aimarkdown_cmark_parse_inlines(parser, cur, refmap, options);
        if (cur->definition.hardbreak) {
          aimarkdown_cmark_node *linebreak = aimarkdown_cmark_node_new_with_mem(AIMARKDOWN_CMARK_NODE_LINEBREAK, parser->mem);
          linebreak->start_line = linebreak->end_line = cur->end_line;
          linebreak->start_column = linebreak->end_column = cur->end_column;
          aimarkdown_cmark_node_append_child(cur, linebreak);
        }
      }
    }
  }

  aimarkdown_cmark_manage_extensions_special_characters(parser, false);

  aimarkdown_cmark_iter_free(iter);
}

static int sort_footnote_by_ix(const void *_a, const void *_b) {
  aimarkdown_cmark_footnote *a = *(aimarkdown_cmark_footnote **)_a;
  aimarkdown_cmark_footnote *b = *(aimarkdown_cmark_footnote **)_b;
  return (int)a->ix - (int)b->ix;
}

static void process_footnotes(aimarkdown_cmark_parser *parser) {
  // * Collect definitions in a map.
  // * Iterate the references in the document in order, assigning indices to
  //   definitions in the order they're seen.
  // * Write out the footnotes at the bottom of the document in index order.

  aimarkdown_cmark_map *map = aimarkdown_cmark_footnote_map_new(parser->mem);

  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(parser->root);
  aimarkdown_cmark_node *cur;
  aimarkdown_cmark_event_type ev_type;

  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    if (ev_type == AIMARKDOWN_CMARK_EVENT_EXIT && cur->type == AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION) {
      aimarkdown_cmark_footnote_create(map, cur);
    }
  }

  aimarkdown_cmark_iter_free(iter);
  iter = aimarkdown_cmark_iter_new(parser->root);
  unsigned int ix = 0;

  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    if (ev_type == AIMARKDOWN_CMARK_EVENT_EXIT && cur->type == AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE) {
      aimarkdown_cmark_footnote *footnote = (aimarkdown_cmark_footnote *)aimarkdown_cmark_map_lookup(map, &cur->as.literal);
      if (footnote) {
        if (!footnote->ix)
          footnote->ix = ++ix;

        // store a reference to this footnote reference's footnote definition
        // this is used by renderers when generating label ids
        cur->parent_footnote_def = footnote->node;

        // keep track of a) count of how many times this footnote def has been
        // referenced, and b) which reference index this footnote ref is at.
        // this is used by renderers when generating links and backreferences.
        cur->footnote.ref_ix = ++footnote->node->footnote.def_count;

        char n[32];
        snprintf(n, sizeof(n), "%d", footnote->ix);
        aimarkdown_cmark_chunk_free(parser->mem, &cur->as.literal);
        aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(parser->mem);
        aimarkdown_cmark_strbuf_puts(&buf, n);

        cur->as.literal = aimarkdown_cmark_chunk_buf_detach(&buf);
      } else {
        aimarkdown_cmark_node *text = (aimarkdown_cmark_node *)parser->mem->calloc(1, sizeof(*text));
        aimarkdown_cmark_strbuf_init(parser->mem, &text->content, 0);
        text->type = (uint16_t) AIMARKDOWN_CMARK_NODE_TEXT;

        aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(parser->mem);
        aimarkdown_cmark_strbuf_puts(&buf, "[^");
        aimarkdown_cmark_strbuf_put(&buf, cur->as.literal.data, cur->as.literal.len);
        aimarkdown_cmark_strbuf_putc(&buf, ']');

        text->as.literal = aimarkdown_cmark_chunk_buf_detach(&buf);
        aimarkdown_cmark_node_insert_after(cur, text);
        aimarkdown_cmark_node_free(cur);
      }
    }
  }

  aimarkdown_cmark_iter_free(iter);

  if (map->sorted) {
    qsort(map->sorted, map->size, sizeof(aimarkdown_cmark_map_entry *), sort_footnote_by_ix);
    for (unsigned int i = 0; i < map->size; ++i) {
      aimarkdown_cmark_footnote *footnote = (aimarkdown_cmark_footnote *)map->sorted[i];
      if (!footnote->ix) {
        aimarkdown_cmark_node_unlink(footnote->node);
        continue;
      }
      aimarkdown_cmark_node_append_child(parser->root, footnote->node);
      footnote->node = NULL;
    }
  }

  aimarkdown_cmark_unlink_footnotes_map(map);
  aimarkdown_cmark_map_free(map);
}

// Attempts to parse a list item marker (bullet or enumerated).
// On success, returns length of the marker, and populates
// data with the details.  On failure, returns 0.
static bufsize_t parse_list_marker(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input,
                                   bufsize_t pos, bool interrupts_paragraph,
                                   aimarkdown_cmark_list **dataptr) {
  unsigned char c;
  bufsize_t startpos;
  aimarkdown_cmark_list *data;
  bufsize_t i;

  startpos = pos;
  c = peek_at(input, pos);

  if (c == '*' || c == '-' || c == '+') {
    pos++;
    if (!aimarkdown_cmark_isspace(peek_at(input, pos))) {
      return 0;
    }

    if (interrupts_paragraph) {
      i = pos;
      // require non-blank content after list marker:
      while (S_is_space_or_tab(peek_at(input, i))) {
        i++;
      }
      if (peek_at(input, i) == '\n') {
        return 0;
      }
    }

    data = (aimarkdown_cmark_list *)mem->calloc(1, sizeof(*data));
    data->marker_offset = 0; // will be adjusted later
    data->list_type = AIMARKDOWN_CMARK_BULLET_LIST;
    data->bullet_char = c;
    data->start = 0;
    data->delimiter = AIMARKDOWN_CMARK_NO_DELIM;
    data->tight = false;
  } else if (aimarkdown_cmark_isdigit(c)) {
    int start = 0;
    int digits = 0;

    do {
      start = (10 * start) + (peek_at(input, pos) - '0');
      pos++;
      digits++;
      // We limit to 9 digits to avoid overflow,
      // assuming max int is 2^31 - 1
      // This also seems to be the limit for 'start' in some browsers.
    } while (digits < 9 && aimarkdown_cmark_isdigit(peek_at(input, pos)));

    if (interrupts_paragraph && start != 1) {
      return 0;
    }
    c = peek_at(input, pos);
    if (c == '.' || c == ')') {
      pos++;
      if (!aimarkdown_cmark_isspace(peek_at(input, pos))) {
        return 0;
      }
      if (interrupts_paragraph) {
        // require non-blank content after list marker:
        i = pos;
        while (S_is_space_or_tab(peek_at(input, i))) {
          i++;
        }
        if (S_is_line_end_char(peek_at(input, i))) {
          return 0;
        }
      }

      data = (aimarkdown_cmark_list *)mem->calloc(1, sizeof(*data));
      data->marker_offset = 0; // will be adjusted later
      data->list_type = AIMARKDOWN_CMARK_ORDERED_LIST;
      data->bullet_char = 0;
      data->start = start;
      data->delimiter = (c == '.' ? AIMARKDOWN_CMARK_PERIOD_DELIM : AIMARKDOWN_CMARK_PAREN_DELIM);
      data->tight = false;
    } else {
      return 0;
    }
  } else {
    return 0;
  }

  *dataptr = data;
  return (pos - startpos);
}

// Return 1 if list item belongs in list, else 0.
static int lists_match(aimarkdown_cmark_list *list_data, aimarkdown_cmark_list *item_data) {
  return (list_data->list_type == item_data->list_type &&
          list_data->delimiter == item_data->delimiter &&
          // list_data->marker_offset == item_data.marker_offset &&
          list_data->bullet_char == item_data->bullet_char);
}

static aimarkdown_cmark_node *finalize_document(aimarkdown_cmark_parser *parser) {
  while (parser->current != parser->root) {
    parser->current = finalize(parser, parser->current);
  }

  finalize(parser, parser->root);

  // Limit total size of extra content created from reference links to
  // document size to avoid superlinear growth. Always allow 100KB.
  if (parser->total_size > 100000)
    parser->refmap->max_ref_size = parser->total_size;
  else
    parser->refmap->max_ref_size = 100000;

  if ((parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES) &&
      (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS)) {
    // Collect all definitions after block parsing so references can precede definitions.
    aimarkdown_cmark_map *map = aimarkdown_cmark_footnote_map_new(parser->mem);
    aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(parser->root);
    aimarkdown_cmark_event_type event;
    while ((event = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
      aimarkdown_cmark_node *node = aimarkdown_cmark_iter_get_node(iter);
      if (event == AIMARKDOWN_CMARK_EVENT_ENTER &&
          node->type == AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION)
        aimarkdown_cmark_footnote_create(map, node);
    }
    aimarkdown_cmark_iter_free(iter);
    parser->footnote_map = map;
    process_inlines(parser, parser->refmap, parser->options);
    parser->footnote_map = NULL;
    // sorted is deduplicated; every entry in the full refs chain only borrows tree nodes.
    for (aimarkdown_cmark_map_entry *entry = map->refs; entry; entry = entry->next)
      ((aimarkdown_cmark_footnote *)entry)->node = NULL;
    aimarkdown_cmark_map_free(map);
  } else {
    process_inlines(parser, parser->refmap, parser->options);
    if (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES)
      process_footnotes(parser);
  }

  return parser->root;
}

aimarkdown_cmark_node *aimarkdown_cmark_parse_file(FILE *f, int options) {
  unsigned char buffer[4096];
  aimarkdown_cmark_parser *parser = aimarkdown_cmark_parser_new(options);
  size_t bytes;
  aimarkdown_cmark_node *document;

  while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    bool eof = bytes < sizeof(buffer);
    S_parser_feed(parser, buffer, bytes, eof);
    if (eof) {
      break;
    }
  }

  document = aimarkdown_cmark_parser_finish(parser);
  aimarkdown_cmark_parser_free(parser);
  return document;
}

aimarkdown_cmark_node *aimarkdown_cmark_parse_document(const char *buffer, size_t len, int options) {
  aimarkdown_cmark_parser *parser = aimarkdown_cmark_parser_new(options);
  aimarkdown_cmark_node *document;

  S_parser_feed(parser, (const unsigned char *)buffer, len, true);

  document = aimarkdown_cmark_parser_finish(parser);
  aimarkdown_cmark_parser_free(parser);
  return document;
}

void aimarkdown_cmark_parser_feed(aimarkdown_cmark_parser *parser, const char *buffer, size_t len) {
  S_parser_feed(parser, (const unsigned char *)buffer, len, false);
}

void aimarkdown_cmark_parser_feed_reentrant(aimarkdown_cmark_parser *parser, const char *buffer, size_t len) {
  aimarkdown_cmark_strbuf saved_linebuf;

  aimarkdown_cmark_strbuf_init(parser->mem, &saved_linebuf, 0);
  aimarkdown_cmark_strbuf_puts(&saved_linebuf, aimarkdown_cmark_strbuf_cstr(&parser->linebuf));
  aimarkdown_cmark_strbuf_clear(&parser->linebuf);

  S_parser_feed(parser, (const unsigned char *)buffer, len, true);

  aimarkdown_cmark_strbuf_sets(&parser->linebuf, aimarkdown_cmark_strbuf_cstr(&saved_linebuf));
  aimarkdown_cmark_strbuf_free(&saved_linebuf);
}

static void S_parser_feed(aimarkdown_cmark_parser *parser, const unsigned char *buffer,
                          size_t len, bool eof) {
  const unsigned char *end = buffer + len;
  static const uint8_t repl[] = {239, 191, 189};
  bool preserveWhitespace = parser->options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE;

  if (len > UINT_MAX - parser->total_size)
    parser->total_size = UINT_MAX;
  else
    parser->total_size += len;

  if (parser->last_buffer_ended_with_cr && *buffer == '\n') {
    // skip NL if last buffer ended with CR ; see #117
    aimarkdown_cmark_node *target = parser->math_eol_target;
    if (target && target->code_metadata && target->code_metadata->source.size &&
        target->code_metadata->source.ptr[target->code_metadata->source.size - 1] == '\r')
      { aimarkdown_cmark_strbuf_putc(&target->code_metadata->source, '\n');
        if (target->code_metadata->kind == 2) aimarkdown_cmark_strbuf_putc(&target->code_metadata->pending, '\n'); }
    if (target && target->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK) aimarkdown_cmark_strbuf_putc(&target->content, '\n');
    else if (target && target->math_line_count) target->math_lines[target->math_line_count - 1].ending = 0x0d0a;
    buffer++;
  }
  parser->last_buffer_ended_with_cr = false;
  while (buffer < end) {
    const unsigned char *eol;
    bufsize_t chunk_len;
    bool process = false;
    for (eol = buffer; eol < end; ++eol) {
      if (S_is_line_end_char(*eol)) {
        process = true;
        break;
      }
      if (*eol == '\0' && eol < end) {
        break;
      }
    }
    if (eol >= end && eof) {
      process = true;
    }

    chunk_len = (bufsize_t)(eol - buffer);
    if (process) {
      parser->math_last_line_ending = eol < end && S_is_line_end_char(*eol);
      parser->math_current_ending = !parser->math_last_line_ending ? 0 : *eol == '\n' ? 10 : eol + 1 < end && eol[1] == '\n' ? 0x0d0a : 13;
      if (parser->linebuf.size > 0) {
        aimarkdown_cmark_strbuf_put(&parser->linebuf, buffer, chunk_len);
        parser->definition_synthetic_ending = eol == end;
        S_process_line(parser, parser->linebuf.ptr, parser->linebuf.size, !preserveWhitespace || !eof || eol < end);
        aimarkdown_cmark_strbuf_clear(&parser->linebuf);
      } else {
        parser->definition_synthetic_ending = eol == end;
        S_process_line(parser, buffer, chunk_len, !preserveWhitespace || !eof || eol < end);
      }
    } else {
      if (eol < end && *eol == '\0') {
        // omit NULL byte
        aimarkdown_cmark_strbuf_put(&parser->linebuf, buffer, chunk_len);
        // add replacement character
        aimarkdown_cmark_strbuf_put(&parser->linebuf, repl, 3);
      } else {
        aimarkdown_cmark_strbuf_put(&parser->linebuf, buffer, chunk_len);
      }
    }

    buffer += chunk_len;
    if (buffer < end) {
      if (*buffer == '\0') {
        // skip over NULL
        buffer++;
      } else {
        // skip over line ending characters
        if (*buffer == '\r') {
          buffer++;
          if (buffer == end)
            parser->last_buffer_ended_with_cr = true;
        }
        if (buffer < end && *buffer == '\n')
          buffer++;
      }
    }
  }
}

static void chop_trailing_hashtags(aimarkdown_cmark_chunk *ch) {
  bufsize_t n, orig_n;

  aimarkdown_cmark_chunk_rtrim(ch);
  orig_n = n = ch->len - 1;

  // if string ends in space followed by #s, remove these:
  while (n >= 0 && peek_at(ch, n) == '#')
    n--;

  // Check for a space before the final #s:
  if (n != orig_n && n >= 0 && S_is_space_or_tab(peek_at(ch, n))) {
    ch->len = n;
    aimarkdown_cmark_chunk_rtrim(ch);
  }
}

// Check for thematic break.  On failure, return 0 and update
// thematic_break_kill_pos with the index at which the
// parse fails.  On success, return length of match.
// "...three or more hyphens, asterisks,
// or underscores on a line by themselves. If you wish, you may use
// spaces between the hyphens or asterisks."
static int S_scan_thematic_break(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                                 bufsize_t offset) {
  bufsize_t i;
  char c;
  char nextc = '\0';
  int count;
  i = offset;
  c = peek_at(input, i);
  if (!(c == '*' || c == '_' || c == '-')) {
    parser->thematic_break_kill_pos = i;
    return 0;
  }
  count = 1;
  while ((nextc = peek_at(input, ++i))) {
    if (nextc == c) {
      count++;
    } else if (nextc != ' ' && nextc != '\t') {
      break;
    }
  }
  if (count >= 3 && (nextc == '\r' || nextc == '\n')) {
    return (i - offset) + 1;
  } else {
    parser->thematic_break_kill_pos = i;
    return 0;
  }
}

// Find first nonspace character from current offset, setting
// parser->first_nonspace, parser->first_nonspace_column,
// parser->indent, and parser->blank. Does not advance parser->offset.
static void S_find_first_nonspace(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input) {
  char c;
  int chars_to_tab = TAB_STOP - (parser->column % TAB_STOP);

  if (parser->first_nonspace <= parser->offset) {
    parser->first_nonspace = parser->offset;
    parser->first_nonspace_column = parser->column;
    while ((c = peek_at(input, parser->first_nonspace))) {
      if (c == ' ') {
        parser->first_nonspace += 1;
        parser->first_nonspace_column += 1;
        chars_to_tab = chars_to_tab - 1;
        if (chars_to_tab == 0) {
          chars_to_tab = TAB_STOP;
        }
      } else if (c == '\t') {
        parser->first_nonspace += 1;
        parser->first_nonspace_column += chars_to_tab;
        chars_to_tab = TAB_STOP;
      } else {
        break;
      }
    }
  }

  parser->indent = parser->first_nonspace_column - parser->column;
  parser->blank = S_is_line_end_char(peek_at(input, parser->first_nonspace));
}

// Advance parser->offset and parser->column.  parser->offset is the
// byte position in input; parser->column is a virtual column number
// that takes into account tabs. (Multibyte characters are not taken
// into account, because the Markdown line prefixes we are interested in
// analyzing are entirely ASCII.)  The count parameter indicates
// how far to advance the offset.  If columns is true, then count
// indicates a number of columns; otherwise, a number of bytes.
// If advancing a certain number of columns partially consumes
// a tab character, parser->partially_consumed_tab is set to true.
static void S_advance_offset(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                             bufsize_t count, bool columns) {
  char c;
  int chars_to_tab;
  int chars_to_advance;
  while (count > 0 && (c = peek_at(input, parser->offset))) {
    if (c == '\t') {
      chars_to_tab = TAB_STOP - (parser->column % TAB_STOP);
      if (columns) {
        parser->partially_consumed_tab = chars_to_tab > count;
        chars_to_advance = MIN(count, chars_to_tab);
        parser->column += chars_to_advance;
        parser->offset += (parser->partially_consumed_tab ? 0 : 1);
        count -= chars_to_advance;
      } else {
        parser->partially_consumed_tab = false;
        parser->column += chars_to_tab;
        parser->offset += 1;
        count -= 1;
      }
    } else {
      parser->partially_consumed_tab = false;
      parser->offset += 1;
      parser->column += 1; // assume ascii; block starts are ascii
      count -= 1;
    }
  }
}

static bool S_last_child_is_open(aimarkdown_cmark_node *container) {
  return container->last_child &&
         (container->last_child->flags & AIMARKDOWN_CMARK_NODE__OPEN);
}

static bool parse_block_quote_prefix(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input) {
  bool res = false;
  bufsize_t matched = 0;

  matched =
      parser->indent <= 3 && peek_at(input, parser->first_nonspace) == '>';
  if (matched) {

    S_advance_offset(parser, input, parser->indent + 1, true);

    if (S_is_space_or_tab(peek_at(input, parser->offset))) {
      S_advance_offset(parser, input, 1, true);
    }

    res = true;
  }
  return res;
}

static bool parse_footnote_definition_block_prefix(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                                                   aimarkdown_cmark_node *container) {
  /* GFM footnote blankLine is a non-consuming continuation. For an open
   * math leaf its whitespace remains math data, even below four columns.
   * Other leaves keep their existing prefix and metadata behavior. */
  if (parser->blank && parser->current->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK) return true;
  if (parser->indent >= 4) {
    if ((parser->options & AIMARKDOWN_CMARK_OPT_CODE_METADATA) && parser->blank && parser->code_footnote_blank_offset < 0) {
      parser->code_footnote_blank_offset = parser->offset;
      parser->code_footnote_blank_container = container;
      parser->code_footnote_blank_column = parser->column;
      parser->code_footnote_blank_partial = parser->partially_consumed_tab;
    }
    S_advance_offset(parser, input, 4, true);
    return true;
  } else if (input->len > 0 && (input->data[0] == '\n' || (input->data[0] == '\r' && input->data[1] == '\n'))) {
    return true;
  }

  return false;
}

static bool parse_node_item_prefix(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                                   aimarkdown_cmark_node *container) {
  bool res = false;

  if (parser->indent >=
      container->as.list.marker_offset + container->as.list.padding) {
    S_advance_offset(parser, input, container->as.list.marker_offset +
                                        container->as.list.padding,
                     true);
    res = true;
  } else if (parser->blank && container->first_child != NULL) {
    // if container->first_child is NULL, then the opening line
    // of the list item was blank after the list marker; in this
    // case, we are done with the list item.
    S_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                     false);
    res = true;
  }
  return res;
}

static bool parse_code_block_prefix(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                                    aimarkdown_cmark_node *container,
                                    bool *should_continue) {
  bool res = false;

  aimarkdown_cmark_list_spread_leaf_prefix(parser, container);
  if (!container->as.code.fenced) { // indented
    if (parser->indent >= CODE_INDENT) {
      aimarkdown_cmark_list_spread_indent(parser, container, true);
      if (container->code_metadata) container->code_metadata->indented_qualified = true;
      S_advance_offset(parser, input, CODE_INDENT, true);
      res = true;
    } else if (parser->blank) {
      aimarkdown_cmark_list_spread_indent(parser, container, false);
      if (container->code_metadata) container->code_metadata->indented_qualified = false;
      S_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                       false);
      res = true;
    }
  } else { // fenced
    bufsize_t matched = 0;

    if (parser->indent <= 3 && (peek_at(input, parser->first_nonspace) ==
                                container->as.code.fence_char)) {
      matched = scan_close_code_fence(input, parser->first_nonspace);
    }

    bufsize_t required = ((parser->options & AIMARKDOWN_CMARK_OPT_WEB_CODE_SYNTAX) &&
        container->as.code.web_fence_length > 0)
        ? container->as.code.web_fence_length : container->as.code.fence_length;
    if (matched >= required) {
      // closing fence - and since we're at
      // the end of a line, we can stop processing it:
      *should_continue = false;
      S_advance_offset(parser, input, matched, false);
      if (container->code_metadata) container->code_metadata->closed = true;
      aimarkdown_cmark_list_spread_leaf(parser, container, true);
      parser->current = finalize(parser, container);
    } else {
      // skip opt. spaces of fence parser->offset
      int i = container->as.code.fence_offset;

      while (i > 0 && S_is_space_or_tab(peek_at(input, parser->offset))) {
        S_advance_offset(parser, input, 1, true);
        i--;
      }
      res = true;
    }
  }

  return res;
}

static bufsize_t math_fence(aimarkdown_cmark_chunk *input, bufsize_t start, bool closing) {
  bufsize_t end = start;
  while (end < input->len && input->data[end] == '$') end++;
  bufsize_t count = end - start;
  if (count < 2) return 0;
  while (end < input->len && !S_is_line_end_char(input->data[end])) {
    if (closing ? !S_is_space_or_tab(input->data[end]) : input->data[end] == '$') return 0;
    end++;
  }
  return count;
}

static bool parse_math_prefix(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                              aimarkdown_cmark_node *container, bool *should_continue) {
  bufsize_t matched = parser->indent <= 3 ? math_fence(input, parser->first_nonspace, true) : 0;
  if (matched >= container->math_fence_length && matched > 0) {
    container->math_closing_fence = true; // finalize this line, not the previous line
    *should_continue = false;
    aimarkdown_cmark_list_spread_leaf(parser, container, true);
    S_advance_offset(parser, input, parser->first_nonspace + matched - parser->offset, false);
    parser->current = finalize(parser, container);
    return false;
  }
  int indent = container->as.code.fence_offset;
  while (indent > 0 && S_is_space_or_tab(peek_at(input, parser->offset))) {
    S_advance_offset(parser, input, 1, true);
    indent--;
  }
  return true;
}

static bool parse_html_block_prefix(aimarkdown_cmark_parser *parser,
                                    aimarkdown_cmark_node *container) {
  bool res = false;
  int html_block_type = container->as.html_block_type;

  assert(html_block_type >= 1 && html_block_type <= 7);
  switch (html_block_type) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    // these types of blocks can accept blanks
    res = true;
    break;
  case 6:
  case 7:
    res = !parser->blank;
    break;
  }

  return res;
}

static bool parse_extension_block(aimarkdown_cmark_parser *parser,
                                  aimarkdown_cmark_node *container,
                                  aimarkdown_cmark_chunk *input)
{
  bool res = false;

  if (container->extension->last_block_matches) {
    if (container->extension->last_block_matches(
        container->extension, parser, input->data, input->len, container))
      res = true;
  }

  return res;
}

/**
 * For each containing node, try to parse the associated line start.
 *
 * Will not close unmatched blocks, as we may have a lazy continuation
 * line -> http://spec.commonmark.org/0.24/#lazy-continuation-line
 *
 * Returns: The last matching node, or NULL
 */
static aimarkdown_cmark_node *check_open_blocks(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                                     bool *all_matched) {
  bool should_continue = true;
  *all_matched = false;
  aimarkdown_cmark_node *container = parser->root;
  aimarkdown_cmark_node_type cont_type;
    
    

  while (S_last_child_is_open(container)) {
    container = container->last_child;
    cont_type = S_type(container);

    S_find_first_nonspace(parser, input);

    if (container->extension) {
      if ((parser->options & AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS) &&
          strcmp(aimarkdown_cmark_node_get_type_string(container), "table") == 0 &&
          parser->indent < 4 && peek_at(input, parser->first_nonspace) == ':' &&
          (S_is_space_or_tab(peek_at(input, parser->first_nonspace + 1)) ||
           S_is_line_end_char(peek_at(input, parser->first_nonspace + 1)))) goto done;
      if (!parse_extension_block(parser, container, input))
        goto done;
      continue;
    }

    switch (cont_type) {
    case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
      if (!parse_block_quote_prefix(parser, input))
        goto done;
      aimarkdown_cmark_list_spread_prefix(parser, container);
      break;
    case AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION:
      aimarkdown_cmark_list_spread_unsupported(parser);
      if (parser->blank) {
        container->definition.blank_lines++;
        S_advance_offset(parser, input, MIN(parser->indent, container->definition.padding), true);
      } else if (container->definition.initial_blank && container->definition.blank_lines > 0) {
        parser->definition_lazy_code = true;
        goto done;
      } else if (parser->indent >= container->definition.padding) {
        S_advance_offset(parser, input, container->definition.padding, true);
        container->definition.blank_lines = 0;
        container->definition.initial_blank = false;
      } else {
        goto done;
      }
      break;
    case AIMARKDOWN_CMARK_NODE_ITEM:
      if (!parse_node_item_prefix(parser, input, container)) {
        /* A list flow boundary excludes its final line ending, just like the
         * footnote container boundary. EOF and explicit fences do not enter
         * this branch, so their trailing blank code lines remain intact. */
        if (parser->options & AIMARKDOWN_CMARK_OPT_CODE_METADATA) {
          for (aimarkdown_cmark_node *open = parser->current; open && open != container; open = open->parent)
            if (open->code_metadata && open->code_metadata->kind == 1)
              open->code_metadata->list_boundary = true;
        }
        goto done;
      }
      break;
    case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
      if (!parse_math_prefix(parser, input, container, &should_continue)) goto done;
      break;
    case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
      if (!parse_code_block_prefix(parser, input, container, &should_continue))
        goto done;
      break;
    case AIMARKDOWN_CMARK_NODE_HEADING:
      // a heading can never contain more than one line
      goto done;
    case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
      aimarkdown_cmark_list_spread_leaf_prefix(parser, container);
      if (!parse_html_block_prefix(parser, container))
        goto done;
      break;
    case AIMARKDOWN_CMARK_NODE_PARAGRAPH:
      if (parser->blank) {
        if ((parser->options & AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS) && parser->column > 0) {
          container->definition.flow_offset = container->content.size;
          container->definition.flow_line = parser->line_number;
          container->definition.flow_column = parser->offset + (parser->partially_consumed_tab ? 2 : 1);
        }
        goto done;
      }
      break;
		case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
      /* Web emits a footnote indent token only on non-blank continuation lines. */
      if (!parser->blank && parser->indent >= 4)
        aimarkdown_cmark_list_spread_prefix(parser, container);
      if (!parse_footnote_definition_block_prefix(parser, input, container)) {
        /* Web continues footnote definitions across every blank line; this
         * parser closes them on whitespace-only or CR blank lines. */
        if (parser->blank)
          aimarkdown_cmark_list_spread_unsupported(parser);
        if (parser->options & AIMARKDOWN_CMARK_OPT_CODE_METADATA) {
          for (aimarkdown_cmark_node *open = parser->current; open && open != container; open = open->parent)
            if (open->code_metadata && open->code_metadata->kind == 1) open->code_metadata->boundary_trim = true;
        }
        goto done;
      }
      break;
    default:
      break;
    }
  }

  *all_matched = true;

done:
  if (!*all_matched) {
    container = container->parent; // back up to last matching node
  }

  if (!should_continue) {
    container = NULL;
  }

  return container;
}

/* Web atxHeading and setextHeading tokens end at the physical end of their last
 * line, including trailing whitespace and closing sequences. */
static void S_web_heading_end(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *heading, aimarkdown_cmark_chunk *input) {
  if (!(parser->options & AIMARKDOWN_CMARK_OPT_WEB_POSITIONS))
    return;
  bufsize_t end = input->len;
  while (end > 0 && S_is_line_end_char(input->data[end - 1]))
    end--;
  aimarkdown_cmark_web_position_set_end(heading, parser->line_number, end + 1);
}

static void open_new_blocks(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node **container,
                            aimarkdown_cmark_chunk *input, bool all_matched) {
  bool indented;
  aimarkdown_cmark_list *data = NULL;
  bool maybe_lazy = S_type(parser->current) == AIMARKDOWN_CMARK_NODE_PARAGRAPH;
  aimarkdown_cmark_node_type cont_type = S_type(*container);
  bufsize_t matched = 0;
  int lev = 0;
  bool save_partially_consumed_tab;
  bool has_content;
  int save_offset;
  int save_column;
  size_t depth = 0;

  while (cont_type != AIMARKDOWN_CMARK_NODE_MATH_BLOCK &&
         cont_type != AIMARKDOWN_CMARK_NODE_CODE_BLOCK &&
         cont_type != AIMARKDOWN_CMARK_NODE_HTML_BLOCK) {
    depth++;
    S_find_first_nonspace(parser, input);
    indented = parser->indent >= CODE_INDENT;

    if (!indented && peek_at(input, parser->first_nonspace) == '>') {

      bufsize_t blockquote_startpos = parser->first_nonspace;

      S_advance_offset(parser, input,
                       parser->first_nonspace + 1 - parser->offset, false);
      // optional following character
      if (S_is_space_or_tab(peek_at(input, parser->offset))) {
        S_advance_offset(parser, input, 1, true);
      }
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE,
                             blockquote_startpos + 1);
      aimarkdown_cmark_list_spread_open(parser, *container);

    } else if (!indented && (matched = scan_atx_heading_start(
                                 input, parser->first_nonspace))) {
      bufsize_t hashpos;
      int level = 0;
      bufsize_t heading_startpos = parser->first_nonspace;

      S_advance_offset(parser, input,
                       parser->first_nonspace + matched - parser->offset,
                       false);
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_HEADING,
                             heading_startpos + 1);

      hashpos = aimarkdown_cmark_chunk_strchr(input, '#', parser->first_nonspace);

      while (peek_at(input, hashpos) == '#') {
        level++;
        hashpos++;
      }

      (*container)->as.heading.level = level;
      (*container)->as.heading.setext = false;
      (*container)->internal_offset = matched;
      aimarkdown_cmark_list_spread_leaf(parser, *container, false);
      if (parser->options & AIMARKDOWN_CMARK_OPT_WEB_POSITIONS) {
        aimarkdown_cmark_web_position_set_start(*container, parser->line_number, heading_startpos + 1);
        S_web_heading_end(parser, *container, input);
      }

    } else if (!indented && (parser->options & AIMARKDOWN_CMARK_OPT_MATH) &&
               (matched = math_fence(input, parser->first_nonspace, false))) {
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_MATH_BLOCK, parser->first_nonspace + 1);
      (*container)->math_fence_length = matched;
      (*container)->as.code.fenced = 1;
      (*container)->as.code.fence_offset = parser->first_nonspace - parser->offset;
      (*container)->as.code.info = aimarkdown_cmark_chunk_literal("");
      S_advance_offset(parser, input, parser->first_nonspace + matched - parser->offset, false);
      // factorySpace before meta accepts spaces and tabs, not arbitrary Unicode whitespace.
      while (S_is_space_or_tab(peek_at(input, parser->offset))) S_advance_offset(parser, input, 1, false);
    } else if (!indented && (matched = scan_open_code_fence(
                                 input, parser->first_nonspace))) {
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_CODE_BLOCK,
                             parser->first_nonspace + 1);
      (*container)->as.code.fenced = true;
      (*container)->as.code.fence_char = peek_at(input, parser->first_nonspace);
      (*container)->as.code.fence_length = (matched > 255) ? 255 : (uint8_t)matched;
      if (parser->options & AIMARKDOWN_CMARK_OPT_WEB_CODE_SYNTAX) (*container)->as.code.web_fence_length = matched;
      /* Metadata for a long opener requires the exact Web closing grammar.
       * Legacy mode still rejects metadata beyond its 255-character cap. */
      if ((parser->options & AIMARKDOWN_CMARK_OPT_CODE_METADATA) &&
          (matched <= 255 || (parser->options & AIMARKDOWN_CMARK_OPT_WEB_CODE_SYNTAX)))
        aimarkdown_cmark_code_metadata_start(*container, 1);
      (*container)->as.code.fence_offset =
          (int8_t)((parser->options & AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS) ? parser->indent : parser->first_nonspace - parser->offset);
      (*container)->as.code.info = aimarkdown_cmark_chunk_literal("");
      S_advance_offset(parser, input,
                       parser->first_nonspace + matched - parser->offset,
                       false);

    } else if (!indented && ((matched = scan_html_block_start(
                                  input, parser->first_nonspace)) ||
                             (cont_type != AIMARKDOWN_CMARK_NODE_PARAGRAPH &&
                              (matched = scan_html_block_start_7(
                                   input, parser->first_nonspace))))) {
      int web_html_start = parser->partially_consumed_tab ? 0 : parser->offset + 1;
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_HTML_BLOCK,
                             parser->first_nonspace + 1);
      (*container)->as.html_block_type = matched;
      /* Web htmlFlow includes the indentation after container prefixes. */
      if ((parser->options & AIMARKDOWN_CMARK_OPT_WEB_POSITIONS) && web_html_start)
        aimarkdown_cmark_web_position_set_start(*container, parser->line_number, web_html_start);
      // note, we don't adjust parser->offset because the tag is part of the
      // text
    } else if (!indented && cont_type == AIMARKDOWN_CMARK_NODE_PARAGRAPH &&
               (lev =
                    scan_setext_heading_line(input, parser->first_nonspace))) {
      // finalize paragraph, resolving reference links
      has_content = resolve_reference_link_definitions(parser, *container);

      if (has_content) {

        (*container)->type = (uint16_t)AIMARKDOWN_CMARK_NODE_HEADING;
        (*container)->as.heading.level = lev;
        (*container)->as.heading.setext = true;
        aimarkdown_cmark_list_spread_leaf(parser, *container, false);
        /* Starts where the first remaining content line starts. */
        if ((parser->options & AIMARKDOWN_CMARK_OPT_WEB_POSITIONS) && (*container)->math_line_count &&
            (*container)->math_lines[0].offset == 0) {
          aimarkdown_cmark_web_position_set_start(*container, (*container)->math_lines[0].line,
                                       (*container)->math_lines[0].column);
          S_web_heading_end(parser, *container, input);
        }
        S_advance_offset(parser, input, input->len - 1 - parser->offset, false);
      }
    } else if (!indented &&
               !(cont_type == AIMARKDOWN_CMARK_NODE_PARAGRAPH && !all_matched) &&
	       (parser->thematic_break_kill_pos <= parser->first_nonspace) &&
               (matched = S_scan_thematic_break(parser, input, parser->first_nonspace))) {
      // it's only now that we know the line is not part of a setext heading:
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK,
                             parser->first_nonspace + 1);
      aimarkdown_cmark_list_spread_leaf(parser, *container, false);
      S_advance_offset(parser, input, input->len - 1 - parser->offset, false);
    } else if (!indented &&
               (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES) &&
               depth < MAX_LIST_DEPTH &&
               (matched = scan_footnote_definition(input, parser->first_nonspace))) {
      aimarkdown_cmark_chunk c = aimarkdown_cmark_chunk_dup(input, parser->first_nonspace + 2, matched - 2);

      while (c.data[c.len - 1] != ']')
        --c.len;
      --c.len;

      aimarkdown_cmark_chunk_to_cstr(parser->mem, &c);

      S_advance_offset(parser, input, parser->first_nonspace + matched - parser->offset, false);
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION, parser->first_nonspace + matched + 1);
      if (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS)
        (*container)->start_column -= matched;
      (*container)->as.literal = c;

      (*container)->internal_offset = matched;
      aimarkdown_cmark_list_spread_open(parser, *container);
    } else if ((!indented || cont_type == AIMARKDOWN_CMARK_NODE_LIST) &&
	       parser->indent < 4 &&
               depth < MAX_LIST_DEPTH &&
               (matched = parse_list_marker(
                    parser->mem, input, parser->first_nonspace,
                    ((*container)->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH ||
                     ((*container)->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION &&
                      (*container)->start_line == parser->line_number && (*container)->definition.interrupt)), &data))) {

      // Note that we can have new list items starting with >= 4
      // spaces indent, as long as the list container is still open.
      int i = 0;

      // compute padding:
      S_advance_offset(parser, input,
                       parser->first_nonspace + matched - parser->offset,
                       false);

      save_partially_consumed_tab = parser->partially_consumed_tab;
      save_offset = parser->offset;
      save_column = parser->column;

      while (parser->column - save_column <= 5 &&
             S_is_space_or_tab(peek_at(input, parser->offset))) {
        S_advance_offset(parser, input, 1, true);
      }

      i = parser->column - save_column;
      /* micromark's listItemPrefix ends after the marker when the rest of the
       * line is blank, after one whitespace for five or more columns, and
       * after all whitespace otherwise; tabs leave the point unknown. */
      int web_prefix_end = 0;
      bool web_blank_after_marker = i < 1 || S_is_line_end_char(peek_at(input, parser->offset));
      bool web_prefix_tab = save_partially_consumed_tab;
      for (bufsize_t k = save_offset; k < parser->offset; k++)
        if (peek_at(input, k) == '\t') web_prefix_tab = true;
      if (!web_prefix_tab)
        web_prefix_end = web_blank_after_marker ? save_offset + 1
                         : i >= 5 ? save_offset + 2 : parser->offset + 1;
      if (i >= 5 || i < 1 ||
          // only spaces after list marker:
          S_is_line_end_char(peek_at(input, parser->offset))) {
        data->padding = matched + 1;
        parser->offset = save_offset;
        parser->column = save_column;
        parser->partially_consumed_tab = save_partially_consumed_tab;
        if (i > 0) {
          S_advance_offset(parser, input, 1, true);
        }
      } else {
        data->padding = matched + i;
      }

      // check container; if it's a list, see if this list item
      // can continue the list; otherwise, create a list container.

      data->marker_offset = parser->indent;

      if (cont_type != AIMARKDOWN_CMARK_NODE_LIST ||
          !lists_match(&((*container)->as.list), data)) {
        *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_LIST,
                               parser->first_nonspace + 1);

        memcpy(&((*container)->as.list), data, sizeof(*data));
        aimarkdown_cmark_list_spread_open(parser, *container);
      }

      // add the list item
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_ITEM,
                             parser->first_nonspace + 1);
      aimarkdown_cmark_list_spread_open(parser, *container);
      aimarkdown_cmark_list_spread_item_prefix(parser, *container, web_prefix_end);
      /* TODO: static */
      memcpy(&((*container)->as.list), data, sizeof(*data));
      parser->mem->free(data);
    } else if (indented && !maybe_lazy && !parser->blank) {
      /* Web codeIndented includes its indentation after container prefixes. */
      int web_code_start = parser->partially_consumed_tab ? 0 : parser->offset + 1;
      S_advance_offset(parser, input, CODE_INDENT, true);
      *container = add_child(parser, *container, AIMARKDOWN_CMARK_NODE_CODE_BLOCK,
                             parser->offset + 1);
      if ((parser->options & AIMARKDOWN_CMARK_OPT_WEB_POSITIONS) && web_code_start)
        aimarkdown_cmark_web_position_set_start(*container, parser->line_number, web_code_start);
      (*container)->definition.single_line = parser->definition_lazy_code;
      (*container)->as.code.fenced = false;
      if (parser->options & AIMARKDOWN_CMARK_OPT_CODE_METADATA) { aimarkdown_cmark_code_metadata_start(*container, 2); (*container)->code_metadata->indented_qualified = true; }
      (*container)->as.code.fence_char = 0;
      (*container)->as.code.fence_length = 0;
      (*container)->as.code.fence_offset = 0;
      (*container)->as.code.info = aimarkdown_cmark_chunk_literal("");
    } else {
      aimarkdown_cmark_llist *tmp;
      aimarkdown_cmark_node *new_container = NULL;

      for (tmp = parser->syntax_extensions; tmp; tmp=tmp->next) {
        aimarkdown_cmark_syntax_extension *ext = (aimarkdown_cmark_syntax_extension *) tmp->data;

        if (ext->try_opening_block) {
          new_container = ext->try_opening_block(
              ext, indented, parser, *container, input->data, input->len);

          if (new_container) {
            *container = new_container;
            if (parser->options & AIMARKDOWN_CMARK_OPT_LIST_SPREAD_METADATA) {
              const char *kind = aimarkdown_cmark_node_get_type_string(new_container);
              if (strcmp(kind, "table") == 0)
                aimarkdown_cmark_list_spread_leaf(parser, new_container, false);
              else if ((strcmp(kind, "table_row") == 0 || strcmp(kind, "table_header") == 0) &&
                       new_container->parent)
                aimarkdown_cmark_list_spread_leaf(parser, new_container->parent, false);
              else if (S_type(new_container) != AIMARKDOWN_CMARK_NODE_PARAGRAPH)
                aimarkdown_cmark_list_spread_unsupported(parser);
            }
            break;
          }
        }
      }

      if (!new_container) {
        break;
      }
    }

    if (S_type(*container) != AIMARKDOWN_CMARK_NODE_CODE_BLOCK) parser->definition_lazy_code = false;
    if (accepts_lines(S_type(*container))) {
      // if it's a line container, it can't contain other containers
      break;
    }

    cont_type = S_type(*container);
    maybe_lazy = false;
  }
}

static void add_text_to_container(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container,
                                  aimarkdown_cmark_node *last_matched_container,
                                  aimarkdown_cmark_chunk *input) {
  aimarkdown_cmark_node *tmp;
  // what remains at parser->offset is a text line.  add the text to the
  // appropriate container.

  S_find_first_nonspace(parser, input);

  if (parser->blank && container->last_child)
    S_set_last_line_blank(container->last_child, true);

  // block quote lines are never blank as they start with >
  // and we don't count blanks in fenced code for purposes of tight/loose
  // lists or breaking out of lists.  we also don't set last_line_blank
  // on an empty list item.
  const aimarkdown_cmark_node_type ctype = S_type(container);
  const bool last_line_blank =
      (parser->blank && ctype != AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE &&
       ctype != AIMARKDOWN_CMARK_NODE_HEADING && ctype != AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK &&
       !(ctype == AIMARKDOWN_CMARK_NODE_CODE_BLOCK && container->as.code.fenced) &&
       ctype != AIMARKDOWN_CMARK_NODE_MATH_BLOCK &&
       !(ctype == AIMARKDOWN_CMARK_NODE_ITEM && container->first_child == NULL &&
         container->start_line == parser->line_number));

  S_set_last_line_blank(container, last_line_blank);

  tmp = container;
  while (tmp->parent) {
    S_set_last_line_blank(tmp->parent, false);
    tmp = tmp->parent;
  }

  // If the last line processed belonged to a paragraph node,
  // and we didn't match all of the line prefixes for the open containers,
  // and we didn't start any new containers,
  // and the line isn't blank,
  // then treat this as a "lazy continuation line" and add it to
  // the open paragraph.
  if (parser->current != last_matched_container &&
      container == last_matched_container && (!parser->blank || (parser->options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE)) &&
      S_type(parser->current) == AIMARKDOWN_CMARK_NODE_PARAGRAPH) {
    if ((parser->options & AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS) && parser->current->definition.lazy_offset == 0) {
      parser->current->definition.lazy_offset = parser->current->content.size;
      parser->current->definition.lazy_line = parser->line_number;
      parser->current->definition.lazy_column = parser->offset + 1;
    }
    aimarkdown_cmark_list_spread_leaf(parser, parser->current, false);
    add_line(parser->current, input, parser);
  } else { // not a lazy continuation
    // Finalize any blocks that were not matched and set cur to container:
    while (parser->current != last_matched_container) {
      parser->current = finalize(parser, parser->current);
      assert(parser->current != NULL);
    }

    if (S_type(container) == AIMARKDOWN_CMARK_NODE_CODE_BLOCK || S_type(container) == AIMARKDOWN_CMARK_NODE_MATH_BLOCK) {
      aimarkdown_cmark_list_spread_leaf(parser, container, false);
      add_line(container, input, parser);
    } else if (S_type(container) == AIMARKDOWN_CMARK_NODE_HTML_BLOCK) {
      add_line(container, input, parser);

      int matches_end_condition;
      switch (container->as.html_block_type) {
      case 1:
        // </script>, </style>, </pre>
        matches_end_condition =
            scan_html_block_end_1(input, parser->first_nonspace);
        break;
      case 2:
        // -->
        matches_end_condition =
            scan_html_block_end_2(input, parser->first_nonspace);
        break;
      case 3:
        // ?>
        matches_end_condition =
            scan_html_block_end_3(input, parser->first_nonspace);
        break;
      case 4:
        // >
        matches_end_condition =
            scan_html_block_end_4(input, parser->first_nonspace);
        break;
      case 5:
        // ]]>
        matches_end_condition =
            scan_html_block_end_5(input, parser->first_nonspace);
        break;
      default:
        matches_end_condition = 0;
        break;
      }

      aimarkdown_cmark_list_spread_leaf(parser, container, matches_end_condition != 0);
      if (matches_end_condition) {
        container = finalize(parser, container);
        assert(parser->current != NULL);
      }
    } else if (parser->blank && (parser->options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) == 0) {
      // ??? do nothing
    } else if (accepts_lines(S_type(container))) {
      if (S_type(container) == AIMARKDOWN_CMARK_NODE_HEADING &&
          container->as.heading.setext == false) {
        chop_trailing_hashtags(input);
      }
      if ((parser->options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) == 0 &&
          !((parser->options & AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS) && container->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH && container->content.size > 0)) {
        parser->math_inline_prefix = parser->first_nonspace - parser->offset;
        S_advance_offset(parser, input, parser->first_nonspace - parser->offset, false);
      }
      aimarkdown_cmark_list_spread_leaf(parser, container, false);
      add_line(container, input, parser);
    } else {
      // create paragraph container for line
      if (parser->options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) {
        container = add_child(parser, container, AIMARKDOWN_CMARK_NODE_PARAGRAPH,
                              parser->offset + 1);
      } else {
        container = add_child(parser, container, AIMARKDOWN_CMARK_NODE_PARAGRAPH,
                              parser->first_nonspace + 1);
        container->definition.term_start_column = parser->offset + (parser->partially_consumed_tab ? 2 : 1);
        S_advance_offset(parser, input, parser->first_nonspace - parser->offset,
                           false);
      }
      aimarkdown_cmark_list_spread_leaf(parser, container, false);
      add_line(container, input, parser);
    }

    parser->current = container;
  }
}

// Definition lists are document containers. Recognize markers while the
// paragraph buffer and container continuation state are still available.
static bool S_open_definition(aimarkdown_cmark_parser *parser, aimarkdown_cmark_chunk *input,
                              aimarkdown_cmark_node **matched) {
  if (!(parser->options & AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS)) return false;
  S_find_first_nonspace(parser, input);
  if (parser->indent >= 4 || peek_at(input, parser->first_nonspace) != ':' ||
      !(S_is_space_or_tab(peek_at(input, parser->first_nonspace + 1)) ||
        S_is_line_end_char(peek_at(input, parser->first_nonspace + 1)))) return false;
  /* Definition-list markers can restructure surrounding flow in Web parsing;
   * list spread is not derived for documents containing a marker candidate. */
  aimarkdown_cmark_list_spread_unsupported(parser);
  aimarkdown_cmark_node *container = *matched;
  aimarkdown_cmark_node *list = NULL;
  aimarkdown_cmark_node *term = NULL;
  aimarkdown_cmark_node *previous_description = NULL;
  bool spread = false;
  bool interrupt = false;
  if (container->type == AIMARKDOWN_CMARK_NODE_DEFINITION_LIST) {
    list = container;
    aimarkdown_cmark_node *previous = list->last_child;
    previous_description = previous;
    spread = previous && (previous->definition.blank_lines > 0 || previous->definition.initial_blank);
    while (parser->current != list) parser->current = finalize(parser, parser->current);
  } else {
    aimarkdown_cmark_node *old = parser->current;
    if (old->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH && old->parent &&
        old->parent->type == AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE && container != old &&
        container != old->parent) {
      int lazy_offset = old->definition.lazy_offset;
      if (lazy_offset > 0) {
        term = make_block(parser->mem, AIMARKDOWN_CMARK_NODE_PARAGRAPH,
                          old->definition.lazy_line, old->definition.lazy_column);
        aimarkdown_cmark_strbuf_put(&term->content, old->content.ptr + lazy_offset, old->content.size - lazy_offset);
        aimarkdown_cmark_strbuf_truncate(&old->content, lazy_offset);
        old->definition.hardbreak = true;
        int final_line = old->definition.lazy_line - 1;
        while (parser->current != container) {
          aimarkdown_cmark_node *closing = parser->current;
          aimarkdown_cmark_node *closing_parent = closing->parent;
          parser->current = finalize(parser, closing);
          if (closing_parent->last_child == closing) { closing->end_line = final_line + 1; closing->end_column = 0; }
        }
        aimarkdown_cmark_node_append_child(container, term);
        term->definition.no_reference = true;
        parser->current = term;
        container = term;
      } else {
        while (parser->current != container) parser->current = finalize(parser, parser->current);
        return false;
      }
    }
    term = container->type == AIMARKDOWN_CMARK_NODE_PARAGRAPH ? container : container->last_child;
    if (!term || term->type != AIMARKDOWN_CMARK_NODE_PARAGRAPH || !term->parent ||
        (term->parent->type != AIMARKDOWN_CMARK_NODE_DOCUMENT &&
         term->parent->type != AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE &&
         term->parent->type != AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION)) return false;
    if (!(term->flags & AIMARKDOWN_CMARK_NODE__OPEN) && parser->line_number - term->end_line > 2) return false;
    interrupt = (term->flags & AIMARKDOWN_CMARK_NODE__OPEN) != 0;
    aimarkdown_cmark_node *parent = term->parent;
    bool omit_term = false;
    if (term->definition.flow_offset > 0) {
      aimarkdown_cmark_node *preceding = term;
      int offset = preceding->definition.flow_offset;
      if (offset == preceding->content.size) {
        omit_term = true;
        interrupt = false;
      } else {
        term = make_block(parser->mem, AIMARKDOWN_CMARK_NODE_PARAGRAPH, preceding->definition.flow_line,
                          preceding->definition.flow_column);
        aimarkdown_cmark_strbuf_put(&term->content, preceding->content.ptr + offset, preceding->content.size - offset);
        aimarkdown_cmark_strbuf_truncate(&preceding->content, offset);
        preceding->definition.hardbreak = true;
        if (preceding->flags & AIMARKDOWN_CMARK_NODE__OPEN) finalize(parser, preceding);
        preceding->end_line = term->start_line;
        preceding->end_column = 0;
        aimarkdown_cmark_node_insert_after(preceding, term);
        term->definition.no_reference = true;
        parser->current = term;
      }
    }
    while (parser->current != parent && parser->current != parser->root)
      parser->current = finalize(parser, parser->current);
    if (parent->last_child != term || term->content.size == 0) { *matched = parent; return false; }
    spread = !omit_term && parent->type != AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION && parser->line_number - term->end_line > 1;
    if (term->prev && term->prev->type == AIMARKDOWN_CMARK_NODE_DEFINITION_LIST) {
      list = term->prev;
      if (list->last_child) {
        list->last_child->end_line = term->start_line;
        list->last_child->end_column = (term->definition.term_start_column > 0 ? term->definition.term_start_column : term->start_column) - 1;
      }
      list->flags |= AIMARKDOWN_CMARK_NODE__OPEN;
    } else {
      list = make_block(parser->mem, AIMARKDOWN_CMARK_NODE_DEFINITION_LIST, term->start_line, term->definition.term_start_column > 0 ? term->definition.term_start_column : term->start_column);
      if (omit_term) {
        list->start_line = term->definition.flow_line;
        list->start_column = term->definition.flow_column;
        aimarkdown_cmark_node_insert_after(term, list);
      }
      else aimarkdown_cmark_node_insert_before(term, list);
    }
    // Each paragraph flow line is a separate term; inline parsing happens later.
    int line = term->start_line;
    for (bufsize_t start = 0; !omit_term && start < term->content.size; ) {
      bufsize_t end = start;
      while (end < term->content.size && !S_is_line_end_char(term->content.ptr[end])) end++;
      aimarkdown_cmark_node *item = make_block(parser->mem, AIMARKDOWN_CMARK_NODE_DEFINITION_TERM, line,
                                   line == term->start_line ? term->start_column : 1);
      if (line == term->start_line && term->definition.term_start_column > 0) {
        item->internal_offset = item->start_column - term->definition.term_start_column;
        item->start_column = term->definition.term_start_column;
      }
      item->flags &= ~AIMARKDOWN_CMARK_NODE__OPEN;
      item->end_line = line;
      item->end_column = item->start_column + end - start - 1;
      bufsize_t content_end = end;
      bufsize_t after_ending = end;
      if (after_ending < term->content.size && term->content.ptr[after_ending] == '\r') after_ending++;
      if (after_ending < term->content.size && term->content.ptr[after_ending] == '\n') after_ending++;
      if (after_ending < term->content.size) {
        if (end - start >= 2 && term->content.ptr[end - 1] == ' ' && term->content.ptr[end - 2] == ' ') {
          item->definition.hardbreak = true;
        } else {
          int slashes = 0;
          while (content_end > start && term->content.ptr[content_end - 1] == '\\') { slashes++; content_end--; }
          content_end = end;
          if (slashes % 2) { item->definition.hardbreak = true; content_end--; }
        }
      }
      aimarkdown_cmark_strbuf_put(&item->content, term->content.ptr + start, content_end - start);
      item->end_line = line + 1;
      item->end_column = 0;
      aimarkdown_cmark_node_append_child(list, item);
      if (end < term->content.size && term->content.ptr[end] == '\r') end++;
      if (end < term->content.size && term->content.ptr[end] == '\n') end++;
      start = end;
      line++;
    }
    if (!omit_term) aimarkdown_cmark_node_free(term);
    parser->current = parent;
  }
  int initial_column = parser->column;
  S_advance_offset(parser, input, parser->first_nonspace + 1 - parser->offset, false);
  int after_marker_offset = parser->offset;
  int after_marker_column = parser->column;
  bool partial_tab = parser->partially_consumed_tab;
  while (S_is_space_or_tab(peek_at(input, parser->offset))) S_advance_offset(parser, input, 1, true);
  bool empty = S_is_line_end_char(peek_at(input, parser->offset));
  if (parser->column - after_marker_column > 4 && !empty) {
    parser->offset = after_marker_offset;
    parser->column = after_marker_column;
    parser->partially_consumed_tab = partial_tab;
    S_advance_offset(parser, input, 1, true);
  }
  int padding = empty ? after_marker_column - initial_column + 1 : parser->column - initial_column;
  if (previous_description) {
    previous_description->end_line = parser->line_number;
    previous_description->end_column = empty ? after_marker_offset : parser->offset;
  }
  aimarkdown_cmark_node *description = make_block(parser->mem, AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION,
                                        parser->line_number, parser->offset + 1);
  if (empty) {
    description->start_column = after_marker_offset + 1;
    if (parser->definition_synthetic_ending && after_marker_offset == input->len - 1) {
      description->start_line = list->start_line;
      description->start_column = list->start_column;
    }
  }
  description->definition.padding = padding;
  description->definition.initial_blank = empty;
  description->definition.spread = spread;
  description->definition.interrupt = interrupt;
  aimarkdown_cmark_node_append_child(list, description);
  parser->current = description;
  parser->definition_lazy_code = false;
  *matched = description;
  return true;
}

/* See http://spec.commonmark.org/0.24/#phase-1-block-structure */
static void S_process_line(aimarkdown_cmark_parser *parser, const unsigned char *buffer,
                           bufsize_t bytes, bool ensureEndsInNewline) {
  aimarkdown_cmark_node *last_matched_container;
  bool all_matched = true;
  aimarkdown_cmark_node *container;
  aimarkdown_cmark_chunk input;
  aimarkdown_cmark_node *current;

  aimarkdown_cmark_strbuf_clear(&parser->curline);
  parser->math_inline_prefix = 0;
  parser->math_eol_target = NULL;
  parser->code_footnote_blank_offset = -1;
  parser->code_footnote_blank_container = NULL;

  if (parser->options & AIMARKDOWN_CMARK_OPT_VALIDATE_UTF8)
    aimarkdown_cmark_utf8proc_check(&parser->curline, buffer, bytes);
  else
    aimarkdown_cmark_strbuf_put(&parser->curline, buffer, bytes);

  bytes = parser->curline.size;

  // ensure line ends with a newline:
  if (ensureEndsInNewline && (bytes == 0 || !S_is_line_end_char(parser->curline.ptr[bytes - 1])))
    aimarkdown_cmark_strbuf_putc(&parser->curline, '\n');

  parser->offset = 0;
  parser->column = 0;
  parser->first_nonspace = 0;
  parser->first_nonspace_column = 0;
  parser->thematic_break_kill_pos = 0;
  parser->indent = 0;
  parser->blank = false;
  parser->partially_consumed_tab = false;
  parser->definition_lazy_code = false;

  input.data = parser->curline.ptr;
  input.len = parser->curline.size;
  input.alloc = 0;

  // Skip UTF-8 BOM.
  if (parser->line_number == 0 &&
      input.len >= 3 &&
      memcmp(input.data, "\xef\xbb\xbf", 3) == 0)
    parser->offset += 3;

  parser->line_number++;
  aimarkdown_cmark_list_spread_line(parser);

  last_matched_container = check_open_blocks(parser, &input, &all_matched);
  aimarkdown_cmark_list_spread_continued(parser, last_matched_container);

  if (!last_matched_container)
    goto finished;

  parser->definition_line_offset = parser->offset;
  if (S_open_definition(parser, &input, &last_matched_container)) {
    all_matched = true;
    aimarkdown_cmark_list_spread_unsupported(parser);
  }
  parser->definition_line_offset = parser->offset;
  container = last_matched_container;

  current = parser->current;

  // Only open new blocks if we're not limited to inline
  if ((parser->options & AIMARKDOWN_CMARK_OPT_INLINE_ONLY) == 0)
    open_new_blocks(parser, &container, &input, all_matched);

  /* Web exits containers after this line's prefixes when input ends there. */
  aimarkdown_cmark_list_spread_prefix_only(parser, input.len - parser->offset <= 1);

  /* parser->current might have changed if feed_reentrant was called */
  if (current == parser->current)
  add_text_to_container(parser, container, last_matched_container, &input);

  if (parser->current && parser->current->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK && parser->current->definition.single_line) {
    parser->current->end_line = parser->line_number + 1;
    parser->current = finalize(parser, parser->current);
  }
finished:
  parser->last_line_length = input.len;
  if (parser->last_line_length &&
      input.data[parser->last_line_length - 1] == '\n')
    parser->last_line_length -= 1;
  if (parser->last_line_length &&
      input.data[parser->last_line_length - 1] == '\r')
    parser->last_line_length -= 1;
  {
    /* The physical line, not `input`: ATX headings chop the chunk's length. */
    bufsize_t physical = parser->curline.size;
    if (physical && parser->curline.ptr[physical - 1] == '\n')
      physical -= 1;
    if (physical && parser->curline.ptr[physical - 1] == '\r')
      physical -= 1;
    aimarkdown_cmark_list_spread_end_line(parser, parser->math_last_line_ending, physical);
  }

  if ((parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES) &&
      (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS)) {
    for (aimarkdown_cmark_node *node = parser->current; node; node = node->parent) {
      if (node->type == AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION &&
          (!parser->blank || node->start_line == parser->line_number)) {
        node->end_line = parser->line_number;
        node->end_column = parser->last_line_length;
      }
    }
  }
  aimarkdown_cmark_strbuf_clear(&parser->curline);
}

aimarkdown_cmark_node *aimarkdown_cmark_parser_finish(aimarkdown_cmark_parser *parser) {
  aimarkdown_cmark_node *res;
  aimarkdown_cmark_llist *extensions;

  /* Parser was already finished once */
  if (parser->root == NULL)
    return NULL;

  if (parser->linebuf.size) {
    parser->definition_synthetic_ending = true;
    parser->math_last_line_ending = false;
    parser->math_current_ending = 0;
    S_process_line(parser, parser->linebuf.ptr, parser->linebuf.size, (parser->options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) == 0);
    aimarkdown_cmark_strbuf_clear(&parser->linebuf);
  }

  aimarkdown_cmark_list_spread_eof(parser);
  finalize_document(parser);

  aimarkdown_cmark_consolidate_text_nodes(parser->root);

  aimarkdown_cmark_strbuf_free(&parser->curline);
  aimarkdown_cmark_strbuf_free(&parser->linebuf);

#if AIMARKDOWN_CMARK_DEBUG_NODES
  if (aimarkdown_cmark_node_check(parser->root, stderr)) {
    abort();
  }
#endif

  for (extensions = parser->syntax_extensions; extensions; extensions = extensions->next) {
    aimarkdown_cmark_syntax_extension *ext = (aimarkdown_cmark_syntax_extension *) extensions->data;
    if (ext->postprocess_func) {
      aimarkdown_cmark_node *processed = ext->postprocess_func(ext, parser, parser->root);
      if (processed)
        parser->root = processed;
    }
  }

  aimarkdown_cmark_list_spread_finish(parser, parser->root);

  res = parser->root;
  parser->root = NULL;

  aimarkdown_cmark_parser_reset(parser);

  return res;
}

int aimarkdown_cmark_parser_get_line_number(aimarkdown_cmark_parser *parser) {
  return parser->line_number;
}

bufsize_t aimarkdown_cmark_parser_get_offset(aimarkdown_cmark_parser *parser) {
  return parser->offset;
}

bufsize_t aimarkdown_cmark_parser_get_column(aimarkdown_cmark_parser *parser) {
  return parser->column;
}

int aimarkdown_cmark_parser_get_first_nonspace(aimarkdown_cmark_parser *parser) {
  return parser->first_nonspace;
}

int aimarkdown_cmark_parser_get_first_nonspace_column(aimarkdown_cmark_parser *parser) {
  return parser->first_nonspace_column;
}

int aimarkdown_cmark_parser_get_indent(aimarkdown_cmark_parser *parser) {
  return parser->indent;
}

int aimarkdown_cmark_parser_is_blank(aimarkdown_cmark_parser *parser) {
  return parser->blank;
}

int aimarkdown_cmark_parser_has_partially_consumed_tab(aimarkdown_cmark_parser *parser) {
  return parser->partially_consumed_tab;
}

int aimarkdown_cmark_parser_get_last_line_length(aimarkdown_cmark_parser *parser) {
  return parser->last_line_length;
}

aimarkdown_cmark_node *aimarkdown_cmark_parser_add_child(aimarkdown_cmark_parser *parser,
                                   aimarkdown_cmark_node   *parent,
                                   aimarkdown_cmark_node_type block_type,
                                   int start_column) {
  return add_child(parser, parent, block_type, start_column);
}

void aimarkdown_cmark_parser_advance_offset(aimarkdown_cmark_parser *parser,
                                 const char *input,
                                 int count,
                                 int columns) {
  aimarkdown_cmark_chunk input_chunk = aimarkdown_cmark_chunk_literal(input);

  S_advance_offset(parser, &input_chunk, count, columns != 0);
}

void aimarkdown_cmark_parser_set_backslash_ispunct_func(aimarkdown_cmark_parser *parser,
                                             aimarkdown_cmark_ispunct_func func) {
  parser->backslash_ispunct = func;
}

aimarkdown_cmark_llist *aimarkdown_cmark_parser_get_syntax_extensions(aimarkdown_cmark_parser *parser) {
  return parser->syntax_extensions;
}
