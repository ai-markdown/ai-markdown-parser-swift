// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-mutex.h"
#include "aimarkdown-syntax_extension.h"
#include "aimarkdown-registry.h"
#include "aimarkdown-plugin.h"

extern aimarkdown_cmark_mem AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR;

static aimarkdown_cmark_llist *syntax_extensions = NULL;

AIMARKDOWN_CMARK_DEFINE_LOCK(extensions);

void aimarkdown_cmark_register_plugin(aimarkdown_cmark_plugin_init_func reg_fn) {
  aimarkdown_cmark_plugin *plugin = aimarkdown_cmark_plugin_new();

  if (!reg_fn(plugin)) {
    aimarkdown_cmark_plugin_free(plugin);
    return;
  }

  aimarkdown_cmark_llist *syntax_extensions_list = aimarkdown_cmark_plugin_steal_syntax_extensions(plugin),
              *it;

  AIMARKDOWN_CMARK_INITIALIZE_AND_LOCK(extensions);
  
  for (it = syntax_extensions_list; it; it = it->next) {
    syntax_extensions = aimarkdown_cmark_llist_append(&AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR, syntax_extensions, it->data);
  }
  
  AIMARKDOWN_CMARK_UNLOCK(extensions);

  aimarkdown_cmark_llist_free(&AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR, syntax_extensions_list);
  aimarkdown_cmark_plugin_free(plugin);
}

void aimarkdown_cmark_release_plugins(void) {
  AIMARKDOWN_CMARK_INITIALIZE_AND_LOCK(extensions);
  
  if (syntax_extensions) {
    aimarkdown_cmark_llist_free_full(
        &AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR,
        syntax_extensions,
        (aimarkdown_cmark_free_func) aimarkdown_cmark_syntax_extension_free);
    syntax_extensions = NULL;
  }
  
  AIMARKDOWN_CMARK_UNLOCK(extensions);
}

aimarkdown_cmark_llist *aimarkdown_cmark_list_syntax_extensions(aimarkdown_cmark_mem *mem) {
  aimarkdown_cmark_llist *it;
  aimarkdown_cmark_llist *res = NULL;

  AIMARKDOWN_CMARK_INITIALIZE_AND_LOCK(extensions);
  
  for (it = syntax_extensions; it; it = it->next) {
    res = aimarkdown_cmark_llist_append(mem, res, it->data);
  }
  
  AIMARKDOWN_CMARK_UNLOCK(extensions);
  return res;
}

aimarkdown_cmark_syntax_extension *aimarkdown_cmark_find_syntax_extension(const char *name) {
  aimarkdown_cmark_llist *tmp;
  aimarkdown_cmark_syntax_extension *res = NULL;

  AIMARKDOWN_CMARK_INITIALIZE_AND_LOCK(extensions);
  
  for (tmp = syntax_extensions; tmp; tmp = tmp->next) {
    aimarkdown_cmark_syntax_extension *ext = (aimarkdown_cmark_syntax_extension *) tmp->data;
    if (!strcmp(ext->name, name)) {
      res = ext;
      break;
    }
  }
  
  AIMARKDOWN_CMARK_UNLOCK(extensions);
  return res;
}
