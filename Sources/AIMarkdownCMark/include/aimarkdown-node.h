// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_NODE_H
#define AIMARKDOWN_CMARK_NODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-cmark-gfm-extension_api.h"
#include "aimarkdown-buffer.h"
#include "aimarkdown-chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

int aimarkdown_cmark_reference_column(int column, const unsigned char *data, bufsize_t size);

typedef struct {
  aimarkdown_cmark_list_type list_type;
  int marker_offset;
  int padding;
  int start;
  aimarkdown_cmark_delim_type delimiter;
  unsigned char bullet_char;
  bool tight;
  bool checked; // For task list extension
} aimarkdown_cmark_list;

typedef struct {
  aimarkdown_cmark_chunk info;
  aimarkdown_cmark_chunk literal;
  uint8_t fence_length;
  bufsize_t web_fence_length; /* Exact parser opener length, only used with WEB_CODE_SYNTAX. */
  uint8_t fence_offset;
  unsigned char fence_char;
  int8_t fenced;
  bool web_single_line; /* A new flow after an unmatched container closes this code chunk at EOL. */
} aimarkdown_cmark_code;

typedef struct {
  int level;
  bool setext;
} aimarkdown_cmark_heading;

typedef struct {
  aimarkdown_cmark_chunk url;
  aimarkdown_cmark_chunk title;
} aimarkdown_cmark_link;

typedef struct {
  aimarkdown_cmark_chunk attributes;
} aimarkdown_cmark_attribute;

typedef struct {
  aimarkdown_cmark_chunk on_enter;
  aimarkdown_cmark_chunk on_exit;
} aimarkdown_cmark_custom;

enum aimarkdown_cmark_node__internal_flags {
  AIMARKDOWN_CMARK_NODE__OPEN = (1 << 0),
  AIMARKDOWN_CMARK_NODE__LAST_LINE_BLANK = (1 << 1),
  AIMARKDOWN_CMARK_NODE__LAST_LINE_CHECKED = (1 << 2),

  // Extensions can register custom flags by calling `cmark_register_node_flag`.
  // This is the starting value for the custom flags.
  AIMARKDOWN_CMARK_NODE__REGISTER_FIRST = (1 << 3),
};

typedef uint16_t aimarkdown_cmark_node_internal_flags;

typedef struct {
  bufsize_t offset;
  int line, column, ending, visual_column, prefix_visual_column;
  int virtual_prefix; /* Spaces emitted by the actually consumed partial tab before offset. */
  aimarkdown_cmark_chunk prefix;
} aimarkdown_cmark_math_line;

typedef struct {
  aimarkdown_cmark_chunk label, identifier_source, destination, title;
  bool has_title;
} aimarkdown_cmark_reference_definition;

typedef struct {
  aimarkdown_cmark_chunk label, source;
  int type; /* 1 shortcut, 2 collapsed, 3 full */
} aimarkdown_cmark_reference_usage;

typedef struct {
  aimarkdown_cmark_strbuf source, compiled, pending;
  aimarkdown_cmark_chunk info, value, lang, meta;
  int kind;
  bool finished, indented_qualified, closed, boundary_trim, list_boundary;
} aimarkdown_cmark_code_metadata;

struct aimarkdown_cmark_node {
  int softbreak_ending; /* 0 absent, 10 LF, 13 CR, 0x0d0a CRLF; parser event evidence. */
  int list_spread; /* 0 not captured, 1 not spread, 2 spread; list/item parser ledger evidence. */
  int list_spread_id; /* Parser-local ledger identity; 0 when the node was never recorded. */
  /* CMARK_OPT_WEB_POSITIONS evidence: WEB_POSITION_* flags, 1-based lines and
   * byte columns, exclusive end. Inline nodes first hold inline-subject offsets. */
  int web_position_flags;
  int web_start_line, web_start_column, web_end_line, web_end_column;
  int web_subject_start, web_subject_end;
  int web_table_cells; /* raw cells in a table row before column truncation; 0 unknown */
  aimarkdown_cmark_code_metadata *code_metadata;
  struct aimarkdown_cmark_syntax_extension *task_extension;
  int task_content_line;
  int reference_visual_column;
  bufsize_t reference_cached_offset;
  int reference_cached_column, reference_cached_line;
  bool reference_cached_valid;
  aimarkdown_cmark_reference_usage *reference_usage;
  aimarkdown_cmark_reference_definition *reference_definition;
  aimarkdown_cmark_strbuf content;

