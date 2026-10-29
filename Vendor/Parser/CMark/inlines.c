// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimarkdown-cmark_ctype.h"
#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-node.h"
#include "aimarkdown-parser.h"
#include "aimarkdown-references.h"
#include "aimarkdown-footnotes.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-houdini.h"
#include "aimarkdown-string_token.h"
#include "aimarkdown-utf8.h"
#include "aimarkdown-scanners.h"
#include "aimarkdown-inlines.h"
#include "aimarkdown-syntax_extension.h"

static const char *EMDASH = "\xE2\x80\x94";
static const char *ENDASH = "\xE2\x80\x93";
static const char *ELLIPSES = "\xE2\x80\xA6";
static const char *LEFTDOUBLEQUOTE = "\xE2\x80\x9C";
static const char *RIGHTDOUBLEQUOTE = "\xE2\x80\x9D";
static const char *LEFTSINGLEQUOTE = "\xE2\x80\x98";
static const char *RIGHTSINGLEQUOTE = "\xE2\x80\x99";

// Macros for creating various kinds of simple.
#define make_str(subj, sc, ec, s) make_literal(subj, AIMARKDOWN_CMARK_NODE_TEXT, sc, ec, s)
#define make_code(subj, sc, ec, s) make_literal(subj, AIMARKDOWN_CMARK_NODE_CODE, sc, ec, s)
#define make_raw_html(subj, sc, ec, s) make_literal(subj, AIMARKDOWN_CMARK_NODE_HTML_INLINE, sc, ec, s)
#define make_linebreak(mem) make_simple(mem, AIMARKDOWN_CMARK_NODE_LINEBREAK)
#define make_softbreak(mem) make_simple(mem, AIMARKDOWN_CMARK_NODE_SOFTBREAK)
#define make_emph(mem) make_simple(mem, AIMARKDOWN_CMARK_NODE_EMPH)
#define make_strong(mem) make_simple(mem, AIMARKDOWN_CMARK_NODE_STRONG)

#define MAXBACKTICKS 80

typedef enum {
  LINK,
  IMAGE,
  ATTRIBUTE
} bracket_type;

typedef struct bracket {
  struct bracket *previous;
  aimarkdown_cmark_node *inl_text;
  bufsize_t position;
  bracket_type type;
  bool active;
  bool bracket_after;
  bool in_bracket[4];
} bracket;

#define FLAG_SKIP_HTML_CDATA        (1u << 0)
#define FLAG_SKIP_HTML_DECLARATION  (1u << 1)
#define FLAG_SKIP_HTML_PI           (1u << 2)
#define FLAG_SKIP_HTML_COMMENT      (1u << 3)

/* Sparse run-length index: fixed 31-bit keys, no allocation by maximum run. */
typedef struct web_tick_node {
  size_t child[2];
  bufsize_t position;
} web_tick_node;

typedef struct subject{
  aimarkdown_cmark_mem *mem;
  aimarkdown_cmark_chunk input;
  unsigned flags;
  int line;
  bufsize_t pos;
  int block_offset;
  int column_offset;
  aimarkdown_cmark_map *refmap;
  delimiter *last_delim;
  bracket *last_bracket;
  bufsize_t backticks[MAXBACKTICKS + 1];
  bool scanned_for_backticks;
  web_tick_node *web_tick_nodes;
  size_t web_tick_count, web_tick_capacity;
  bufsize_t *math_last;
  bufsize_t math_capacity;
  bool math_scanned;
  aimarkdown_cmark_node *math_parent;
  bool no_link_openers;
} subject;

void aimarkdown_cmark_set_default_skip_chars(int8_t **skip_chars, bool use_memcpy) {
  static int8_t default_skip_chars[256];

  if (use_memcpy)
    memcpy(*skip_chars, &default_skip_chars, 256);
  else
    *skip_chars = default_skip_chars;
}

static inline bool S_is_line_end_char(char c) {
  return (c == '\n' || c == '\r');
}

static delimiter *S_insert_emph(subject *subj, delimiter *opener, bool web_emphasis,
                                delimiter *closer);

static int reference_label(subject *subj, aimarkdown_cmark_chunk *label);
static int reference_factory_size(aimarkdown_cmark_node *parent, bufsize_t at, const unsigned char *data, bufsize_t size);
static aimarkdown_cmark_chunk reference_string(aimarkdown_cmark_mem *mem, aimarkdown_cmark_node *parent, bufsize_t at,
                                    const unsigned char *data, bufsize_t size, bool decode, bool title);
static int parse_inline(aimarkdown_cmark_parser *parser, subject *subj, aimarkdown_cmark_node *parent, int options);

static void subject_from_buf(aimarkdown_cmark_mem *mem, int line_number, int block_offset, subject *e,
                             aimarkdown_cmark_chunk *buffer, aimarkdown_cmark_map *refmap);
static bufsize_t subject_find_special_char(aimarkdown_cmark_parser *parser, subject *subj, int options);

// Create an inline with a literal string value.
static inline aimarkdown_cmark_node *make_literal(subject *subj, aimarkdown_cmark_node_type t,
                                       int start_column, int end_column,
                                       aimarkdown_cmark_chunk s) {
  aimarkdown_cmark_node *e = (aimarkdown_cmark_node *)subj->mem->calloc(1, sizeof(*e));
  aimarkdown_cmark_strbuf_init(subj->mem, &e->content, 0);
  e->type = (uint16_t)t;
  e->as.literal = s;
  e->start_line = e->end_line = subj->line;
  // columns are 1 based.
  e->start_column = start_column + 1 + subj->column_offset + subj->block_offset;
  e->end_column = end_column + 1 + subj->column_offset + subj->block_offset;
  return e;
}

// Create an inline with no value.
static inline aimarkdown_cmark_node *make_simple(aimarkdown_cmark_mem *mem, aimarkdown_cmark_node_type t) {
  aimarkdown_cmark_node *e = (aimarkdown_cmark_node *)mem->calloc(1, sizeof(*e));
  aimarkdown_cmark_strbuf_init(mem, &e->content, 0);
  e->type = (uint16_t)t;
  return e;
}

// Like make_str, but parses entities.
static aimarkdown_cmark_node *make_str_with_entities(subject *subj,
                                          int start_column, int end_column,
                                          aimarkdown_cmark_chunk *content) {
  aimarkdown_cmark_strbuf unescaped = AIMARKDOWN_CMARK_BUF_INIT(subj->mem);

  if (aimarkdown_houdini_unescape_html(&unescaped, content->data, content->len)) {
    return make_str(subj, start_column, end_column, aimarkdown_cmark_chunk_buf_detach(&unescaped));
  } else {
    return make_str(subj, start_column, end_column, *content);
  }
}

// Like cmark_node_append_child but without costly sanity checks.
// Assumes that child was newly created.
static void append_child(aimarkdown_cmark_node *node, aimarkdown_cmark_node *child) {
  aimarkdown_cmark_node *old_last_child = node->last_child;

  child->next = NULL;
  child->prev = old_last_child;
  child->parent = node;
  node->last_child = child;

  if (old_last_child) {
    old_last_child->next = child;
  } else {
    // Also set first_child if node previously had no children.
    node->first_child = child;
  }
}

// Duplicate a chunk by creating a copy of the buffer not by reusing the
// buffer like cmark_chunk_dup does.
static aimarkdown_cmark_chunk chunk_clone(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *src) {
  aimarkdown_cmark_chunk c;
  bufsize_t len = src->len;

  c.len = len;
  c.data = (unsigned char *)mem->calloc(len + 1, 1);
  c.alloc = 1;
  if (len)
    memcpy(c.data, src->data, len);
  c.data[len] = '\0';

  return c;
}

static aimarkdown_cmark_chunk aimarkdown_cmark_clean_autolink(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *url,
                                        int is_email) {
  aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(mem);

  aimarkdown_cmark_chunk_trim(url);

  if (url->len == 0) {
    aimarkdown_cmark_chunk result = AIMARKDOWN_CMARK_CHUNK_EMPTY;
    return result;
  }

  if (is_email)
    aimarkdown_cmark_strbuf_puts(&buf, "mailto:");

  aimarkdown_houdini_unescape_html_f(&buf, url->data, url->len);
  return aimarkdown_cmark_chunk_buf_detach(&buf);
}

static inline aimarkdown_cmark_node *make_autolink(subject *subj, int start_column,
                                        int end_column, aimarkdown_cmark_chunk url,
                                        int is_email, bool web_tokens) {
  aimarkdown_cmark_node *link = make_simple(subj->mem, AIMARKDOWN_CMARK_NODE_LINK);
  if (web_tokens) {
    aimarkdown_cmark_strbuf value = AIMARKDOWN_CMARK_BUF_INIT(subj->mem);
    if (is_email) aimarkdown_cmark_strbuf_puts(&value, "mailto:");
    aimarkdown_cmark_strbuf_put(&value, url.data, url.len);
    link->as.link.url = aimarkdown_cmark_chunk_buf_detach(&value);
  } else link->as.link.url = aimarkdown_cmark_clean_autolink(subj->mem, &url, is_email);
  link->as.link.title = aimarkdown_cmark_chunk_literal("");
  link->start_line = link->end_line = subj->line;
  link->start_column = start_column + 1 + subj->column_offset + subj->block_offset;
  link->end_column = end_column + 1 + subj->column_offset + subj->block_offset;
  append_child(link, web_tokens ? make_str(subj, start_column + 1, end_column - 1, url) : make_str_with_entities(subj, start_column + 1, end_column - 1, &url));
  return link;
}

static void subject_from_buf(aimarkdown_cmark_mem *mem, int line_number, int block_offset, subject *e,
                             aimarkdown_cmark_chunk *chunk, aimarkdown_cmark_map *refmap) {
  int i;
  e->mem = mem;
  e->input = *chunk;
  e->flags = 0;
  e->line = line_number;
  e->pos = 0;
  e->block_offset = block_offset;
  e->column_offset = 0;
  e->refmap = refmap;
  e->last_delim = NULL;
  e->last_bracket = NULL;
  for (i = 0; i <= MAXBACKTICKS; i++) {
    e->backticks[i] = 0;
  }
  e->scanned_for_backticks = false;
  e->web_tick_nodes = NULL; e->web_tick_count = 0; e->web_tick_capacity = 0;
  e->math_last = NULL; e->math_capacity = 0; e->math_scanned = false; e->math_parent = NULL;
  e->no_link_openers = true;
}

static inline int isbacktick(int c) { return (c == '`'); }

static inline unsigned char peek_char_n(subject *subj, bufsize_t n) {
  // NULL bytes should have been stripped out by now.  If they're
  // present, it's a programming error:
  assert(!(subj->pos + n < subj->input.len && subj->input.data[subj->pos + n] == 0));
  return (subj->pos + n < subj->input.len) ? subj->input.data[subj->pos + n] : 0;
}

static inline unsigned char peek_char(subject *subj) {
  return peek_char_n(subj, 0);
}

static inline unsigned char peek_at(subject *subj, bufsize_t pos) {
  return subj->input.data[pos];
}

// Return true if there are more characters in the subject.
static inline int is_eof(subject *subj) {
  return (subj->pos >= subj->input.len);
}

// Advance the subject.  Doesn't check for eof.
#define advance(subj) (subj)->pos += 1

static inline bool skip_spaces(subject *subj) {
  bool skipped = false;
  while (peek_char(subj) == ' ' || peek_char(subj) == '\t') {
    advance(subj);
    skipped = true;
  }
  return skipped;
}

static inline bool skip_line_end(subject *subj) {
  bool seen_line_end_char = false;
  if (peek_char(subj) == '\r') {
    advance(subj);
    seen_line_end_char = true;
  }
  if (peek_char(subj) == '\n') {
    advance(subj);
    seen_line_end_char = true;
  }
  return seen_line_end_char || is_eof(subj);
}

// Take characters while a predicate holds, and return a string.
static inline aimarkdown_cmark_chunk take_while(subject *subj, int (*f)(int)) {
  unsigned char c;
  bufsize_t startpos = subj->pos;
  bufsize_t len = 0;

  while ((c = peek_char(subj)) && (*f)(c)) {
    advance(subj);
    len++;
  }

  return aimarkdown_cmark_chunk_dup(&subj->input, startpos, len);
}

// Return the number of newlines in a given span of text in a subject.  If
// the number is greater than zero, also return the number of characters
// between the last newline and the end of the span in `since_newline`.
static int count_newlines(subject *subj, bufsize_t from, bufsize_t len, int *since_newline) {
  int nls = 0;
  int since_nl = 0;

  while (len--) {
    if (subj->input.data[from++] == '\n') {
      ++nls;
      since_nl = 0;
    } else {
      ++since_nl;
    }
  }

  if (!nls)
    return 0;

  *since_newline = since_nl;
  return nls;
}

// Adjust `node`'s `end_line`, `end_column`, and `subj`'s `line` and
// `column_offset` according to the number of newlines in a just-matched span
// of text in `subj`.
static void adjust_subj_node_newlines(subject *subj, aimarkdown_cmark_node *node, int matchlen, int extra, int options) {
  if (!(options & AIMARKDOWN_CMARK_OPT_SOURCEPOS)) {
    return;
  }

  int since_newline;
  int newlines = count_newlines(subj, subj->pos - matchlen - extra, matchlen, &since_newline);
  if (newlines) {
    subj->line += newlines;
    node->end_line += newlines;
    node->end_column = since_newline;
    subj->column_offset = -subj->pos + since_newline + extra;
  }
}

