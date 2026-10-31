// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_GFM_H
#define AIMARKDOWN_CMARK_GFM_H

#include <stdio.h>
#include <stdint.h>
#include "aimarkdown-export.h"
#include "aimarkdown-cmark-gfm_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/** # NAME
 *
 * **cmark-gfm** - CommonMark parsing, manipulating, and rendering
 */

/** # DESCRIPTION
 *
 * ## Simple Interface
 */

/** Convert 'text' (assumed to be a UTF-8 encoded string with length
 * 'len') from CommonMark Markdown to HTML, returning a null-terminated,
 * UTF-8-encoded string. It is the caller's responsibility
 * to free the returned buffer.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_markdown_to_html(const char *text, size_t len, int options);

/** ## Node Structure
 */

#define AIMARKDOWN_CMARK_NODE_TYPE_PRESENT (0x8000)
#define AIMARKDOWN_CMARK_NODE_TYPE_BLOCK (AIMARKDOWN_CMARK_NODE_TYPE_PRESENT | 0x0000)
#define AIMARKDOWN_CMARK_NODE_TYPE_INLINE (AIMARKDOWN_CMARK_NODE_TYPE_PRESENT | 0x4000)
#define AIMARKDOWN_CMARK_NODE_TYPE_MASK (0xc000)
#define AIMARKDOWN_CMARK_NODE_VALUE_MASK (0x3fff)

typedef enum {
  /* Error status */
  AIMARKDOWN_CMARK_NODE_NONE = 0x0000,

  /* Block */
  AIMARKDOWN_CMARK_NODE_DOCUMENT       = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0001,
  AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE    = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0002,
  AIMARKDOWN_CMARK_NODE_LIST           = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0003,
  AIMARKDOWN_CMARK_NODE_ITEM           = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0004,
  AIMARKDOWN_CMARK_NODE_CODE_BLOCK     = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0005,
  AIMARKDOWN_CMARK_NODE_HTML_BLOCK     = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0006,
  AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK   = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0007,
  AIMARKDOWN_CMARK_NODE_PARAGRAPH      = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0008,
  AIMARKDOWN_CMARK_NODE_HEADING        = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0009,
  AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x000a,
  AIMARKDOWN_CMARK_NODE_FOOTNOTE_DEFINITION = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x000b,

  AIMARKDOWN_CMARK_NODE_DEFINITION_LIST = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x000c,
  AIMARKDOWN_CMARK_NODE_DEFINITION_TERM = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x000d,
  AIMARKDOWN_CMARK_NODE_DEFINITION_DESCRIPTION = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x000e,
  AIMARKDOWN_CMARK_NODE_MATH_BLOCK = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x000f,

  AIMARKDOWN_CMARK_NODE_LINK_REFERENCE_DEFINITION = AIMARKDOWN_CMARK_NODE_TYPE_BLOCK | 0x0010,

  /* Inline */
  AIMARKDOWN_CMARK_NODE_TEXT          = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0001,
  AIMARKDOWN_CMARK_NODE_SOFTBREAK     = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0002,
  AIMARKDOWN_CMARK_NODE_LINEBREAK     = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0003,
  AIMARKDOWN_CMARK_NODE_CODE          = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0004,
  AIMARKDOWN_CMARK_NODE_HTML_INLINE   = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0005,
  AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0006,
  AIMARKDOWN_CMARK_NODE_EMPH          = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0007,
  AIMARKDOWN_CMARK_NODE_STRONG        = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0008,
  AIMARKDOWN_CMARK_NODE_LINK          = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x0009,
  AIMARKDOWN_CMARK_NODE_IMAGE         = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x000a,
  AIMARKDOWN_CMARK_NODE_FOOTNOTE_REFERENCE = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x000b,
  AIMARKDOWN_CMARK_NODE_ATTRIBUTE          = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x000c,
  AIMARKDOWN_CMARK_NODE_MATH_INLINE = AIMARKDOWN_CMARK_NODE_TYPE_INLINE | 0x000d,
} aimarkdown_cmark_node_type;

extern aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_LAST_BLOCK;
extern aimarkdown_cmark_node_type AIMARKDOWN_CMARK_NODE_LAST_INLINE;

/* For backwards compatibility: */
#define AIMARKDOWN_CMARK_OPT_DEFINITION_LISTS (1 << 24)

#define AIMARKDOWN_CMARK_NODE_HEADER AIMARKDOWN_CMARK_NODE_HEADING
#define AIMARKDOWN_CMARK_NODE_HRULE AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK
#define AIMARKDOWN_CMARK_NODE_HTML AIMARKDOWN_CMARK_NODE_HTML_BLOCK
#define AIMARKDOWN_CMARK_NODE_INLINE_HTML AIMARKDOWN_CMARK_NODE_HTML_INLINE

typedef enum {
  AIMARKDOWN_CMARK_NO_LIST,
  AIMARKDOWN_CMARK_BULLET_LIST,
  AIMARKDOWN_CMARK_ORDERED_LIST
} aimarkdown_cmark_list_type;