  struct aimarkdown_cmark_node *next;
  struct aimarkdown_cmark_node *prev;
  struct aimarkdown_cmark_node *parent;
  struct aimarkdown_cmark_node *first_child;
  struct aimarkdown_cmark_node *last_child;

  void *user_data;
  aimarkdown_cmark_free_func user_data_free_func;

  int start_line;
  int start_column;
  int end_line;
  int end_column;
  int internal_offset;
  uint16_t type;
  aimarkdown_cmark_node_internal_flags flags;
  int backtick_count;
  struct {
    int padding;
    int blank_lines;
    bool initial_blank;
    bool spread;
    bool hardbreak;
    bool softbreak;
    int break_line, break_column, break_ending;
    int lazy_offset;
    int lazy_line;
    int lazy_column;
    int flow_offset;
    int flow_line;
    int flow_column;
    bool no_reference;
    bool interrupt;
    bool single_line;
    int term_start_column;
  } definition;
  int math_fence_length;
  bool math_closing_fence;
  /* Last physical quote continuation, before document's EOF exit moves its
   * line ending outside mathFlow. Zero when no such line was consumed. */
  int math_quote_line, math_quote_column;
  bool math_quote_prefix_only;
  int math_container_eof_line; /* Only actual container prefixes remained at EOF. */
  aimarkdown_cmark_math_line *math_lines;
  size_t math_line_count, math_line_capacity;

  aimarkdown_cmark_syntax_extension *extension;

  /**
   * Used during cmark_render() to cache the most recent non-NULL
   * extension, if you go up the parent chain like this:
   *
   * node->parent->...parent->extension
   */
  aimarkdown_cmark_syntax_extension *ancestor_extension;

  union {
    int ref_ix;
    int def_count;
  } footnote;

  aimarkdown_cmark_node *parent_footnote_def;

  union {
    aimarkdown_cmark_chunk literal;
    aimarkdown_cmark_list list;
    aimarkdown_cmark_code code;
    aimarkdown_cmark_heading heading;
    aimarkdown_cmark_link link;
    aimarkdown_cmark_attribute attribute;
    aimarkdown_cmark_custom custom;
    int html_block_type;
    void *opaque;
  } as;
};

/**
 * Syntax extensions can use this function to register a custom node
 * flag. The flags are stored in the `flags` field of the `cmark_node`
 * struct. The `flags` parameter should be the address of a global variable
 * which will store the flag value.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_register_node_flag(aimarkdown_cmark_node_internal_flags *flags);

/**
 * DEPRECATED.
 *
 * This function was added in cmark-gfm version 0.29.0.gfm.7, and was
 * required to be called at program start time, which caused
 * backwards-compatibility issues in applications that use cmark-gfm as a
 * library. It is now a no-op.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_init_standard_node_flags(void);

static inline aimarkdown_cmark_mem *aimarkdown_cmark_node_mem(aimarkdown_cmark_node *node) {
  return node->content.mem;
}
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_check(aimarkdown_cmark_node *node, FILE *out);

static inline bool AIMARKDOWN_CMARK_NODE_TYPE_BLOCK_P(aimarkdown_cmark_node_type node_type) {
	return (node_type & AIMARKDOWN_CMARK_NODE_TYPE_MASK) == AIMARKDOWN_CMARK_NODE_TYPE_BLOCK;
}

static inline bool AIMARKDOWN_CMARK_NODE_BLOCK_P(aimarkdown_cmark_node *node) {
	return node != NULL && AIMARKDOWN_CMARK_NODE_TYPE_BLOCK_P((aimarkdown_cmark_node_type) node->type);
}

static inline bool AIMARKDOWN_CMARK_NODE_TYPE_INLINE_P(aimarkdown_cmark_node_type node_type) {
	return (node_type & AIMARKDOWN_CMARK_NODE_TYPE_MASK) == AIMARKDOWN_CMARK_NODE_TYPE_INLINE;
}

static inline bool AIMARKDOWN_CMARK_NODE_INLINE_P(aimarkdown_cmark_node *node) {
	return node != NULL && AIMARKDOWN_CMARK_NODE_TYPE_INLINE_P((aimarkdown_cmark_node_type) node->type);
}

AIMARKDOWN_CMARK_GFM_EXPORT bool aimarkdown_cmark_node_can_contain_type(aimarkdown_cmark_node *node, aimarkdown_cmark_node_type child_type);

/**
 * Enable (or disable) extra safety checks. These extra checks cause
 * extra performance overhead (in some cases quadratic), so they are only
 * intended to be used during testing.
 */