/* Return an initialized pool index. The maximum possible key is INT32_MAX.
 * Index zero is absent, root is one; geometrically growing pool owns only
 * prefixes of observed lengths. Allocation is bounded by actual run keys. */
static size_t web_tick_new_node(subject *subj) {
  if (subj->web_tick_count == subj->web_tick_capacity) {
    size_t old = subj->web_tick_capacity;
    if (old > SIZE_MAX / 2) abort();
    size_t capacity = old ? old * 2 : 16;
    if (capacity > SIZE_MAX / sizeof(web_tick_node)) abort();
    web_tick_node *nodes = subj->mem->realloc(subj->web_tick_nodes,
                                             capacity * sizeof(web_tick_node));
    if (!nodes) abort();
    memset(nodes + old, 0, (capacity - old) * sizeof(web_tick_node));
    subj->web_tick_nodes = nodes;
    subj->web_tick_capacity = capacity;
  }
  return subj->web_tick_count++;
}

static void web_tick_record(subject *subj, bufsize_t length, bufsize_t position) {
  if (!subj->web_tick_count) { web_tick_new_node(subj); web_tick_new_node(subj); }
  size_t index = 1;
  uint32_t key = (uint32_t)length;
  for (int bit = 30; bit >= 0; bit--) {
    unsigned direction = (key >> bit) & 1;
    size_t child = subj->web_tick_nodes[index].child[direction];
    if (!child) {
      child = web_tick_new_node(subj);
      /* Pool growth may move the array: resolve parent by index afterwards. */
      subj->web_tick_nodes[index].child[direction] = child;
    }
    index = child;
  }
  if (subj->web_tick_nodes[index].position < position)
    subj->web_tick_nodes[index].position = position;
}

static bufsize_t web_tick_last(subject *subj, bufsize_t length) {
  if (!subj->web_tick_count) return 0;
  size_t index = 1;
  uint32_t key = (uint32_t)length;
  for (int bit = 30; bit >= 0; bit--) {
    index = subj->web_tick_nodes[index].child[(key >> bit) & 1];
    if (!index) return 0;
  }
  return subj->web_tick_nodes[index].position;
}

/* Successful scans consume their interval; the first unsuccessful scan visits
 * the remaining suffix once and records every run. Later failed lookups take
 * at most 31 steps. No repeated suffix scan for absent or exhausted lengths. */
static bufsize_t scan_to_closing_web_backticks(subject *subj, bufsize_t length) {
  if (subj->scanned_for_backticks && web_tick_last(subj, length) <= subj->pos)
    return 0;
  for (;;) {
    unsigned char c;
    while ((c = peek_char(subj)) && c != '`') advance(subj);
    if (is_eof(subj)) break;
    bufsize_t count = 0;
    while (peek_char(subj) == '`') { advance(subj); count++; }
    web_tick_record(subj, count, subj->pos - count);
    if (count == length) return subj->pos;
  }
  subj->scanned_for_backticks = true;
  return 0;
}

// Try to process a backtick code span that began with a
// span of ticks of length openticklength length (already
// parsed).  Return 0 if you don't find matching closing
// backticks, otherwise return the position in the subject
// after the closing backticks.
static bufsize_t scan_to_closing_backticks(subject *subj,
                                           bufsize_t openticklength) {

  bool found = false;
  if (openticklength > MAXBACKTICKS) {
    // we limit backtick string length because of the array subj->backticks:
    return 0;
  }
  if (subj->scanned_for_backticks &&
      subj->backticks[openticklength] <= subj->pos) {
    // return if we already know there's no closer
    return 0;
  }
  while (!found) {
    // read non backticks
    unsigned char c;
    while ((c = peek_char(subj)) && c != '`') {
      advance(subj);
    }
    if (is_eof(subj)) {
      break;
    }
    bufsize_t numticks = 0;
    while (peek_char(subj) == '`') {
      advance(subj);
      numticks++;
    }
    // store position of ender
    if (numticks <= MAXBACKTICKS) {
      subj->backticks[numticks] = subj->pos - numticks;
    }
    if (numticks == openticklength) {
      return (subj->pos);
    }
  }
  // got through whole input without finding closer
  subj->scanned_for_backticks = true;
  return 0;
}

// Destructively modify string, converting newlines to
// spaces, then removing a single leading + trailing space,
// unless the code span consists entirely of space characters.
static void S_normalize_code(aimarkdown_cmark_strbuf *s) {
  bufsize_t r, w;
  bool contains_nonspace = false;

  for (r = 0, w = 0; r < s->size; ++r) {
    switch (s->ptr[r]) {
    case '\r':
      if (s->ptr[r + 1] != '\n') {
	s->ptr[w++] = ' ';
      }
      break;
    case '\n':
      s->ptr[w++] = ' ';
      break;
    default:
      s->ptr[w++] = s->ptr[r];
    }
    if (s->ptr[r] != ' ') {
      contains_nonspace = true;
    }
  }

  // begins and ends with space?
  if (contains_nonspace &&
      s->ptr[0] == ' ' && s->ptr[w - 1] == ' ') {
    aimarkdown_cmark_strbuf_drop(s, 1);
    aimarkdown_cmark_strbuf_truncate(s, w - 2);
  } else {
    aimarkdown_cmark_strbuf_truncate(s, w);
  }

}


// Parse backtick code section or raw backticks, return an inline.
// Assumes that the subject has a backtick at the current position.
static aimarkdown_cmark_node *handle_backticks(subject *subj, int options) {
  bufsize_t initpos = subj->pos;
  aimarkdown_cmark_chunk openticks = take_while(subj, isbacktick);
  bufsize_t startpos = subj->pos;
  bufsize_t endpos = (options & AIMARKDOWN_CMARK_OPT_WEB_CODE_SYNTAX)
      ? scan_to_closing_web_backticks(subj, openticks.len)
      : scan_to_closing_backticks(subj, openticks.len);

  if (endpos == 0) {      // not found
    subj->pos = startpos; // rewind
    return make_str(subj, initpos, initpos + openticks.len - 1, openticks);
  } else {
    aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(subj->mem);

    aimarkdown_cmark_strbuf_set(&buf, subj->input.data + startpos,
                     endpos - startpos - openticks.len);
    S_normalize_code(&buf);

    aimarkdown_cmark_node *node = make_code(subj, startpos, endpos - openticks.len - 1, aimarkdown_cmark_chunk_buf_detach(&buf));
    if (options & AIMARKDOWN_CMARK_OPT_CODE_METADATA) {
      aimarkdown_cmark_chunk source = reference_string(subj->mem, subj->math_parent, startpos,
          subj->input.data + startpos, endpos - startpos - openticks.len, false, false);
      aimarkdown_cmark_code_metadata_inline(node, source);
      aimarkdown_cmark_chunk_free(subj->mem, &source);
    }
    node->backtick_count = openticks.len;
    adjust_subj_node_newlines(subj, node, endpos - startpos, openticks.len, options);
    return node;
  }
}


// Fixed default Web micromark-extension-cjk-friendly Unicode scalar classes.
#include "web-emphasis-unicode.inc"
static int web_emphasis_classify(int32_t code) {
  if (code < 0) return 1;
  size_t left = 0, right = sizeof(web_emphasis_character_classes) /
                                  sizeof(web_emphasis_character_classes[0]);
  while (left < right) {
    size_t middle = left + (right - left) / 2;
    if ((uint32_t)code < web_emphasis_character_classes[middle][0]) right = middle;
    else if ((uint32_t)code > web_emphasis_character_classes[middle][1]) left = middle + 1;
    else return web_emphasis_character_classes[middle][2];
  }
  return 0;
}

static void web_emphasis_flanking(subject *subj, int32_t previous,
                                   bufsize_t previous_pos, int32_t following,
                                   unsigned char marker, bool marker_before,
                                   bool marker_after, bool *can_open, bool *can_close) {
  int before = web_emphasis_classify(previous);
  int after = web_emphasis_classify(following);
  // General-use VS observes its primary scalar; FE01 selects CJK quote forms.
  if (before == 0x4000 && previous_pos > 0) {
    bufsize_t position = previous_pos - 1;
    while (position > 0 && (subj->input.data[position] & 0xc0) == 0x80) position--;
    int32_t primary = 0;
    if (aimarkdown_cmark_utf8proc_iterate(subj->input.data + position, previous_pos - position,
                               &primary) > 0) {
      int category = web_emphasis_classify(primary);
      if (primary && !(category & 1)) {
        bool cjk_quote = previous == 0xfe01 &&
                         (primary == 0x2018 || primary == 0x2019 ||
                          primary == 0x201c || primary == 0x201d);
        before = cjk_quote ? 0x1002 : (category & ~0x2000);
      }
    }
  }
  bool before_punctuation = (before & 0x1002) == 2;
  bool after_punctuation = (after & 0x1002) == 2;
  bool before_space_or_punctuation = before_punctuation || (before & 1);
  bool after_space_or_punctuation = after_punctuation || (after & 1);
  bool open = !after_space_or_punctuation ||
              (after_punctuation && (before_space_or_punctuation || (before & 0x3000))) || marker_after;
  bool close = !before_space_or_punctuation ||
               (before_punctuation && (after_space_or_punctuation || (after & 0x1000))) || marker_before;
  *can_open = marker == '*' ? open : open && ((before & 3) || !close);
  *can_close = marker == '*' ? close : close && ((after & 3) || !open);
}

static void math_cache_run(subject *subj, bufsize_t length, bufsize_t position) {
  if (length >= subj->math_capacity) {
    bufsize_t old = subj->math_capacity;
    bufsize_t capacity = old ? old : 16;
    while (capacity <= length) capacity = capacity > subj->input.len / 2 ? subj->input.len + 1 : capacity * 2;
    subj->math_last = subj->mem->realloc(subj->math_last, (size_t)capacity * sizeof(bufsize_t));
    memset(subj->math_last + old, 0, (size_t)(capacity - old) * sizeof(bufsize_t));
    subj->math_capacity = capacity;
  }
  if (subj->math_last[length] < position) subj->math_last[length] = position;
}

static const aimarkdown_cmark_math_line *math_line_at(subject *subj, bufsize_t offset) {
  aimarkdown_cmark_node *parent = subj->math_parent;
  if (!parent || !parent->math_line_count) return NULL;
  size_t lo = 0, hi = parent->math_line_count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (parent->math_lines[mid].offset <= offset) lo = mid + 1;
    else hi = mid;
  }
  return lo ? &parent->math_lines[lo - 1] : NULL;
}

static void math_source_point(subject *subj, bufsize_t offset, int *line, int *column) {
  const aimarkdown_cmark_math_line *row = math_line_at(subj, offset);
  if (row) { *line = row->line; *column = row->column + offset - row->offset; }
}

static aimarkdown_cmark_node *handle_math(subject *subj, int options) {
  bufsize_t start = subj->pos;
  while (peek_char(subj) == '$') advance(subj);
  bufsize_t length = subj->pos - start;
  bufsize_t body = subj->pos;
  bool previous_literal_dollar = start > 0 && subj->input.data[start - 1] == '$';
  if (previous_literal_dollar) {
    bufsize_t j = start - 1;
    while (j > 0 && subj->input.data[j - 1] == '\\') j--;
    if ((start - 1 - j) % 2) previous_literal_dollar = false;
  }
  if (length < 2 || previous_literal_dollar ||
      (subj->math_scanned && (length >= subj->math_capacity || subj->math_last[length] <= body)))
    return make_str(subj, start, body - 1, aimarkdown_cmark_chunk_dup(&subj->input, start, length));
  bufsize_t close = body;
  while (close < subj->input.len) {
    if (subj->input.data[close] != '$') { close++; continue; }
    bufsize_t end = close;
    while (end < subj->input.len && subj->input.data[end] == '$') end++;
    math_cache_run(subj, end - close, close);
    if (end - close == length) {
      aimarkdown_cmark_strbuf value = AIMARKDOWN_CMARK_BUF_INIT(subj->mem);
      bool data = false;
      for (bufsize_t i = body; i < close; i++) {
        unsigned char c = subj->input.data[i];
        if (c == '\n') {
          const aimarkdown_cmark_math_line *row = math_line_at(subj, i);
          int ending = row ? row->ending : 10;
          if (ending != 10 && ending != 0) aimarkdown_cmark_strbuf_putc(&value, '\r');
          if (ending != 13) aimarkdown_cmark_strbuf_putc(&value, '\n');
          const aimarkdown_cmark_math_line *next = math_line_at(subj, i + 1);
          if (next && next != row) aimarkdown_cmark_strbuf_put(&value, next->prefix.data, next->prefix.len);
        } else { if (c != ' ' && c != '\r') data = true; aimarkdown_cmark_strbuf_putc(&value, c); }
      }
      bool first_padding = value.size && (value.ptr[0] == ' ' || S_is_line_end_char(value.ptr[0]));
      bool last_padding = value.size && (value.ptr[value.size - 1] == ' ' || S_is_line_end_char(value.ptr[value.size - 1]));
      if (data && first_padding && last_padding) {
        int head = value.ptr[0] == '\r' && value.size > 1 && value.ptr[1] == '\n' ? 2 : 1;
        int tail = value.ptr[value.size - 1] == '\n' && value.size > 1 && value.ptr[value.size - 2] == '\r' ? 2 : 1;
        aimarkdown_cmark_strbuf_drop(&value, head); aimarkdown_cmark_strbuf_truncate(&value, value.size - tail);
      }
      subj->pos = end;
      aimarkdown_cmark_node *node = make_literal(subj, AIMARKDOWN_CMARK_NODE_MATH_INLINE, start, end - 1, aimarkdown_cmark_chunk_buf_detach(&value));
      adjust_subj_node_newlines(subj, node, end - start, 0, options);
      if (options & AIMARKDOWN_CMARK_OPT_SOURCEPOS) {
        math_source_point(subj, start, &node->start_line, &node->start_column);
        int line = node->end_line, column = node->end_column + 1;
        math_source_point(subj, end, &line, &column);
        node->end_line = line; node->end_column = column - 1;
      }
      return node;
    }
    close = end;
  }
  subj->math_scanned = true;
  return make_str(subj, start, body - 1, aimarkdown_cmark_chunk_dup(&subj->input, start, length));
}

