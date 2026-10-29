// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_BUFFER_H
#define AIMARKDOWN_CMARK_BUFFER_H

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  aimarkdown_cmark_mem *mem;
  unsigned char *ptr;
  bufsize_t asize, size;
} aimarkdown_cmark_strbuf;

extern unsigned char aimarkdown_cmark_strbuf__initbuf[];

#define AIMARKDOWN_CMARK_BUF_INIT(mem)                                                    \
  { mem, aimarkdown_cmark_strbuf__initbuf, 0, 0 }

/**
 * Initialize a cmark_strbuf structure.
 *
 * For the cases where CMARK_BUF_INIT cannot be used to do static
 * initialization.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_init(aimarkdown_cmark_mem *mem, aimarkdown_cmark_strbuf *buf,
                       bufsize_t initial_size);

/**
 * Grow the buffer to hold at least `target_size` bytes.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_grow(aimarkdown_cmark_strbuf *buf, bufsize_t target_size);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_free(aimarkdown_cmark_strbuf *buf);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_swap(aimarkdown_cmark_strbuf *buf_a, aimarkdown_cmark_strbuf *buf_b);

AIMARKDOWN_CMARK_GFM_EXPORT
bufsize_t aimarkdown_cmark_strbuf_len(const aimarkdown_cmark_strbuf *buf);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_strbuf_cmp(const aimarkdown_cmark_strbuf *a, const aimarkdown_cmark_strbuf *b);

AIMARKDOWN_CMARK_GFM_EXPORT
unsigned char *aimarkdown_cmark_strbuf_detach(aimarkdown_cmark_strbuf *buf);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_copy_cstr(char *data, bufsize_t datasize,
                            const aimarkdown_cmark_strbuf *buf);

static inline const char *aimarkdown_cmark_strbuf_cstr(const aimarkdown_cmark_strbuf *buf) {
  return (char *)buf->ptr;
}

#define aimarkdown_cmark_strbuf_at(buf, n) ((buf)->ptr[n])

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_set(aimarkdown_cmark_strbuf *buf, const unsigned char *data,
                      bufsize_t len);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_sets(aimarkdown_cmark_strbuf *buf, const char *string);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_putc(aimarkdown_cmark_strbuf *buf, int c);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_put(aimarkdown_cmark_strbuf *buf, const unsigned char *data,
                      bufsize_t len);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_puts(aimarkdown_cmark_strbuf *buf, const char *string);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_clear(aimarkdown_cmark_strbuf *buf);

AIMARKDOWN_CMARK_GFM_EXPORT
bufsize_t aimarkdown_cmark_strbuf_strchr(const aimarkdown_cmark_strbuf *buf, int c, bufsize_t pos);

AIMARKDOWN_CMARK_GFM_EXPORT
bufsize_t aimarkdown_cmark_strbuf_strrchr(const aimarkdown_cmark_strbuf *buf, int c, bufsize_t pos);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_drop(aimarkdown_cmark_strbuf *buf, bufsize_t n);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_truncate(aimarkdown_cmark_strbuf *buf, bufsize_t len);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_rtrim(aimarkdown_cmark_strbuf *buf);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_trim(aimarkdown_cmark_strbuf *buf);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_normalize_whitespace(aimarkdown_cmark_strbuf *s);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_strbuf_unescape(aimarkdown_cmark_strbuf *s);

#ifdef __cplusplus
}
#endif

#endif
