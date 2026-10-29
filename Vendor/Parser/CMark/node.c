// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-mutex.h"
#include "aimarkdown-node.h"
#include "aimarkdown-syntax_extension.h"

AIMARKDOWN_CMARK_DEFINE_LOCK(nextflag)
AIMARKDOWN_CMARK_DEFINE_LOCK(safety)

/**
 * Expensive safety checks are off by default, but can be enabled
 * by calling cmark_enable_safety_checks().
 */
static bool enable_safety_checks = false;

void aimarkdown_cmark_enable_safety_checks(bool enable) {
  AIMARKDOWN_CMARK_INITIALIZE_AND_LOCK(safety);
  enable_safety_checks = enable;
  AIMARKDOWN_CMARK_UNLOCK(safety);
}

/**
 * Check whether safety checks have been enabled in a function to guard access
 * behind a lock.
 */
static bool S_safety_checks_enabled() {
  AIMARKDOWN_CMARK_INITIALIZE_AND_LOCK(safety);
  bool safety_enabled = enable_safety_checks;
  AIMARKDOWN_CMARK_UNLOCK(safety);
  return safety_enabled;
}

static void S_node_unlink(aimarkdown_cmark_node *node);

#define NODE_MEM(node) aimarkdown_cmark_node_mem(node)

void aimarkdown_cmark_register_node_flag(aimarkdown_cmark_node_internal_flags *flags) {
  AIMARKDOWN_CMARK_INITIALIZE_AND_LOCK(nextflag);

  static aimarkdown_cmark_node_internal_flags nextflag = AIMARKDOWN_CMARK_NODE__REGISTER_FIRST;

  // flags should be a pointer to a global variable and this function
  // should only be called once to initialize its value.
  if (*flags) {
    fprintf(stderr, "flag initialization error in cmark_register_node_flag\n");
    abort();
  }

  // Check that we haven't run out of bits.
  if (nextflag == 0) {
    fprintf(stderr, "too many flags in cmark_register_node_flag\n");
    abort();
  }

  *flags = nextflag;
  nextflag <<= 1;

  AIMARKDOWN_CMARK_UNLOCK(nextflag);
}

void aimarkdown_cmark_init_standard_node_flags(void) {}

bool aimarkdown_cmark_node_can_contain_type(aimarkdown_cmark_node *node, aimarkdown_cmark_node_type child_type) {
  if (child_type == AIMARKDOWN_CMARK_NODE_DOCUMENT) {
      return false;
    }

  if (node->extension && node->extension->can_contain_func) {
    return node->extension->can_contain_func(node->extension, node, child_type) != 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_DOCUMENT:
  case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
  case AIMARKDOWN_CMARK_NODE_ITEM:
  case AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION:
    return AIMARKDOWN_CMARK_NODE_TYPE_BLOCK_P(child_type) && child_type != AIMARKDOWN_CMARK_NODE_ITEM;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_LIST:
    return child_type == AIMARKDOWN_CMARK_NODE_DEFINITION_TERM || child_type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION;

  case AIMARKDOWN_CMARK_NODE_LIST:
    return child_type == AIMARKDOWN_CMARK_NODE_ITEM;

  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    return true;

  case AIMARKDOWN_CMARK_NODE_DEFINITION_TERM:
  case AIMARKDOWN_CMARK_NODE_PARAGRAPH:
  case AIMARKDOWN_CMARK_NODE_HEADING:
  case AIMARKDOWN_CMARK_NODE_EMPH:
  case AIMARKDOWN_CMARK_NODE_STRONG:
  case AIMARKDOWN_CMARK_NODE_LINK:
  case AIMARKDOWN_CMARK_NODE_IMAGE:
  case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
    return AIMARKDOWN_CMARK_NODE_TYPE_INLINE_P(child_type);

  default:
    break;
  }

  return false;
}