// Scan ***, **, or * and return number scanned, or 0.
// Advances position.
static int scan_delims(aimarkdown_cmark_parser *parser, subject *subj, unsigned char c,
                       bool *can_open, bool *can_close) {
  int numdelims = 0;
  bufsize_t before_char_pos = 0, after_char_pos;
  int32_t after_char = 0;
  int32_t before_char = 0;
  int len;
  bool left_flanking, right_flanking;
  // Web attention observes adjacent source characters, including extension delimiters.
  // Preserve cmark extension transparency unless the caller explicitly opts in.
  bool skip_extension_delimiters = !(parser->options & AIMARKDOWN_CMARK_OPT_WEB_EMPHASIS) ||
                                   (c != '*' && c != '_');

  if (subj->pos == 0) {
    before_char = 10;
  } else {
    before_char_pos = subj->pos - 1;

    // walk back to the beginning of the UTF_8 sequence:
    while ((peek_at(subj, before_char_pos) >> 6 == 2 || (skip_extension_delimiters && parser->skip_chars[peek_at(subj, before_char_pos)])) && before_char_pos > 0) {
      before_char_pos -= 1;
    }
    len = aimarkdown_cmark_utf8proc_iterate(subj->input.data + before_char_pos,
                                 subj->pos - before_char_pos, &before_char);
    if (len == -1 || (skip_extension_delimiters && before_char < 256 && parser->skip_chars[(unsigned char) before_char])) {
      before_char = 10;
    }
  }

  if (c == '\'' || c == '"') {
    numdelims++;
    advance(subj); // limit to 1 delim for quotes
  } else {
    while (peek_char(subj) == c) {
      numdelims++;
      advance(subj);
    }
  }

  if (subj->pos == subj->input.len) {
    after_char = 10;
  } else {
    after_char_pos = subj->pos;

    while ((skip_extension_delimiters && parser->skip_chars[peek_at(subj, after_char_pos)]) && after_char_pos < subj->input.len) {
      after_char_pos += 1;
    }
    len = aimarkdown_cmark_utf8proc_iterate(subj->input.data + after_char_pos,
                                 subj->input.len - after_char_pos, &after_char);
    if (len == -1 || (skip_extension_delimiters && after_char < 256 && parser->skip_chars[(unsigned char) after_char])) {
      after_char = 10;
    }
  }

  if (!skip_extension_delimiters) {
    bool marker_before = before_char == '*' || before_char == '_' || ((before_char == '~' || before_char == '=') && parser->skip_chars[before_char]);
    bool marker_after = after_char == '*' || after_char == '_' || ((after_char == '~' || after_char == '=') && parser->skip_chars[after_char]);
    web_emphasis_flanking(subj, before_char, before_char_pos, after_char, c,
                           marker_before, marker_after, can_open, can_close);
    return numdelims;
  }

  left_flanking = numdelims > 0 && !aimarkdown_cmark_utf8proc_is_space(after_char) &&
                  (!aimarkdown_cmark_utf8proc_is_punctuation(after_char) ||
                   aimarkdown_cmark_utf8proc_is_space(before_char) ||
                   aimarkdown_cmark_utf8proc_is_punctuation(before_char));
  right_flanking = numdelims > 0 && !aimarkdown_cmark_utf8proc_is_space(before_char) &&
                   (!aimarkdown_cmark_utf8proc_is_punctuation(before_char) ||
                    aimarkdown_cmark_utf8proc_is_space(after_char) ||
                    aimarkdown_cmark_utf8proc_is_punctuation(after_char));
  if (c == '_') {
    *can_open = left_flanking &&
                (!right_flanking || aimarkdown_cmark_utf8proc_is_punctuation(before_char));
    *can_close = right_flanking &&
                 (!left_flanking || aimarkdown_cmark_utf8proc_is_punctuation(after_char));
  } else if (c == '\'' || c == '"') {
    *can_open = left_flanking && !right_flanking &&
	         before_char != ']' && before_char != ')';
    *can_close = right_flanking;
  } else {
    *can_open = left_flanking;
    *can_close = right_flanking;
  }
  return numdelims;
}

/*
static void print_delimiters(subject *subj)
{
        delimiter *delim;
        delim = subj->last_delim;
        while (delim != NULL) {
                printf("Item at stack pos %p: %d %d %d next(%p) prev(%p)\n",
                       (void*)delim, delim->delim_char,
                       delim->can_open, delim->can_close,
                       (void*)delim->next, (void*)delim->previous);
                delim = delim->previous;
        }
}
*/

static void remove_delimiter(subject *subj, delimiter *delim) {
  if (delim == NULL)
    return;
  if (delim->next == NULL) {
    // end of list:
    assert(delim == subj->last_delim);
    subj->last_delim = delim->previous;
  } else {
    delim->next->previous = delim->previous;
  }
  if (delim->previous != NULL) {
    delim->previous->next = delim->next;
  }
  subj->mem->free(delim);
}

static void pop_bracket(subject *subj) {
  bracket *b;
  if (subj->last_bracket == NULL)
    return;
  b = subj->last_bracket;
  subj->last_bracket = subj->last_bracket->previous;
  subj->mem->free(b);
}

static void push_delimiter(subject *subj, unsigned char c, bool can_open,
                           bool can_close, aimarkdown_cmark_node *inl_text) {
  delimiter *delim = (delimiter *)subj->mem->calloc(1, sizeof(delimiter));
  delim->delim_char = c;
  delim->can_open = can_open;
  delim->can_close = can_close;
  delim->inl_text = inl_text;
  delim->position = subj->pos;
  delim->length = inl_text->as.literal.len;
  delim->previous = subj->last_delim;
  delim->next = NULL;
  if (delim->previous != NULL) {
    delim->previous->next = delim;
  }
  subj->last_delim = delim;
}

static void push_bracket(subject *subj, bracket_type type, aimarkdown_cmark_node *inl_text) {
  bracket *b = (bracket *)subj->mem->calloc(1, sizeof(bracket));
  if (subj->last_bracket != NULL) {
    subj->last_bracket->bracket_after = true;
    memcpy(b->in_bracket, subj->last_bracket->in_bracket, sizeof(b->in_bracket));
  }
  b->type = type;
  b->active = true;
  b->inl_text = inl_text;
  b->previous = subj->last_bracket;
  b->position = subj->pos;
  b->bracket_after = false;
  b->in_bracket[type] = true;
  subj->last_bracket = b;
  if (type != IMAGE) {
    subj->no_link_openers = false;
  }
}

// Assumes the subject has a c at the current position.
static aimarkdown_cmark_node *handle_delim(aimarkdown_cmark_parser *parser, subject *subj, unsigned char c, bool smart) {
  bufsize_t numdelims;
  aimarkdown_cmark_node *inl_text;
  bool can_open, can_close;
  aimarkdown_cmark_chunk contents;

  numdelims = scan_delims(parser, subj, c, &can_open, &can_close);

  if (c == '\'' && smart) {
    contents = aimarkdown_cmark_chunk_literal(RIGHTSINGLEQUOTE);
  } else if (c == '"' && smart) {
    contents =
        aimarkdown_cmark_chunk_literal(can_close ? RIGHTDOUBLEQUOTE : LEFTDOUBLEQUOTE);
  } else {
    contents = aimarkdown_cmark_chunk_dup(&subj->input, subj->pos - numdelims, numdelims);
  }

  inl_text = make_str(subj, subj->pos - numdelims, subj->pos - 1, contents);

  if ((can_open || can_close) && (!(c == '\'' || c == '"') || smart)) {
    push_delimiter(subj, c, can_open, can_close, inl_text);
  }

  return inl_text;
}

// Assumes we have a hyphen at the current position.
static aimarkdown_cmark_node *handle_hyphen(subject *subj, bool smart) {
  int startpos = subj->pos;

  advance(subj);

  if (!smart || peek_char(subj) != '-') {
    return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("-"));
  }

  while (smart && peek_char(subj) == '-') {
    advance(subj);
  }

  int numhyphens = subj->pos - startpos;
  int en_count = 0;
  int em_count = 0;
  int i;
  aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(subj->mem);

  if (numhyphens % 3 == 0) { // if divisible by 3, use all em dashes
    em_count = numhyphens / 3;
  } else if (numhyphens % 2 == 0) { // if divisible by 2, use all en dashes
    en_count = numhyphens / 2;
  } else if (numhyphens % 3 == 2) { // use one en dash at end
    en_count = 1;
    em_count = (numhyphens - 2) / 3;
  } else { // use two en dashes at the end
    en_count = 2;
    em_count = (numhyphens - 4) / 3;
  }

  for (i = em_count; i > 0; i--) {
    aimarkdown_cmark_strbuf_puts(&buf, EMDASH);
  }

  for (i = en_count; i > 0; i--) {
    aimarkdown_cmark_strbuf_puts(&buf, ENDASH);
  }

  return make_str(subj, startpos, subj->pos - 1, aimarkdown_cmark_chunk_buf_detach(&buf));
}

// Assumes we have a period at the current position.
static aimarkdown_cmark_node *handle_period(subject *subj, bool smart) {
  advance(subj);
  if (smart && peek_char(subj) == '.') {
    advance(subj);
    if (peek_char(subj) == '.') {
      advance(subj);
      return make_str(subj, subj->pos - 3, subj->pos - 1, aimarkdown_cmark_chunk_literal(ELLIPSES));
    } else {
      return make_str(subj, subj->pos - 2, subj->pos - 1, aimarkdown_cmark_chunk_literal(".."));
    }
  } else {
    return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("."));
  }
}

static aimarkdown_cmark_syntax_extension *get_extension_for_special_char(aimarkdown_cmark_parser *parser, unsigned char c) {
  aimarkdown_cmark_llist *tmp_ext;

  for (tmp_ext = parser->inline_syntax_extensions; tmp_ext; tmp_ext=tmp_ext->next) {
    aimarkdown_cmark_syntax_extension *ext = (aimarkdown_cmark_syntax_extension *) tmp_ext->data;
    aimarkdown_cmark_llist *tmp_char;
    for (tmp_char = ext->special_inline_chars; tmp_char; tmp_char=tmp_char->next) {
      unsigned char tmp_c = (unsigned char)(size_t)tmp_char->data;

      if (tmp_c == c) {
        return ext;
      }
    }
  }

  return NULL;
}

static void process_emphasis(aimarkdown_cmark_parser *parser, subject *subj, bufsize_t stack_bottom) {
  delimiter *candidate;
  delimiter *closer = NULL;
  delimiter *opener;
  delimiter *old_closer;
  bool opener_found;
  bufsize_t openers_bottom[3][128];
  int i;

  // initialize openers_bottom:
  memset(&openers_bottom, 0, sizeof(openers_bottom));
  for (i=0; i < 3; i++) {
    openers_bottom[i]['*'] = stack_bottom;
    openers_bottom[i]['_'] = stack_bottom;
    openers_bottom[i]['\''] = stack_bottom;
    openers_bottom[i]['"'] = stack_bottom;
  }

  // move back to first relevant delim.
  candidate = subj->last_delim;
  while (candidate != NULL && candidate->position >= stack_bottom) {
    closer = candidate;
    candidate = candidate->previous;
  }

  // now move forward, looking for closers, and handling each
  while (closer != NULL) {
    aimarkdown_cmark_syntax_extension *extension = get_extension_for_special_char(parser, closer->delim_char);
    if (closer->can_close) {
      // Now look backwards for first matching opener:
      opener = closer->previous;
      opener_found = false;
      while (opener != NULL && opener->position >= stack_bottom &&
             opener->position >= openers_bottom[closer->length % 3][closer->delim_char]) {
        if (opener->can_open && opener->delim_char == closer->delim_char) {
          // interior closer of size 2 can't match opener of size 1
          // or of size 1 can't match 2
          if (!(closer->can_open || opener->can_close) ||
	      closer->length % 3 == 0 ||
              (opener->length + closer->length) % 3 != 0) {
            opener_found = true;
            break;
          }
        }
        opener = opener->previous;
      }
      old_closer = closer;

      if (extension) {
        if (opener_found)
          closer = extension->insert_inline_from_delim(extension, parser, subj, opener, closer);
        else
          closer = closer->next;
      } else if (closer->delim_char == '*' || closer->delim_char == '_') {
        if (opener_found) {
          closer = S_insert_emph(subj, opener, (parser->options & AIMARKDOWN_CMARK_OPT_WEB_EMPHASIS) != 0, closer);
        } else {
          closer = closer->next;
        }
      } else if (closer->delim_char == '\'' || closer->delim_char == '"') {
        aimarkdown_cmark_chunk_free(subj->mem, &closer->inl_text->as.literal);
        if (closer->delim_char == '\'') {
          closer->inl_text->as.literal = aimarkdown_cmark_chunk_literal(RIGHTSINGLEQUOTE);
        } else {
          closer->inl_text->as.literal = aimarkdown_cmark_chunk_literal(RIGHTDOUBLEQUOTE);
        }
        closer = closer->next;
        if (opener_found) {
          aimarkdown_cmark_chunk_free(subj->mem, &opener->inl_text->as.literal);
          if (old_closer->delim_char == '\'') {
            opener->inl_text->as.literal = aimarkdown_cmark_chunk_literal(LEFTSINGLEQUOTE);
          } else {
            opener->inl_text->as.literal = aimarkdown_cmark_chunk_literal(LEFTDOUBLEQUOTE);
          }
          remove_delimiter(subj, opener);
          remove_delimiter(subj, old_closer);
        }
      }
      if (!opener_found) {
        // set lower bound for future searches for openers
        openers_bottom[old_closer->length % 3][old_closer->delim_char] =
                old_closer->position;
        if (!old_closer->can_open) {
          // we can remove a closer that can't be an
          // opener, once we've seen there's no
          // matching opener:
          remove_delimiter(subj, old_closer);
        }
      }
    } else {
      closer = closer->next;
    }
  }
  // free all delimiters in list until stack_bottom:
  while (subj->last_delim != NULL &&
         subj->last_delim->position >= stack_bottom) {
    remove_delimiter(subj, subj->last_delim);
  }
}

