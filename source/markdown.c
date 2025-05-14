#include "../libs/markdown.h"

#define SUCCESS 0 
#define INVALID_CURSOR_POS -1
#define DELETE_POSITION -2
#define OUTDATE_VERSION -3

// === Init and Free ===
document *markdown_init(void) {
    document *doc = (document*)malloc(sizeof(document));
    doc->version = 0;
    doc->head = malloc(sizeof(chunk));
    doc->head->chunksize = 0;
    doc->head->chunkversion = doc->version;
    doc->head->data = NULL;
    doc->head->is_deleted = 0;
    doc->head->next = NULL;
    doc->head->prev = NULL;
    doc->size = 0;
    doc->version = 0;////to be changed
    return doc;
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

//hepler function to find the chunk that contains the position
chunk* find_chunk_at_logical_pos(chunk *current, size_t pos, size_t *out_offset) {
    size_t logical_pos = 0;
    while (current) {
        if (!current->is_deleted && current->chunkversion == 0) {
            if (pos < logical_pos + current->chunksize) {
                *out_offset = pos - logical_pos;
                return current;
            }
            logical_pos += current->chunksize;
        }
        if(logical_pos == pos){
            *out_offset = 0;
            return current;
        }
        current = current->next;
    }
    return NULL; // Not found
}

//hepler function to check last element is newline
int check_prev_char_newline(chunk* head, uint64_t pos) {
    if (pos == 0){
        return 1;
    }
    uint64_t current_pos;
    chunk* current = find_chunk_at_logical_pos(head, pos, &current_pos);
    if(current_pos == 0){
        if(current->data[current->chunksize-1] == '\n') {
            return 1;
        }else {
            return 0;
        }
    }else if(current_pos > 0) {
        if(current->data[current_pos-1] == '\n') {
            return 1;
        }else {
            return 0;
        }
    }///// 1 has \n, 0 not has \n
    return 0;
}


//hepler function split a chunk into two chunks
chunk* split_chunk(chunk* current, size_t pos, size_t len) {
    //pos must be greater than 0!!!!!
    chunk* new_chunk = malloc(sizeof(chunk));
    new_chunk->is_deleted = 0;

    if(len < current->chunksize) {    
        if(len == 0 || len > current->chunksize-pos) {
            new_chunk->data = malloc(current->chunksize - pos);
            memcpy(new_chunk->data, current->data + pos, current->chunksize - pos);
            new_chunk->chunksize = current->chunksize - pos;    
            new_chunk->chunkversion = current->chunkversion;
            new_chunk->next = current->next;
            new_chunk->next->prev = new_chunk;
            new_chunk->prev = current;
            current->next = new_chunk;
            current->chunksize = pos;
            //current->data = realloc(current->data, pos);            
        }else if(pos != 0 && len != 0) {
            ///current  -> new_chunk -> new_chunk1
            //middle chunk
            new_chunk->data = malloc(len+1);
            memcpy(new_chunk->data, current->data + pos, len);
            new_chunk->data[len] = '\0';
            new_chunk->chunksize = len;    
            new_chunk->chunkversion = current->chunkversion;
            // last chunk
            chunk* new_chunk1 =malloc(sizeof(chunk));
            new_chunk1->is_deleted = 0;
            new_chunk1->data = malloc(current->chunksize - pos - len);
            memcpy(new_chunk1->data, current->data + pos + len, current->chunksize - pos - len);
            new_chunk1->chunksize = current->chunksize - pos - len;
            new_chunk1->next = current->next;
            new_chunk->next = new_chunk1;
            current->next = new_chunk;
            new_chunk1->prev = new_chunk;
            new_chunk->prev = current;
            current->chunksize = pos;

            //printf("1:%s,2:%s,3:%s\n",current->data,new_chunk->data,new_chunk1->data);
            //current->data = realloc(current->data, current->chunksize);            
        }
    }


    return current;
}
// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content) {
    //(void)doc; (void)version; (void)pos; (void)content;
    if(content == NULL) {
        return DELETE_POSITION;
    }
    
    if(doc == NULL) {
        doc = markdown_init();
        doc->head->data = malloc(strlen(content)+1);
        doc->head->is_deleted = 0;
        strcpy(doc->head->data, content);
        doc->head->data[strlen(content)] = '\0';
        doc->head->chunksize = strlen(content);
        doc->version = 0;
        return SUCCESS;
    }
    if(version != doc->version) {
        printf("version is out of date!%ld,%ld\n",version,doc->version);
        return OUTDATE_VERSION;
    }
    if(pos > doc->size) {
        return INVALID_CURSOR_POS;
    }
    // check the version no sure if necessary
    
    uint64_t current_pos;
    //find the chunk that contains the position
    // while (current != NULL && current_pos >= current->chunksize) {
    //     if(current->is_deleted == 0 && current->chunkversion == 0) {/// chunk version may be greater
    //         current_pos -= current->chunksize;
    //     }
    //     if(current_pos > 0 || (current_pos == 0 && current->is_deleted == 1)) {
    //         current = current->next;
    //     }else {
    //         break;
    //     }         
    // }
    chunk* current = find_chunk_at_logical_pos(doc->head, pos, &current_pos);
    // split the chunk if the position is in the middle of a chunk
    if (current_pos > 0 && current != NULL) {
            current = split_chunk(current, current_pos,0);
            current_pos = 0;
    }
    // insert
    if(current_pos == 0) {
        chunk* new_chunk = malloc(sizeof(chunk));
        new_chunk->is_deleted = 0;
        new_chunk->data = malloc(strlen(content)+1);
        memcpy(new_chunk->data, content,strlen(content));//////
        new_chunk->chunksize = strlen(content); 
        new_chunk->chunkversion = current->chunkversion + 1;    ///////
        if(current->next == NULL && pos != 0) {

            new_chunk->next = NULL;
            // find last chunk
            chunk* last_chunk = current;
            new_chunk->prev = last_chunk;
            last_chunk->next = new_chunk;
            return SUCCESS;
        }else if(current->next != NULL && pos !=0) {
            // create a new chunk
            new_chunk->next = current->next;
            new_chunk->prev = current;
            current->next->prev = new_chunk;
            current->next = new_chunk;   
        }else if(pos == 0) {
            new_chunk->next = current;
            new_chunk->prev = NULL;
            current->prev = new_chunk;
            doc->head = new_chunk;

        }

    }
        
    return SUCCESS;
}

