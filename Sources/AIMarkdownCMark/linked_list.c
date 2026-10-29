// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#include <stdlib.h>

#include "aimarkdown-cmark-gfm.h"

aimarkdown_cmark_llist *aimarkdown_cmark_llist_append(aimarkdown_cmark_mem *mem, aimarkdown_cmark_llist *head, void *data) {
  aimarkdown_cmark_llist *tmp;
  aimarkdown_cmark_llist *new_node = (aimarkdown_cmark_llist *) mem->calloc(1, sizeof(aimarkdown_cmark_llist));

  new_node->data = data;
  new_node->next = NULL;

  if (!head)
    return new_node;

  for (tmp = head; tmp->next; tmp=tmp->next);

  tmp->next = new_node;

  return head;
}

void aimarkdown_cmark_llist_free_full(aimarkdown_cmark_mem *mem, aimarkdown_cmark_llist *head, aimarkdown_cmark_free_func free_func) {
  aimarkdown_cmark_llist *tmp, *prev;

  for (tmp = head; tmp;) {
    if (free_func)
      free_func(mem, tmp->data);

    prev = tmp;
    tmp = tmp->next;
    mem->free(prev);
  }
}

void aimarkdown_cmark_llist_free(aimarkdown_cmark_mem *mem, aimarkdown_cmark_llist *head) {
  aimarkdown_cmark_llist_free_full(mem, head, NULL);
}
