// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
/* Port of micromark string-token semantics; provenance and licenses are tracked
 * in AIMarkdown/provenance.json and AIMarkdown/ThirdPartyLicenses. */
#include "aimarkdown-string_token.h"
#include "aimarkdown-houdini.h"
#include "aimarkdown-utf8.h"
#include "aimarkdown-cmark_ctype.h"

// micromark-util-decode-numeric-character-reference and character-reference digit limits.
static bufsize_t string_entity(aimarkdown_cmark_strbuf *out, const unsigned char *data, bufsize_t size) {
  if (!size || data[0] != '#') return aimarkdown_houdini_unescape_ent(out, data, size);
  bufsize_t i = 1;
  int radix = 10, limit = 7, count = 0;
  if (i < size && (data[i] == 'x' || data[i] == 'X')) { radix = 16; limit = 6; i++; }
  int code = 0;
  for (; i < size; i++) {
    unsigned char c = data[i]; int digit = -1;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (radix == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (radix == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    if (digit < 0) break;
    count++; code = code * radix + digit;
    if (code > 0x110000) code = 0x110000;
  }
  if (!count || count > limit || i >= size || data[i] != ';') return 0;
  if (code < 9 || code == 11 || (code > 13 && code < 32) ||
      (code > 126 && code < 160) || (code >= 0xd800 && code < 0xe000) ||
      (code >= 0xfdd0 && code <= 0xfdef) || (code & 0xffff) == 0xffff ||
      (code & 0xffff) == 0xfffe || code > 0x10ffff) code = 0xfffd;
  aimarkdown_cmark_utf8proc_encode_char(code, out);
  return i + 1;
}

bufsize_t aimarkdown_cmark_decode_string_token_part(aimarkdown_cmark_strbuf *out, const unsigned char *data, bufsize_t size) {
  if (!size) return 0;
  if (data[0] == '\\' && size > 1 && aimarkdown_cmark_ispunct(data[1])) {
    aimarkdown_cmark_strbuf_putc(out, data[1]);
    return 2;
  }
  if (data[0] == '&') {
    bufsize_t consumed = string_entity(out, data + 1, size - 1);
    if (consumed) return consumed + 1;
  }
  aimarkdown_cmark_strbuf_putc(out, data[0]);
  return 1;
}

void aimarkdown_cmark_decode_string_token(aimarkdown_cmark_strbuf *out, const unsigned char *data, bufsize_t size) {
  for (bufsize_t i = 0; i < size; ) i += aimarkdown_cmark_decode_string_token_part(out, data + i, size - i);
}

/* Length-aware bridge to the existing single-pass decoder with explicit ownership; parser options are unchanged. */
unsigned char *aimarkdown_cmark_decode_reference_label(const char *source, size_t size, size_t *decoded_size) {
  if (!source || !decoded_size || size > INT32_MAX) return NULL;
  aimarkdown_cmark_mem *mem = aimarkdown_cmark_get_default_mem_allocator();
  aimarkdown_cmark_strbuf out = AIMARKDOWN_CMARK_BUF_INIT(mem);
  aimarkdown_cmark_decode_string_token(&out, (const unsigned char *)source, (bufsize_t)size);
  *decoded_size = (size_t)out.size;
  return aimarkdown_cmark_strbuf_detach(&out);
}
void aimarkdown_cmark_decoded_reference_label_free(unsigned char *value) {
  aimarkdown_cmark_get_default_mem_allocator()->free(value);
}
