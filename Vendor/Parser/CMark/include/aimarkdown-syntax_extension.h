// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_SYNTAX_EXTENSION_H
#define AIMARKDOWN_CMARK_SYNTAX_EXTENSION_H

#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-cmark-gfm-extension_api.h"
#include "aimarkdown-cmark-gfm_config.h"

#include <stdbool.h>

struct aimarkdown_cmark_syntax_extension {
  aimarkdown_cmark_match_block_func          last_block_matches;
  aimarkdown_cmark_open_block_func           try_opening_block;
  aimarkdown_cmark_match_inline_func         match_inline;
  aimarkdown_cmark_inline_from_delim_func    insert_inline_from_delim;
  aimarkdown_cmark_llist                   * special_inline_chars;
  char                          * name;
  void                          * priv;
  bool                            emphasis;
  aimarkdown_cmark_free_func                 free_function;
  aimarkdown_cmark_get_type_string_func      get_type_string_func;
  aimarkdown_cmark_can_contain_func          can_contain_func;
  aimarkdown_cmark_contains_inlines_func     contains_inlines_func;
  aimarkdown_cmark_common_render_func        commonmark_render_func;
  aimarkdown_cmark_common_render_func        plaintext_render_func;
  aimarkdown_cmark_common_render_func        latex_render_func;
  aimarkdown_cmark_xml_attr_func             xml_attr_func;
  aimarkdown_cmark_common_render_func        man_render_func;
  aimarkdown_cmark_html_render_func          html_render_func;
  aimarkdown_cmark_html_filter_func          html_filter_func;
  aimarkdown_cmark_postprocess_func          postprocess_func;
  aimarkdown_cmark_opaque_alloc_func         opaque_alloc_func;
  aimarkdown_cmark_opaque_free_func          opaque_free_func;
  aimarkdown_cmark_commonmark_escape_func    commonmark_escape_func;
};

#endif
