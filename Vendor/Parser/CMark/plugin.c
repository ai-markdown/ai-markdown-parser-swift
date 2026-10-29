// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <stdlib.h>

#include "aimarkdown-plugin.h"

extern aimarkdown_cmark_mem AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR;

int aimarkdown_cmark_plugin_register_syntax_extension(aimarkdown_cmark_plugin    * plugin,
                                        aimarkdown_cmark_syntax_extension * extension) {
  plugin->syntax_extensions = aimarkdown_cmark_llist_append(&AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR, plugin->syntax_extensions, extension);
  return 1;
}

aimarkdown_cmark_plugin *
aimarkdown_cmark_plugin_new(void) {
  aimarkdown_cmark_plugin *res = (aimarkdown_cmark_plugin *) AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR.calloc(1, sizeof(aimarkdown_cmark_plugin));

  res->syntax_extensions = NULL;

  return res;
}

void
aimarkdown_cmark_plugin_free(aimarkdown_cmark_plugin *plugin) {
  aimarkdown_cmark_llist_free_full(&AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR,
                        plugin->syntax_extensions,
                        (aimarkdown_cmark_free_func) aimarkdown_cmark_syntax_extension_free);
  AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR.free(plugin);
}

aimarkdown_cmark_llist *
aimarkdown_cmark_plugin_steal_syntax_extensions(aimarkdown_cmark_plugin *plugin) {
  aimarkdown_cmark_llist *res = plugin->syntax_extensions;

  plugin->syntax_extensions = NULL;
  return res;
}
