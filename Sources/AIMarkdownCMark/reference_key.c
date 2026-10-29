// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/* Fixed Web normalizeIdentifier key: ASCII Markdown whitespace + JS lower/upper.
 * Unicode mapping provenance is recorded by the vendor generator. */
#include "aimarkdown-map.h"
#include "aimarkdown-utf8.h"
#include "reference_keys.inc"
unsigned char *aimarkdown_cmark_reference_key(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *label) {
  aimarkdown_cmark_strbuf out = AIMARKDOWN_CMARK_BUF_INIT(mem);
  bool space = false;
  for (bufsize_t i = 0; i < label->len; ) {
    int32_t code;
    int n = aimarkdown_cmark_utf8proc_iterate(label->data + i, label->len - i, &code);
    if (n < 1) { code = 0xfffd; n = 1; }
    i += n;
    if (code == 9 || code == 10 || code == 13 || code == 32) { space = out.size != 0; continue; }
    if (space) { aimarkdown_cmark_strbuf_putc(&out, ' '); space = false; }
    size_t lo = 0, hi = sizeof(reference_keys) / sizeof(reference_keys[0]);
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (reference_keys[mid].code < code) lo = mid + 1; else hi = mid; }
    if (lo < sizeof(reference_keys) / sizeof(reference_keys[0]) && reference_keys[lo].code == code) aimarkdown_cmark_strbuf_puts(&out, reference_keys[lo].key);
    else aimarkdown_cmark_utf8proc_encode_char(code, &out);
  }
  if (!out.size) { aimarkdown_cmark_strbuf_free(&out); return NULL; }
  return aimarkdown_cmark_strbuf_detach(&out);
}
static unsigned char reference_properties(int32_t code) {
  size_t lo = 0, hi = sizeof(reference_casing_properties) / sizeof(reference_casing_properties[0]);
  while (lo < hi) { size_t mid = lo + (hi - lo) / 2;
    if (code < reference_casing_properties[mid].first) hi = mid;
    else if (code > reference_casing_properties[mid].last) lo = mid + 1;
    else return reference_casing_properties[mid].flags;
  }
  return 0;
}
char *aimarkdown_cmark_reference_identifier(const char *source) {
  aimarkdown_cmark_mem *mem = aimarkdown_cmark_get_default_mem_allocator();
  aimarkdown_cmark_chunk input = aimarkdown_cmark_chunk_literal(source);
  unsigned char *key = aimarkdown_cmark_reference_key(mem, &input);
  if (!key) return NULL;
  size_t size = strlen((const char *)key), count = 0;
  int32_t *codes = mem->calloc(size, sizeof(int32_t));
  for (size_t i = 0; i < size; ) {
    int n = aimarkdown_cmark_utf8proc_iterate(key + i, (bufsize_t)(size - i), &codes[count++]);
    i += (size_t)n;
  }
  bool following = false;
  for (size_t i = count; i > 0; ) {
    int32_t code = codes[--i]; unsigned char flags = reference_properties(code);
    if (code == 0x3a3 && !following) codes[i] = -code;
    if (!(flags & 2)) following = (flags & 1) != 0;
  }
  aimarkdown_cmark_strbuf out = AIMARKDOWN_CMARK_BUF_INIT(mem);
  bool preceding = false;
  for (size_t i = 0; i < count; i++) {
    int32_t code = codes[i];
    if (code < 0) { code = -code; if (preceding) { aimarkdown_cmark_utf8proc_encode_char(0x3c2, &out); goto next; } }
    size_t lo = 0, hi = sizeof(reference_lower_keys) / sizeof(reference_lower_keys[0]);
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (reference_lower_keys[mid].code < code) lo = mid + 1; else hi = mid; }
    if (lo < sizeof(reference_lower_keys) / sizeof(reference_lower_keys[0]) && reference_lower_keys[lo].code == code) aimarkdown_cmark_strbuf_puts(&out, reference_lower_keys[lo].key);
    else aimarkdown_cmark_utf8proc_encode_char(code, &out);
next:
    { unsigned char flags = reference_properties(code); if (!(flags & 2)) preceding = (flags & 1) != 0; }
  }
  mem->free(codes); mem->free(key);
  return (char *)aimarkdown_cmark_strbuf_detach(&out);
}
void aimarkdown_cmark_reference_identifier_free(char *value) { aimarkdown_cmark_get_default_mem_allocator()->free(value); }

int aimarkdown_cmark_reference_column(int column, const unsigned char *data, bufsize_t size) {
  for (bufsize_t i = 0; i < size; i++) {
    unsigned char c = data[i];
    if (c == '\t') column += 4 - ((column - 1) % 4);
    else if (c == '\r' || c == '\n') column = 1;
    else if (c < 0x80 || c >= 0xc0) column += c >= 0xf0 ? 2 : 1;
  }
  return column;
}