static delimiter *S_insert_emph(subject *subj, delimiter *opener, bool web_emphasis,
                                delimiter *closer) {
  delimiter *delim, *tmp_delim;
  bufsize_t use_delims;
  aimarkdown_cmark_node *opener_inl = opener->inl_text;
  aimarkdown_cmark_node *closer_inl = closer->inl_text;
  bufsize_t opener_num_chars = opener_inl->as.literal.len;
  bufsize_t closer_num_chars = closer_inl->as.literal.len;
  aimarkdown_cmark_node *tmp, *tmpnext, *emph;

  // calculate the actual number of characters used from this closer
  use_delims = (closer_num_chars >= 2 && opener_num_chars >= 2) ? 2 : 1;

  // remove used characters from associated inlines.
  opener_num_chars -= use_delims;
  closer_num_chars -= use_delims;
  opener_inl->as.literal.len = opener_num_chars;
  closer_inl->as.literal.len = closer_num_chars;

  // free delimiters between opener and closer
  delim = closer->previous;
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    remove_delimiter(subj, delim);
    delim = tmp_delim;
  }

  // create new emph or strong, and splice it in to our inlines
  // between the opener and closer
  emph = use_delims == 1 ? make_emph(subj->mem) : make_strong(subj->mem);

  tmp = opener_inl->next;
  while (tmp && tmp != closer_inl) {
    tmpnext = tmp->next;
    aimarkdown_cmark_node_unlink(tmp);
    append_child(emph, tmp);
    tmp = tmpnext;
  }
  aimarkdown_cmark_node_insert_after(opener_inl, emph);

  emph->start_line = opener_inl->start_line;
  emph->end_line = closer_inl->end_line;
  emph->start_column = opener_inl->start_column;
  emph->end_column = closer_inl->end_column;
  if (web_emphasis) {
    // A partial run consumes the opener suffix and closer prefix, not their
    // complete source spans. Keep leftover runs aligned for nested matches.
    emph->start_column = opener_inl->start_column + opener_num_chars;
    emph->end_column = closer_inl->start_column + use_delims - 1;
    if (opener_num_chars) opener_inl->end_column = emph->start_column - 1;
    if (closer_num_chars) closer_inl->start_column = emph->end_column + 1;
  }

  // if opener has 0 characters, remove it and its associated inline
  if (opener_num_chars == 0) {
    aimarkdown_cmark_node_free(opener_inl);
    remove_delimiter(subj, opener);
  }

  // if closer has 0 characters, remove it and its associated inline
  if (closer_num_chars == 0) {
    // remove empty closer inline
    aimarkdown_cmark_node_free(closer_inl);
    // remove closer from list
    tmp_delim = closer->next;
    remove_delimiter(subj, closer);
    closer = tmp_delim;
  }

  return closer;
}

// Parse backslash-escape or just a backslash, returning an inline.
static aimarkdown_cmark_node *handle_backslash(aimarkdown_cmark_parser *parser, subject *subj) {
  advance(subj);
  unsigned char nextchar = peek_char(subj);
  if ((parser->backslash_ispunct ? parser->backslash_ispunct : aimarkdown_cmark_ispunct)(nextchar)) {
    // only ascii symbols and newline can be escaped
    advance(subj);
    return make_str(subj, subj->pos - 2, subj->pos - 1, aimarkdown_cmark_chunk_dup(&subj->input, subj->pos - 1, 1));
  } else if (!is_eof(subj) && skip_line_end(subj)) {
    return make_linebreak(subj->mem);
  } else {
    return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("\\"));
  }
}

// Parse an entity or a regular "&" string.
// Assumes the subject has an '&' character at the current position.
static aimarkdown_cmark_node *handle_entity(subject *subj, bool reference_tokens, bool web_tokens) {
  aimarkdown_cmark_strbuf ent = AIMARKDOWN_CMARK_BUF_INIT(subj->mem);
  bufsize_t len;

  if (web_tokens) {
    bufsize_t start = subj->pos;
    len = aimarkdown_cmark_decode_string_token_part(&ent, subj->input.data + start, subj->input.len - start);
    subj->pos += len;
    return make_str(subj, start, subj->pos - 1, aimarkdown_cmark_chunk_buf_detach(&ent));
  }
  advance(subj);

  len = aimarkdown_houdini_unescape_ent(&ent, subj->input.data + subj->pos,
                             subj->input.len - subj->pos);

  if (len == 0)
    return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("&"));

  subj->pos += len;
  aimarkdown_cmark_node *node = make_str(subj, subj->pos - 1 - len, subj->pos - 1, aimarkdown_cmark_chunk_buf_detach(&ent));
  if (reference_tokens) {
    aimarkdown_cmark_strbuf web = AIMARKDOWN_CMARK_BUF_INIT(subj->mem);
    aimarkdown_cmark_decode_string_token(&web, subj->input.data + subj->pos - 1 - len, len + 1);
    if (web.size != node->as.literal.len || memcmp(web.ptr, node->as.literal.data, web.size)) {
      node->reference_usage = subj->mem->calloc(1, sizeof(aimarkdown_cmark_reference_usage));
      node->reference_usage->label = aimarkdown_cmark_chunk_buf_detach(&web);
    }
    aimarkdown_cmark_strbuf_free(&web);
  }
  return node;
}

// Clean a URL: remove surrounding whitespace, and remove \ that escape
// punctuation.
aimarkdown_cmark_chunk aimarkdown_cmark_clean_url(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *url) {
  aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(mem);

  aimarkdown_cmark_chunk_trim(url);

  if (url->len == 0) {
    aimarkdown_cmark_chunk result = AIMARKDOWN_CMARK_CHUNK_EMPTY;
    return result;
  }

  aimarkdown_houdini_unescape_html_f(&buf, url->data, url->len);

  aimarkdown_cmark_strbuf_unescape(&buf);
  return aimarkdown_cmark_chunk_buf_detach(&buf);
}

aimarkdown_cmark_chunk aimarkdown_cmark_clean_title(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *title) {
  aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(mem);
  unsigned char first, last;

  if (title->len == 0) {
    aimarkdown_cmark_chunk result = AIMARKDOWN_CMARK_CHUNK_EMPTY;
    return result;
  }

  first = title->data[0];
  last = title->data[title->len - 1];

  // remove surrounding quotes if any:
  if ((first == '\'' && last == '\'') || (first == '(' && last == ')') ||
      (first == '"' && last == '"')) {
    aimarkdown_houdini_unescape_html_f(&buf, title->data + 1, title->len - 2);
  } else {
    aimarkdown_houdini_unescape_html_f(&buf, title->data, title->len);
  }

  aimarkdown_cmark_strbuf_unescape(&buf);
  return aimarkdown_cmark_chunk_buf_detach(&buf);
}

aimarkdown_cmark_chunk aimarkdown_cmark_clean_url_web(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *url) {
  aimarkdown_cmark_strbuf out = AIMARKDOWN_CMARK_BUF_INIT(mem);
  aimarkdown_cmark_decode_string_token(&out, url->data, url->len);
  return aimarkdown_cmark_chunk_buf_detach(&out);
}
aimarkdown_cmark_chunk aimarkdown_cmark_clean_title_web(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *title) {
  aimarkdown_cmark_strbuf out = AIMARKDOWN_CMARK_BUF_INIT(mem);
  if (title->len >= 2) aimarkdown_cmark_decode_string_token(&out, title->data + 1, title->len - 2);
  return aimarkdown_cmark_chunk_buf_detach(&out);
}

// Clean custom attributes. This function uses `cmark_clean_url` internaly
// because the requirements are the same
aimarkdown_cmark_chunk aimarkdown_cmark_clean_attributes(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *attributes) {
  return aimarkdown_cmark_clean_url(mem, attributes);
}

// Parse an autolink or HTML tag.
// Assumes the subject has a '<' character at the current position.
static aimarkdown_cmark_node *handle_pointy_brace(subject *subj, int options) {
  bufsize_t matchlen = 0;
  aimarkdown_cmark_chunk contents;

  advance(subj); // advance past first <

  // first try to match a URL autolink
  matchlen = scan_autolink_uri(&subj->input, subj->pos);
  if (matchlen > 0) {
    contents = aimarkdown_cmark_chunk_dup(&subj->input, subj->pos, matchlen - 1);
    subj->pos += matchlen;

    return make_autolink(subj, subj->pos - 1 - matchlen, subj->pos - 1, contents, 0, (options & AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES) != 0);
  }

  // next try to match an email autolink
  matchlen = scan_autolink_email(&subj->input, subj->pos);
  if (matchlen > 0) {
    contents = aimarkdown_cmark_chunk_dup(&subj->input, subj->pos, matchlen - 1);
    subj->pos += matchlen;

    return make_autolink(subj, subj->pos - 1 - matchlen, subj->pos - 1, contents, 1, (options & AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES) != 0);
  }

  // finally, try to match an html tag
  if (subj->pos + 2 <= subj->input.len) {
    int c = subj->input.data[subj->pos];
    if (c == '!' && (subj->flags & FLAG_SKIP_HTML_COMMENT) == 0) {
      c = subj->input.data[subj->pos+1];
      if (c == '-' && subj->input.data[subj->pos+2] == '-') {
        if (subj->input.data[subj->pos+3] == '>') {
          matchlen = 4;
        } else if (subj->input.data[subj->pos+3] == '-' &&
                   subj->input.data[subj->pos+4] == '>') {
          matchlen = 5;
        } else {
          matchlen = scan_html_comment(&subj->input, subj->pos + 1);
          if (matchlen > 0) {
            matchlen += 1; // prefix "<"
          } else { // no match through end of input: set a flag so
                   // we don't reparse looking for -->:
            subj->flags |= FLAG_SKIP_HTML_COMMENT;
          }
        }
      } else if (c == '[') {
        if ((subj->flags & FLAG_SKIP_HTML_CDATA) == 0) {
          matchlen = scan_html_cdata(&subj->input, subj->pos + 2);
          if (matchlen > 0) {
            // The regex doesn't require the final "]]>". But if we're not at
            // the end of input, it must come after the match. Otherwise,
            // disable subsequent scans to avoid quadratic behavior.
            matchlen += 5; // prefix "![", suffix "]]>"
            if (subj->pos + matchlen > subj->input.len) {
              subj->flags |= FLAG_SKIP_HTML_CDATA;
              matchlen = 0;
            }
          }
        }
      } else if ((subj->flags & FLAG_SKIP_HTML_DECLARATION) == 0) {
        matchlen = scan_html_declaration(&subj->input, subj->pos + 1);
        if (matchlen > 0) {
          matchlen += 2; // prefix "!", suffix ">"
          if (subj->pos + matchlen > subj->input.len) {
            subj->flags |= FLAG_SKIP_HTML_DECLARATION;
            matchlen = 0;
          }
        }
      }
    } else if (c == '?') {
      if ((subj->flags & FLAG_SKIP_HTML_PI) == 0) {
        // Note that we allow an empty match.
        matchlen = scan_html_pi(&subj->input, subj->pos + 1);
        matchlen += 3; // prefix "?", suffix "?>"
        if (subj->pos + matchlen > subj->input.len) {
          subj->flags |= FLAG_SKIP_HTML_PI;
          matchlen = 0;
        }
      }
    } else {
      matchlen = scan_html_tag(&subj->input, subj->pos);
    }
  }
  if (matchlen > 0) {
    contents = aimarkdown_cmark_chunk_dup(&subj->input, subj->pos - 1, matchlen + 1);
    subj->pos += matchlen;
    aimarkdown_cmark_node *node = make_raw_html(subj, subj->pos - matchlen - 1, subj->pos - 1, contents);
    adjust_subj_node_newlines(subj, node, matchlen, 1, options);
    return node;
  }

  if (options & AIMARKDOWN_CMARK_OPT_LIBERAL_HTML_TAG) {
    matchlen = scan_liberal_html_tag(&subj->input, subj->pos);
    if (matchlen > 0) {
      contents = aimarkdown_cmark_chunk_dup(&subj->input, subj->pos - 1, matchlen + 1);
      subj->pos += matchlen;
      aimarkdown_cmark_node *node = make_raw_html(subj, subj->pos - matchlen - 1, subj->pos - 1, contents);
      adjust_subj_node_newlines(subj, node, matchlen, 1, options);
      return node;
    }
  }

  // if nothing matches, just return the opening <:
  return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("<"));
}

