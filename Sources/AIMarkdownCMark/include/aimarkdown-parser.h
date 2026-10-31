// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_PARSER_H
#define AIMARKDOWN_CMARK_PARSER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "aimarkdown-references.h"
#include "aimarkdown-node.h"
#include "aimarkdown-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LINK_LABEL_LENGTH 1000

bool aimarkdown_cmark_resolve_reference_definitions(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *node);

struct aimarkdown_cmark_parser {
  bufsize_t code_footnote_blank_offset;
  int code_footnote_blank_column;
  aimarkdown_cmark_node *code_footnote_blank_container;
  bool code_footnote_blank_partial;
  struct aimarkdown_cmark_mem *mem;
  /* A hashtable of urls in the current document for cross-references */
  struct aimarkdown_cmark_map *refmap;
  /* Lossless mode: a temporary map borrowing definition nodes only during inline parsing. */
  struct aimarkdown_cmark_map *footnote_map;
  /* The root node of the parser, always a CMARK_NODE_DOCUMENT */
  struct aimarkdown_cmark_node *root;
  /* The last open block after a line is fully processed */
  struct aimarkdown_cmark_node *current;
  /* See the documentation for cmark_parser_get_line_number() in cmark.h */
  int line_number;
  /* See the documentation for cmark_parser_get_offset() in cmark.h */
  bufsize_t offset;
  /* See the documentation for cmark_parser_get_column() in cmark.h */
  bufsize_t column;
  /* See the documentation for cmark_parser_get_first_nonspace() in cmark.h */
  bufsize_t first_nonspace;
  /* See the documentation for cmark_parser_get_first_nonspace_column() in cmark.h */
  bufsize_t first_nonspace_column;
  bufsize_t thematic_break_kill_pos;
  /* See the documentation for cmark_parser_get_indent() in cmark.h */
  int indent;
  /* See the documentation for cmark_parser_is_blank() in cmark.h */
  bool blank;
  /* See the documentation for cmark_parser_has_partially_consumed_tab() in cmark.h */
  bool partially_consumed_tab;
  bool definition_lazy_code;
  bool definition_marker_interrupt;
  bool definition_synthetic_ending;
  int definition_line_offset;
  int definition_prefix_size; /* Last consumed definition linePrefix on this physical line. */
  /* Contains the currently processed line */
  aimarkdown_cmark_strbuf curline;
  /* See the documentation for cmark_parser_get_last_line_length() in cmark.h */
  bufsize_t last_line_length;
  /* FIXME: not sure about the difference with curline */
  aimarkdown_cmark_strbuf linebuf;
  /* Options set by the user, see the Options section in cmark.h */
  int options;
  bool last_buffer_ended_with_cr;
  bool math_last_line_ending;
  int math_current_ending;
  int math_inline_prefix;
  aimarkdown_cmark_node *math_eol_target;
  size_t total_size;
  aimarkdown_cmark_llist *syntax_extensions;
  aimarkdown_cmark_llist *inline_syntax_extensions;
  aimarkdown_cmark_ispunct_func backslash_ispunct;
  /* used when parsing inlines, can be populated by extensions if any are loaded */
  int8_t *skip_chars;
  int8_t *special_chars;
  /* Opt-in container/line ledger used only to derive list and item spread. */
  struct aimarkdown_cmark_list_spread_ledger *list_spread;
};

#ifdef __cplusplus
}
#endif

#endif
