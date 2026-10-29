// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include "aimarkdown-cmark-gfm-core-extensions.h"
#include "aimarkdown-autolink.h"
#include "aimarkdown-mutex.h"
#include "aimarkdown-node.h"
#include "aimarkdown-strikethrough.h"
#include "aimarkdown-highlight.h"
#include "aimarkdown-web-autolink.h"
#include "aimarkdown-table.h"
#include "aimarkdown-tagfilter.h"
#include "aimarkdown-tasklist.h"
#include "aimarkdown-registry.h"
#include "aimarkdown-plugin.h"

static int core_extensions_registration(aimarkdown_cmark_plugin *plugin) {
  aimarkdown_cmark_plugin_register_syntax_extension(plugin, aimarkdown_create_table_extension());
  aimarkdown_cmark_plugin_register_syntax_extension(plugin,
                                         aimarkdown_create_strikethrough_extension());
  aimarkdown_cmark_plugin_register_syntax_extension(plugin, aimarkdown_create_autolink_extension());
  aimarkdown_cmark_plugin_register_syntax_extension(plugin, aimarkdown_create_tagfilter_extension());
  aimarkdown_cmark_plugin_register_syntax_extension(plugin, aimarkdown_create_tasklist_extension());
  aimarkdown_cmark_plugin_register_syntax_extension(plugin, aimarkdown_create_highlight_extension());
  aimarkdown_cmark_plugin_register_syntax_extension(plugin, aimarkdown_create_web_autolink_extension());
  return 1;
}

AIMARKDOWN_CMARK_DEFINE_ONCE(registered);

static void register_plugins(void) {
  aimarkdown_cmark_register_plugin(core_extensions_registration);
}

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_gfm_core_extensions_ensure_registered(void) {
  AIMARKDOWN_CMARK_RUN_ONCE(registered, register_plugins);
}
