// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "aimarkdown-cmark-gfm_config.h"
#include "aimarkdown-node.h"
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-iterator.h"

aimarkdown_cmark_iter *aimarkdown_cmark_iter_new(aimarkdown_cmark_node *root) {
  if (root == NULL) {
    return NULL;
  }
  aimarkdown_cmark_mem *mem = root->content.mem;
  aimarkdown_cmark_iter *iter = (aimarkdown_cmark_iter *)mem->calloc(1, sizeof(aimarkdown_cmark_iter));
  iter->mem = mem;
  iter->root = root;
  iter->cur.ev_type = AIMARKDOWN_CMARK_EVENT_NONE;
  iter->cur.node = NULL;
  iter->next.ev_type = AIMARKDOWN_CMARK_EVENT_ENTER;
  iter->next.node = root;
  return iter;
}

void aimarkdown_cmark_iter_free(aimarkdown_cmark_iter *iter) { iter->mem->free(iter); }

static bool S_is_leaf(aimarkdown_cmark_node *node) {
  switch (node->type) {
  case AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION:
  case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
  case AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK:
  case AIMARKDOWN_CMARK_NODE_MATH_BLOCK:
  case AIMARKDOWN_CMARK_NODE_MATH_INLINE:
  case AIMARKDOWN_CMARK_NODE_CODE_BLOCK:
  case AIMARKDOWN_CMARK_NODE_TEXT:
  case AIMARKDOWN_CMARK_NODE_SOFTBREAK:
  case AIMARKDOWN_CMARK_NODE_LINEBREAK:
  case AIMARKDOWN_CMARK_NODE_CODE:
  case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
    return 1;
  }
  return 0;
}

aimarkdown_cmark_event_type aimarkdown_cmark_iter_next(aimarkdown_cmark_iter *iter) {
  aimarkdown_cmark_event_type ev_type = iter->next.ev_type;
  aimarkdown_cmark_node *node = iter->next.node;

  iter->cur.ev_type = ev_type;
  iter->cur.node = node;

  if (ev_type == AIMARKDOWN_CMARK_EVENT_DONE) {
    return ev_type;
  }

  /* roll forward to next item, setting both fields */
  if (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER && !S_is_leaf(node)) {
    if (node->first_child == NULL) {
      /* stay on this node but exit */
      iter->next.ev_type = AIMARKDOWN_CMARK_EVENT_EXIT;
    } else {
      iter->next.ev_type = AIMARKDOWN_CMARK_EVENT_ENTER;
      iter->next.node = node->first_child;
    }
  } else if (node == iter->root) {
    /* don't move past root */
    iter->next.ev_type = AIMARKDOWN_CMARK_EVENT_DONE;
    iter->next.node = NULL;
  } else if (node->next) {
    iter->next.ev_type = AIMARKDOWN_CMARK_EVENT_ENTER;
    iter->next.node = node->next;
  } else if (node->parent) {
    iter->next.ev_type = AIMARKDOWN_CMARK_EVENT_EXIT;
    iter->next.node = node->parent;
  } else {
    assert(false);
    iter->next.ev_type = AIMARKDOWN_CMARK_EVENT_DONE;
    iter->next.node = NULL;
  }

  return ev_type;
}

void aimarkdown_cmark_iter_reset(aimarkdown_cmark_iter *iter, aimarkdown_cmark_node *current,
                      aimarkdown_cmark_event_type event_type) {
  iter->next.ev_type = event_type;
  iter->next.node = current;
  aimarkdown_cmark_iter_next(iter);
}

aimarkdown_cmark_node *aimarkdown_cmark_iter_get_node(aimarkdown_cmark_iter *iter) { return iter->cur.node; }

aimarkdown_cmark_event_type aimarkdown_cmark_iter_get_event_type(aimarkdown_cmark_iter *iter) {
  return iter->cur.ev_type;
}

aimarkdown_cmark_node *aimarkdown_cmark_iter_get_root(aimarkdown_cmark_iter *iter) { return iter->root; }

void aimarkdown_cmark_consolidate_text_nodes(aimarkdown_cmark_node *root) {
  if (root == NULL) {
    return;
  }
  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(root);
  aimarkdown_cmark_strbuf buf = AIMARKDOWN_CMARK_BUF_INIT(iter->mem);
  aimarkdown_cmark_event_type ev_type;
  aimarkdown_cmark_node *cur, *tmp, *next;

  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    if (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER && cur->type == AIMARKDOWN_CMARK_NODE_TEXT &&
        cur->next && cur->next->type == AIMARKDOWN_CMARK_NODE_TEXT) {
      aimarkdown_cmark_strbuf_clear(&buf);
      aimarkdown_cmark_strbuf_put(&buf, cur->as.literal.data, cur->as.literal.len);
      tmp = cur->next;
      while (tmp && tmp->type == AIMARKDOWN_CMARK_NODE_TEXT) {
        aimarkdown_cmark_iter_next(iter); // advance pointer
        aimarkdown_cmark_strbuf_put(&buf, tmp->as.literal.data, tmp->as.literal.len);
        cur->end_column = tmp->end_column;
        next = tmp->next;
        aimarkdown_cmark_node_free(tmp);
        tmp = next;
      }
      aimarkdown_cmark_chunk_free(iter->mem, &cur->as.literal);
      cur->as.literal = aimarkdown_cmark_chunk_buf_detach(&buf);
    }
  }

  aimarkdown_cmark_strbuf_free(&buf);
  aimarkdown_cmark_iter_free(iter);
}

void aimarkdown_cmark_node_own(aimarkdown_cmark_node *root) {
  if (root == NULL) {
    return;
  }
  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(root);
  aimarkdown_cmark_event_type ev_type;
  aimarkdown_cmark_node *cur;

  while ((ev_type = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    cur = aimarkdown_cmark_iter_get_node(iter);
    if (ev_type == AIMARKDOWN_CMARK_EVENT_ENTER) {
      switch (cur->type) {
      case AIMARKDOWN_CMARK_NODE_TEXT:
      case AIMARKDOWN_CMARK_NODE_HTML_INLINE:
      case AIMARKDOWN_CMARK_NODE_CODE:
      case AIMARKDOWN_CMARK_NODE_HTML_BLOCK:
        aimarkdown_cmark_chunk_to_cstr(iter->mem, &cur->as.literal);
        break;
      case AIMARKDOWN_CMARK_NODE_LINK:
        aimarkdown_cmark_chunk_to_cstr(iter->mem, &cur->as.link.url);
        aimarkdown_cmark_chunk_to_cstr(iter->mem, &cur->as.link.title);
        break;
      case AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE:
        aimarkdown_cmark_chunk_to_cstr(iter->mem, &cur->as.custom.on_enter);
        aimarkdown_cmark_chunk_to_cstr(iter->mem, &cur->as.custom.on_exit);
        break;
      }
    }
  }

  aimarkdown_cmark_iter_free(iter);
}
