// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_UTF8_H
#define AIMARKDOWN_CMARK_UTF8_H

#include <stdint.h>
#include "aimarkdown-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_utf8proc_case_fold(aimarkdown_cmark_strbuf *dest, const uint8_t *str,
                              bufsize_t len);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_utf8proc_encode_char(int32_t uc, aimarkdown_cmark_strbuf *buf);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_utf8proc_iterate(const uint8_t *str, bufsize_t str_len, int32_t *dst);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_utf8proc_check(aimarkdown_cmark_strbuf *dest, const uint8_t *line,
                          bufsize_t size);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_utf8proc_is_space(int32_t uc);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_utf8proc_is_punctuation(int32_t uc);

#ifdef __cplusplus
}
#endif

#endif