AIMARKDOWN_CMARK_GFM_EXPORT void aimarkdown_cmark_enable_safety_checks(bool enable);

void aimarkdown_cmark_code_metadata_start(aimarkdown_cmark_node *node, int kind);
void aimarkdown_cmark_code_metadata_finish(aimarkdown_cmark_node *node);
void aimarkdown_cmark_code_metadata_clear(aimarkdown_cmark_node *node);
void aimarkdown_cmark_code_metadata_inline(aimarkdown_cmark_node *node, aimarkdown_cmark_chunk source);
void aimarkdown_cmark_code_metadata_table(aimarkdown_cmark_node *node);

/* List spread ledger hooks; each is a no-op unless CMARK_OPT_LIST_SPREAD_METADATA is set. */
struct aimarkdown_cmark_parser;
void aimarkdown_cmark_list_spread_line(struct aimarkdown_cmark_parser *parser);
void aimarkdown_cmark_list_spread_prefix(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container);
void aimarkdown_cmark_list_spread_open(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container);
void aimarkdown_cmark_list_spread_close(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container);
void aimarkdown_cmark_list_spread_boundary(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container);
void aimarkdown_cmark_list_spread_leaf(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *leaf, bool closed);
void aimarkdown_cmark_list_spread_prefix_only(struct aimarkdown_cmark_parser *parser, bool prefix_only);
void aimarkdown_cmark_list_spread_end_line(struct aimarkdown_cmark_parser *parser, bool has_line_ending, int content_length);
void aimarkdown_cmark_list_spread_eof(struct aimarkdown_cmark_parser *parser);
void aimarkdown_cmark_list_spread_unsupported(struct aimarkdown_cmark_parser *parser);
void aimarkdown_cmark_list_spread_finish(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *root);
void aimarkdown_cmark_list_spread_free(struct aimarkdown_cmark_parser *parser);
void aimarkdown_cmark_list_spread_continued(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *last_matched);
void aimarkdown_cmark_list_spread_leaf_prefix(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *leaf);
void aimarkdown_cmark_list_spread_indent(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *code, bool qualified);
void aimarkdown_cmark_list_spread_item_prefix(struct aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *item, int end_column);

enum {
  AIMARKDOWN_CMARK_WEB_POSITION_START = 1,
  AIMARKDOWN_CMARK_WEB_POSITION_END = 2,
  /* Inline-subject offsets are pending conversion to source points. */
  AIMARKDOWN_CMARK_WEB_POSITION_SUBJECT = 4,
  /* The end offset is just after a line ending; Web places it at the next line start. */
  AIMARKDOWN_CMARK_WEB_POSITION_SUBJECT_EOL = 8
};
void aimarkdown_cmark_web_position_set_start(aimarkdown_cmark_node *node, int line, int column);
void aimarkdown_cmark_web_position_set_end(aimarkdown_cmark_node *node, int line, int column);

#ifdef __cplusplus
}
#endif

#endif
