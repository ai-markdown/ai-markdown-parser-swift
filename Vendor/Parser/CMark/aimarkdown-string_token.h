// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_STRING_TOKEN_H
#define AIMARKDOWN_CMARK_STRING_TOKEN_H
#include "aimarkdown-buffer.h"
/* Source string-token decoding. Never rescan decoded output. These entry points
 * are intentionally separate from legacy CommonMark code/link decoding. */
bufsize_t aimarkdown_cmark_decode_string_token_part(aimarkdown_cmark_strbuf *out, const unsigned char *data, bufsize_t size);
void aimarkdown_cmark_decode_string_token(aimarkdown_cmark_strbuf *out, const unsigned char *data, bufsize_t size);
#endif