static bool S_can_contain(aimarkdown_cmark_node *node, aimarkdown_cmark_node *child) {
  if (node == NULL || child == NULL) {
    return false;
  }
  if (NODE_MEM(node) != NODE_MEM(child)) {
    return 0;
  }

  if (S_safety_checks_enabled()) {
    // Verify that child is not an ancestor of node or equal to node.
    aimarkdown_cmark_node *cur = node;
    do {
      if (cur == child) {
        return false;
      }
      cur = cur->parent;
    } while (cur != NULL);
  }

  return aimarkdown_cmark_node_can_contain_type(node, (aimarkdown_cmark_node_type) child->type);
}

aimarkdown_cmark_node *aimarkdown_cmark_node_new_with_mem_and_ext(aimarkdown_cmark_node_type type, aimarkdown_cmark_mem *mem, aimarkdown_cmark_syntax_extension *extension) {
  aimarkdown_cmark_node *node = (aimarkdown_cmark_node *)mem->calloc(1, sizeof(*node));
  aimarkdown_cmark_strbuf_init(mem, &node->content, 0);
  node->type = (uint16_t)type;
  node->extension = extension;

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_HEADING:
    node->as.heading.level = 1;
    break;

  case AIMARKDOWN_CMARK_NODE_LIST: {
    aimarkdown_cmark_list *list = &node->as.list;
    list->list_type = AIMARKDOWN_CMARK_BULLET_LIST;
    list->start = 0;
    list->tight = false;
    break;
  }

  default:
    break;
  }

  if (node->extension && node->extension->opaque_alloc_func) {
    node->extension->opaque_alloc_func(node->extension, mem, node);
  }

  return node;
}

aimarkdown_cmark_node *aimarkdown_cmark_node_new_with_ext(aimarkdown_cmark_node_type type, aimarkdown_cmark_syntax_extension *extension) {
  extern aimarkdown_cmark_mem AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR;
  return aimarkdown_cmark_node_new_with_mem_and_ext(type, &AIMARKDOWN_CMARK_DEFAULT_MEM_ALLOCATOR, extension);
}

aimarkdown_cmark_node *aimarkdown_cmark_node_new_with_mem(aimarkdown_cmark_node_type type, aimarkdown_cmark_mem *mem)
{
  return aimarkdown_cmark_node_new_with_mem_and_ext(type, mem, NULL);
}

int aimarkdown_cmark_node_get_softbreak_ending(aimarkdown_cmark_node *node) {
  return node && node->type == AIMARKDOWN_CMARK_NODE_SOFTBREAK ? node->softbreak_ending : 0;
}

aimarkdown_cmark_node *aimarkdown_cmark_node_new(aimarkdown_cmark_node_type type) {
  return aimarkdown_cmark_node_new_with_ext(type, NULL);
}

static void clear_reference_usage(aimarkdown_cmark_node *node) {
  if (node->reference_usage) {
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->reference_usage->label);
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->reference_usage->source);
    NODE_MEM(node)->free(node->reference_usage);
    node->reference_usage = NULL;
  }
}

static void free_node_as(aimarkdown_cmark_node *node) {
  node->softbreak_ending = 0;
  clear_reference_usage(node);
  if (node->reference_definition) {
    aimarkdown_cmark_reference_definition *def = node->reference_definition;
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &def->label);
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &def->identifier_source);
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &def->destination);
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &def->title);
    NODE_MEM(node)->free(def); node->reference_definition = NULL;
  }
  for (size_t i = 0; i < node->math_line_count; i++) aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->math_lines[i].prefix);
  NODE_MEM(node)->free(node->math_lines);
  node->math_lines = NULL; node->math_line_count = node->math_line_capacity = 0;

  aimarkdown_cmark_code_metadata_clear(node);
  switch (node->type) {
    case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.code.info);
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.code.literal);
      break;
    case AIMARKDOWN_CMARK_NODE_TEXT:
    case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
    case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
  case AIMARKDOWN_CMARK_NODE_CODE:
    case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
    case AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE:
    case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.literal);
      break;
    case AIMARKDOWN_CMARK_NODE_LINK:
    case AIMARKDOWN_CMARK_NODE_IMAGE:
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.link.url);
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.link.title);
      break;
    case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.attribute.attributes);
      break;
    case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.custom.on_enter);
    aimarkdown_cmark_chunk_free(NODE_MEM(node), &node->as.custom.on_exit);
      break;
    default:
      break;
    }
}

