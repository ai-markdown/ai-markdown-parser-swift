// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
/* Opt-in list and list-item spread evidence.
 *
 * The block parser records which containers continue, open and close on each
 * physical line, which leaf block consumed the line, and whether the line had
 * an actual line ending. After parsing, the ledger is expanded into the small
 * subset of micromark document events that mdast-util-from-markdown's
 * `prepareList` reads: list and block quote enter/exit, list item prefixes,
 * block quote prefixes, footnote indents, flow-level `lineEnding` and
 * `lineEndingBlank` tokens, and opaque content. The container-exit reordering
 * of micromark-util-subtokenize and the two independent `prepareList` rules are
 * then applied to those events.
 *
 * The result is assigned only when the items discovered by the event pass are
 * exactly the parsed list's items. Otherwise, or for unsupported syntax such
 * as definition lists, the value stays 0 ("not captured"); callers must not
 * substitute a default.
 */

#include <stdlib.h>
#include <string.h>

#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-node.h"
#include "aimarkdown-parser.h"
#include "aimarkdown-iterator.h"

enum {
  R_LINE = 1,
  R_PREFIX_QUOTE,
  R_PREFIX_FOOTNOTE,
  R_OPEN_LIST,
  R_OPEN_ITEM,     /* a: list id, b: 1 when the list existed before this line */
  R_OPEN_QUOTE,
  R_OPEN_FOOTNOTE,
  R_CLOSE,         /* a: exit event type, b: container depth */
  R_LEAF,          /* a: LEAF_* flags */
  R_PREFIX_ONLY,   /* nothing but container prefixes remained on the line */
  R_END_LINE,      /* a: 1 when the line had a physical line ending */
  R_EOF
};

enum {
  LEAF_ABSORBING = 1, /* fenced code, math, HTML kinds 1-5: blank lines stay inside */
  LEAF_INDENTED = 2,  /* indented code: trailing blank lines are flow blank lines */
  LEAF_BLANK = 4,     /* the consumed line was blank after container prefixes */
  LEAF_CLOSED = 8     /* the block's closing condition matched on this line */
};

enum {
  E_LIST_ENTER = 1,
  E_LIST_EXIT,
  E_QUOTE_ENTER,
  E_QUOTE_EXIT,
  E_FOOTNOTE_ENTER,
  E_FOOTNOTE_EXIT,
  E_ITEM_PREFIX,
  E_QUOTE_PREFIX,
  E_FOOTNOTE_PREFIX,
  E_LINE_ENDING,
  E_LINE_ENDING_BLANK,
  E_CONTENT
};

typedef struct {
  int kind, id, a, b;
} ls_record;

typedef struct {
  int type, id;
} ls_event;

struct aimarkdown_cmark_list_spread_ledger {
  ls_record *records;
  size_t count, capacity;
  int next_id;
  bool in_line, unsupported, failed;
};

static struct aimarkdown_cmark_list_spread_ledger *ledger(aimarkdown_cmark_parser *parser) {
  if (!(parser->options & AIMARKDOWN_CMARK_OPT_LIST_SPREAD_METADATA))
    return NULL;
  if (!parser->list_spread)
    parser->list_spread = (struct aimarkdown_cmark_list_spread_ledger *)parser->mem->calloc(
        1, sizeof(struct aimarkdown_cmark_list_spread_ledger));
  return parser->list_spread;
}

static int node_id(struct aimarkdown_cmark_list_spread_ledger *l, aimarkdown_cmark_node *node) {
  if (!node->list_spread_id)
    node->list_spread_id = ++l->next_id;
  return node->list_spread_id;
}

static void push(aimarkdown_cmark_parser *parser, int kind, int id, int a, int b) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  if (!l || l->failed)
    return;
  if (l->count == l->capacity) {
    size_t capacity = l->capacity ? l->capacity * 2 : 256;
    ls_record *records = (ls_record *)parser->mem->realloc(
        l->records, capacity * sizeof(ls_record));
    l->records = records;
    l->capacity = capacity;
  }
  l->records[l->count++] = (ls_record){kind, id, a, b};
}

void aimarkdown_cmark_list_spread_line(aimarkdown_cmark_parser *parser) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  if (!l)
    return;
  if (l->in_line)
    l->unsupported = true; /* re-entrant line processing */
  l->in_line = true;
  push(parser, R_LINE, 0, 0, 0);
}

void aimarkdown_cmark_list_spread_prefix(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  if (!l)
    return;
  if (container->type == AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE)
    push(parser, R_PREFIX_QUOTE, node_id(l, container), 0, 0);
  else if (container->type == AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION)
    push(parser, R_PREFIX_FOOTNOTE, node_id(l, container), 0, 0);
}

