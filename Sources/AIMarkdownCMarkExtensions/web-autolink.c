// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/* Web micromark-extension-gfm-autolink-literal 2.1.0 syntax.js state rules.
 * Copyright (c) 2020 Titus Wormer. MIT; see bundled license.
 * Adapted for cmark inline extension ownership and UTF-8 source coordinates.
 * This scanner consumes original source before emphasis/highlight delimiters.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "aimarkdown-web-autolink.h"
#include "aimarkdown-parser.h"
#include "aimarkdown-utf8.h"
#include "highlight-unicode.inc"

static bool alpha(int32_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool alnum(int32_t c) { return alpha(c) || (c >= '0' && c <= '9'); }
static bool atext(int32_t c) { return alnum(c) || c == '+' || c == '-' || c == '.' || c == '_'; }
static bool whitespace(int32_t c) {
  return c < 0 || (c >= 9 && c <= 13) || c == 32 || c == 160 || c == 0x1680 ||
    (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
    c == 0x205f || c == 0x3000 || c == 0xfeff;
}
static bool punctuation(int32_t c) {
  if (c < 0 || c > 0xffff) return false; /* JS observes UTF-16 code units. */
  size_t lo = 0, hi = sizeof(highlight_punctuation) / sizeof(highlight_punctuation[0]);
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (c < highlight_punctuation[mid][0]) hi = mid;
    else if (c > highlight_punctuation[mid][1]) lo = mid + 1;
    else return true;
  }
  return false;
}
static int32_t code_at(aimarkdown_cmark_chunk *input, int offset, int *size) {
  if (offset >= input->len) { *size = 0; return -1; }
  int32_t c = 0;
  int n = aimarkdown_cmark_utf8proc_iterate(input->data + offset, input->len - offset, &c);
  *size = n > 0 ? n : 1;
  return n > 0 ? c : 0xfffd;
}
static int32_t previous(aimarkdown_cmark_chunk *input, int offset) {
  if (offset <= 0) return -1;
  int p = offset - 1;
  while (p > 0 && (input->data[p] & 0xc0) == 0x80) p--;
  int size;
  return code_at(input, p, &size);
}
static bool regular_trail(int32_t c) {
  return c > 0 && c < 128 && strchr("!\"')*,.:;?_~", c) != NULL;
}
/* tokenizeTrail: true means punctuation is followed by a terminal boundary. */
static bool trail(aimarkdown_cmark_chunk *input, int offset) {
  while (true) {
    int size; int32_t c = code_at(input, offset, &size);
    if (regular_trail(c)) { offset += size; continue; }
    if (c == '&') {
      offset++;
      c = code_at(input, offset, &size);
      if (!alpha(c)) return false;
      do { offset += size; c = code_at(input, offset, &size); } while (alpha(c));
      if (c != ';') return false;
      offset++; continue;
    }
    if (c == ']') {
      offset++;
      c = code_at(input, offset, &size);
      if (c == '(' || c == '[' || whitespace(c)) return true;
      continue;
    }
    return c == '<' || whitespace(c);
  }
}
/* tokenizeDomain: underscores are forbidden in the last two segments. */
static int domain(aimarkdown_cmark_chunk *input, int start) {
  int p = start;
  bool underscore = false, previous_underscore = false, seen = false;
  while (true) {
    int size; int32_t c = code_at(input, p, &size);
    if (c == '.' || c == '_') {
      if (trail(input, p)) break;
      if (c == '_') underscore = true;
      else { previous_underscore = underscore; underscore = false; }
      p += size; continue;
    }
    if (whitespace(c) || (c != '-' && punctuation(c))) break;
    seen = true;
    p += size;
  }
  return seen && !underscore && !previous_underscore ? p : -1;
}
/* tokenizePath preserves balanced ')' and tests the original, undecoded trail. */
static int path(aimarkdown_cmark_chunk *input, int start) {
  int p = start, opened = 0, closed = 0;
  while (true) {
    int size; int32_t c = code_at(input, p, &size);
    if (c == '(') { opened++; p += size; continue; }
    if (c == ')' && closed < opened) { closed++; p += size; continue; }
    if (regular_trail(c) || c == '&' || c == '<' || c == ']') {
      if (trail(input, p)) return p;
      if (c == ')') closed++;
      p += size; continue;
    }
    if (whitespace(c)) return p;
    p += size;
  }
}
static bool previous_www(int32_t c) {
  /* micromark markdownLineEndingOrSpace includes tab, LF, CR, and space only. */
  return c < 0 || c == '(' || c == '*' || c == '_' || c == '[' || c == ']' ||
    c == '~' || c == 9 || c == 10 || c == 13 || c == 32;
}
static int email(aimarkdown_cmark_chunk *input, int start, int32_t before) {
  if (before == '/' || atext(before)) return -1;
  int p = start;
  while (p < input->len && atext(input->data[p])) p++;
  if (p == start || p >= input->len || input->data[p] != '@') return -1;
  p++;
  bool dot = false, data = false;
  while (p < input->len) {
    int c = input->data[p];
    if (c == '.') {
      if (p + 1 >= input->len || !alnum(input->data[p + 1])) break;
      dot = true; p++; continue;
    }
    if (c == '-' || c == '_' || alnum(c)) { data = true; p++; continue; }
    break;
  }
  return data && dot && alpha(input->data[p - 1]) ? p : -1;
}
static int url(aimarkdown_cmark_chunk *input, int start, int32_t before, bool *www) {
  int p = start;
  *www = false;
  if ((input->data[p] == 'w' || input->data[p] == 'W') && previous_www(before)) {
    if (input->len - p <= 4) return -1;
    for (int i = 0; i < 3; i++) if (input->data[p+i] != 'w' && input->data[p+i] != 'W') return -1;
    if (input->data[p+3] != '.') return -1;
    *www = true;
  } else {
    if (alpha(before)) return -1;
    int length = 0;
    const char *scheme = "https";
    while (p < input->len && alpha(input->data[p]) && length < 5) {
      int lower = input->data[p] | 32;
      if (lower != scheme[length]) return -1;
      length++; p++;
    }
    if (length != 4 && length != 5) return -1;
    if (input->len - p < 3 || memcmp(input->data+p, "://", 3)) return -1;
    p += 3;
    int size; int32_t c = code_at(input,p,&size);
    if (c < 32 || c == 127 || whitespace(c) || punctuation(c)) return -1;
  }
  int end = domain(input, p);
  return end < 0 ? -1 : path(input,end);
}
static aimarkdown_cmark_node *match(aimarkdown_cmark_syntax_extension *extension, aimarkdown_cmark_parser *parser,
                        aimarkdown_cmark_node *parent, unsigned char character,
                        aimarkdown_cmark_inline_parser *inline_parser) {
  /* An open link/image label owns this source, matching previousUnbalanced. */
  if (aimarkdown_cmark_inline_parser_in_bracket(inline_parser,0) || aimarkdown_cmark_inline_parser_in_bracket(inline_parser,1)) return NULL;
  aimarkdown_cmark_chunk *input = aimarkdown_cmark_inline_parser_get_chunk(inline_parser);
  int start = aimarkdown_cmark_inline_parser_get_offset(inline_parser);
  int32_t before = previous(input,start);
  int end = email(input,start,before);
  bool is_email = end >= 0, is_www = false;
  if (end < 0 && (character == 'h' || character == 'H' || character == 'w' || character == 'W'))
    end = url(input,start,before,&is_www);
  if (end < 0) return NULL;
  int column = aimarkdown_cmark_inline_parser_get_column(inline_parser);
  aimarkdown_cmark_node *node = aimarkdown_cmark_node_new_with_mem(AIMARKDOWN_CMARK_NODE_LINK,parser->mem);
  aimarkdown_cmark_node *text = aimarkdown_cmark_node_new_with_mem(AIMARKDOWN_CMARK_NODE_TEXT,parser->mem);
  aimarkdown_cmark_strbuf destination;
  aimarkdown_cmark_strbuf_init(parser->mem,&destination,32);
  if (is_email) aimarkdown_cmark_strbuf_puts(&destination,"mailto:");
  else if (is_www) aimarkdown_cmark_strbuf_puts(&destination,"http://");
  aimarkdown_cmark_strbuf_put(&destination,input->data+start,end-start);
  node->as.link.url = aimarkdown_cmark_chunk_buf_detach(&destination);
  text->as.literal = aimarkdown_cmark_chunk_dup(input,start,end-start);
  (void)aimarkdown_cmark_chunk_to_cstr(parser->mem,&text->as.literal);
  aimarkdown_cmark_node_append_child(node,text);
  /* Preserve the matched token span before inline-subject coordinates are
   * mapped through each physical content line, including container prefixes.
   * A leading underscore belongs to Web attention; its later transformed link
   * is intentionally not presented as a positioned Web literal token. */
  if ((parser->options & AIMARKDOWN_CMARK_OPT_WEB_POSITIONS) && character != '_') {
    node->web_subject_start = text->web_subject_start = start;
    node->web_subject_end = text->web_subject_end = end;
    node->web_position_flags = text->web_position_flags = AIMARKDOWN_CMARK_WEB_POSITION_SUBJECT;
  }
  node->start_line = text->start_line = node->end_line = text->end_line = aimarkdown_cmark_inline_parser_get_line(inline_parser);
  node->start_column = text->start_column = column;
  node->end_column = text->end_column = column + end - start - 1;
  aimarkdown_cmark_inline_parser_set_offset(inline_parser,end);
  return node;
}
aimarkdown_cmark_syntax_extension *aimarkdown_create_web_autolink_extension(void) {
  aimarkdown_cmark_syntax_extension *extension = aimarkdown_cmark_syntax_extension_new("web_autolink");
  aimarkdown_cmark_syntax_extension_set_match_inline_func(extension,match);
  aimarkdown_cmark_llist *characters = NULL;
  aimarkdown_cmark_mem *mem = aimarkdown_cmark_get_default_mem_allocator();
  for (int c = 0; c < 128; c++) if (alnum(c) || c == '+' || c == '-' || c == '.') characters = aimarkdown_cmark_llist_append(mem,characters,(void *)(intptr_t)c);
  aimarkdown_cmark_syntax_extension_set_special_inline_chars(extension,characters);
  return extension;
}