// Parse a link label.  Returns 1 if successful.
// Note:  unescaped brackets are not allowed in labels.
// The label begins with `[` and ends with the first `]` character
// encountered.  Backticks in labels do not start code spans.
static int link_label(subject *subj, aimarkdown_cmark_chunk *raw_label, bool parse_attribute_label) {
  bufsize_t startpos = subj->pos;
  int length = 0;
  unsigned char c;

  // If we are parsing attribute label, advance past ^
  if (parse_attribute_label) {
    if (peek_char(subj) == '^') {
      advance(subj);
    } else {
      return 0;
    }
  }

  // advance past [
  if (peek_char(subj) == '[') {
    advance(subj);
  } else {
    return 0;
  }

  while ((c = peek_char(subj)) && c != '[' && c != ']') {
    if (c == '\\') {
      advance(subj);
      length++;
      if (aimarkdown_cmark_ispunct(peek_char(subj))) {
        advance(subj);
        length++;
      }
    } else {
      advance(subj);
      length++;
    }
    if (length > MAX_LINK_LABEL_LENGTH) {
      goto noMatch;
    }
  }

  if (c == ']') { // match found
    bufsize_t position = parse_attribute_label ? startpos + 2 : startpos + 1;
    *raw_label =
        aimarkdown_cmark_chunk_dup(&subj->input, position, subj->pos - position);
    aimarkdown_cmark_chunk_trim(raw_label);
    advance(subj); // advance past ]
    return 1;
  }

noMatch:
  subj->pos = startpos; // rewind
  return 0;
}

static bufsize_t manual_scan_link_url_2(aimarkdown_cmark_chunk *input, bufsize_t offset,
                                        aimarkdown_cmark_chunk *output) {
  bufsize_t i = offset;
  size_t nb_p = 0;

  while (i < input->len) {
    if (input->data[i] == '\\' &&
        i + 1 < input-> len &&
        aimarkdown_cmark_ispunct(input->data[i+1]))
      i += 2;
    else if (input->data[i] == '(') {
      ++nb_p;
      ++i;
      if (nb_p > 32)
        return -1;
    } else if (input->data[i] == ')') {
      if (nb_p == 0)
        break;
      --nb_p;
      ++i;
    } else if (aimarkdown_cmark_isspace(input->data[i])) {
      if (i == offset) {
        return -1;
      }
      break;
    } else {
      ++i;
    }
  }

  if (i >= input->len)
    return -1;

  {
    aimarkdown_cmark_chunk result = {input->data + offset, i - offset, 0};
    *output = result;
  }
  return i - offset;
}

static bufsize_t manual_scan_link_url(aimarkdown_cmark_chunk *input, bufsize_t offset,
                                      aimarkdown_cmark_chunk *output) {
  bufsize_t i = offset;

  if (i < input->len && input->data[i] == '<') {
    ++i;
    while (i < input->len) {
      if (input->data[i] == '>') {
        ++i;
        break;
      } else if (input->data[i] == '\\')
        i += 2;
      else if (input->data[i] == '\n' || input->data[i] == '<')
        return -1;
      else
        ++i;
    }
  } else {
    return manual_scan_link_url_2(input, offset, output);
  }

  if (i >= input->len)
    return -1;

  {
    aimarkdown_cmark_chunk result = {input->data + offset + 1, i - 2 - offset, 0};
    *output = result;
  }
  return i - offset;
}

static bufsize_t manual_scan_attribute_attributes(aimarkdown_cmark_chunk *input, bufsize_t offset,
                                                aimarkdown_cmark_chunk *output) {
  bufsize_t i = offset;
  size_t nb_p = 0;

  while (i < input->len) {
    if (input->data[i] == '\\' &&
        i + 1 < input->len &&
        aimarkdown_cmark_ispunct(input->data[i+1]))
      i += 2;
    else if (input->data[i] == '(') {
      ++nb_p;
      ++i;
      if (nb_p > 32)
        return -1;
    } else if (input->data[i] == ')') {
      if (nb_p == 0)
        break;
      --nb_p;
      ++i;
    } else {
      ++i;
    }
  }

  if (i >= input->len)
    return -1;

  {
    aimarkdown_cmark_chunk result = {input->data + offset, i - offset, 0};
    *output = result;
  }
  return i - offset;
}

static aimarkdown_cmark_node *handle_close_bracket_attribute(aimarkdown_cmark_parser *parser, subject *subj, bracket *opener) {
  bufsize_t startattributes, endattributes;
  aimarkdown_cmark_chunk attributes = AIMARKDOWN_CMARK_CHUNK_EMPTY;
  bufsize_t n;
  aimarkdown_cmark_node *inl;
  aimarkdown_cmark_chunk raw_label;
  int found_label;
  aimarkdown_cmark_node *tmp, *tmpnext;
  aimarkdown_cmark_reference *ref = NULL;
  bool isAttributesNode = false;

  // ^name[content](attributes)
  // TODO: support name. we will not even enter this with a name because we fail the match first

  startattributes = subj->pos + 1;

  if (peek_char(subj) == '(' &&
      ((n = manual_scan_attribute_attributes(&subj->input, subj->pos + 1,
                                 &attributes)) > -1)) {

    endattributes = subj->pos + 1 + n;

    if (peek_at(subj, endattributes) == ')') {
      subj->pos = endattributes + 1;
      isAttributesNode = true;
      if (endattributes - startattributes == 0) {
        attributes = aimarkdown_cmark_chunk_literal(NULL);
      } else {
        attributes = aimarkdown_cmark_chunk_dup(&subj->input, startattributes, endattributes - startattributes);
      }
    }
  }

  // If we can't match direct link, look for [link label] that matches in refmap
  raw_label = aimarkdown_cmark_chunk_literal("");
  found_label = link_label(subj, &raw_label, false);
  if (found_label) {
    ref = (aimarkdown_cmark_reference *)aimarkdown_cmark_map_lookup(subj->refmap, &raw_label);
    aimarkdown_cmark_chunk_free(subj->mem, &raw_label);

    if (ref && ref->is_attributes_reference) {
      isAttributesNode = true;
      attributes = chunk_clone(subj->mem, &ref->attributes);
    }
  }

  if (!isAttributesNode) {
    // The current node can't be parsed as attribute node, turn it to a TEXT node instead.
    pop_bracket(subj);
    return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("]"));
  }

  inl = make_simple(subj->mem, AIMARKDOWN_CMARK_NODE_ATTRIBUTE);
  inl->as.attribute.attributes = attributes;
  inl->start_line = inl->end_line = subj->line;
  inl->start_column = opener->inl_text->start_column;
  inl->end_column = subj->pos + subj->column_offset + subj->block_offset;
  aimarkdown_cmark_node_insert_before(opener->inl_text, inl);
  // Add content text:
  tmp = opener->inl_text->next;
  while (tmp) {
    tmpnext = tmp->next;
    aimarkdown_cmark_node_append_child(inl, tmp);
    tmp = tmpnext;
  }

  // Free the bracket ^[:
  aimarkdown_cmark_node_free(opener->inl_text);

  process_emphasis(parser, subj, opener->position);
  pop_bracket(subj);

  return NULL;
}

// Return a link, an image, an attribute, or a literal close bracket.
static aimarkdown_cmark_node *handle_close_bracket(aimarkdown_cmark_parser *parser, subject *subj) {
  bufsize_t initial_pos, after_link_text_pos;
  bufsize_t endurl, starttitle, endtitle, endall;
  bufsize_t sps, n;
  aimarkdown_cmark_reference *ref = NULL;
  aimarkdown_cmark_chunk url_chunk, title_chunk;
  aimarkdown_cmark_chunk url, title;
  bracket *opener;
  aimarkdown_cmark_node *inl;
  aimarkdown_cmark_chunk raw_label;
  int found_label;
  aimarkdown_cmark_node *tmp, *tmpnext;
  bool is_image;
  int usage_type = 0;
  aimarkdown_cmark_chunk usage_source = AIMARKDOWN_CMARK_CHUNK_EMPTY, usage_label = AIMARKDOWN_CMARK_CHUNK_EMPTY;

  advance(subj); // advance past ]
  initial_pos = subj->pos;

  // get last [ or ![ or ^[
  opener = subj->last_bracket;

  if (opener == NULL) {
    return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("]"));
  }

  if (opener->type == ATTRIBUTE) {
    return handle_close_bracket_attribute(parser, subj, opener);
  }

  // If we got here, we matched a potential link/image text.
  // Now we check to see if it's a link/image.
  is_image = opener->type == IMAGE;

  if (!is_image && subj->no_link_openers) {
    // take delimiter off stack
    pop_bracket(subj);
    return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("]"));
  }

  after_link_text_pos = subj->pos;

  // First, look for an inline link.
  if (peek_char(subj) == '(' &&
      ((sps = scan_spacechars(&subj->input, subj->pos + 1)) > -1) &&
      ((n = manual_scan_link_url(&subj->input, subj->pos + 1 + sps,
                                 &url_chunk)) > -1)) {

    // try to parse an explicit link:
    endurl = subj->pos + 1 + sps + n;
    starttitle = endurl + scan_spacechars(&subj->input, endurl);

    // ensure there are spaces btw url and title
    endtitle = (starttitle == endurl)
                   ? starttitle
                   : starttitle + scan_link_title(&subj->input, starttitle);

    endall = endtitle + scan_spacechars(&subj->input, endtitle);

    if (peek_at(subj, endall) == ')') {
      subj->pos = endall + 1;

      title_chunk =
          aimarkdown_cmark_chunk_dup(&subj->input, starttitle, endtitle - starttitle);
      if (parser->options & AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES) {
        url = reference_string(subj->mem, subj->math_parent, (bufsize_t)(url_chunk.data - subj->input.data), url_chunk.data, url_chunk.len, true, false);
        title = title_chunk.len >= 2 ? reference_string(subj->mem, subj->math_parent, starttitle + 1, title_chunk.data + 1, title_chunk.len - 2, true, true) : aimarkdown_cmark_chunk_literal("");
      } else {
        url = aimarkdown_cmark_clean_url(subj->mem, &url_chunk);
        title = aimarkdown_cmark_clean_title(subj->mem, &title_chunk);
      }
      aimarkdown_cmark_chunk_free(subj->mem, &url_chunk);
      aimarkdown_cmark_chunk_free(subj->mem, &title_chunk);
      goto match;

    } else {
      // it could still be a shortcut reference link
      subj->pos = after_link_text_pos;
    }
  }

  if (parser->options & AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES) {
    bufsize_t label_at = opener->position;
    bufsize_t label_size = initial_pos - opener->position - 1;
    usage_type = 1;
    if (peek_char(subj) == '[') {
      if (peek_at(subj, subj->pos + 1) == ']') {
        subj->pos += 2; usage_type = 2;
      } else {
        bufsize_t label_start = subj->pos;
        aimarkdown_cmark_chunk parsed;
        if (!reference_label(subj, &parsed) || parsed.len == 0) goto noMatch;
        label_at = label_start + 1; label_size = subj->pos - label_start - 2;
        usage_type = 3;
      }
    }
    usage_source = reference_string(subj->mem, subj->math_parent, label_at,
                                   subj->input.data + label_at, label_size, false, false);
    // Full labels have factoryLabel's UTF-16 limit. Shortcut/collapsed use the
    // first label whose definition existence has already constrained its size.
    if (usage_type == 3) {
      if (reference_factory_size(subj->math_parent, label_at, subj->input.data + label_at, label_size) > 999) goto noMatch;
    }
    ref = (aimarkdown_cmark_reference *)aimarkdown_cmark_map_lookup(subj->refmap, &usage_source);
    if (!ref || ref->is_attributes_reference) goto noMatch;
    usage_label = reference_string(subj->mem, subj->math_parent, label_at,
                                  subj->input.data + label_at, label_size, true, false);
    url = chunk_clone(subj->mem, &ref->url);
    title = chunk_clone(subj->mem, &ref->title);
    goto match;
  }
  // Next, look for a following [link label] that matches in refmap.
  // skip spaces
  raw_label = aimarkdown_cmark_chunk_literal("");
  found_label = link_label(subj, &raw_label, false);
  if (!found_label) {
    // If we have a shortcut reference link, back up
    // to before the spacse we skipped.
    subj->pos = initial_pos;
  }

  if ((!found_label || raw_label.len == 0) && !opener->bracket_after) {
    aimarkdown_cmark_chunk_free(subj->mem, &raw_label);
    raw_label = aimarkdown_cmark_chunk_dup(&subj->input, opener->position,
                                initial_pos - opener->position - 1);
    found_label = true;
  }

  if (found_label) {
    ref = (aimarkdown_cmark_reference *)aimarkdown_cmark_map_lookup(subj->refmap, &raw_label);
    aimarkdown_cmark_chunk_free(subj->mem, &raw_label);
  }

  if (ref != NULL && !ref->is_attributes_reference) { // found
    url = chunk_clone(subj->mem, &ref->url);
    title = chunk_clone(subj->mem, &ref->title);
    goto match;
  } else {
    goto noMatch;
  }