typedef enum {
  AIMARKDOWN_CMARK_NO_DELIM,
  AIMARKDOWN_CMARK_PERIOD_DELIM,
  AIMARKDOWN_CMARK_PAREN_DELIM
} aimarkdown_cmark_delim_type;

typedef enum {
  AIMARKDOWN_CMARK_NO_LIST_MARKER,
  AIMARKDOWN_CMARK_HYPHEN_LIST_MARKER,
  AIMARKDOWN_CMARK_PLUS_LIST_MARKER,
  AIMARKDOWN_CMARK_ASTERISK_LIST_MARKER
} aimarkdown_cmark_list_marker_type;

typedef struct aimarkdown_cmark_node aimarkdown_cmark_node;
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_definition_spread(aimarkdown_cmark_node *node);
typedef struct aimarkdown_cmark_parser aimarkdown_cmark_parser;
typedef struct aimarkdown_cmark_iter aimarkdown_cmark_iter;
typedef struct aimarkdown_cmark_syntax_extension aimarkdown_cmark_syntax_extension;

/**
 * ## Custom memory allocator support
 */

/** Defines the memory allocation functions to be used by CMark
 * when parsing and allocating a document tree
 */
typedef struct aimarkdown_cmark_mem {
  void *(*calloc)(size_t, size_t);
  void *(*realloc)(void *, size_t);
  void (*free)(void *);
} aimarkdown_cmark_mem;

/** The default memory allocator; uses the system's calloc,
 * realloc and free.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_mem *aimarkdown_cmark_get_default_mem_allocator(void);

/** An arena allocator; uses system calloc to allocate large
 * slabs of memory.  Memory in these slabs is not reused at all.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_mem *aimarkdown_cmark_get_arena_mem_allocator(void);

/** Resets the arena allocator, quickly returning all used memory
 * to the operating system.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_arena_reset(void);

/** Callback for freeing user data with a 'cmark_mem' context.
 */
typedef void (*aimarkdown_cmark_free_func) (aimarkdown_cmark_mem *mem, void *user_data);


/*
 * ## Basic data structures
 *
 * To keep dependencies to the strict minimum, libcmark implements
 * its own versions of "classic" data structures.
 */

/**
 * ### Linked list
 */

/** A generic singly linked list.
 */
typedef struct _cmark_llist
{
  struct _cmark_llist *next;
  void         *data;
} aimarkdown_cmark_llist;

/** Append an element to the linked list, return the possibly modified
 * head of the list.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_llist * aimarkdown_cmark_llist_append    (aimarkdown_cmark_mem         * mem,
                                     aimarkdown_cmark_llist       * head,
                                     void              * data);

/** Free the list starting with 'head', calling 'free_func' with the
 *  data pointer of each of its elements
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void          aimarkdown_cmark_llist_free_full (aimarkdown_cmark_mem         * mem,
                                     aimarkdown_cmark_llist       * head,
                                     aimarkdown_cmark_free_func     free_func);

/** Free the list starting with 'head'
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void          aimarkdown_cmark_llist_free      (aimarkdown_cmark_mem         * mem,
                                     aimarkdown_cmark_llist       * head);

/**
 * ## Creating and Destroying Nodes
 */

/** Creates a new node of type 'type'.  Note that the node may have
 * other required properties, which it is the caller's responsibility
 * to assign.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_new(aimarkdown_cmark_node_type type);

/** Same as `cmark_node_new`, but explicitly listing the memory
 * allocator used to allocate the node.  Note:  be sure to use the same
 * allocator for every node in a tree, or bad things can happen.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_new_with_mem(aimarkdown_cmark_node_type type,
                                                 aimarkdown_cmark_mem *mem);

AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_new_with_ext(aimarkdown_cmark_node_type type,
                                                aimarkdown_cmark_syntax_extension *extension);

AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_new_with_mem_and_ext(aimarkdown_cmark_node_type type,
                                                aimarkdown_cmark_mem *mem,
                                                aimarkdown_cmark_syntax_extension *extension);

/** Frees the memory allocated for a node and any children.
 */
AIMARKDOWN_CMARK_GFM_EXPORT void aimarkdown_cmark_node_free(aimarkdown_cmark_node *node);

/**
 * ## Tree Traversal
 */

