// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_REGISTRY_H
#define AIMARKDOWN_CMARK_REGISTRY_H

#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_register_plugin(aimarkdown_cmark_plugin_init_func reg_fn);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_release_plugins(void);

AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_llist *aimarkdown_cmark_list_syntax_extensions(aimarkdown_cmark_mem *mem);

#ifdef __cplusplus
}
#endif

#endif