int markdown_delete(document *doc, uint64_t version, size_t pos, size_t len) {
    //(void)doc; (void)version; (void)pos; (void)len;
    //do some basic delete, do not handle version, just delete the given pos and len.
    if(doc == NULL) {
        return SUCCESS;
    }
    if(doc->size == 0) {
        return SUCCESS;
    }
    if(version != doc->version) {
        return OUTDATE_VERSION;
    }
    if(pos > doc->size) {
        return INVALID_CURSOR_POS;
    }
    if(pos == doc->size) {
        return SUCCESS;
    }
    if(len == 0) {
        return SUCCESS;
    }
    if(pos + len > doc->size) {
        len = doc->size - pos;
    }
    // delete the whole document
    if(pos == 0 && len == doc->size) {
        chunk* t1 = doc->head;
        chunk* t2 = doc->head;
        while(t1 != NULL) {
            t2 = t1->next;
            t1->is_deleted = 1;
            t1 = t2;
        }
        return SUCCESS;
    }
    // find the chunk that contains the position
    chunk* current = doc->head;
    chunk* last_chunk = doc->head;
    uint64_t current_pos = pos;
    uint64_t last_pos = pos + len;
    //find the chunk that contains the position
    while (current != NULL && current_pos >= current->chunksize) {
        if(current->is_deleted == 0 && current->chunkversion == 0) {/// chunk version may be greater
            current_pos -= current->chunksize;
        }    
        if(current_pos > 0 ||(current_pos == 0 && current->is_deleted == 1)) {
            current = current->next;
        }else {
            break;
        }        
    }
    // find the last chunk that contains the position
    while (last_chunk != NULL && last_pos >= last_chunk->chunksize) {
        last_pos -= last_chunk->chunksize;
        if(last_pos > 0 ) {
            last_chunk = last_chunk->next;
        }else {
            break;
        }         
    }
    
    // split the chunk if the position is in the middle of a chunk
    if(current != last_chunk){

        // get the prev and after chunk of deleted chunk
        if(current_pos > 0) {
            current = split_chunk(current, current_pos, 0);
            current_pos = 0;
        }else if(current_pos == 0) {
            
        }
        if(last_pos > 0) {
                last_chunk = split_chunk(last_chunk, last_pos, 0);
                last_chunk = last_chunk->next;
                last_pos = 0;
        }else if(last_pos == 0) {

        }        
        // delete the chunk
        chunk* temp1 = current->next;
        chunk* temp2 = current->next;
        while(temp1 != last_chunk) {
            temp1 = temp1->next;
            temp2->is_deleted = 1;
            temp2 = temp1;
        }
        current->next = last_chunk;
        last_chunk->prev = current;
    }else if (current == last_chunk) {
        //current must be 3 parts 
        //printf("currentpos: %ld, len: %ld, size: %ld\n",current_pos,len,doc->size);
        current = split_chunk(current, current_pos, len);

        chunk *to_delete = current->next;
        to_delete->is_deleted = 1;
        current->next = to_delete->next;
        if (to_delete->next != NULL) {
            to_delete->next->prev = current;
        }
        current->next->prev = current;
    }
    return SUCCESS;
}