// Free a cmark_node list and any children.
static void S_free_nodes(aimarkdown_cmark_node *e) {
  aimarkdown_cmark_node *next;
  while (e != NULL) {
    aimarkdown_cmark_strbuf_free(&e->content);

    if (e->user_data && e->user_data_free_func)
      e->user_data_free_func(NODE_MEM(e), e->user_data);

    if (e->as.opaque && e->extension && e->extension->opaque_free_func)
      e->extension->opaque_free_func(e->extension, NODE_MEM(e), e);

    free_node_as(e);

    if (e->last_child) {
      // Splice children into list
      e->last_child->next = e->next;
      e->next = e->first_child;
    }
    next = e->next;
    NODE_MEM(e)->free(e);
    e = next;
  }
}

void aimarkdown_cmark_node_free(aimarkdown_cmark_node *node) {
  S_node_unlink(node);
  node->next = NULL;
  S_free_nodes(node);
}

aimarkdown_cmark_node_type aimarkdown_cmark_node_get_type(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return AIMARKDOWN_CMARK_NODE_NONE;
  } else {
    return (aimarkdown_cmark_node_type)node->type;
  }
}

int aimarkdown_cmark_node_set_type(aimarkdown_cmark_node * node, aimarkdown_cmark_node_type type) {
  aimarkdown_cmark_node_type initial_type;

  if (type == node->type)
    return 1;

  initial_type = (aimarkdown_cmark_node_type) node->type;
  node->type = (uint16_t)type;

  if (!S_can_contain(node->parent, node)) {
    node->type = (uint16_t)initial_type;
    return 0;
  }

  /* We rollback the type to free the union members appropriately */
  node->type = (uint16_t)initial_type;
  free_node_as(node);

  node->type = (uint16_t)type;

  return 1;
}

const char *aimarkdown_cmark_node_get_type_string(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return "NONE";
  }

  if (node->extension && node->extension->get_type_string_func) {
    return node->extension->get_type_string_func(node->extension, node);
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_NONE:
    return "none";
  case AIMARKDOWN_CMARK_NODE_DOCUMENT:
    return "document";
  case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
    return "block_quote";
  case AIMARKDOWN_CMARK_NODE_LIST:
    return "list";
  case AIMARKDOWN_CMARK_NODE_ITEM:
    return "item";
  case AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION:
    return "link_reference_definition";
  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
    return "math_block";
  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    return "code_block";
  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
    return "html_block";
  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    return "custom_block";
  case AIMARKDOWN_CMARK_NODE_DEFINITION_LIST: return "definition_list";
  case AIMARKDOWN_CMARK_NODE_DEFINITION_TERM: return "definition_term";
  case AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION: return "definition_description";
  case AIMARKDOWN_CMARK_NODE_PARAGRAPH:
    return "paragraph";
  case AIMARKDOWN_CMARK_NODE_HEADING:
    return "heading";
  case AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK:
    return "thematic_break";
  case AIMARKDOWN_CMARK_NODE_TEXT:
    return "text";
  case AIMARKDOWN_CMARK_NODE_SOFTBREAK:
    return "softbreak";
  case AIMARKDOWN_CMARK_NODE_LINEBREAK:
    return "linebreak";
  case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
    return "math_inline";
  case AIMARKDOWN_CMARK_NODE_CODE:
    return "code";
  case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
    return "html_inline";
  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
    return "custom_inline";
  case AIMARKDOWN_CMARK_NODE_EMPH:
    return "emph";
  case AIMARKDOWN_CMARK_NODE_STRONG:
    return "strong";
  case AIMARKDOWN_CMARK_NODE_LINK:
    return "link";
  case AIMARKDOWN_CMARK_NODE_IMAGE:
    return "image";
  case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
    return "attribute";
  }

  return "<unknown>";
}