void aimarkdown_cmark_list_spread_open(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  if (!l)
    return;
  switch (container->type) {
  case AIMARKDOWN_CMARK_NODE_LIST:
    push(parser, R_OPEN_LIST, node_id(l, container), 0, 0);
    break;
  case AIMARKDOWN_CMARK_NODE_ITEM:
    push(parser, R_OPEN_ITEM, node_id(l, container), node_id(l, container->parent),
         container->parent->start_line < parser->line_number);
    break;
  case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
    push(parser, R_OPEN_QUOTE, node_id(l, container), 0, 0);
    break;
  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
    push(parser, R_OPEN_FOOTNOTE, node_id(l, container), 0, 0);
    break;
  default:
    break;
  }
}

void aimarkdown_cmark_list_spread_close(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *container) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  int type;
  if (!l)
    return;
  switch (container->type) {
  case AIMARKDOWN_CMARK_NODE_LIST:
    type = E_LIST_EXIT;
    break;
  case AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE:
    type = E_QUOTE_EXIT;
    break;
  case AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION:
    type = E_FOOTNOTE_EXIT;
    break;
  default:
    return;
  }
  int depth = 0;
  for (aimarkdown_cmark_node *node = container->parent; node; node = node->parent)
    depth++;
  push(parser, R_CLOSE, node_id(l, container), type, depth);
}

void aimarkdown_cmark_list_spread_leaf(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *leaf, bool closed) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  int flags = 0;
  if (!l)
    return;
  if (leaf->type == AIMARKDOWN_CMARK_NODE_MATH_BLOCK ||
      (leaf->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK && leaf->as.code.fenced) ||
      (leaf->type == AIMARKDOWN_CMARK_NODE_HTML_BLOCK && leaf->as.html_block_type >= 1 &&
       leaf->as.html_block_type <= 5))
    flags |= LEAF_ABSORBING;
  if (leaf->type == AIMARKDOWN_CMARK_NODE_CODE_BLOCK && !leaf->as.code.fenced)
    flags |= LEAF_INDENTED;
  if (parser->blank)
    flags |= LEAF_BLANK;
  if (closed)
    flags |= LEAF_CLOSED;
  push(parser, R_LEAF, node_id(l, leaf), flags, 0);
}

void aimarkdown_cmark_list_spread_prefix_only(aimarkdown_cmark_parser *parser, bool prefix_only) {
  if (prefix_only && ledger(parser))
    push(parser, R_PREFIX_ONLY, 0, 0, 0);
}

void aimarkdown_cmark_list_spread_end_line(aimarkdown_cmark_parser *parser, bool has_line_ending) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  if (!l)
    return;
  l->in_line = false;
  push(parser, R_END_LINE, 0, has_line_ending, 0);
}

void aimarkdown_cmark_list_spread_eof(aimarkdown_cmark_parser *parser) {
  if (ledger(parser))
    push(parser, R_EOF, 0, 0, 0);
}

void aimarkdown_cmark_list_spread_unsupported(aimarkdown_cmark_parser *parser) {
  struct aimarkdown_cmark_list_spread_ledger *l = ledger(parser);
  if (l)
    l->unsupported = true;
}

void aimarkdown_cmark_list_spread_free(aimarkdown_cmark_parser *parser) {
  struct aimarkdown_cmark_list_spread_ledger *l = parser->list_spread;
  if (!l)
    return;
  parser->mem->free(l->records);
  parser->mem->free(l);
  parser->list_spread = NULL;
}

/* ---- Event expansion ---------------------------------------------------- */

typedef struct {
  ls_event *items;
  size_t count, capacity;
  aimarkdown_cmark_mem *mem;
} ls_events;

static void emit(ls_events *events, int type, int id) {
  if (events->count == events->capacity) {
    size_t capacity = events->capacity ? events->capacity * 2 : 256;
    events->items = (ls_event *)events->mem->realloc(events->items,
                                                     capacity * sizeof(ls_event));
    events->capacity = capacity;
  }
  events->items[events->count++] = (ls_event){type, id};
}

typedef struct {
  int last_line, last_content_line, closed_line;
} ls_leaf_info;

/* Emit close records in [begin, end) innermost first, keeping record order
 * among containers at the same depth. */
static void emit_closes(ls_events *events, const ls_record *records, size_t begin,
                        size_t end) {
  int max_depth = -1;
  for (size_t i = begin; i < end; i++)
    if (records[i].kind == R_CLOSE && records[i].b > max_depth)
      max_depth = records[i].b;
  for (int depth = max_depth; depth >= 0; depth--)
    for (size_t i = begin; i < end; i++)
      if (records[i].kind == R_CLOSE && records[i].b == depth)
        emit(events, records[i].a, records[i].id);
}