noMatch:
  aimarkdown_cmark_chunk_free(subj->mem, &usage_source);
  aimarkdown_cmark_chunk_free(subj->mem, &usage_label);
  // If we fall through to here, it means we didn't match a link.
  // What if we're a footnote link?
  if (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES &&
      opener->inl_text->next &&
      opener->inl_text->next->type == AIMARKDOWN_CMARK_NODE_TEXT) {

    aimarkdown_cmark_chunk *literal = &opener->inl_text->next->as.literal;

    // look back to the opening '[', and skip ahead to the next character
    // if we're looking at a '[^' sequence, and there is other text or nodes
    // after the ^, let's call it a footnote reference.
    if ((literal->len > 0 && literal->data[0] == '^') && (literal->len > 1 || opener->inl_text->next->next)) {

      aimarkdown_cmark_footnote *definition = NULL;
      if (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS) {
        // Slice the label from the original subject, not from text nodes that may have been escaped or split.
        aimarkdown_cmark_chunk label = aimarkdown_cmark_chunk_dup(&subj->input, opener->position + 1,
                                            initial_pos - opener->position - 2);
        definition = (aimarkdown_cmark_footnote *)aimarkdown_cmark_map_lookup(parser->footnote_map, &label);
        aimarkdown_cmark_chunk_free(subj->mem, &label);
        // Preserve preceding delimiter/text nodes for normal emphasis, escape, and other inline processing.
        if (!definition)
          goto noFootnoteMatch;
      }

      // Before we got this far, the `handle_close_bracket` function may have
      // advanced the current state beyond our footnote's actual closing
      // bracket, ie if it went looking for a `link_label`.
      // Let's just rewind the subject's position:
      subj->pos = initial_pos;

      aimarkdown_cmark_node *fnref = make_simple(subj->mem, AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE);

      // the start and end of the footnote ref is the opening and closing brace
      // i.e. the subject's current position, and the opener's start_column
      int fnref_end_column = subj->pos + subj->column_offset + subj->block_offset;
      int fnref_start_column = opener->inl_text->start_column;

      // any given node delineates a substring of the line being processed,
      // with the remainder of the line being pointed to thru its 'literal'
      // struct member.
      // here, we copy the literal's pointer, moving it past the '^' character
      // for a length equal to the size of footnote reference text.
      // i.e. end_col minus start_col, minus the [ and the ^ characters
      //
      // this copies the footnote reference string, even if between the
      // `opener` and the subject's current position there are other nodes
      //
      // (first, check for underflows)
      if (parser->options & AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS) {
        fnref->as.literal = aimarkdown_cmark_chunk_dup(&subj->input, opener->position + 1,
                                           initial_pos - opener->position - 2);
        fnref->parent_footnote_def = definition->node;
      } else if ((fnref_start_column + 2) <= fnref_end_column) {
        fnref->as.literal = aimarkdown_cmark_chunk_dup(literal, 1, (fnref_end_column - fnref_start_column) - 2);
      } else {
        fnref->as.literal = aimarkdown_cmark_chunk_dup(literal, 1, 0);
      }

      fnref->start_line = fnref->end_line = subj->line;
      fnref->start_column = fnref_start_column;
      fnref->end_column = fnref_end_column;

      // we then replace the opener with this new fnref node, the net effect
      // being replacing the opening '[' text node with a `^footnote-ref]` node.
      aimarkdown_cmark_node_insert_before(opener->inl_text, fnref);

      process_emphasis(parser, subj, opener->position);
      // sometimes, the footnote reference text gets parsed into multiple nodes
      // i.e. '[^example]' parsed into '[', '^exam', 'ple]'.
      // this happens for ex with the autolink extension. when the autolinker
      // finds the 'w' character, it will split the text into multiple nodes
      // in hopes of being able to match a 'www.' substring.
      //
      // because this function is called one character at a time via the
      // `parse_inlines` function, and the current subj->pos is pointing at the
      // closing ] brace, and because we copy all the text between the [ ]
      // braces, we should be able to safely ignore and delete any nodes after
      // the opener->inl_text->next.
      //
      // therefore, here we walk thru the list and free them all up
      aimarkdown_cmark_node *next_node;
      aimarkdown_cmark_node *current_node = opener->inl_text->next;
      while(current_node) {
        next_node = current_node->next;
        aimarkdown_cmark_node_free(current_node);
        current_node = next_node;
      }

      aimarkdown_cmark_node_free(opener->inl_text);

      pop_bracket(subj);
      return NULL;
    }
  }

noFootnoteMatch:
  pop_bracket(subj); // remove this opener from delimiter list
  subj->pos = initial_pos;
  return make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("]"));

match:
  inl = make_simple(subj->mem, is_image ? AIMARKDOWN_CMARK_NODE_IMAGE : AIMARKDOWN_CMARK_NODE_LINK);
  inl->as.link.url = url;
  inl->as.link.title = title;
  if (usage_type) {
    inl->reference_usage = subj->mem->calloc(1, sizeof(aimarkdown_cmark_reference_usage));
    inl->reference_usage->source = usage_source;
    inl->reference_usage->label = usage_label;
    inl->reference_usage->type = usage_type;
  }
  inl->start_line = opener->inl_text->start_line;
  inl->start_column = opener->inl_text->start_column;
  inl->end_line = subj->line;
  inl->end_column = subj->pos + subj->column_offset + subj->block_offset;
  if (usage_type) {
    math_source_point(subj, opener->position - (is_image ? 2 : 1), &inl->start_line, &inl->start_column);
    int column = inl->end_column + 1;
    math_source_point(subj, subj->pos, &inl->end_line, &column);
    inl->end_column = column - 1;
    subj->line = inl->end_line;
    subj->column_offset = column - 1 - subj->pos - subj->block_offset;
  }
  aimarkdown_cmark_node_insert_before(opener->inl_text, inl);
  // Add link text:
  tmp = opener->inl_text->next;
  while (tmp) {
    tmpnext = tmp->next;
    aimarkdown_cmark_node_unlink(tmp);
    append_child(inl, tmp);
    tmp = tmpnext;
  }

  if (usage_type) {
    // Pending Web entity values are committed only inside an actually matched
    // reference label; direct links and surrounding ordinary text keep legacy values.
    aimarkdown_cmark_node *current = inl->first_child;
    while (current) {
      if (current->type == AIMARKDOWN_CMARK_NODE_TEXT && current->reference_usage && !current->reference_usage->type) {
        aimarkdown_cmark_chunk_free(subj->mem, &current->as.literal);
        current->as.literal = current->reference_usage->label;
        current->reference_usage->label = aimarkdown_cmark_chunk_literal("");
      }
      if (current->first_child) current = current->first_child;
      else { while (current != inl && !current->next) current = current->parent; current = current == inl ? NULL : current->next; }
    }
  }
  // Free the bracket [:
  aimarkdown_cmark_node_free(opener->inl_text);

  process_emphasis(parser, subj, opener->position);
  pop_bracket(subj);

  // Now, if we have a link, we also want to deactivate links until
  // we get a new opener. (This code can be removed if we decide to allow links
  // inside links.)
  if (!is_image) {
    subj->no_link_openers = true;
  }

  return NULL;
}

// Parse a hard or soft linebreak, returning an inline.
// Assumes the subject has a cr or newline at the current position.
static aimarkdown_cmark_node *handle_newline(subject *subj, int options) {
  bufsize_t nlpos = subj->pos;
  // skip over cr, crlf, or lf:
  if (peek_at(subj, subj->pos) == '\r') {
    advance(subj);
  }
  if (peek_at(subj, subj->pos) == '\n') {
    advance(subj);
  }
  ++subj->line;
  subj->column_offset = -subj->pos;
  // skip spaces at beginning of line
  skip_spaces(subj);
  if (nlpos > 1 && peek_at(subj, nlpos - 1) == ' ' &&
      peek_at(subj, nlpos - 2) == ' ') {
    return make_linebreak(subj->mem);
  } else {
    aimarkdown_cmark_node *node = make_softbreak(subj->mem);
    if (options & AIMARKDOWN_CMARK_OPT_SOFTBREAK_METADATA) {
      /* nlpos is this event's consumed inline-subject EOL. Its row was captured
       * by add_line before line-ending normalization, including cross-feed CRLF.
       * No source search, parent range or inferred prefix supplies this value. */
      const aimarkdown_cmark_math_line *row = math_line_at(subj, nlpos);
      if (row) node->softbreak_ending = row->ending;
    }
    return node;
  }
}

// "\r\n\\`&_*[]<!"
void aimarkdown_cmark_set_default_special_chars(int8_t **special_chars, bool use_memcpy) {
  static int8_t default_special_chars[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  if (use_memcpy)
    memcpy(*special_chars, &default_special_chars, 256);
  else
    *special_chars = default_special_chars;
}

// " ' . -
static const char SMART_PUNCT_CHARS[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static bufsize_t subject_find_special_char(aimarkdown_cmark_parser *parser, subject *subj, int options) {
  bufsize_t n = subj->pos + 1;

  while (n < subj->input.len) {
    if ((options & AIMARKDOWN_CMARK_OPT_MATH) && subj->input.data[n] == '$') return n;
    if (parser->special_chars[subj->input.data[n]])
      return n;
    if (options & AIMARKDOWN_CMARK_OPT_SMART && SMART_PUNCT_CHARS[subj->input.data[n]])
      return n;
    n++;
  }

  return subj->input.len;
}

void aimarkdown_cmark_inlines_add_special_character(aimarkdown_cmark_parser *parser, unsigned char c, bool emphasis) {
  parser->special_chars[c] = 1;
  if (emphasis)
    parser->skip_chars[c] = 1;
}

void aimarkdown_cmark_inlines_remove_special_character(aimarkdown_cmark_parser *parser, unsigned char c, bool emphasis) {
  parser->special_chars[c] = 0;
  if (emphasis)
    parser->skip_chars[c] = 0;
}

static aimarkdown_cmark_node *try_extensions(aimarkdown_cmark_parser *parser,
                                  aimarkdown_cmark_node *parent,
                                  unsigned char c,
                                  subject *subj) {
  aimarkdown_cmark_node *res = NULL;
  aimarkdown_cmark_llist *tmp;

  for (tmp = parser->inline_syntax_extensions; tmp; tmp = tmp->next) {
    aimarkdown_cmark_syntax_extension *ext = (aimarkdown_cmark_syntax_extension *) tmp->data;
    res = ext->match_inline(ext, parser, parent, c, subj);

    if (res)
      break;
  }

  return res;
}

// Parse an inline, advancing subject, and add it as a child of parent.
// Return 0 if no inline can be parsed, 1 otherwise.
static int parse_inline(aimarkdown_cmark_parser *parser, subject *subj, aimarkdown_cmark_node *parent, int options) {
  aimarkdown_cmark_node *new_inl = NULL;
  aimarkdown_cmark_chunk contents;
  unsigned char c;
  bufsize_t startpos, endpos;
  bufsize_t initpos = subj->pos;
  c = peek_char(subj);
  if (c == 0) {
    return 0;
  }
  // Web literal emails may start with punctuation owned by builtin parsers.
  // These characters must not be registered as extension delimiters: doing so
  // would redirect emphasis resolution to an extension without an insert hook.
  if (c == '_' || c == '-' || c == '.') {
    for (aimarkdown_cmark_llist *item = parser->inline_syntax_extensions; item; item = item->next) {
      aimarkdown_cmark_syntax_extension *extension = (aimarkdown_cmark_syntax_extension *)item->data;
      if (strcmp(extension->name, "web_autolink") == 0) {
        new_inl = extension->match_inline(extension, parser, parent, c, subj);
        if (new_inl) {
          aimarkdown_cmark_node_append_child(parent, new_inl);
          return 1;
        }
        break;
      }
    }
  }
  switch (c) {
  case '\r':
  case '\n':
    if (options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) {
      advance(subj);
      new_inl = make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_dup(&subj->input, subj->pos - 1, 1));
    } else {
      new_inl = handle_newline(subj, options);
    }
    break;
  case '$':
    if (options & AIMARKDOWN_CMARK_OPT_MATH) new_inl = handle_math(subj, options);
    break;
  case '`':
    new_inl = handle_backticks(subj, options);
    if ((options & AIMARKDOWN_CMARK_OPT_CODE_METADATA) && new_inl->type == AIMARKDOWN_CMARK_NODE_CODE &&
        strcmp(aimarkdown_cmark_node_get_type_string(parent), "table_cell") == 0) {
      aimarkdown_cmark_code_metadata_table(new_inl);
    }
    if ((options & (AIMARKDOWN_CMARK_OPT_MATH | AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES)) && new_inl->type == AIMARKDOWN_CMARK_NODE_CODE &&
        strcmp(aimarkdown_cmark_node_get_type_string(parent), "table_cell") == 0) {
      aimarkdown_cmark_chunk *literal = &new_inl->as.literal;
      bufsize_t w = 0;
      for (bufsize_t r = 0; r < literal->len; r++) {
        if (literal->data[r] == '\\' && r + 1 < literal->len && literal->data[r + 1] == '|') r++;
        literal->data[w++] = literal->data[r];
      }
      literal->len = w;
      if (literal->alloc) literal->data[w] = 0;
    }
    break;
  case '\\':
    new_inl = handle_backslash(parser, subj);
    break;
  case '&':
    new_inl = handle_entity(subj, (options & AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES) != 0, (options & AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES) != 0);
    break;
  case '<':
    new_inl = handle_pointy_brace(subj, options);
    break;
  case '*':
  case '_':
  case '\'':
  case '"':
    new_inl = handle_delim(parser, subj, c, (options & AIMARKDOWN_CMARK_OPT_SMART) != 0);
    break;
  case '-':
    new_inl = handle_hyphen(subj, (options & AIMARKDOWN_CMARK_OPT_SMART) != 0);
    break;
  case '.':
    new_inl = handle_period(subj, (options & AIMARKDOWN_CMARK_OPT_SMART) != 0);
    break;
  case '[':
    advance(subj);
    new_inl = make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_literal("["));
    push_bracket(subj, LINK, new_inl);
    break;
  case ']':
    new_inl = handle_close_bracket(parser, subj);
    break;
  case '!':
    // specifically check for '![' not followed by '^'
    if (peek_char_n(subj, 1) == '[' && peek_char_n(subj, 2) != '^') {
      advance(subj);
      advance(subj);
      new_inl = make_str(subj, subj->pos - 2, subj->pos - 1, aimarkdown_cmark_chunk_literal("!["));
      push_bracket(subj, IMAGE, new_inl);
    }
    break;
  case '^':
    // TODO: Support a name between ^ and [
    // specifically check for '^['
    if (!(options & AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES) && peek_char_n(subj, 1) == '[') {
      advance(subj);
      advance(subj);
      new_inl = make_str(subj, subj->pos - 2, subj->pos - 1, aimarkdown_cmark_chunk_literal("^["));
      push_bracket(subj, ATTRIBUTE, new_inl);
    }
    break;
  }

  if (subj->pos == initpos) {
    if (!new_inl)
      new_inl = try_extensions(parser, parent, c, subj);

    if (!new_inl) {
      endpos = subject_find_special_char(parser, subj, options);
      if (endpos == subj->pos) {
        advance(subj);
        new_inl = make_str(subj, subj->pos - 1, subj->pos - 1, aimarkdown_cmark_chunk_dup(&subj->input, subj->pos - 1, 1));
      } else {
        contents = aimarkdown_cmark_chunk_dup(&subj->input, subj->pos, endpos - subj->pos);
        startpos = subj->pos;
        subj->pos = endpos;

        // if we're at a newline, strip trailing spaces.
        if ((options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) == 0 && S_is_line_end_char(peek_char(subj))) {
          aimarkdown_cmark_chunk_rtrim(&contents);
        }

        new_inl = make_str(subj, startpos, endpos - 1, contents);
      }
    }
  }

  if (new_inl != NULL) {
    append_child(parent, new_inl);
  }

  return 1;
}

