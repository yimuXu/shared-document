#include "../libs/markdown.h"

#define SUCCESS 0 
#define INVALID_CURSOR_POS -1
#define DELETE_POSITION -2
#define OUTDATE_VERSION -3

// === Init and Free ===
document *markdown_init(void) {
    document *doc = malloc(sizeof(document));
    doc->head->chunksize = 0;
    doc->head->data = NULL;
    doc->head->next = NULL;
    doc->head->prev = NULL;
    doc->head = malloc(sizeof(chunk));
    doc->size = 0;
    return NULL;
}

void markdown_free(document *doc) {
    if (doc == NULL) {
        return;
    }
    chunk *current = doc->head;
    while (current != NULL) {
        chunk *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    free(doc);
}

//hepler function split a chunk into two chunks
chunk* split_chunk(chunk* current, size_t pos) {
    chunk* new_chunk = malloc(sizeof(chunk));
    new_chunk->data = malloc(current->chunksize - pos);
    memcpy(new_chunk->data, current->data + pos, current->chunksize - pos);
    new_chunk->chunksize = current->chunksize - pos;
    new_chunk->next = current->next;
    new_chunk->next->prev = new_chunk;
    new_chunk->prev = current;
    current->next = new_chunk;
    current->chunksize = pos;
    current->data = realloc(current->data, pos);
    return current;
}
// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content) {
    //(void)doc; (void)version; (void)pos; (void)content;
    if(pos > doc->size) {
        return INVALID_CURSOR_POS;
    }
    // check the version no sure if necessary
    if(version != doc->head->chunksize) {
        return OUTDATE_VERSION;
    }
    chunk* current = doc->head;
    uint64_t current_pos = pos;
    //find the chunk that contains the position
    while (current != NULL && current_pos > current->chunksize) {
        current_pos -= current->chunksize;
        if(current_pos > 0 ) {
            current = current->next;
        }else {
            break;
        }         
    }
    // split the chunk if the position is in the middle of a chunk
    if( current_pos > 0 && current != NULL) {
            current = split_chunk(current, current_pos);
            current_pos = 0;
    }

    if(current_pos == 0) {
        if(current->next == NULL) {
            chunk* new_chunk = malloc(sizeof(chunk));
            new_chunk->data = content;
            new_chunk->chunksize = strlen(content);
            new_chunk->next = NULL;
            // find last chunk
            chunk* last_chunk = find_chunk(doc->head, pos);
            new_chunk->prev = last_chunk;
            last_chunk->next = new_chunk;
            doc->size += new_chunk->chunksize;
            return SUCCESS;
        }else if(current->next != NULL) {
            // create a new chunk
            chunk* new_chunk = malloc(sizeof(chunk));
            new_chunk->data = content;
            new_chunk->chunksize = strlen(content);
            new_chunk->next = current->next;
            new_chunk->prev = current;
            current->next->prev = new_chunk;
            current->next = new_chunk;   
            doc->size += new_chunk->chunksize;
        }        
    }
    return SUCCESS;
}

int markdown_delete(document *doc, uint64_t version, size_t pos, size_t len) {
    (void)doc; (void)version; (void)pos; (void)len;
    return SUCCESS;
}

// === Formatting Commands ===
int markdown_newline(document *doc, int version, int pos) {
    (void)doc; (void)version; (void)pos;
    return SUCCESS;
}

int markdown_heading(document *doc, uint64_t version, int level, size_t pos) {
    (void)doc; (void)version; (void)level; (void)pos;
    return SUCCESS;
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end) {
    (void)doc; (void)version; (void)start; (void)end;
    return SUCCESS;
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end) {
    (void)doc; (void)version; (void)start; (void)end;
    return SUCCESS;
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos) {
    (void)doc; (void)version; (void)pos;
    return SUCCESS;
}

int markdown_ordered_list(document *doc, uint64_t version, size_t pos) {
    (void)doc; (void)version; (void)pos;
    return SUCCESS;
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos) {
    (void)doc; (void)version; (void)pos;
    return SUCCESS;
}

int markdown_code(document *doc, uint64_t version, size_t start, size_t end) {
    (void)doc; (void)version; (void)start; (void)end;
    return SUCCESS;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos) {
    (void)doc; (void)version; (void)pos;
    return SUCCESS;
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url) {
    (void)doc; (void)version; (void)start; (void)end; (void)url;
    return SUCCESS;
}

// === Utilities ===
void markdown_print(const document *doc, FILE *stream) {
    (void)doc; (void)stream;
}

char *markdown_flatten(const document *doc) {
    (void)doc;
    return NULL;
}

// === Versioning ===
void markdown_increment_version(document *doc) {
    (void)doc;
}

