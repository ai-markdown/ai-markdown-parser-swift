// AI Markdown custom parser with isolated modules and C symbols; provenance and rebuild rules: Vendor/Parser/manifest.json.
#include "aimarkdown-node.h"
#include "aimarkdown-string_token.h"

/* Sidecar values come from parser token buffers before legacy literal cleaning.
 * Source capture is parser-deindented text, not an inverse source-range map. */
void aimarkdown_cmark_code_metadata_start(aimarkdown_cmark_node *node, int kind) {
  aimarkdown_cmark_mem *mem = node->content.mem;
  node->code_metadata = mem->calloc(1, sizeof(aimarkdown_cmark_code_metadata));
  node->code_metadata->kind = kind;
  aimarkdown_cmark_strbuf_init(mem, &node->code_metadata->source, 0);
  aimarkdown_cmark_strbuf_init(mem, &node->code_metadata->compiled, 0);
  aimarkdown_cmark_strbuf_init(mem, &node->code_metadata->pending, 0);
}

void aimarkdown_cmark_code_metadata_clear(aimarkdown_cmark_node *node) {
  if (!node || !node->code_metadata) return;
  aimarkdown_cmark_mem *mem = node->content.mem;
  aimarkdown_cmark_code_metadata *data = node->code_metadata;
  aimarkdown_cmark_strbuf_free(&data->source);
  aimarkdown_cmark_strbuf_free(&data->compiled);
  aimarkdown_cmark_strbuf_free(&data->pending);
  aimarkdown_cmark_chunk_free(mem, &data->info);
  aimarkdown_cmark_chunk_free(mem, &data->value);
  aimarkdown_cmark_chunk_free(mem, &data->lang);
  aimarkdown_cmark_chunk_free(mem, &data->meta);
  mem->free(data); node->code_metadata = NULL;
}

static aimarkdown_cmark_chunk owned(aimarkdown_cmark_mem *mem, const unsigned char *bytes, bufsize_t size, bool decode) {
  aimarkdown_cmark_strbuf out = AIMARKDOWN_CMARK_BUF_INIT(mem);
  if (decode) aimarkdown_cmark_decode_string_token(&out, bytes, size);
  else aimarkdown_cmark_strbuf_put(&out, bytes, size);
  return aimarkdown_cmark_chunk_buf_detach(&out);
}

static bufsize_t without_last_eol(const unsigned char *bytes, bufsize_t size) {
  if (size && bytes[size - 1] == '\n') {
    size--;
    if (size && bytes[size - 1] == '\r') size--;
  } else if (size && bytes[size - 1] == '\r') size--;
  return size;
}

void aimarkdown_cmark_code_metadata_finish(aimarkdown_cmark_node *node) {
  aimarkdown_cmark_code_metadata *data = node->code_metadata;
  if (!data || data->finished) return;
  aimarkdown_cmark_mem *mem = node->content.mem;
  bufsize_t end = data->source.size;
  if (data->kind == 1) {
    bufsize_t info_end = 0;
    while (info_end < end && data->source.ptr[info_end] != '\r' && data->source.ptr[info_end] != '\n') info_end++;
    data->info = owned(mem, data->source.ptr, info_end, false);
    bufsize_t start = 0, stop;
    while (start < info_end && (data->source.ptr[start] == ' ' || data->source.ptr[start] == '\t')) start++;
    stop = start;
    while (stop < info_end && data->source.ptr[stop] != ' ' && data->source.ptr[stop] != '\t') stop++;
    if (start < stop) data->lang = owned(mem, data->source.ptr + start, stop - start, true);
    start = stop;
    while (start < info_end && (data->source.ptr[start] == ' ' || data->source.ptr[start] == '\t')) start++;
    if (start < info_end) data->meta = owned(mem, data->source.ptr + start, info_end - start, true);
    bufsize_t body = info_end;
    if (body < end && data->source.ptr[body] == '\r') body++;
    if (body < end && data->source.ptr[body] == '\n') body++;
    aimarkdown_cmark_strbuf_drop(&data->source, body);
    end = without_last_eol(data->source.ptr, data->source.size);
    if (data->boundary_trim) end = without_last_eol(data->source.ptr, end);
    data->value = owned(mem, data->source.ptr, end, false);
  } else {
    end = without_last_eol(data->compiled.ptr, data->compiled.size);
    data->value = owned(mem, data->compiled.ptr, end, false);
  }
  data->finished = true;
}