aimarkdown_cmark_node *aimarkdown_cmark_node_next(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->next;
  }
}

aimarkdown_cmark_node *aimarkdown_cmark_node_previous(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->prev;
  }
}

aimarkdown_cmark_node *aimarkdown_cmark_node_parent(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->parent;
  }
}

aimarkdown_cmark_node *aimarkdown_cmark_node_first_child(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->first_child;
  }
}

aimarkdown_cmark_node *aimarkdown_cmark_node_last_child(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->last_child;
  }
}

aimarkdown_cmark_node *aimarkdown_cmark_node_nth_child(aimarkdown_cmark_node *node, int n) {
  if (node == NULL) {
    return NULL;
  }
  int i = 0;
  aimarkdown_cmark_node *ret = node->first_child;
  while (ret && i < n) {
    ret = ret->next;
    ++i;
  }
  return ret;
}

aimarkdown_cmark_node *aimarkdown_cmark_node_parent_footnote_def(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->parent_footnote_def;
  }
}

void *aimarkdown_cmark_node_get_user_data(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  } else {
    return node->user_data;
  }
}

int aimarkdown_cmark_node_set_user_data(aimarkdown_cmark_node *node, void *user_data) {
  if (node == NULL) {
    return 0;
  }
  node->user_data = user_data;
  return 1;
}

int aimarkdown_cmark_node_set_user_data_free_func(aimarkdown_cmark_node *node,
                                        aimarkdown_cmark_free_func free_func) {
  if (node == NULL) {
    return 0;
  }
  node->user_data_free_func = free_func;
  return 1;
}

const char *aimarkdown_cmark_node_get_literal(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
  case AIMARKDOWN_CMARK_NODE_TEXT:
  case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
  case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
  case AIMARKDOWN_CMARK_NODE_CODE:
  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE:
  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.literal);

  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.code.literal);

  default:
    break;
  }

  return NULL;
}

int aimarkdown_cmark_node_get_backtick_count(aimarkdown_cmark_node *node) {
  return node->backtick_count;
}

int aimarkdown_cmark_node_set_literal(aimarkdown_cmark_node *node, const char *content) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
  case AIMARKDOWN_CMARK_NODE_TEXT:
  case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
  case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
  case AIMARKDOWN_CMARK_NODE_CODE:
  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE:
    aimarkdown_cmark_code_metadata_clear(node);
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.literal, content);
    return 1;

  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
    aimarkdown_cmark_code_metadata_clear(node);
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.code.literal, content);
    return 1;

  default:
    break;
  }

  return 0;
}

const char *aimarkdown_cmark_node_get_string_content(aimarkdown_cmark_node *node) {
  return (char *) node->content.ptr;
}

int aimarkdown_cmark_node_set_string_content(aimarkdown_cmark_node *node, const char *content) {
  aimarkdown_cmark_strbuf_sets(&node->content, content);
  return true;
}

int aimarkdown_cmark_node_get_heading_level(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_HEADING:
    return node->as.heading.level;

  default:
    break;
  }

  return 0;
}

int aimarkdown_cmark_node_set_heading_level(aimarkdown_cmark_node *node, int level) {
  if (node == NULL || level < 1 || level > 6) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_HEADING:
    node->as.heading.level = level;
    return 1;

  default:
    break;
  }

  return 0;
}

aimarkdown_cmark_list_type aimarkdown_cmark_node_get_list_type(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return AIMARKDOWN_CMARK_NO_LIST;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    return node->as.list.list_type;
  } else {
    return AIMARKDOWN_CMARK_NO_LIST;
  }
}