// === Formatting Commands ===
int markdown_newline(document *doc, int version, int pos) {
    //(void)doc; (void)version; (void)pos;
    markdown_insert(doc, version, pos, "\n");

    return SUCCESS;
}

int markdown_heading(document *doc, uint64_t version, int level, size_t pos) {
    char* buf = malloc((level + 2) * sizeof(char));
    char* buf1 = malloc((level + 3) * sizeof(char));
    for(int i = 0; i < level; i++) {
        buf[i] = '#';
        buf[i+1] = '#';
    }
    buf1[0] = '\n';
    buf[level] = ' ';
    buf1[level+1] = ' ';
    buf1[level+2] = '\0';
    buf[level+1] = '\0';
    chunk* current = doc->head;
    // uint64_t current_pos;
    // chunk* current = find_chunk_at_logical_pos(current, pos, &current_pos);
    // if(current_pos == 0) {
    //     if(current->data[current->chunksize-1] != '\n') {
    //         markdown_insert(doc, version, pos+1, buf1);
    //     }else {
    //         markdown_insert(doc, version, pos, buf);
    //     }
        
    // }else{
    //     if(current->data[current_pos-1] != '\n') {
    //         markdown_insert(doc, version, pos+1, buf1);
    //     }else {
    //         markdown_insert(doc, version, pos, buf);
    
    //     }        
    // }
    int is_newline = check_prev_char_newline(current, pos);
    if(is_newline == 1) {
        markdown_insert(doc, version, pos, buf);
    }else {
        markdown_insert(doc, version, pos, buf1);
    }
    free(buf);
    free(buf1);
    return SUCCESS;
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end) {
    //(void)doc; (void)version; (void)start; (void)end;
    const char *buf = "**";
    markdown_insert(doc, version, start, buf);
    markdown_insert(doc, version, end, buf);
    return SUCCESS;
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end) {
    //(void)doc; (void)version; (void)start; (void)end;
    const char *buf = "*";
    markdown_insert(doc, version, start, buf);
    markdown_insert(doc, version, end, buf);
    return SUCCESS;
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos) {
    //(void)doc; (void)version; (void)pos;
    const char *buf = "> ";
    const char *buf1 = "\n> ";
    int is_newline = check_prev_char_newline(doc->head, pos);
    if(is_newline == 1) {
        markdown_insert(doc, version, pos, buf);
    }else {
        markdown_insert(doc, version, pos, buf1);
    }
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
    //(void)doc; (void)version; (void)start; (void)end;
    const char *buf = "`";
    markdown_insert(doc, version, start, buf);
    markdown_insert(doc, version, end, buf);
    return SUCCESS;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos) {
    //(void)doc; (void)version; (void)pos;
    const char *buf = "---\n";
    const char *buf1 = "\n---\n";
    int is_newline = check_prev_char_newline(doc->head, pos);
    if(is_newline == 1) {
        markdown_insert(doc, version, pos, buf);
    }else {
        markdown_insert(doc, version, pos, buf1);
    }
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
    char* result = malloc(doc->size + 1);
    chunk* current = doc->head;
    size_t offset = 0;
    while(current){
        if(current->is_deleted == 0 && current->chunkversion == 0) {
            for(size_t i = 0; i < current->chunksize; i++) {
                result[offset + i] = current->data[i];
            }
            offset += current->chunksize;
        }
        current = current->next;
    }

    return result;
}

// update the chunck version
int markdown_update_chunk_version(document *doc) {
    chunk* current = doc->head;
    while(current != NULL) {
        current->chunkversion = doc->version;
        current = current->next;
    }
    return SUCCESS;
}
// === Versioning ===
void markdown_increment_version(document *doc) {
    doc->version++;

}