static void emit_prefixes(ls_events *events, const ls_record *records, size_t begin,
                          size_t end) {
  for (size_t i = begin; i < end; i++) {
    if (records[i].kind == R_PREFIX_QUOTE)
      emit(events, E_QUOTE_PREFIX, records[i].id);
    else if (records[i].kind == R_PREFIX_FOOTNOTE)
      emit(events, E_FOOTNOTE_PREFIX, records[i].id);
  }
}

static void emit_opens(ls_events *events, const ls_record *records, size_t begin,
                       size_t end) {
  for (size_t i = begin; i < end; i++) {
    switch (records[i].kind) {
    case R_OPEN_LIST:
      emit(events, E_LIST_ENTER, records[i].id);
      break;
    case R_OPEN_ITEM:
      emit(events, E_ITEM_PREFIX, records[i].id);
      break;
    case R_OPEN_QUOTE:
      emit(events, E_QUOTE_ENTER, records[i].id);
      emit(events, E_QUOTE_PREFIX, records[i].id);
      break;
    case R_OPEN_FOOTNOTE:
      emit(events, E_FOOTNOTE_ENTER, records[i].id);
      break;
    default:
      break;
    }
  }
}

/* micromark-util-subtokenize: a container exit moves before directly
 * preceding line endings; the first moved ending stays `lineEnding` and later
 * ones become `lineEndingBlank`. Line prefixes and item indents are not
 * represented because they never stop either scan. */
static void move_container_exits(ls_events *events) {
  for (size_t index = 0; index < events->count; index++) {
    int type = events->items[index].type;
    if (type != E_LIST_EXIT && type != E_QUOTE_EXIT && type != E_FOOTNOTE_EXIT)
      continue;
    size_t other = index;
    size_t line_index = 0;
    bool found = false;
    while (other-- > 0) {
      ls_event *event = &events->items[other];
      if (event->type == E_LINE_ENDING || event->type == E_LINE_ENDING_BLANK) {
        if (found)
          events->items[line_index].type = E_LINE_ENDING_BLANK;
        event->type = E_LINE_ENDING;
        line_index = other;
        found = true;
      } else {
        break;
      }
    }
    if (found) {
      ls_event exit = events->items[index];
      memmove(&events->items[line_index + 1], &events->items[line_index],
              (index - line_index) * sizeof(ls_event));
      events->items[line_index] = exit;
    }
  }
}

typedef struct {
  int *items;      /* item ids per list in discovery order */
  size_t count, capacity;
} ls_ids;

static void ids_push(aimarkdown_cmark_mem *mem, ls_ids *ids, int id) {
  if (ids->count == ids->capacity) {
    size_t capacity = ids->capacity ? ids->capacity * 2 : 8;
    ids->items = (int *)mem->realloc(ids->items, capacity * sizeof(int));
    ids->capacity = capacity;
  }
  ids->items[ids->count++] = id;
}

/* mdast-util-from-markdown `prepareList`, without inserting listItem tokens:
 * inserted tokens sit next to an item prefix or inside an already processed
 * inner list, so no later scan of this pass can reach them first. */
static void prepare_list(ls_events *events, size_t start, size_t end, int *spread,
                         ls_ids *items, aimarkdown_cmark_mem *mem) {
  int balance = -1;
  bool list_spread = false;
  int item = 0;
  bool at_marker = false;
  bool has_first_blank = false;
  size_t first_blank = 0;
  for (size_t index = start; index <= end; index++) {
    ls_event *event = &events->items[index];
    switch (event->type) {
    case E_LIST_ENTER:
    case E_QUOTE_ENTER:
      balance++;
      at_marker = false;
      break;
    case E_LIST_EXIT:
    case E_QUOTE_EXIT:
      balance--;
      at_marker = false;
      break;
    case E_LINE_ENDING_BLANK:
      if (item && !at_marker && !balance && !has_first_blank) {
        has_first_blank = true;
        first_blank = index;
      }
      at_marker = false;
      break;
    case E_ITEM_PREFIX:
      break;
    default:
      at_marker = false;
      break;
    }
    if ((!balance && event->type == E_ITEM_PREFIX) ||
        (balance == -1 && event->type == E_LIST_EXIT)) {
      if (item) {
        size_t tail = index;
        bool has_line = false;
        size_t line_index = 0;
        while (tail-- > 0) {
          ls_event *tail_event = &events->items[tail];
          if (tail_event->type == E_LINE_ENDING ||
              tail_event->type == E_LINE_ENDING_BLANK) {
            if (has_line) {
              events->items[line_index].type = E_LINE_ENDING_BLANK;
              list_spread = true;
            }
            tail_event->type = E_LINE_ENDING;
            line_index = tail;
            has_line = true;
          } else if (tail_event->type == E_QUOTE_PREFIX) {
            /* blockQuotePrefix and its parts are skipped. */
          } else {
            break;
          }
        }
        spread[item] = has_first_blank && (!has_line || first_blank < line_index) ? 2 : 1;
      }
      if (event->type == E_ITEM_PREFIX) {
        item = event->id;
        ids_push(mem, items, item);
        has_first_blank = false;
        at_marker = true;
      }
    }
  }
  spread[events->items[start].id] = list_spread ? 2 : 1;
}