int aimarkdown_cmark_node_set_list_type(aimarkdown_cmark_node *node, aimarkdown_cmark_list_type type) {
  if (!(type == AIMARKDOWN_CMARK_BULLET_LIST || type == AIMARKDOWN_CMARK_ORDERED_LIST)) {
    return 0;
  }

  if (node == NULL) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    node->as.list.list_type = type;
    return 1;
  } else {
    return 0;
  }
}

aimarkdown_cmark_list_marker_type aimarkdown_cmark_node_get_list_marker(aimarkdown_cmark_node *node) {
  if (aimarkdown_cmark_node_get_list_type(node) != AIMARKDOWN_CMARK_BULLET_LIST) {
    return AIMARKDOWN_CMARK_NO_LIST_MARKER;
  }
  
  switch (node->as.list.bullet_char) {
    case '-': return AIMARKDOWN_CMARK_HYPHEN_LIST_MARKER;
    case '+': return AIMARKDOWN_CMARK_PLUS_LIST_MARKER;
    case '*': return AIMARKDOWN_CMARK_ASTERISK_LIST_MARKER;
    default: return AIMARKDOWN_CMARK_NO_LIST_MARKER;
  }
}

int aimarkdown_cmark_node_set_list_marker(aimarkdown_cmark_node *node, aimarkdown_cmark_list_marker_type listMarker) {
  if (!(listMarker == AIMARKDOWN_CMARK_HYPHEN_LIST_MARKER || listMarker == AIMARKDOWN_CMARK_PLUS_LIST_MARKER || listMarker == AIMARKDOWN_CMARK_ASTERISK_LIST_MARKER)) {
    return 0;
  }
  
  if (aimarkdown_cmark_node_get_list_type(node) != AIMARKDOWN_CMARK_BULLET_LIST) {
    return 0;
  }
  
  switch (listMarker) {
    case AIMARKDOWN_CMARK_HYPHEN_LIST_MARKER:
      node->as.list.bullet_char = '-';
      return 1;
    case AIMARKDOWN_CMARK_PLUS_LIST_MARKER:
      node->as.list.bullet_char = '+';
      return 1;
    case AIMARKDOWN_CMARK_ASTERISK_LIST_MARKER:
      node->as.list.bullet_char = '*';
      return 1;
    default:
      return 0;
  }
}

aimarkdown_cmark_delim_type aimarkdown_cmark_node_get_list_delim(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return AIMARKDOWN_CMARK_NO_DELIM;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    return node->as.list.delimiter;
  } else {
    return AIMARKDOWN_CMARK_NO_DELIM;
  }
}

