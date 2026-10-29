// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_CONFIG_H
#define AIMARKDOWN_CMARK_CONFIG_H

#ifdef AIMARKDOWN_CMARK_USE_CMAKE_HEADERS
// if the CMake config header exists, use that instead of this Swift package prebuilt one
// we need to undefine the header guard, since config.h uses the same one
#undef AIMARKDOWN_CMARK_CONFIG_H
#include "config.h"
#else

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AIMARKDOWN_CMARK_THREADING
#if defined(__wasi__) && !defined(_REENTRANT)
#define AIMARKDOWN_CMARK_THREADING 0
#else
#define AIMARKDOWN_CMARK_THREADING 1
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* not CMARK_USE_CMAKE_HEADERS */

#endif /* not CMARK_CONFIG_H */