// Parse the actual GFM task marker on a complete first paragraph. This avoids
// promoting checkbox body text to a definition and handles byte-feed EOF safely.
static bool web_task_marker(subject *subj, aimarkdown_cmark_node *parent, bufsize_t *body) {
  aimarkdown_cmark_node *item = parent->parent;
  if (parent->type != AIMARKDOWN_CMARK_NODE_PARAGRAPH || !item || item->type != AIMARKDOWN_CMARK_NODE_ITEM ||
      item->first_child != parent || !item->task_extension || subj->input.len < 4 || subj->input.data[0] != '[') return false;
  const unsigned char *data = subj->input.data;
  bool unchecked = data[1] == ' ' || data[1] == '\n' || data[1] == '\r';
  if (data[1] == '\t') {
    const aimarkdown_cmark_math_line *row = math_line_at(subj, 0);
    int column = row ? row->visual_column : parent->start_column;
    unchecked = (column + 1) % 4 == 0;
  }
  if (!(unchecked || data[1] == 'x' || data[1] == 'X') || data[2] != ']') return false;
  if (!(data[3] == ' ' || data[3] == '\t' || data[3] == '\n' || data[3] == '\r')) return false;
  // input has already dropped paragraph-ending whitespace, so anything here
  // after the marker has real paragraph content after the required whitespace.
  aimarkdown_cmark_node_set_syntax_extension(item, item->task_extension);
  item->as.list.checked = data[1] == 'x' || data[1] == 'X';
  item->task_content_line = parent->start_line;
  parent->definition.no_reference = true;
  subj->pos = 4;
  int line = subj->line, column = subj->block_offset + 5;
  math_source_point(subj, subj->pos, &line, &column);
  subj->line = line;
  subj->column_offset = column - 1 - subj->pos - subj->block_offset;
  if (data[3] == '\n' || data[3] == '\r') skip_spaces(subj);
  *body = subj->pos;
  return true;
}

// Parse inlines from parent's string_content, adding as children of parent.
void aimarkdown_cmark_parse_inlines(aimarkdown_cmark_parser *parser,
                         aimarkdown_cmark_node *parent,
                         aimarkdown_cmark_map *refmap,
                         int options) {
  subject subj;
  aimarkdown_cmark_chunk content = {parent->content.ptr, parent->content.size, 0};
  subject_from_buf(parser->mem, parent->start_line, parent->start_column - 1 + parent->internal_offset, &subj, &content, refmap);
  subj.math_parent = parent;
  if ((options & AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE) == 0)
    aimarkdown_cmark_chunk_rtrim(&subj.input);

  bufsize_t task_body = 0;
  bool task = web_task_marker(&subj, parent, &task_body);

  while (!is_eof(&subj) && parse_inline(parser, &subj, parent, options))
    ;

  subj.mem->free(subj.math_last);
  if (subj.web_tick_nodes) subj.mem->free(subj.web_tick_nodes);
  process_emphasis(parser, &subj, 0);
  if (task && parent->first_child && parent->first_child->type == AIMARKDOWN_CMARK_NODE_TEXT) {
    math_source_point(&subj, task_body, &parent->start_line, &parent->start_column);
  }
  // free bracket and delim stack
  while (subj.last_delim) {
    remove_delimiter(&subj, subj.last_delim);
  }
  while (subj.last_bracket) {
    pop_bracket(&subj);
  }
}

// Parse zero or more space characters, including at most one newline.
static void spnl(subject *subj) {
  skip_spaces(subj);
  if (skip_line_end(subj)) {
    skip_spaces(subj);
  }
}

// micromark's label size counts UTF-16 units, excluding line-ending events.
static int reference_label(subject *subj, aimarkdown_cmark_chunk *label) {
  if (peek_char(subj) != '[') return 0;
  advance(subj);
  bufsize_t start = subj->pos;
  int length = 0;
  unsigned char c;
  while ((c = peek_char(subj)) && c != '[' && c != ']') {
    if (c == '\\') {
      advance(subj); length++;
      if (aimarkdown_cmark_ispunct(peek_char(subj))) { advance(subj); length++; }
    } else {
      advance(subj);
      if (c != '\n' && c != '\r' && (c < 0x80 || c >= 0xc0)) length += c >= 0xf0 ? 2 : 1;
    }
    if (length > 999) return 0;
  }
  if (c != ']') return 0;
  *label = aimarkdown_cmark_chunk_dup(&subj->input, start, subj->pos - start);
  while (label->len && (label->data[0] == ' ' || label->data[0] == '\t' || label->data[0] == '\r' || label->data[0] == '\n')) { label->data++; label->len--; }
  while (label->len && (label->data[label->len - 1] == ' ' || label->data[label->len - 1] == '\t' || label->data[label->len - 1] == '\r' || label->data[label->len - 1] == '\n')) label->len--;
  advance(subj);
  return 1;
}

// micromark-factory-destination: definition destinations have unbounded balanced parentheses.
static bufsize_t reference_destination(aimarkdown_cmark_chunk *input, bufsize_t start, aimarkdown_cmark_chunk *out) {
  bufsize_t i = start;
  if (i >= input->len) return -1;
  if (input->data[i] == '<') {
    i++;
    bufsize_t body = i;
    while (i < input->len) {
      unsigned char c = input->data[i];
      if (c == '>') { *out = aimarkdown_cmark_chunk_dup(input, body, i - body); return i + 1 - start; }
      if (c == '<' || c == '\r' || c == '\n') return -1;
      if (c == '\\' && i + 1 < input->len &&
          (input->data[i + 1] == '<' || input->data[i + 1] == '>' || input->data[i + 1] == '\\')) i++;
      i++;
    }
    return -1;
  }
  size_t balance = 0;
  while (i < input->len) {
    unsigned char c = input->data[i];
    if (!balance && (c == ')' || c == ' ' || c == '\t' || c == '\r' || c == '\n')) break;
    if (c == '(') balance++;
    else if (c == ')') balance--;
    else if (c <= 32 || c == 127) return -1;
    if (c == '\\' && i + 1 < input->len &&
        (input->data[i + 1] == '(' || input->data[i + 1] == ')' || input->data[i + 1] == '\\')) i++;
    i++;
  }
  if (i == start || balance) return -1;
  *out = aimarkdown_cmark_chunk_dup(input, start, i - start);
  return i - start;
}

// micromark-factory-title: marker/backslash escapes, continuation prefixes, no blank line.
static bufsize_t reference_title(aimarkdown_cmark_chunk *input, bufsize_t start) {
  if (start >= input->len) return 0;
  unsigned char marker = input->data[start];
  if (marker != '\"' && marker != '\'' && marker != '(') return 0;
  if (marker == '(') marker = ')';
  bufsize_t i = start + 1;
  while (i < input->len) {
    unsigned char c = input->data[i];
    if (c == marker) return i + 1 - start;
    if (c == '\\' && i + 1 < input->len &&
        (input->data[i + 1] == marker || input->data[i + 1] == '\\')) { i += 2; continue; }
    if (c == '\r' || c == '\n') {
      if (c == '\r' && i + 1 < input->len && input->data[i + 1] == '\n') i++;
      i++;
      while (i < input->len && (input->data[i] == ' ' || input->data[i] == '\t')) i++;
      if (i >= input->len || input->data[i] == '\r' || input->data[i] == '\n') return 0;
    } else i++;
  }
  return 0;
}

// Source rows retain physical byte columns and CR/CRLF after cmark's flow normalization.
static const aimarkdown_cmark_math_line *reference_row(aimarkdown_cmark_node *parent, bufsize_t at) {
  size_t lo = 0, hi = parent->math_line_count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (parent->math_lines[mid].offset <= at) lo = mid + 1;
    else hi = mid;
  }
  return lo ? &parent->math_lines[lo - 1] : NULL;
}
// factoryLabel consumes virtual tab spaces as events, not merely source UTF-16.
static int reference_factory_size(aimarkdown_cmark_node *parent, bufsize_t at, const unsigned char *data, bufsize_t size) {
  const aimarkdown_cmark_math_line *row = reference_row(parent, at);
  int line = row ? row->line : parent->start_line;
  bufsize_t offset = row ? row->offset : 0;
  int start_column = row ? row->visual_column : parent->reference_visual_column ? parent->reference_visual_column : parent->start_column + parent->internal_offset;
  if (parent->reference_cached_valid && parent->reference_cached_line == line && parent->reference_cached_offset <= at) {
    offset = parent->reference_cached_offset; start_column = parent->reference_cached_column;
  }
  int column = aimarkdown_cmark_reference_column(start_column, parent->content.ptr + offset, at - offset);
  parent->reference_cached_offset = at; parent->reference_cached_column = column;
  parent->reference_cached_line = line; parent->reference_cached_valid = true;
  int length = 0;
  for (bufsize_t i = 0; i < size; i++) {
    unsigned char c = data[i];
    if (c == '\n') {
      const aimarkdown_cmark_math_line *next = reference_row(parent, at + i + 1);
      if (next && next != row) {
        int end = aimarkdown_cmark_reference_column(next->prefix_visual_column, next->prefix.data, next->prefix.len);
        length += end - next->prefix_visual_column;
        column = next->visual_column; row = next;
      } else column = 1;
    } else if (c != '\r' && (c < 0x80 || c >= 0xc0)) {
      int next = c == '\t' ? column + 4 - ((column - 1) % 4) : column + (c >= 0xf0 ? 2 : 1);
      length += next - column; column = next;
    }
    if (length > 999) return length;
  }
  return length;
}
static aimarkdown_cmark_chunk reference_string(aimarkdown_cmark_mem *mem, aimarkdown_cmark_node *parent,
                                    bufsize_t at, const unsigned char *data, bufsize_t size,
                                    bool decode, bool title) {
  aimarkdown_cmark_strbuf out = AIMARKDOWN_CMARK_BUF_INIT(mem);
  for (bufsize_t i = 0; i < size; i++) {
    unsigned char c = data[i];
    if (c == '\n') {
      const aimarkdown_cmark_math_line *row = reference_row(parent, at + i);
      int ending = row ? row->ending : 10;
      if (ending != 10 && ending != 0) aimarkdown_cmark_strbuf_putc(&out, '\r');
      if (ending != 13) aimarkdown_cmark_strbuf_putc(&out, '\n');
      if (!title) {
        const aimarkdown_cmark_math_line *next = reference_row(parent, at + i + 1);
        if (next && next != row) aimarkdown_cmark_strbuf_put(&out, next->prefix.data, next->prefix.len);
      } else while (i + 1 < size && (data[i + 1] == ' ' || data[i + 1] == '\t')) i++;
    } else if (decode) {
      i += aimarkdown_cmark_decode_string_token_part(&out, data + i, size - i) - 1;
    } else aimarkdown_cmark_strbuf_putc(&out, c);
  }
  return aimarkdown_cmark_chunk_buf_detach(&out);
}
static void reference_point(aimarkdown_cmark_node *parent, bufsize_t at, int *line, int *column) {
  const aimarkdown_cmark_math_line *row = reference_row(parent, at);
  if (row) { *line = row->line; *column = row->column + at - row->offset; }
}

