// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_CMARK_CTYPE_H
#define AIMARKDOWN_CMARK_CMARK_CTYPE_H

#include "aimarkdown-export.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Locale-independent versions of functions from ctype.h.
 * We want cmark to behave the same no matter what the system locale.
 */

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_isspace(char c);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_ispunct(char c);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_isalnum(char c);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_isdigit(char c);

AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_isalpha(char c);

#ifdef __cplusplus
}
#endif

#endif
