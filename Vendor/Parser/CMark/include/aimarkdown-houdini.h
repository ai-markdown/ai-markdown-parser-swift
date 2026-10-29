// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_HOUDINI_H
#define AIMARKDOWN_CMARK_HOUDINI_H

#include <stdint.h>
#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HOUDINI_USE_LOCALE
#define _isxdigit(c) isxdigit(c)
#define _isdigit(c) isdigit(c)
#else
/*
 * Helper _isdigit methods -- do not trust the current locale
 * */
#define _isxdigit(c) (strchr("0123456789ABCDEFabcdef", (c)) != NULL)
#define _isdigit(c) ((c) >= '0' && (c) <= '9')
#endif

#define HOUDINI_ESCAPED_SIZE(x) (((x)*12) / 10)
#define HOUDINI_UNESCAPED_SIZE(x) (x)

AIMARKDOWN_CMARK_GFM_EXPORT
bufsize_t aimarkdown_houdini_unescape_ent(aimarkdown_cmark_strbuf *ob, const uint8_t *src,
                                      bufsize_t size);
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_houdini_escape_html(aimarkdown_cmark_strbuf *ob, const uint8_t *src,
                               bufsize_t size);
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_houdini_escape_html0(aimarkdown_cmark_strbuf *ob, const uint8_t *src,
                                bufsize_t size, int secure);
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_houdini_unescape_html(aimarkdown_cmark_strbuf *ob, const uint8_t *src,
                                 bufsize_t size);
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_houdini_unescape_html_f(aimarkdown_cmark_strbuf *ob, const uint8_t *src,
                                    bufsize_t size);
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_houdini_escape_href(aimarkdown_cmark_strbuf *ob, const uint8_t *src,
                               bufsize_t size);

#ifdef __cplusplus
}
#endif

#endif