// Parse reference.  Assumes string begins with '[' character.
// Modify refmap if a reference is encountered.
// Return 0 if no reference found, otherwise position of subject
// after reference is parsed.
static bufsize_t parse_reference(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input,
                                aimarkdown_cmark_map *refmap, aimarkdown_cmark_node *parent,
                                bufsize_t base, aimarkdown_cmark_node **definition) {
  subject subj;

  aimarkdown_cmark_chunk lab;
  aimarkdown_cmark_chunk url;
  aimarkdown_cmark_chunk title;

  bufsize_t matchlen = 0;
  bufsize_t beforetitle;

  subject_from_buf(mem, -1, 0, &subj, input, NULL);

  // parse label:
  if (!(parent ? reference_label(&subj, &lab) : link_label(&subj, &lab, false)) || lab.len == 0)
    return 0;

  bufsize_t label_end = subj.pos;

  // colon:
  if (peek_char(&subj) == ':') {
    advance(&subj);
  } else {
    return 0;
  }

  // parse link url:
  spnl(&subj);
  if ((matchlen = parent ? reference_destination(&subj.input, subj.pos, &url) : manual_scan_link_url(&subj.input, subj.pos, &url)) > -1) {
    subj.pos += matchlen;
  } else {
    return 0;
  }

  bufsize_t destination_start = url.data ? (bufsize_t)(url.data - input->data) : subj.pos;
  bufsize_t destination_size = url.len;
  bool has_title = false;
  bufsize_t title_start = 0, title_size = 0;

  // parse optional link_title
  beforetitle = subj.pos;
  spnl(&subj);
  matchlen = subj.pos == beforetitle ? 0 : parent ? reference_title(&subj.input, subj.pos) : scan_link_title(&subj.input, subj.pos);
  if (matchlen) {
    has_title = true; title_start = subj.pos; title_size = matchlen;
    title = aimarkdown_cmark_chunk_dup(&subj.input, subj.pos, matchlen);
    subj.pos += matchlen;
  } else {
    subj.pos = beforetitle;
    title = aimarkdown_cmark_chunk_literal("");
  }

  // parse final spaces and newline:
  skip_spaces(&subj);
  if (!skip_line_end(&subj)) {
    if (matchlen) { // try rewinding before title
      subj.pos = beforetitle;
      has_title = false;
      skip_spaces(&subj);
      if (!skip_line_end(&subj)) {
        return 0;
      }
    } else {
      return 0;
    }
  }
  aimarkdown_cmark_chunk source_label = aimarkdown_cmark_chunk_literal("");
  if (definition) {
    source_label = reference_string(mem, parent, base + 1, input->data + 1, label_end - 2, false, false);
    if (reference_factory_size(parent, base + 1, input->data + 1, label_end - 2) > 999) {
      aimarkdown_cmark_chunk_free(mem, &source_label); return 0;
    }
    int length = 0;
    for (bufsize_t i = 0; i < source_label.len; i++) {
      unsigned char c = source_label.data[i];
      if (c != '\r' && c != '\n' && (c < 0x80 || c >= 0xc0)) length += c >= 0xf0 ? 2 : 1;
      if (length > 999) { aimarkdown_cmark_chunk_free(mem, &source_label); return 0; }
    }
  }
  // insert reference into refmap
  if (!refmap->web_reference_labels) aimarkdown_cmark_reference_create(refmap, &lab, &url, &title);
  if (definition) {
    aimarkdown_cmark_node *node = aimarkdown_cmark_node_new_with_mem(AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION, mem);
    node->reference_definition = mem->calloc(1, sizeof(aimarkdown_cmark_reference_definition));
    aimarkdown_cmark_reference_definition *def = node->reference_definition;
    def->identifier_source = source_label;
    def->label = reference_string(mem, parent, base + 1, input->data + 1, label_end - 2, true, false);
    def->destination = reference_string(mem, parent, base + destination_start, input->data + destination_start, destination_size, true, false);
    def->has_title = has_title;
    if (has_title) {
      def->title = reference_string(mem, parent, base + title_start + 1, input->data + title_start + 1, title_size - 2, true, true);
      def->has_title = def->title.len != 0;
    }
    if (refmap->web_reference_labels) {
      aimarkdown_cmark_chunk quoted_title = def->has_title ? def->title : aimarkdown_cmark_chunk_literal("");
      // Allocate an entry through the existing map path, then replace legacy
      // clean_url/title output with the independently owned Web token values.
      aimarkdown_cmark_reference_create(refmap, &def->identifier_source, &url, &title);
      if (refmap->refs) {
        aimarkdown_cmark_reference *entry = (aimarkdown_cmark_reference *)refmap->refs;
        aimarkdown_cmark_chunk_free(mem, &entry->url); aimarkdown_cmark_chunk_free(mem, &entry->title);
        entry->url = chunk_clone(mem, &def->destination);
        entry->title = chunk_clone(mem, &quoted_title);
        entry->entry.size = entry->url.len + entry->title.len;
      }
    }
    bufsize_t last = subj.pos;
    if (last && input->data[last - 1] == '\n') last--;
    if (last && input->data[last - 1] == '\r') last--;
    reference_point(parent, base, &node->start_line, &node->start_column);
    reference_point(parent, base + last, &node->end_line, &node->end_column);
    node->end_column--;
    *definition = node;
  }
  return subj.pos;
}

bufsize_t aimarkdown_cmark_parse_reference_inline(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input, aimarkdown_cmark_map *refmap) {
  return parse_reference(mem, input, refmap, NULL, 0, NULL);
}
bufsize_t aimarkdown_cmark_parse_reference_definition(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input, aimarkdown_cmark_map *refmap,
                                          aimarkdown_cmark_node *parent, bufsize_t base, aimarkdown_cmark_node **definition) {
  return parse_reference(mem, input, refmap, parent, base, definition);
}

bufsize_t aimarkdown_cmark_parse_reference_attributes_inline(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input,
                                                  aimarkdown_cmark_map *refmap) {
  subject subj;

  aimarkdown_cmark_chunk lab;
  aimarkdown_cmark_chunk attributes;

  bufsize_t matchlen = 0;
  unsigned char c;

  subject_from_buf(mem, -1, 0, &subj, input, NULL);

  // parse attribute label:
  if (!link_label(&subj, &lab, true) || lab.len == 0) {
    return 0;
  }

  // Colon:
  if (peek_char(&subj) == ':') {
    advance(&subj);
  } else {
    return 0;
  }

  // parse attributes
  spnl(&subj);
  // read until next newline
  bufsize_t startpos = subj.pos;
  while ((c = peek_char(&subj)) && !S_is_line_end_char(c)) {
    advance(&subj);
    matchlen++;
  }

  if (matchlen == 0) {
    return 0;
  }

  attributes = aimarkdown_cmark_chunk_dup(&subj.input, startpos, matchlen);

  // parse final spaces and newline:
  skip_spaces(&subj);
  if (!skip_line_end(&subj)) {
    return 0;
  }

  // insert reference into refmap
  aimarkdown_cmark_reference_create_attributes(refmap, &lab, &attributes);
  return subj.pos;
}

unsigned char aimarkdown_cmark_inline_parser_peek_char(aimarkdown_cmark_inline_parser *parser) {
  return peek_char(parser);
}

unsigned char aimarkdown_cmark_inline_parser_peek_at(aimarkdown_cmark_inline_parser *parser, bufsize_t pos) {
  return peek_at(parser, pos);
}

int aimarkdown_cmark_inline_parser_is_eof(aimarkdown_cmark_inline_parser *parser) {
  return is_eof(parser);
}

static char *
my_strndup (const char *s, size_t n)
{
  char *result;
  size_t len = strlen (s);

  if (n < len)
    len = n;

  result = (char *) malloc (len + 1);
  if (!result)
    return 0;

  result[len] = '\0';
  return (char *) memcpy (result, s, len);
}

char *aimarkdown_cmark_inline_parser_take_while(aimarkdown_cmark_inline_parser *parser, aimarkdown_cmark_inline_predicate pred) {
  unsigned char c;
  bufsize_t startpos = parser->pos;
  bufsize_t len = 0;

  while ((c = peek_char(parser)) && (*pred)(c)) {
    advance(parser);
    len++;
  }

  return my_strndup((const char *) parser->input.data + startpos, len);
}

void aimarkdown_cmark_inline_parser_push_delimiter(aimarkdown_cmark_inline_parser *parser,
                                  unsigned char c,
                                  int can_open,
                                  int can_close,
                                  aimarkdown_cmark_node *inl_text) {
  push_delimiter(parser, c, can_open != 0, can_close != 0, inl_text);
}

void aimarkdown_cmark_inline_parser_remove_delimiter(aimarkdown_cmark_inline_parser *parser, delimiter *delim) {
  remove_delimiter(parser, delim);
}

int aimarkdown_cmark_inline_parser_scan_delimiters(aimarkdown_cmark_inline_parser *parser,
                                  int max_delims,
                                  unsigned char c,
                                  int *left_flanking,
                                  int *right_flanking,
                                  int *punct_before,
                                  int *punct_after) {
  int numdelims = 0;
  bufsize_t before_char_pos;
  int32_t after_char = 0;
  int32_t before_char = 0;
  int len;
  bool space_before, space_after;

  if (parser->pos == 0) {
    before_char = 10;
  } else {
    before_char_pos = parser->pos - 1;
    // walk back to the beginning of the UTF_8 sequence:
    while (peek_at(parser, before_char_pos) >> 6 == 2 && before_char_pos > 0) {
      before_char_pos -= 1;
    }
    len = aimarkdown_cmark_utf8proc_iterate(parser->input.data + before_char_pos,
                                 parser->pos - before_char_pos, &before_char);
    if (len == -1) {
      before_char = 10;
    }
  }

  while (peek_char(parser) == c && numdelims < max_delims) {
    numdelims++;
    advance(parser);
  }

  len = aimarkdown_cmark_utf8proc_iterate(parser->input.data + parser->pos,
                               parser->input.len - parser->pos, &after_char);
  if (len == -1) {
    after_char = 10;
  }

  *punct_before = aimarkdown_cmark_utf8proc_is_punctuation(before_char);
  *punct_after = aimarkdown_cmark_utf8proc_is_punctuation(after_char);
  space_before = aimarkdown_cmark_utf8proc_is_space(before_char) != 0;
  space_after = aimarkdown_cmark_utf8proc_is_space(after_char) != 0;

  *left_flanking = numdelims > 0 && !aimarkdown_cmark_utf8proc_is_space(after_char) &&
                  !(*punct_after && !space_before && !*punct_before);
  *right_flanking = numdelims > 0 && !aimarkdown_cmark_utf8proc_is_space(before_char) &&
                  !(*punct_before && !space_after && !*punct_after);

  return numdelims;
}

void aimarkdown_cmark_inline_parser_advance_offset(aimarkdown_cmark_inline_parser *parser) {
  advance(parser);
}

int aimarkdown_cmark_inline_parser_get_offset(aimarkdown_cmark_inline_parser *parser) {
  return parser->pos;
}

void aimarkdown_cmark_inline_parser_set_offset(aimarkdown_cmark_inline_parser *parser, int offset) {
  parser->pos = offset;
}

int aimarkdown_cmark_inline_parser_get_column(aimarkdown_cmark_inline_parser *parser) {
  return parser->pos + 1 + parser->column_offset + parser->block_offset;
}

aimarkdown_cmark_chunk *aimarkdown_cmark_inline_parser_get_chunk(aimarkdown_cmark_inline_parser *parser) {
  return &parser->input;
}

int aimarkdown_cmark_inline_parser_in_bracket(aimarkdown_cmark_inline_parser *parser, int type) {
  bracket *b = parser->last_bracket;
  if (!b) {
    return 0;
  }
  switch (type) {
    case 0:
      return b->in_bracket[LINK];
    case 1:
      return b->in_bracket[IMAGE];
    case 2:
      return b->in_bracket[ATTRIBUTE];
    default:
      return 0;
  }
}

void aimarkdown_cmark_node_unput(aimarkdown_cmark_node *node, int n) {
	node = node->last_child;
	while (n > 0 && node && node->type == AIMARKDOWN_CMARK_NODE_TEXT) {
		if (node->as.literal.len < n) {
			n -= node->as.literal.len;
			node->as.literal.len = 0;
		} else {
			node->as.literal.len -= n;
			n = 0;
		}
		node = node->prev;
	}
}

delimiter *aimarkdown_cmark_inline_parser_get_last_delimiter(aimarkdown_cmark_inline_parser *parser) {
  return parser->last_delim;
}

int aimarkdown_cmark_inline_parser_get_line(aimarkdown_cmark_inline_parser *parser) {
  return parser->line;
}
