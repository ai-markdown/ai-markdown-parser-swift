// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_PLUGIN_H
#define AIMARKDOWN_CMARK_PLUGIN_H

#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-cmark-gfm-extension_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cmark_plugin:
 *
 * A plugin structure, which should be filled by plugin's
 * init functions.
 */
struct aimarkdown_cmark_plugin {
  aimarkdown_cmark_llist *syntax_extensions;
};

aimarkdown_cmark_llist *
aimarkdown_cmark_plugin_steal_syntax_extensions(aimarkdown_cmark_plugin *plugin);

aimarkdown_cmark_plugin *
aimarkdown_cmark_plugin_new(void);

void
aimarkdown_cmark_plugin_free(aimarkdown_cmark_plugin *plugin);

#ifdef __cplusplus
}
#endif

#endif