static bool has_unsupported_nodes(aimarkdown_cmark_node *root) {
  aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(root);
  aimarkdown_cmark_event_type event;
  bool result = false;
  while ((event = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
    aimarkdown_cmark_node *node = aimarkdown_cmark_iter_get_node(iter);
    if (event == AIMARKDOWN_CMARK_EVENT_ENTER &&
        (node->type == AIMARKDOWN_CMARK_NODE_DEFINITION_LIST ||
         node->type == AIMARKDOWN_CMARK_NODE_DEFINITION_TERM ||
         node->type == AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION)) {
      result = true;
      break;
    }
  }
  aimarkdown_cmark_iter_free(iter);
  return result;
}

void aimarkdown_cmark_list_spread_finish(aimarkdown_cmark_parser *parser, aimarkdown_cmark_node *root) {
  struct aimarkdown_cmark_list_spread_ledger *l = parser->list_spread;
  if (!l)
    return;
  if (l->unsupported || l->failed || l->in_line || has_unsupported_nodes(root)) {
    aimarkdown_cmark_list_spread_free(parser);
    return;
  }
  aimarkdown_cmark_mem *mem = parser->mem;
  const ls_record *records = l->records;
  size_t count = l->count;
  int ids = l->next_id + 1;
  ls_leaf_info *leaves = (ls_leaf_info *)mem->calloc((size_t)ids, sizeof(ls_leaf_info));
  for (int i = 0; i < ids; i++)
    leaves[i] = (ls_leaf_info){-1, -1, -1};

  /* Pass 1: the last line, last non-blank line and closing line of each leaf. */
  int line = -1;
  bool valid = true;
  for (size_t i = 0; i < count && valid; i++) {
    const ls_record *r = &records[i];
    if (r->kind == R_LINE) {
      line++;
    } else if (r->kind == R_LEAF) {
      ls_leaf_info *info = &leaves[r->id];
      info->last_line = line;
      if (!(r->a & LEAF_BLANK) || !(r->a & LEAF_INDENTED))
        info->last_content_line = line;
      if (r->a & LEAF_CLOSED)
        info->closed_line = line;
    }
  }

  /* Pass 2: expand each line into ordered document and flow-boundary events. */
  ls_events events = {NULL, 0, 0, mem};
  line = -1;
  size_t i = 0;
  while (i < count && valid) {
    if (records[i].kind == R_EOF) {
      emit_closes(&events, records, i + 1, count);
      i = count;
      break;
    }
    if (records[i].kind != R_LINE) {
      valid = false;
      break;
    }
    size_t begin = ++i;
    while (i < count && records[i].kind != R_END_LINE && records[i].kind != R_LINE &&
           records[i].kind != R_EOF)
      i++;
    if (i >= count || records[i].kind != R_END_LINE) {
      valid = false;
      break;
    }
    size_t end = i++;
    line++;
    bool new_item_in_list = false, new_container = false, prefix_only = false;
    int leaf = 0, leaf_flags = 0;
    for (size_t j = begin; j < end; j++) {
      const ls_record *r = &records[j];
      if (r->kind == R_OPEN_ITEM && r->b)
        new_item_in_list = true;
      if (r->kind >= R_OPEN_LIST && r->kind <= R_OPEN_FOOTNOTE)
        new_container = true;
      if (r->kind == R_PREFIX_ONLY)
        prefix_only = true;
      if (r->kind == R_LEAF) {
        if (leaf && leaf != r->id)
          valid = false; /* one flow leaf per physical line is expected */
        leaf = r->id;
        leaf_flags |= r->a;
      }
    }
    if (!valid)
      break;
    /* A new item in a continued list closes the previous item's flow before
     * this line's prefixes; another new container exits unmatched containers
     * after continued prefixes; a non-lazy line without new containers places
     * exits after the previous line. */
    bool has_eol = records[end].a != 0;
    /* At end of input with only prefixes on the final line, micromark never
     * starts a flow chunk and exits remaining containers after the prefixes. */
    if ((new_container && !new_item_in_list) ||
        (!new_container && prefix_only && !has_eol)) {
      emit_prefixes(&events, records, begin, end);
      emit_closes(&events, records, begin, end);
    } else {
      emit_closes(&events, records, begin, end);
      emit_prefixes(&events, records, begin, end);
    }
    emit_opens(&events, records, begin, end);

    if (!leaf) {
      if (has_eol)
        emit(&events, E_LINE_ENDING_BLANK, 0);
      continue;
    }
    const ls_leaf_info *info = &leaves[leaf];
    if ((leaf_flags & LEAF_INDENTED) && (leaf_flags & LEAF_BLANK)) {
      if (info->last_content_line > line)
        emit(&events, E_CONTENT, 0);
      else if (has_eol)
        emit(&events, E_LINE_ENDING_BLANK, 0);
      continue;
    }
    emit(&events, E_CONTENT, 0);
    if (!has_eol)
      continue;
    bool flow_ending;
    if (leaf_flags & LEAF_ABSORBING)
      flow_ending = info->closed_line == line;
    else
      flow_ending = info->last_content_line == line;
    emit(&events, flow_ending ? E_LINE_ENDING : E_CONTENT, 0);
  }

  int *spread = NULL;
  ls_ids *list_items = NULL;
  if (valid) {
    move_container_exits(&events);
    spread = (int *)mem->calloc((size_t)ids, sizeof(int));
    list_items = (ls_ids *)mem->calloc((size_t)ids, sizeof(ls_ids));
    /* Lists are prepared in exit order, inner lists first, as in `compile`. */
    size_t *stack = (size_t *)mem->calloc(events.count + 1, sizeof(size_t));
    int *types = (int *)mem->calloc(events.count + 1, sizeof(int));
    size_t depth = 0;
    for (size_t index = 0; index < events.count && valid; index++) {
      int type = events.items[index].type;
      if (type == E_LIST_ENTER || type == E_QUOTE_ENTER || type == E_FOOTNOTE_ENTER) {
        types[depth] = type;
        stack[depth++] = index;
      } else if (type == E_LIST_EXIT || type == E_QUOTE_EXIT || type == E_FOOTNOTE_EXIT) {
        if (!depth || types[depth - 1] != type - 1 ||
            events.items[stack[depth - 1]].id != events.items[index].id) {
          valid = false;
          break;
        }
        depth--;
        if (type == E_LIST_EXIT) {
          ls_ids *ids_for_list = &list_items[events.items[index].id];
          if (ids_for_list->count) {
            valid = false; /* a list must be prepared once */
            break;
          }
          prepare_list(&events, stack[depth], index, spread, ids_for_list, mem);
        }
      }
    }
    if (depth)
      valid = false;
    mem->free(stack);
    mem->free(types);
  }

  if (valid) {
    /* Assign only lists whose discovered items are exactly the parsed items. */
    aimarkdown_cmark_iter *iter = aimarkdown_cmark_iter_new(root);
    aimarkdown_cmark_event_type event;
    while ((event = aimarkdown_cmark_iter_next(iter)) != AIMARKDOWN_CMARK_EVENT_DONE) {
      aimarkdown_cmark_node *node = aimarkdown_cmark_iter_get_node(iter);
      if (event != AIMARKDOWN_CMARK_EVENT_ENTER || node->type != AIMARKDOWN_CMARK_NODE_LIST)
        continue;
      int id = node->list_spread_id;
      if (id <= 0 || id >= ids || !spread[id])
        continue;
      ls_ids *found = &list_items[id];
      size_t index = 0;
      bool matches = true;
      for (aimarkdown_cmark_node *child = node->first_child; child; child = child->next, index++) {
        if (child->type != AIMARKDOWN_CMARK_NODE_ITEM || index >= found->count ||
            child->list_spread_id != found->items[index] || !spread[found->items[index]]) {
          matches = false;
          break;
        }
      }
      if (!matches || index != found->count)
        continue;
      node->list_spread = spread[id];
      for (aimarkdown_cmark_node *child = node->first_child; child; child = child->next)
        child->list_spread = spread[child->list_spread_id];
    }
    aimarkdown_cmark_iter_free(iter);
  }

  if (list_items) {
    for (int id = 0; id < ids; id++)
      mem->free(list_items[id].items);
    mem->free(list_items);
  }
  mem->free(spread);
  mem->free(events.items);
  mem->free(leaves);
  aimarkdown_cmark_list_spread_free(parser);
}