int aimarkdown_cmark_node_set_list_delim(aimarkdown_cmark_node *node, aimarkdown_cmark_delim_type delim) {
  if (!(delim == AIMARKDOWN_CMARK_PERIOD_DELIM || delim == AIMARKDOWN_CMARK_PAREN_DELIM)) {
    return 0;
  }

  if (node == NULL) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    node->as.list.delimiter = delim;
    return 1;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_get_list_start(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    return node->as.list.start;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_set_list_start(aimarkdown_cmark_node *node, int start) {
  if (node == NULL || start < 0) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    node->as.list.start = start;
    return 1;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_get_list_tight(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    return node->as.list.tight;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_set_list_tight(aimarkdown_cmark_node *node, int tight) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_LIST) {
    node->as.list.tight = tight == 1;
    return 1;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_get_item_index(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_ITEM) {
    return node->as.list.start;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_set_item_index(aimarkdown_cmark_node *node, int idx) {
  if (node == NULL || idx < 0) {
    return 0;
  }

  if (node->type == AIMARKDOWN_CMARK_NODE_ITEM) {
    node->as.list.start = idx;
    return 1;
  } else {
    return 0;
  }
}

const char *aimarkdown_cmark_node_get_fence_info(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  if ((node->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK || node->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK)) {
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.code.info);
  } else {
    return NULL;
  }
}

int aimarkdown_cmark_node_set_fence_info(aimarkdown_cmark_node *node, const char *info) {
  if (node == NULL) {
    return 0;
  }

  if ((node->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK || node->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK)) {
    aimarkdown_cmark_code_metadata_clear(node);
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.code.info, info);
    return 1;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_get_fenced(aimarkdown_cmark_node *node, int *length, int *offset, char *character) {
  if (node == NULL) {
    return 0;
  }

  if ((node->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK || node->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK)) {
    *length = node->as.code.fence_length;
    *offset = node->as.code.fence_offset;
    *character = node->as.code.fence_char;
    return node->as.code.fenced;
  } else {
    return 0;
  }
}

int aimarkdown_cmark_node_set_fenced(aimarkdown_cmark_node * node, int fenced,
    int length, int offset, char character) {
  if (node == NULL) {
    return 0;
  }

  if ((node->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK || node->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK)) {
    aimarkdown_cmark_code_metadata_clear(node);
    node->as.code.web_fence_length = 0; /* A mutation is not a parser opener. */
    node->as.code.fenced = (int8_t)fenced;
    node->as.code.fence_length = (uint8_t)length;
    node->as.code.fence_offset = (uint8_t)offset;
    node->as.code.fence_char = character;
    return 1;
  } else {
    return 0;
  }
}

const char *aimarkdown_cmark_node_get_url(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_LINK:
  case AIMARKDOWN_CMARK_NODE_IMAGE:
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.link.url);
  default:
    break;
  }

  return NULL;
}

int aimarkdown_cmark_node_set_url(aimarkdown_cmark_node *node, const char *url) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_LINK:
  case AIMARKDOWN_CMARK_NODE_IMAGE:
    clear_reference_usage(node);
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.link.url, url);
    return 1;
  default:
    break;
  }

  return 0;
}

const char *aimarkdown_cmark_node_get_title(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_LINK:
  case AIMARKDOWN_CMARK_NODE_IMAGE:
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.link.title);
  default:
    break;
  }

  return NULL;
}

int aimarkdown_cmark_node_set_title(aimarkdown_cmark_node *node, const char *title) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_LINK:
  case AIMARKDOWN_CMARK_NODE_IMAGE:
    clear_reference_usage(node);
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.link.title, title);
    return 1;
  default:
    break;
  }

  return 0;
}

const char *aimarkdown_cmark_node_get_attributes(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.attribute.attributes);
  default:
    break;
  }

  return NULL;
}

int aimarkdown_cmark_node_set_attributes(aimarkdown_cmark_node *node, const char *attributes) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_ATTRIBUTE:
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.attribute.attributes, attributes);
    return 1;
  default:
    break;
  }

  return 0;
}

const char *aimarkdown_cmark_node_get_on_enter(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.custom.on_enter);
  default:
    break;
  }

  return NULL;
}

int aimarkdown_cmark_node_set_on_enter(aimarkdown_cmark_node *node, const char *on_enter) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.custom.on_enter, on_enter);
    return 1;
  default:
    break;
  }

  return 0;
}

const char *aimarkdown_cmark_node_get_on_exit(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    return aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->as.custom.on_exit);
  default:
    break;
  }

  return NULL;
}

int aimarkdown_cmark_node_set_on_exit(aimarkdown_cmark_node *node, const char *on_exit) {
  if (node == NULL) {
    return 0;
  }

  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
  case AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK:
    aimarkdown_cmark_chunk_set_cstr(NODE_MEM(node), &node->as.custom.on_exit, on_exit);
    return 1;
  default:
    break;
  }

  return 0;
}

aimarkdown_cmark_syntax_extension *aimarkdown_cmark_node_get_syntax_extension(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return NULL;
  }

  return node->extension;
}