/** Returns the next node in the sequence after 'node', or NULL if
 * there is none.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_next(aimarkdown_cmark_node *node);

/** Returns the previous node in the sequence after 'node', or NULL if
 * there is none.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_previous(aimarkdown_cmark_node *node);

/** Returns the parent of 'node', or NULL if there is none.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_parent(aimarkdown_cmark_node *node);

/** Returns the first child of 'node', or NULL if 'node' has no children.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_first_child(aimarkdown_cmark_node *node);

/** Returns the last child of 'node', or NULL if 'node' has no children.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_last_child(aimarkdown_cmark_node *node);

/** Returns the N'th child of 'node', or NULL if 'node' does not have at least N children.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_nth_child(aimarkdown_cmark_node *node, int n);

/** Returns the footnote reference of 'node', or NULL if 'node' doesn't have a
 * footnote reference.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node *aimarkdown_cmark_node_parent_footnote_def(aimarkdown_cmark_node *node);

/**
 * ## Iterator
 *
 * An iterator will walk through a tree of nodes, starting from a root
 * node, returning one node at a time, together with information about
 * whether the node is being entered or exited.  The iterator will
 * first descend to a child node, if there is one.  When there is no
 * child, the iterator will go to the next sibling.  When there is no
 * next sibling, the iterator will return to the parent (but with
 * a 'cmark_event_type' of `CMARK_EVENT_EXIT`).  The iterator will
 * return `CMARK_EVENT_DONE` when it reaches the root node again.
 * One natural application is an HTML renderer, where an `ENTER` event
 * outputs an open tag and an `EXIT` event outputs a close tag.
 * An iterator might also be used to transform an AST in some systematic
 * way, for example, turning all level-3 headings into regular paragraphs.
 *
 *     void
 *     usage_example(cmark_node *root) {
 *         cmark_event_type ev_type;
 *         cmark_iter *iter = cmark_iter_new(root);
 *
 *         while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
 *             cmark_node *cur = cmark_iter_get_node(iter);
 *             // Do something with `cur` and `ev_type`
 *         }
 *
 *         cmark_iter_free(iter);
 *     }
 *
 * Iterators will never return `EXIT` events for leaf nodes, which are nodes
 * of type:
 *
 * * CMARK_NODE_HTML_BLOCK
 * * CMARK_NODE_THEMATIC_BREAK
 * * CMARK_NODE_CODE_BLOCK
 * * CMARK_NODE_TEXT
 * * CMARK_NODE_SOFTBREAK
 * * CMARK_NODE_LINEBREAK
 * * CMARK_NODE_CODE
 * * CMARK_NODE_HTML_INLINE
 *
 * Nodes must only be modified after an `EXIT` event, or an `ENTER` event for
 * leaf nodes.
 */

typedef enum {
  AIMARKDOWN_CMARK_EVENT_NONE,
  AIMARKDOWN_CMARK_EVENT_DONE,
  AIMARKDOWN_CMARK_EVENT_ENTER,
  AIMARKDOWN_CMARK_EVENT_EXIT
} aimarkdown_cmark_event_type;

/** Creates a new iterator starting at 'root'.  The current node and event
 * type are undefined until 'cmark_iter_next' is called for the first time.
 * The memory allocated for the iterator should be released using
 * 'cmark_iter_free' when it is no longer needed.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_iter *aimarkdown_cmark_iter_new(aimarkdown_cmark_node *root);

/** Frees the memory allocated for an iterator.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_iter_free(aimarkdown_cmark_iter *iter);

/** Advances to the next node and returns the event type (`CMARK_EVENT_ENTER`,
 * `CMARK_EVENT_EXIT` or `CMARK_EVENT_DONE`).
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_event_type aimarkdown_cmark_iter_next(aimarkdown_cmark_iter *iter);

/** Returns the current node.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_node *aimarkdown_cmark_iter_get_node(aimarkdown_cmark_iter *iter);

/** Returns the current event type.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_event_type aimarkdown_cmark_iter_get_event_type(aimarkdown_cmark_iter *iter);

/** Returns the root node.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_node *aimarkdown_cmark_iter_get_root(aimarkdown_cmark_iter *iter);

/** Resets the iterator so that the current node is 'current' and
 * the event type is 'event_type'.  The new current node must be a
 * descendant of the root node or the root node itself.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_iter_reset(aimarkdown_cmark_iter *iter, aimarkdown_cmark_node *current,
                      aimarkdown_cmark_event_type event_type);

/**
 * ## Accessors
 */

/** Returns the user data of 'node'.
 */
AIMARKDOWN_CMARK_GFM_EXPORT void *aimarkdown_cmark_node_get_user_data(aimarkdown_cmark_node *node);

/** Sets arbitrary user data for 'node'.  Returns 1 on success,
 * 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_user_data(aimarkdown_cmark_node *node, void *user_data);

/** Set free function for user data */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_node_set_user_data_free_func(aimarkdown_cmark_node *node,
                                        aimarkdown_cmark_free_func free_func);

/** Returns the type of 'node', or `CMARK_NODE_NONE` on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_node_type aimarkdown_cmark_node_get_type(aimarkdown_cmark_node *node);

/** Like 'cmark_node_get_type', but returns a string representation
    of the type, or `"<unknown>"`.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
const char *aimarkdown_cmark_node_get_type_string(aimarkdown_cmark_node *node);

/** Returns the string contents of 'node', or an empty
    string if none is set.  Returns NULL if called on a
    node that does not have string content.
 */
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_literal(aimarkdown_cmark_node *node);