void aimarkdown_cmark_code_metadata_inline(aimarkdown_cmark_node *node, aimarkdown_cmark_chunk source) {
  aimarkdown_cmark_code_metadata_start(node, 3);
  aimarkdown_cmark_code_metadata *data = node->code_metadata;
  aimarkdown_cmark_strbuf_put(&data->source, source.data, source.len);
  bufsize_t start = 0, end = source.len;
  bool actual_data = false;
  for (bufsize_t i = 0; i < end; i++) if (source.data[i] != ' ' && source.data[i] != '\r' && source.data[i] != '\n') actual_data = true;
  if (actual_data && end && (source.data[0] == ' ' || source.data[0] == '\r' || source.data[0] == '\n') &&
      (source.data[end - 1] == ' ' || source.data[end - 1] == '\r' || source.data[end - 1] == '\n')) {
    start = source.data[0] == '\r' && end > 1 && source.data[1] == '\n' ? 2 : 1;
    end = without_last_eol(source.data, end);
    if (end == source.len) end--; /* A space is one padding token. */
  }
  data->value = owned(node->content.mem, source.data + start, end - start, false);
  data->finished = true;
}

/* Fixed mdast-util-gfm-table exitCodeText: consume backslash pairs once. */
void aimarkdown_cmark_code_metadata_table(aimarkdown_cmark_node *node) {
  aimarkdown_cmark_code_metadata *data = node ? node->code_metadata : NULL;
  if (!data || data->kind != 3 || !data->finished || !data->value.len) return;
  aimarkdown_cmark_chunk *value = &data->value;
  bufsize_t read = 0, write = 0;
  while (read < value->len) {
    unsigned char byte = value->data[read++];
    if (byte == '\\' && read < value->len &&
        (value->data[read] == '\\' || value->data[read] == '|')) {
      unsigned char next = value->data[read++];
      if (next == '\\') value->data[write++] = byte;
      value->data[write++] = next;
    } else value->data[write++] = byte;
  }
  value->len = write;
  value->data[write] = 0;
}

static aimarkdown_cmark_code_metadata *metadata(aimarkdown_cmark_node *node) {
  if (!node || (node->type != AIMARKDOWN_CMARK_NODE_CODE_BLOCK && node->type != AIMARKDOWN_CMARK_NODE_CODE)) return NULL;
  return node->code_metadata && node->code_metadata->finished ? node->code_metadata : NULL;
}
int aimarkdown_cmark_node_get_code_metadata_kind(aimarkdown_cmark_node *node) { aimarkdown_cmark_code_metadata *data = metadata(node); return data ? data->kind : 0; }
const char *aimarkdown_cmark_node_get_code_metadata_source(aimarkdown_cmark_node *node) { aimarkdown_cmark_code_metadata *data = metadata(node); return data ? (const char *)aimarkdown_cmark_strbuf_cstr(&data->source) : NULL; }
#define FIELD_GETTER(name) const char *aimarkdown_cmark_node_get_code_metadata_##name(aimarkdown_cmark_node *node) { aimarkdown_cmark_code_metadata *data = metadata(node); return data && data->name.data ? aimarkdown_cmark_chunk_to_cstr(node->content.mem, &data->name) : NULL; }
FIELD_GETTER(value)
FIELD_GETTER(info)
FIELD_GETTER(lang)
FIELD_GETTER(meta)