int aimarkdown_cmark_node_set_syntax_extension(aimarkdown_cmark_node *node, aimarkdown_cmark_syntax_extension *extension) {
  if (node == NULL) {
    return 0;
  }

  node->extension = extension;
  return 1;
}

int aimarkdown_cmark_node_get_start_line(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->start_line;
}

int aimarkdown_cmark_node_get_start_column(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->start_column;
}

int aimarkdown_cmark_node_get_end_line(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->end_line;
}

int aimarkdown_cmark_node_get_end_column(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return 0;
  }
  return node->end_column;
}

// Unlink a node without adjusting its next, prev, and parent pointers.
static void S_node_unlink(aimarkdown_cmark_node *node) {
  if (node == NULL) {
    return;
  }

  if (node->prev) {
    node->prev->next = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }

  // Adjust first_child and last_child of parent.
  aimarkdown_cmark_node *parent = node->parent;
  if (parent) {
    if (parent->first_child == node) {
      parent->first_child = node->next;
    }
    if (parent->last_child == node) {
      parent->last_child = node->prev;
    }
  }
}

void aimarkdown_cmark_node_unlink(aimarkdown_cmark_node *node) {
  S_node_unlink(node);

  node->next = NULL;
  node->prev = NULL;
  node->parent = NULL;
}

int aimarkdown_cmark_node_insert_before(aimarkdown_cmark_node *node, aimarkdown_cmark_node *sibling) {
  if (node == NULL || sibling == NULL) {
    return 0;
  }

  if (!node->parent || !S_can_contain(node->parent, sibling)) {
    return 0;
  }

  S_node_unlink(sibling);

  aimarkdown_cmark_node *old_prev = node->prev;

  // Insert 'sibling' between 'old_prev' and 'node'.
  if (old_prev) {
    old_prev->next = sibling;
  }
  sibling->prev = old_prev;
  sibling->next = node;
  node->prev = sibling;

  // Set new parent.
  aimarkdown_cmark_node *parent = node->parent;
  sibling->parent = parent;

  // Adjust first_child of parent if inserted as first child.
  if (parent && !old_prev) {
    parent->first_child = sibling;
  }

  return 1;
}

int aimarkdown_cmark_node_insert_after(aimarkdown_cmark_node *node, aimarkdown_cmark_node *sibling) {
  if (node == NULL || sibling == NULL) {
    return 0;
  }

  if (!node->parent || !S_can_contain(node->parent, sibling)) {
    return 0;
  }

  S_node_unlink(sibling);

  aimarkdown_cmark_node *old_next = node->next;

  // Insert 'sibling' between 'node' and 'old_next'.
  if (old_next) {
    old_next->prev = sibling;
  }
  sibling->next = old_next;
  sibling->prev = node;
  node->next = sibling;

  // Set new parent.
  aimarkdown_cmark_node *parent = node->parent;
  sibling->parent = parent;

  // Adjust last_child of parent if inserted as last child.
  if (parent && !old_next) {
    parent->last_child = sibling;
  }

  return 1;
}

int aimarkdown_cmark_node_replace(aimarkdown_cmark_node *oldnode, aimarkdown_cmark_node *newnode) {
  if (!aimarkdown_cmark_node_insert_before(oldnode, newnode)) {
    return 0;
  }
  aimarkdown_cmark_node_unlink(oldnode);
  return 1;
}

int aimarkdown_cmark_node_prepend_child(aimarkdown_cmark_node *node, aimarkdown_cmark_node *child) {
  if (!S_can_contain(node, child)) {
    return 0;
  }

  S_node_unlink(child);

  aimarkdown_cmark_node *old_first_child = node->first_child;

  child->next = old_first_child;
  child->prev = NULL;
  child->parent = node;
  node->first_child = child;

  if (old_first_child) {
    old_first_child->prev = child;
  } else {
    // Also set last_child if node previously had no children.
    node->last_child = child;
  }

  return 1;
}