/** Returns the number of backtick characters used to open the
    node if it is an inline code span, otherwise returns 0.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_backtick_count(aimarkdown_cmark_node *node);

/** Sets the string contents of 'node'.  Returns 1 on success,
 * 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_literal(aimarkdown_cmark_node *node, const char *content);

/** Returns the heading level of 'node', or 0 if 'node' is not a heading.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_heading_level(aimarkdown_cmark_node *node);

/* For backwards compatibility */
#define aimarkdown_cmark_node_get_header_level aimarkdown_cmark_node_get_heading_level
#define aimarkdown_cmark_node_set_header_level aimarkdown_cmark_node_set_heading_level

/** Sets the heading level of 'node', returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_heading_level(aimarkdown_cmark_node *node, int level);

/** Returns the list type of 'node', or `CMARK_NO_LIST` if 'node'
 * is not a list.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_list_type aimarkdown_cmark_node_get_list_type(aimarkdown_cmark_node *node);

/** Sets the list type of 'node', returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_list_type(aimarkdown_cmark_node *node,
                                          aimarkdown_cmark_list_type type);

/** Returns the list marker of 'node', or `CMARK_NO_LIST_MARKER` if 'node'
 * is not a list.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_list_marker_type aimarkdown_cmark_node_get_list_marker(aimarkdown_cmark_node *node);

/** Sets the list marker of 'node', returning 1 on success and 0 on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_list_marker(aimarkdown_cmark_node *node,
                                                aimarkdown_cmark_list_marker_type listMarker);

/** Returns the list delimiter type of 'node', or `CMARK_NO_DELIM` if 'node'
 * is not a list.
 */
AIMARKDOWN_CMARK_GFM_EXPORT aimarkdown_cmark_delim_type aimarkdown_cmark_node_get_list_delim(aimarkdown_cmark_node *node);

/** Sets the list delimiter type of 'node', returning 1 on success and 0
 * on error.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_list_delim(aimarkdown_cmark_node *node,
                                           aimarkdown_cmark_delim_type delim);

/** Returns starting number of 'node', if it is an ordered list, otherwise 0.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_list_start(aimarkdown_cmark_node *node);

/** Sets starting number of 'node', if it is an ordered list. Returns 1
 * on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_list_start(aimarkdown_cmark_node *node, int start);

/** Returns 1 if 'node' is a tight list, 0 otherwise.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_list_tight(aimarkdown_cmark_node *node);

/** Sets the "tightness" of a list.  Returns 1 on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_list_tight(aimarkdown_cmark_node *node, int tight);

/**
 * Returns item index of 'node'. This is only used when rendering output
 * formats such as commonmark, which need to output the index. It is not
 * required for formats such as html or latex.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_item_index(aimarkdown_cmark_node *node);

/** Sets item index of 'node'. Returns 1 on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_item_index(aimarkdown_cmark_node *node, int idx);

/** Returns the info string from a fenced code block.
 */
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_fence_info(aimarkdown_cmark_node *node);

/** Sets the info string in a fenced code block, returning 1 on
 * success and 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_fence_info(aimarkdown_cmark_node *node, const char *info);

/** Sets code blocks fencing details
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_fenced(aimarkdown_cmark_node * node, int fenced,
    int length, int offset, char character);

/** Returns code blocks fencing details
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_fenced(aimarkdown_cmark_node *node, int *length, int *offset, char *character);

/** Returns the URL of a link or image 'node', or an empty string
    if no URL is set.  Returns NULL if called on a node that is
    not a link or image.
 */
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_url(aimarkdown_cmark_node *node);

/** Sets the URL of a link or image 'node'. Returns 1 on success,
 * 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_url(aimarkdown_cmark_node *node, const char *url);

/** Returns the title of a link or image 'node', or an empty
    string if no title is set.  Returns NULL if called on a node
    that is not a link or image.
 */
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_title(aimarkdown_cmark_node *node);

/** Sets the title of a link or image 'node'. Returns 1 on success,
 * 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_title(aimarkdown_cmark_node *node, const char *title);

/** Returns the attributes of an attribute 'node', or an empty string
    if no attributes are set.  Returns NULL if called on a node that is
    not an attribute.
 */
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_attributes(aimarkdown_cmark_node *node);

/** Sets the attributes of an attribute 'node'. Returns 1 on success,
 * 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_attributes(aimarkdown_cmark_node *node, const char *attributes);

/** Returns the literal "on enter" text for a custom 'node', or
    an empty string if no on_enter is set.  Returns NULL if called
    on a non-custom node.
 */
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_on_enter(aimarkdown_cmark_node *node);

/** Sets the literal text to render "on enter" for a custom 'node'.
    Any children of the node will be rendered after this text.
    Returns 1 on success 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_on_enter(aimarkdown_cmark_node *node,
                                         const char *on_enter);

/** Returns the literal "on exit" text for a custom 'node', or
    an empty string if no on_exit is set.  Returns NULL if
    called on a non-custom node.
 */
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_on_exit(aimarkdown_cmark_node *node);

