// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_GFM_EXPORT_H
#define AIMARKDOWN_CMARK_GFM_EXPORT_H

#ifdef AIMARKDOWN_CMARK_GFM_STATIC_DEFINE
#  define AIMARKDOWN_CMARK_GFM_EXPORT
#  define AIMARKDOWN_CMARK_GFM_NO_EXPORT
#else
#  if defined(_WIN32)
#    if defined(libcmark_gfm_EXPORTS)
#      define AIMARKDOWN_CMARK_GFM_EXPORT __declspec(dllexport)
#    else
#      define AIMARKDOWN_CMARK_GFM_EXPORT __declspec(dllimport)
#    endif
#    define AIMARKDOWN_CMARK_GFM_NO_EXPORT
#  else
#    if defined(libcmark_gfm_EXPORTS)
#      define AIMARKDOWN_CMARK_GFM_EXPORT __attribute__((__visibility__("default")))
#    else
#      define AIMARKDOWN_CMARK_GFM_EXPORT __attribute__((__visibility__("default")))
#    endif
#    define AIMARKDOWN_CMARK_GFM_NO_EXPORT __attribute__((__visibility__("hidden")))
#  endif
#endif

#ifndef AIMARKDOWN_CMARK_GFM_DEPRECATED
#  if defined(_WIN32)
#    define AIMARKDOWN_CMARK_GFM_DEPRECATED __declspec(deprecated)
#  else
#    define AIMARKDOWN_CMARK_GFM_DEPRECATED __attribute__ ((__deprecated__))
#  endif
#endif

#ifndef AIMARKDOWN_CMARK_GFM_DEPRECATED_EXPORT
#  define AIMARKDOWN_CMARK_GFM_DEPRECATED_EXPORT AIMARKDOWN_CMARK_GFM_EXPORT AIMARKDOWN_CMARK_GFM_DEPRECATED
#endif

#ifndef AIMARKDOWN_CMARK_GFM_DEPRECATED_NO_EXPORT
#  define AIMARKDOWN_CMARK_GFM_DEPRECATED_NO_EXPORT AIMARKDOWN_CMARK_GFM_NO_EXPORT AIMARKDOWN_CMARK_GFM_DEPRECATED
#endif

#endif /* not CMARK_GFM_EXPORT_H */