int aimarkdown_cmark_node_append_child(aimarkdown_cmark_node *node, aimarkdown_cmark_node *child) {
  if (!S_can_contain(node, child)) {
    return 0;
  }

  S_node_unlink(child);

  aimarkdown_cmark_node *old_last_child = node->last_child;

  child->next = NULL;
  child->prev = old_last_child;
  child->parent = node;
  node->last_child = child;

  if (old_last_child) {
    old_last_child->next = child;
  } else {
    // Also set first_child if node previously had no children.
    node->first_child = child;
  }

  return 1;
}

static void S_print_error(FILE *out, aimarkdown_cmark_node *node, const char *elem) {
  if (out == NULL) {
    return;
  }
  fprintf(out, "Invalid '%s' in node type %s at %d:%d\n", elem,
          aimarkdown_cmark_node_get_type_string(node), node->start_line,
          node->start_column);
}

int aimarkdown_cmark_node_check(aimarkdown_cmark_node *node, FILE *out) {
  aimarkdown_cmark_node *cur;
  int errors = 0;

  if (!node) {
    return 0;
  }

  cur = node;
  for (;;) {
    if (cur->first_child) {
      if (cur->first_child->prev != NULL) {
        S_print_error(out, cur->first_child, "prev");
        cur->first_child->prev = NULL;
        ++errors;
      }
      if (cur->first_child->parent != cur) {
        S_print_error(out, cur->first_child, "parent");
        cur->first_child->parent = cur;
        ++errors;
      }
      cur = cur->first_child;
      continue;
    }

  next_sibling:
    if (cur == node) {
      break;
    }
    if (cur->next) {
      if (cur->next->prev != cur) {
        S_print_error(out, cur->next, "prev");
        cur->next->prev = cur;
        ++errors;
      }
      if (cur->next->parent != cur->parent) {
        S_print_error(out, cur->next, "parent");
        cur->next->parent = cur->parent;
        ++errors;
      }
      cur = cur->next;
      continue;
    }

    if (cur->parent->last_child != cur) {
      S_print_error(out, cur->parent, "last_child");
      cur->parent->last_child = cur;
      ++errors;
    }
    cur = cur->parent;
    goto next_sibling;
  }

  return errors;
}

int aimarkdown_cmark_node_get_definition_spread(aimarkdown_cmark_node *node) {
  return node && node->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION && node->definition.spread;
}

const char *aimarkdown_cmark_node_get_reference_label(aimarkdown_cmark_node *node) {
  return node && node->reference_definition ? aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->reference_definition->label) : NULL;
}
const char *aimarkdown_cmark_node_get_reference_identifier_source(aimarkdown_cmark_node *node) {
  return node && node->reference_definition ? aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->reference_definition->identifier_source) : NULL;
}
const char *aimarkdown_cmark_node_get_reference_destination(aimarkdown_cmark_node *node) {
  return node && node->reference_definition ? aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->reference_definition->destination) : NULL;
}
const char *aimarkdown_cmark_node_get_reference_title(aimarkdown_cmark_node *node) {
  return node && node->reference_definition && node->reference_definition->has_title ? aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->reference_definition->title) : NULL;
}

const char *aimarkdown_cmark_node_get_reference_usage_label(aimarkdown_cmark_node *node) {
  return node && node->reference_usage && node->reference_usage->type ? aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->reference_usage->label) : NULL;
}
const char *aimarkdown_cmark_node_get_reference_usage_source(aimarkdown_cmark_node *node) {
  return node && node->reference_usage && node->reference_usage->type ? aimarkdown_cmark_chunk_to_cstr(NODE_MEM(node), &node->reference_usage->source) : NULL;
}
int aimarkdown_cmark_node_get_reference_usage_type(aimarkdown_cmark_node *node) { return node && node->reference_usage ? node->reference_usage->type : 0; }