/** Sets the literal text to render "on exit" for a custom 'node'.
    Any children of the node will be rendered before this text.
    Returns 1 on success 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_set_on_exit(aimarkdown_cmark_node *node, const char *on_exit);

/** Returns the line on which 'node' begins.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_start_line(aimarkdown_cmark_node *node);

/** Returns the column at which 'node' begins.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_start_column(aimarkdown_cmark_node *node);

/** Returns the line on which 'node' ends.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_end_line(aimarkdown_cmark_node *node);

/** Returns the column at which 'node' ends.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_end_column(aimarkdown_cmark_node *node);

/**
 * ## Tree Manipulation
 */

/** Unlinks a 'node', removing it from the tree, but not freeing its
 * memory.  (Use 'cmark_node_free' for that.)
 */
AIMARKDOWN_CMARK_GFM_EXPORT void aimarkdown_cmark_node_unlink(aimarkdown_cmark_node *node);

/** Inserts 'sibling' before 'node'.  Returns 1 on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_insert_before(aimarkdown_cmark_node *node,
                                          aimarkdown_cmark_node *sibling);

/** Inserts 'sibling' after 'node'. Returns 1 on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_insert_after(aimarkdown_cmark_node *node, aimarkdown_cmark_node *sibling);

/** Replaces 'oldnode' with 'newnode' and unlinks 'oldnode' (but does
 * not free its memory).
 * Returns 1 on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_replace(aimarkdown_cmark_node *oldnode, aimarkdown_cmark_node *newnode);

/** Adds 'child' to the beginning of the children of 'node'.
 * Returns 1 on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_prepend_child(aimarkdown_cmark_node *node, aimarkdown_cmark_node *child);

/** Adds 'child' to the end of the children of 'node'.
 * Returns 1 on success, 0 on failure.
 */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_append_child(aimarkdown_cmark_node *node, aimarkdown_cmark_node *child);

/** Consolidates adjacent text nodes.
 */
AIMARKDOWN_CMARK_GFM_EXPORT void aimarkdown_cmark_consolidate_text_nodes(aimarkdown_cmark_node *root);

/** Ensures a node and all its children own their own chunk memory.
 */
AIMARKDOWN_CMARK_GFM_EXPORT void aimarkdown_cmark_node_own(aimarkdown_cmark_node *root);

/**
 * ## Parsing
 *
 * Simple interface:
 *
 *     cmark_node *document = cmark_parse_document("Hello *world*", 13,
 *                                                 CMARK_OPT_DEFAULT);
 *
 * Streaming interface:
 *
 *     cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
 *     FILE *fp = fopen("myfile.md", "rb");
 *     while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
 *     	   cmark_parser_feed(parser, buffer, bytes);
 *     	   if (bytes < sizeof(buffer)) {
 *     	       break;
 *     	   }
 *     }
 *     document = cmark_parser_finish(parser);
 *     cmark_parser_free(parser);
 */

/** Creates a new parser object.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_parser *aimarkdown_cmark_parser_new(int options);

/** Creates a new parser object with the given memory allocator
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_parser *aimarkdown_cmark_parser_new_with_mem(int options, aimarkdown_cmark_mem *mem);

/** Frees memory allocated for a parser object.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_parser_free(aimarkdown_cmark_parser *parser);

/** Feeds a string of length 'len' to 'parser'.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
void aimarkdown_cmark_parser_feed(aimarkdown_cmark_parser *parser, const char *buffer, size_t len);

/** Finish parsing and return a pointer to a tree of nodes.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_node *aimarkdown_cmark_parser_finish(aimarkdown_cmark_parser *parser);

/** Parse a CommonMark document in 'buffer' of length 'len'.
 * Returns a pointer to a tree of nodes.  The memory allocated for
 * the node tree should be released using 'cmark_node_free'
 * when it is no longer needed.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_node *aimarkdown_cmark_parse_document(const char *buffer, size_t len, int options);

/** Parse a CommonMark document in file 'f', returning a pointer to
 * a tree of nodes.  The memory allocated for the node tree should be
 * released using 'cmark_node_free' when it is no longer needed.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
aimarkdown_cmark_node *aimarkdown_cmark_parse_file(FILE *f, int options);

/**
 * ## Rendering
 */

