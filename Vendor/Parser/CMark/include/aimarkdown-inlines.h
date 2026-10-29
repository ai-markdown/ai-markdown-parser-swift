// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#ifndef AIMARKDOWN_CMARK_INLINES_H
#define AIMARKDOWN_CMARK_INLINES_H

#include <stdbool.h>
#include <stdlib.h>

#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-references.h"

#ifdef __cplusplus
extern "C" {
#endif

aimarkdown_cmark_chunk aimarkdown_cmark_clean_url_web(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *url);
aimarkdown_cmark_chunk aimarkdown_cmark_clean_title_web(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *title);
aimarkdown_cmark_chunk aimarkdown_cmark_clean_url(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *url);
aimarkdown_cmark_chunk aimarkdown_cmark_clean_title(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *title);
aimarkdown_cmark_chunk aimarkdown_cmark_clean_attributes(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *attributes);

AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_parse_inlines(aimarkdown_cmark_parser *parser,
                         aimarkdown_cmark_node *parent,
                         aimarkdown_cmark_map *refmap,
                         int options);

bufsize_t aimarkdown_cmark_parse_reference_definition(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input,
                                          aimarkdown_cmark_map *refmap, aimarkdown_cmark_node *parent,
                                          bufsize_t base, aimarkdown_cmark_node **definition);

bufsize_t aimarkdown_cmark_parse_reference_inline(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input,
                                       aimarkdown_cmark_map *refmap);

bufsize_t aimarkdown_cmark_parse_reference_attributes_inline(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *input,
                                                  aimarkdown_cmark_map *refmap);

void aimarkdown_cmark_inlines_add_special_character(aimarkdown_cmark_parser *parser, unsigned char c, bool emphasis);
void aimarkdown_cmark_inlines_remove_special_character(aimarkdown_cmark_parser *parser, unsigned char c, bool emphasis);

void aimarkdown_cmark_set_default_skip_chars(int8_t **skip_chars, bool use_memcpy);
void aimarkdown_cmark_set_default_special_chars(int8_t **special_chars, bool use_memcpy);

#ifdef __cplusplus
}
#endif

#endif
