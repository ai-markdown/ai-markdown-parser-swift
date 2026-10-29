// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: scripts/parser-manifest.json.
#ifndef AIMARKDOWN_CMARK_CHUNK_H
#define AIMARKDOWN_CMARK_CHUNK_H

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "aimarkdown-cmark-gfm.h"
#include "aimarkdown-buffer.h"
#include "aimarkdown-cmark_ctype.h"

#define AIMARKDOWN_CMARK_CHUNK_EMPTY                                                      \
  { NULL, 0, 0 }

typedef struct aimarkdown_cmark_chunk {
  unsigned char *data;
  bufsize_t len;
  bufsize_t alloc; // also implies a NULL-terminated string
} aimarkdown_cmark_chunk;

static inline void aimarkdown_cmark_chunk_free(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *c) {
  if (c->alloc)
    mem->free(c->data);

  c->data = NULL;
  c->alloc = 0;
  c->len = 0;
}

static inline void aimarkdown_cmark_chunk_ltrim(aimarkdown_cmark_chunk *c) {
  assert(!c->alloc);

  while (c->len && aimarkdown_cmark_isspace(c->data[0])) {
    c->data++;
    c->len--;
  }
}

static inline void aimarkdown_cmark_chunk_rtrim(aimarkdown_cmark_chunk *c) {
  assert(!c->alloc);

  while (c->len > 0) {
    if (!aimarkdown_cmark_isspace(c->data[c->len - 1]))
      break;

    c->len--;
  }
}

static inline void aimarkdown_cmark_chunk_trim(aimarkdown_cmark_chunk *c) {
  aimarkdown_cmark_chunk_ltrim(c);
  aimarkdown_cmark_chunk_rtrim(c);
}

static inline bufsize_t aimarkdown_cmark_chunk_strchr(aimarkdown_cmark_chunk *ch, int c,
                                           bufsize_t offset) {
  const unsigned char *p =
      (unsigned char *)memchr(ch->data + offset, c, ch->len - offset);
  return p ? (bufsize_t)(p - ch->data) : ch->len;
}

static inline const char *aimarkdown_cmark_chunk_to_cstr(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *c) {
  unsigned char *str;

  if (c->alloc) {
    return (char *)c->data;
  }
  str = (unsigned char *)mem->calloc(c->len + 1, 1);
  if (c->len > 0) {
    memcpy(str, c->data, c->len);
  }
  str[c->len] = 0;
  c->data = str;
  c->alloc = 1;

  return (char *)str;
}

static inline void aimarkdown_cmark_chunk_set_cstr(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *c,
                                        const char *str) {
  unsigned char *old = c->alloc ? c->data : NULL;
  if (str == NULL) {
    c->len = 0;
    c->data = NULL;
    c->alloc = 0;
  } else {
    c->len = (bufsize_t)strlen(str);
    c->data = (unsigned char *)mem->calloc(c->len + 1, 1);
    c->alloc = 1;
    memcpy(c->data, str, c->len + 1);
  }
  if (old != NULL) {
    mem->free(old);
  }
}

static inline aimarkdown_cmark_chunk aimarkdown_cmark_chunk_literal(const char *data) {
  bufsize_t len = data ? (bufsize_t)strlen(data) : 0;
  aimarkdown_cmark_chunk c = {(unsigned char *)data, len, 0};
  return c;
}

static inline aimarkdown_cmark_chunk aimarkdown_cmark_chunk_dup(const aimarkdown_cmark_chunk *ch, bufsize_t pos,
                                          bufsize_t len) {
  aimarkdown_cmark_chunk c = {ch->data + pos, len, 0};
  return c;
}

static inline aimarkdown_cmark_chunk aimarkdown_cmark_chunk_buf_detach(aimarkdown_cmark_strbuf *buf) {
  aimarkdown_cmark_chunk c;

  c.len = buf->size;
  c.data = aimarkdown_cmark_strbuf_detach(buf);
  c.alloc = 1;

  return c;
}

/* trim_new variants are to be used when the source chunk may or may not be
 * allocated; forces a newly allocated chunk. */
static inline aimarkdown_cmark_chunk aimarkdown_cmark_chunk_ltrim_new(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *c) {
  aimarkdown_cmark_chunk r = aimarkdown_cmark_chunk_dup(c, 0, c->len);
  aimarkdown_cmark_chunk_ltrim(&r);
  aimarkdown_cmark_chunk_to_cstr(mem, &r);
  return r;
}

static inline aimarkdown_cmark_chunk aimarkdown_cmark_chunk_rtrim_new(aimarkdown_cmark_mem *mem, aimarkdown_cmark_chunk *c) {
  aimarkdown_cmark_chunk r = aimarkdown_cmark_chunk_dup(c, 0, c->len);
  aimarkdown_cmark_chunk_rtrim(&r);
  aimarkdown_cmark_chunk_to_cstr(mem, &r);
  return r;
}

#endif