/** Render a 'node' tree as XML.  It is the caller's responsibility
 * to free the returned buffer.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_xml(aimarkdown_cmark_node *root, int options);

/** As for 'cmark_render_xml', but specifying the allocator to use for
 * the resulting string.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_xml_with_mem(aimarkdown_cmark_node *root, int options, aimarkdown_cmark_mem *mem);

/** Render a 'node' tree as an HTML fragment.  It is up to the user
 * to add an appropriate header and footer. It is the caller's
 * responsibility to free the returned buffer.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_html(aimarkdown_cmark_node *root, int options, aimarkdown_cmark_llist *extensions);

/** As for 'cmark_render_html', but specifying the allocator to use for
 * the resulting string.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_html_with_mem(aimarkdown_cmark_node *root, int options, aimarkdown_cmark_llist *extensions, aimarkdown_cmark_mem *mem);

/** Render a 'node' tree as a groff man page, without the header.
 * It is the caller's responsibility to free the returned buffer.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_man(aimarkdown_cmark_node *root, int options, int width);

/** As for 'cmark_render_man', but specifying the allocator to use for
 * the resulting string.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_man_with_mem(aimarkdown_cmark_node *root, int options, int width, aimarkdown_cmark_mem *mem);

/** Render a 'node' tree as a commonmark document.
 * It is the caller's responsibility to free the returned buffer.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_commonmark(aimarkdown_cmark_node *root, int options, int width);

/** As for 'cmark_render_commonmark', but specifying the allocator to use for
 * the resulting string.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_commonmark_with_mem(aimarkdown_cmark_node *root, int options, int width, aimarkdown_cmark_mem *mem);

/** Render a 'node' tree as a plain text document.
 * It is the caller's responsibility to free the returned buffer.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_plaintext(aimarkdown_cmark_node *root, int options, int width);

/** As for 'cmark_render_plaintext', but specifying the allocator to use for
 * the resulting string.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_plaintext_with_mem(aimarkdown_cmark_node *root, int options, int width, aimarkdown_cmark_mem *mem);

/** Render a 'node' tree as a LaTeX document.
 * It is the caller's responsibility to free the returned buffer.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_latex(aimarkdown_cmark_node *root, int options, int width);

/** As for 'cmark_render_latex', but specifying the allocator to use for
 * the resulting string.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
char *aimarkdown_cmark_render_latex_with_mem(aimarkdown_cmark_node *root, int options, int width, aimarkdown_cmark_mem *mem);

/**
 * ## Options
 */

/** Default options.
 */
#define AIMARKDOWN_CMARK_OPT_DEFAULT 0

/**
 * ### Options affecting rendering
 */

/** Include a `data-sourcepos` attribute on all block elements.
 */
#define AIMARKDOWN_CMARK_OPT_SOURCEPOS (1 << 1)

/** Render `softbreak` elements as hard line breaks.
 */
#define AIMARKDOWN_CMARK_OPT_HARDBREAKS (1 << 2)

/** `CMARK_OPT_SAFE` is defined here for API compatibility,
    but it no longer has any effect. "Safe" mode is now the default:
    set `CMARK_OPT_UNSAFE` to disable it.
 */
#define AIMARKDOWN_CMARK_OPT_SAFE (1 << 3)

/** Render raw HTML and unsafe links (`javascript:`, `vbscript:`,
 * `file:`, and `data:`, except for `image/png`, `image/gif`,
 * `image/jpeg`, or `image/webp` mime types).  By default,
 * raw HTML is replaced by a placeholder HTML comment. Unsafe
 * links are replaced by empty strings.
 */
#define AIMARKDOWN_CMARK_OPT_UNSAFE (1 << 17)

/** Render `softbreak` elements as spaces.
 */
#define AIMARKDOWN_CMARK_OPT_NOBREAKS (1 << 4)

/**
 * ### Options affecting parsing
 */

/** Legacy option (no effect).
 */
#define AIMARKDOWN_CMARK_OPT_NORMALIZE (1 << 8)

/** Validate UTF-8 in the input before parsing, replacing illegal
 * sequences with the replacement character U+FFFD.
 */
#define AIMARKDOWN_CMARK_OPT_VALIDATE_UTF8 (1 << 9)

/** Convert straight quotes to curly, --- to em dashes, -- to en dashes.
 */
#define AIMARKDOWN_CMARK_OPT_SMART (1 << 10)

/** Use GitHub-style <pre lang="x"> tags for code blocks instead of <pre><code
 * class="language-x">.
 */
#define AIMARKDOWN_CMARK_OPT_GITHUB_PRE_LANG (1 << 11)

/** Be liberal in interpreting inline HTML tags.
 */
#define AIMARKDOWN_CMARK_OPT_LIBERAL_HTML_TAG (1 << 12)

/** Parse footnotes.
 */
#define AIMARKDOWN_CMARK_OPT_FOOTNOTES (1 << 13)

/** Experimental lossless footnote AST; CMARK_OPT_FOOTNOTES must also be enabled.
 * Definitions retain their source positions, including unused and duplicate definitions; reference literals retain parser labels.
 * Undefined references follow normal inline parsing; definition ranges exclude trailing blank lines.
 * Read labels with cmark_node_get_literal; their lifetime matches the node.
 * No numbering or backlinks are generated; existing footnote renderers cannot consume this tree directly.
 * Only existing cmark footnote recognition results are preserved; GFM syntax equivalence is not guaranteed.
 */
