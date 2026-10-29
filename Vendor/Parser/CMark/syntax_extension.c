// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-syntax_extension.h"
#include "aimarkdown-buffer.h"

extern aimarkdown_cmark_mem AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR;

static aimarkdown_cmark_mem *_mem = &AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR;

void aimarkdown_cmark_syntax_extension_free(aimarkdown_cmark_mem *mem, aimarkdown_cmark_syntax_extension *extension) {
  if (extension->free_function && extension->priv) {
    extension->free_function(mem, extension->priv);
  }

  aimarkdown_cmark_llist_free(mem, extension->special_inline_chars);
  mem->free(extension->name);
  mem->free(extension);
}

aimarkdown_cmark_syntax_extension *aimarkdown_cmark_syntax_extension_new(const char *name) {
  aimarkdown_cmark_syntax_extension *res = (aimarkdown_cmark_syntax_extension *) _mem->calloc(1, sizeof(aimarkdown_cmark_syntax_extension));
  size_t size = strlen(name) + 1;
  res->name = (char *) _mem->calloc(size, sizeof(char));
#if defined(_WIN32)
  strcpy_s(res->name, size, name);
#else
  strcpy(res->name, name);
#endif
  return res;
}

aimarkdown_cmark_node_type aimarkdown_cmark_syntax_extension_add_node(int is_inline) {
  aimarkdown_cmark_node_type *ref = !is_inline ? &AIMARKDOWN_CMARK_NODE_LAST_BLOCK : &AIMARKDOWN_CMARK_NODE_LAST_INLINE;

  if ((*ref & AIMARKDOWN_CMARK_NODE_VALUE_MASK) == AIMARKDOWN_CMARK_NODE_VALUE_MASK) {
    assert(false);
    return (aimarkdown_cmark_node_type) 0;
  }

  return *ref = (aimarkdown_cmark_node_type) ((int) *ref + 1);
}

void aimarkdown_cmark_syntax_extension_set_emphasis(aimarkdown_cmark_syntax_extension *extension,
                                         int emphasis) {
  extension->emphasis = emphasis == 1;
}

void aimarkdown_cmark_syntax_extension_set_open_block_func(aimarkdown_cmark_syntax_extension *extension,
                                                aimarkdown_cmark_open_block_func func) {
  extension->try_opening_block = func;
}

void aimarkdown_cmark_syntax_extension_set_match_block_func(aimarkdown_cmark_syntax_extension *extension,
                                                 aimarkdown_cmark_match_block_func func) {
  extension->last_block_matches = func;
}

void aimarkdown_cmark_syntax_extension_set_match_inline_func(aimarkdown_cmark_syntax_extension *extension,
                                                  aimarkdown_cmark_match_inline_func func) {
  extension->match_inline = func;
}

void aimarkdown_cmark_syntax_extension_set_inline_from_delim_func(aimarkdown_cmark_syntax_extension *extension,
                                                       aimarkdown_cmark_inline_from_delim_func func) {
  extension->insert_inline_from_delim = func;
}

void aimarkdown_cmark_syntax_extension_set_special_inline_chars(aimarkdown_cmark_syntax_extension *extension,
                                                     aimarkdown_cmark_llist *special_chars) {
  extension->special_inline_chars = special_chars;
}

void aimarkdown_cmark_syntax_extension_set_get_type_string_func(aimarkdown_cmark_syntax_extension *extension,
                                                     aimarkdown_cmark_get_type_string_func func) {
  extension->get_type_string_func = func;
}

void aimarkdown_cmark_syntax_extension_set_can_contain_func(aimarkdown_cmark_syntax_extension *extension,
                                                 aimarkdown_cmark_can_contain_func func) {
  extension->can_contain_func = func;
}

void aimarkdown_cmark_syntax_extension_set_contains_inlines_func(aimarkdown_cmark_syntax_extension *extension,
                                                      aimarkdown_cmark_contains_inlines_func func) {
  extension->contains_inlines_func = func;
}

void aimarkdown_cmark_syntax_extension_set_commonmark_render_func(aimarkdown_cmark_syntax_extension *extension,
                                                       aimarkdown_cmark_common_render_func func) {
  extension->commonmark_render_func = func;
}

void aimarkdown_cmark_syntax_extension_set_plaintext_render_func(aimarkdown_cmark_syntax_extension *extension,
                                                      aimarkdown_cmark_common_render_func func) {
  extension->plaintext_render_func = func;
}

void aimarkdown_cmark_syntax_extension_set_latex_render_func(aimarkdown_cmark_syntax_extension *extension,
                                                  aimarkdown_cmark_common_render_func func) {
  extension->latex_render_func = func;
}

void aimarkdown_cmark_syntax_extension_set_xml_attr_func(aimarkdown_cmark_syntax_extension *extension,
                                              aimarkdown_cmark_xml_attr_func func) {
  extension->xml_attr_func = func;
}

void aimarkdown_cmark_syntax_extension_set_man_render_func(aimarkdown_cmark_syntax_extension *extension,
                                                aimarkdown_cmark_common_render_func func) {
  extension->man_render_func = func;
}

void aimarkdown_cmark_syntax_extension_set_html_render_func(aimarkdown_cmark_syntax_extension *extension,
                                                 aimarkdown_cmark_html_render_func func) {
  extension->html_render_func = func;
}

void aimarkdown_cmark_syntax_extension_set_html_filter_func(aimarkdown_cmark_syntax_extension *extension,
                                                 aimarkdown_cmark_html_filter_func func) {
  extension->html_filter_func = func;
}

void aimarkdown_cmark_syntax_extension_set_postprocess_func(aimarkdown_cmark_syntax_extension *extension,
                                                 aimarkdown_cmark_postprocess_func func) {
  extension->postprocess_func = func;
}

void aimarkdown_cmark_syntax_extension_set_private(aimarkdown_cmark_syntax_extension *extension,
                                        void *priv,
                                        aimarkdown_cmark_free_func free_func) {
  extension->priv = priv;
  extension->free_function = free_func;
}

void *aimarkdown_cmark_syntax_extension_get_private(aimarkdown_cmark_syntax_extension *extension) {
    return extension->priv;
}

void aimarkdown_cmark_syntax_extension_set_opaque_alloc_func(aimarkdown_cmark_syntax_extension *extension,
                                                  aimarkdown_cmark_opaque_alloc_func func) {
  extension->opaque_alloc_func = func;
}

void aimarkdown_cmark_syntax_extension_set_opaque_free_func(aimarkdown_cmark_syntax_extension *extension,
                                                 aimarkdown_cmark_opaque_free_func func) {
  extension->opaque_free_func = func;
}

void aimarkdown_cmark_syntax_extension_set_commonmark_escape_func(aimarkdown_cmark_syntax_extension *extension,
                                                       aimarkdown_cmark_commonmark_escape_func func) {
  extension->commonmark_escape_func = func;
}