#define AIMARKDOWN_CMARK_OPT_FOOTNOTES_LOSSLESS (1 << 22)

/** Observe raw adjacent characters for Web-compatible asterisk/underscore emphasis. */
#define AIMARKDOWN_CMARK_OPT_WEB_EMPHASIS (1 << 23)

/** Opt-in math grammar matching remark-math singleDollarTextMath=false. */
#define AIMARKDOWN_CMARK_OPT_MATH (1 << 25)

/** Preserve ordinary link reference definitions as lossless leaves in source order; reference usage rewriting remains disabled. */
#define AIMARKDOWN_CMARK_OPT_REFERENCE_DEFINITIONS (1 << 26)
/** Preserve actual reference usage identity; implies REFERENCE_DEFINITIONS. */
#define AIMARKDOWN_CMARK_OPT_REFERENCE_USAGES (1 << 27)
/** Match Web inline character references without changing code or TeX bodies. */
#define AIMARKDOWN_CMARK_OPT_WEB_CHARACTER_REFERENCES (1 << 28)
/** Capture source-based Web code fields; legacy display literals are unchanged. */
#define AIMARKDOWN_CMARK_OPT_CODE_METADATA (1 << 29)

/** Opt-in Web code delimiter lengths without the legacy 80/255 limits. */
#define AIMARKDOWN_CMARK_OPT_WEB_CODE_SYNTAX (1 << 30)

/** Capture physical endings on actual soft-break parser events; display is unchanged. */
enum { AIMARKDOWN_CMARK_OPT_SOFTBREAK_METADATA = -2147483647 - 1 };
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_softbreak_ending(aimarkdown_cmark_node *node);

/** Derive Web mdast list and list-item spread from a container and line-ending
 * ledger recorded during block parsing; rendering and list tightness are unchanged. */
#define AIMARKDOWN_CMARK_OPT_LIST_SPREAD_METADATA (1 << 5)
/** Returns 0 when spread was not captured, 1 when not spread, 2 when spread. */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_list_spread(aimarkdown_cmark_node *node);

/** Record Web mdast positions derived from actual parser events: container and
 * code block boundaries from the block-parser ledger, and inline nodes from
 * inline-subject offsets mapped through each content line's physical start.
 * Existing source positions, rendering and tightness are unchanged. */
#define AIMARKDOWN_CMARK_OPT_WEB_POSITIONS (1 << 6)

/** Parse Web-compatible table dividers and retain each row's actual cells.
 * The table extension must also be attached. Header alignments remain unchanged.
 */
#define AIMARKDOWN_CMARK_OPT_WEB_TABLES (1 << 7)
/** Writes the captured Web position as 1-based lines and byte columns with an
 * exclusive end. Returns a mask: 1 when the start was captured, 2 when the end
 * was captured; 0 when nothing was captured. Callers must not substitute values. */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_web_position(aimarkdown_cmark_node *node, int *start_line,
                                                 int *start_column, int *end_line,
                                                 int *end_column);
/** With CMARK_OPT_WEB_POSITIONS, the number of cells the table row scan found
 * before cells beyond the header's columns were dropped or missing cells were
 * added; 0 when not captured. */
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_web_table_cells(aimarkdown_cmark_node *node);

AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_code_metadata_kind(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_code_metadata_value(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_code_metadata_info(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_code_metadata_lang(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_code_metadata_meta(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_code_metadata_source(aimarkdown_cmark_node *node);

AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_reference_label(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_reference_identifier_source(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_reference_destination(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_reference_title(aimarkdown_cmark_node *node);

/** Only parse strikethroughs if surrounded by exactly 2 tildes.
 * Gives some compatibility with redcarpet.
 */
#define AIMARKDOWN_CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE (1 << 14)

/** Use style attributes to align table cells instead of align attributes.
 */
#define AIMARKDOWN_CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES (1 << 15)

/** Include the remainder of the info string in code blocks in
 * a separate attribute.
 */
#define AIMARKDOWN_CMARK_OPT_FULL_INFO_STRING (1 << 16)

/** Parse only inline markdown directives. Block directives will not be
 * parsed (their literal representations will remain in the output).
 */
#define AIMARKDOWN_CMARK_OPT_INLINE_ONLY (1 << 18)

/** Parse the markdown input without removing preceding/trailing whitespace and
 * without converting newline characters to breaks. Using this option also
 * enables the CMARK_OPT_INLINE_ONLY option.
 */
#define AIMARKDOWN_CMARK_OPT_PRESERVE_WHITESPACE ((1 << 19) | AIMARKDOWN_CMARK_OPT_INLINE_ONLY)

/** Parse row- and column-span in tables.
 */
#define AIMARKDOWN_CMARK_OPT_TABLE_SPANS (1 << 20)

/** Parse table cells defining row span using a double-quote symbol (`"`, or "ditto mark")
 * instead of the default caret symbol (`^`).
 *
 * Does nothing unless \c CMARK_OPT_TABLE_SPANS is also set.
 */
#define AIMARKDOWN_CMARK_OPT_TABLE_ROWSPAN_DITTO (1 << 21)

/**
 * ## Version information
 */

/** The library version as integer for runtime checks. Also available as
 * macro CMARK_VERSION for compile time checks.
 *
 * * Bits 16-23 contain the major version.
 * * Bits 8-15 contain the minor version.
 * * Bits 0-7 contain the patchlevel.
 *
 * In hexadecimal format, the number 0x010203 represents version 1.2.3.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
int aimarkdown_cmark_version(void);

/** The library version string for runtime checks. Also available as
 * macro CMARK_VERSION_STRING for compile time checks.
 */
AIMARKDOWN_CMARK_GFM_EXPORT
const char *aimarkdown_cmark_version_string(void);

/** # AUTHORS
 *
 * John MacFarlane, Vicent Marti,  Kārlis Gaņģis, Nick Wellnhofer.
 */

#if !defined(AIMARKDOWN_CMARK_NO_SHORT_NAMES)
#define NODE_DOCUMENT AIMARKDOWN_CMARK_NODE_DOCUMENT
#define NODE_BLOCK_QUOTE AIMARKDOWN_CMARK_NODE_BLOCK_QUOTE
#define NODE_LIST AIMARKDOWN_CMARK_NODE_LIST
#define NODE_ITEM AIMARKDOWN_CMARK_NODE_ITEM
#define NODE_CODE_BLOCK AIMARKDOWN_CMARK_NODE_CODE_BLOCK
#define NODE_HTML_BLOCK AIMARKDOWN_CMARK_NODE_HTML_BLOCK
#define NODE_CUSTOM_BLOCK AIMARKDOWN_CMARK_NODE_CUSTOM_BLOCK
#define NODE_PARAGRAPH AIMARKDOWN_CMARK_NODE_PARAGRAPH
#define NODE_HEADING AIMARKDOWN_CMARK_NODE_HEADING
#define NODE_HEADER AIMARKDOWN_CMARK_NODE_HEADER
#define NODE_THEMATIC_BREAK AIMARKDOWN_CMARK_NODE_THEMATIC_BREAK
#define NODE_HRULE AIMARKDOWN_CMARK_NODE_HRULE
#define NODE_TEXT AIMARKDOWN_CMARK_NODE_TEXT
#define NODE_SOFTBREAK AIMARKDOWN_CMARK_NODE_SOFTBREAK
#define NODE_LINEBREAK AIMARKDOWN_CMARK_NODE_LINEBREAK
#define NODE_CODE AIMARKDOWN_CMARK_NODE_CODE
#define NODE_HTML_INLINE AIMARKDOWN_CMARK_NODE_HTML_INLINE
#define NODE_CUSTOM_INLINE AIMARKDOWN_CMARK_NODE_CUSTOM_INLINE
#define NODE_EMPH AIMARKDOWN_CMARK_NODE_EMPH
#define NODE_STRONG AIMARKDOWN_CMARK_NODE_STRONG
#define NODE_LINK AIMARKDOWN_CMARK_NODE_LINK
#define NODE_IMAGE AIMARKDOWN_CMARK_NODE_IMAGE
#define BULLET_LIST AIMARKDOWN_CMARK_BULLET_LIST
#define ORDERED_LIST AIMARKDOWN_CMARK_ORDERED_LIST
#define PERIOD_DELIM AIMARKDOWN_CMARK_PERIOD_DELIM
#define PAREN_DELIM AIMARKDOWN_CMARK_PAREN_DELIM
#define HYPHEN_LIST_MARKER AIMARKDOWN_CMARK_HYPHEN_LIST_MARKER
#define PLUS_LIST_MARKER AIMARKDOWN_CMARK_PLUS_LIST_MARKER
#define ASTERISK_LIST_MARKER AIMARKDOWN_CMARK_ASTERISK_LIST_MARKER
#endif

typedef int32_t bufsize_t;


AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_reference_usage_label(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT const char *aimarkdown_cmark_node_get_reference_usage_source(aimarkdown_cmark_node *node);
AIMARKDOWN_CMARK_GFM_EXPORT int aimarkdown_cmark_node_get_reference_usage_type(aimarkdown_cmark_node *node);

AIMARKDOWN_CMARK_GFM_EXPORT char *aimarkdown_cmark_reference_identifier(const char *source);
AIMARKDOWN_CMARK_GFM_EXPORT void aimarkdown_cmark_reference_identifier_free(char *value);
AIMARKDOWN_CMARK_GFM_EXPORT unsigned char *aimarkdown_cmark_decode_reference_label(const char *source, size_t size, size_t *decoded_size);
AIMARKDOWN_CMARK_GFM_EXPORT void aimarkdown_cmark_decoded_reference_label_free(unsigned char *value);

#ifdef __cplusplus
}
#endif

#endif
