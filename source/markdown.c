#include "../libs/markdown.h"

#define SUCCESS 0 
#define INVALID_CURSOR_POS -1
#define DELETE_POSITION -2
#define OUTDATE_VERSION -3

char* bufdoc = NULL;
// === Init and Free ===
document *markdown_init(void) {
    document *doc = (document*)malloc(sizeof(document));
    doc->text = (char*)malloc(1024);
    doc->version = 0;
    doc->size = 0;
    doc->head = (chunk*)malloc(sizeof(chunk));
    doc->head->data = NULL;
    doc->head->chunksize = 0;
    doc->head->chunkversion = doc->version;
    doc->head->next = NULL;
    doc->head->prev = NULL;
    doc->head->is_deleted = 0;

    return doc;
}

void markdown_free(document *doc) {
    //free(bufdoc);
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
    free(doc->text);
    free(doc);
    doc = NULL;
}
//get old version
uint64_t markdown_get_size(const document *doc) {
    if(doc == NULL) {
        return 0;
    }
    uint64_t size = 0;
    chunk* current = doc->head;
    while (current != NULL) {
        if(current->chunkversion == doc->version &&(current->is_deleted == 0 || current->is_deleted > doc->version)) {
            size += current->chunksize;
        }
        current = current->next;
    }
    return size;
}

//hepler function to find the chunk that contains the position
chunk* find_chunk_at_logical_pos(document* doc, size_t pos, size_t *out_offset) {
    size_t logical_pos = 0;
    chunk* current = doc->head;
    while (current) {
        if (current->chunkversion == doc->version) {
            if (pos < logical_pos + current->chunksize) {
                *out_offset = pos - logical_pos;
                return current;
            }
            logical_pos += current->chunksize;
        }
        // if(logical_pos == pos){
        //     *out_offset = 0;
        //     return current;
        // }
        if(current->next == NULL) {
            *out_offset = 0;
            return current;
        }
        current = current->next;

    }
    printf("current_pos is %ld\n",logical_pos);

    return NULL; // insert at the beginning
}

//hepler function to check last element is newline
int check_prev_char_newline(document* doc, uint64_t pos) {
    
    if (pos == 0){
        return 1;
    }
    uint64_t current_pos;
    chunk* current = find_chunk_at_logical_pos(doc, pos, &current_pos);
    if(current_pos == 0){
        if(current->prev->data[current->chunksize-1] == '\n') {
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
chunk* split_chunk(chunk* current, size_t pos, size_t len, document* doc) {
    //pos must be greater than 0!!!!!
    chunk* new_chunk = malloc(sizeof(chunk));
    new_chunk->is_deleted = current->is_deleted;

    if(len < current->chunksize) {  // split in two 
        //printf("pos: %ld\n",pos);
        if(pos!= 0 && (len == 0 || len >= current->chunksize-pos)) {
            new_chunk->data = malloc(current->chunksize - pos);
            memcpy(new_chunk->data, current->data + pos, current->chunksize - pos);
            new_chunk->chunksize = current->chunksize - pos;    
            new_chunk->chunkversion = doc->version;
            new_chunk->next = current->next;
            new_chunk->next->prev = new_chunk;
            new_chunk->prev = current;
            current->next = new_chunk;
            current->chunksize = pos;
            return current->next;            
        }else if(pos != 0 && len > 0 && len < current->chunksize - pos) { // split in three
            ///current  -> new_chunk -> new_chunk1
            //middle chunk
            new_chunk->data = malloc(len+1);
            memcpy(new_chunk->data, current->data + pos, len);
            new_chunk->data[len] = '\0';
            new_chunk->chunksize = len;    
            new_chunk->chunkversion = doc->version;
            // last chunk
            chunk* new_chunk1 =malloc(sizeof(chunk));
            new_chunk1->is_deleted = current->is_deleted;
            new_chunk1->data = malloc(current->chunksize - pos - len);
            memcpy(new_chunk1->data, current->data + pos + len, current->chunksize - pos - len);
            new_chunk1->chunksize = current->chunksize - pos - len;
            new_chunk1->next = current->next;
            new_chunk->next = new_chunk1;
            current->next = new_chunk;
            new_chunk1->prev = new_chunk;
            new_chunk->prev = current;
            current->chunksize = pos;
            return new_chunk1;
            //printf("1:%s,2:%s,3:%s\n",current->data,new_chunk->data,new_chunk1->data);        
        }else if (pos == 0 && len > 0 && len < current->chunksize) { // split in two
            printf("split in two\n");
            new_chunk->data = malloc(current->chunksize - len+1);
            memcpy(new_chunk->data, current->data + len, current->chunksize - len);
            new_chunk->data[current->chunksize - len] = '\0';
            new_chunk->chunksize = current->chunksize - len;    
            new_chunk->chunkversion = doc->version;
            new_chunk->prev = current;
            new_chunk->next = current->next;
            current->next = new_chunk;
            current->chunksize = len;
            return new_chunk;
        }
        
    }


    return current;
}
// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content) {
    //(void)doc; (void)version; (void)pos; (void)content;
    uint64_t size = markdown_get_size(doc);
    if(content == NULL) {
        printf("content is NULL\n");
        return DELETE_POSITION;
    }
    
    if(doc == NULL) {
        doc = markdown_init();
        doc->head->data = malloc(strlen(content)+1);
        doc->head->is_deleted = 0;
        strcpy(doc->head->data, content);
        doc->head->data[strlen(content)] = '\0';
        doc->head->chunksize = strlen(content);
        doc->size = strlen(content);
        return SUCCESS;
    }
    if(version != doc->version) {
        printf("version is out of date!%ld,%ld\n",version,doc->version);
        return OUTDATE_VERSION;
    }
    if(pos > size) {
        printf("invalid cursor position!%ld,%ld\n",pos,size);
        return INVALID_CURSOR_POS;
    }
    // check the version no sure if necessary
    uint64_t current_pos;
    
    
    chunk* current = find_chunk_at_logical_pos(doc, pos, &current_pos);
    //printf("current_pos after func: %ld\n",current_pos);
    // split the chunk if the position is in the middle of a chunk
    if (current_pos > 0 && current != NULL) {
            current = split_chunk(current, current_pos,0, doc);
            current_pos = 0;
    }
    
    // insert
    doc->size += strlen(content);
    if(current_pos == 0) {
        chunk* new_chunk = malloc(sizeof(chunk));
        new_chunk->is_deleted = 0;
        new_chunk->data = malloc(strlen(content)+1);
        memcpy(new_chunk->data, content,strlen(content));//////
        new_chunk->chunksize = strlen(content); 
        new_chunk->chunkversion = doc->version + 1;
        
        if(current->next == NULL && pos != 0) {/////add to the end
            printf("insert at the end\n");
            new_chunk->next = NULL;
            // find last chunk
            chunk* last_chunk = current->prev;
            new_chunk->prev = last_chunk;
            last_chunk->next = new_chunk;
            return SUCCESS;
        }else if(current->prev != NULL && pos !=0) {// insert in the middle
            // create a new chunk
            printf("insert in the middle\n");
            new_chunk->next = current;
            new_chunk->prev = current->prev;
            current->prev->next = new_chunk;
            current->prev = new_chunk;   
        }else if(pos == 0) {// insert at the beginning
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
    uint64_t size = markdown_get_size(doc);
    if(doc == NULL) {
        return SUCCESS;
    }
    if(size == 0) {
        return SUCCESS;
    }
    if(version != doc->version) {
        printf("version is out of date!%ld,%ld\n",version,doc->version);
        return OUTDATE_VERSION;
    }
    if(pos > size) {
        return INVALID_CURSOR_POS;
    }
    if(pos == size) {
        return SUCCESS;
    }
    if(len == 0) {
        return SUCCESS;
    }
    if(pos + len > size) {
        len = size - pos;
    }
    // delete the whole document
    if(pos == 0 && len == size) {
        chunk* t1 = doc->head;
        chunk* t2 = doc->head;
        while(t1 != NULL) {
            t2 = t1->next;
            if(t1->chunkversion == doc->version) {
                t1->is_deleted = doc->version + 1;
            }
            
            t1 = t2;
        }
        doc->size = 0;
        return SUCCESS;
    }
    // find the chunk that contains the position
    uint64_t current_pos;
    uint64_t last_pos;
    chunk* current = find_chunk_at_logical_pos(doc, pos, &current_pos);
    chunk* last_chunk = find_chunk_at_logical_pos(doc, pos + len, &last_pos);

    // split the chunk if the position is in the middle of a chunk
    if(current != last_chunk){

        // get the prev and after chunk of deleted chunk
        if(current_pos > 0) {
            current = split_chunk(current, current_pos, 0, doc);
            current_pos = 0;
        }
        if(last_pos > 0) {
                last_chunk = split_chunk(last_chunk, last_pos, 0, doc);
                last_pos = 0;
        }
        // mark the chunk as deleted
        chunk* temp1 = current;
        chunk* temp2 = current;
        while(temp1 != last_chunk) {
            temp1 = temp1->next;
            
            if(temp2->chunkversion == doc->version) {
                temp2->is_deleted = doc->version + 1;
            }
            temp2 = temp1;
        }

    }else if (current == last_chunk) {
        //current must be 3 parts 
        //printf("currentpos: %ld, len: %ld, size: %ld\n",current_pos,len,doc->size);

        current = split_chunk(current, current_pos, len, doc);
        current->prev->is_deleted = doc->version + 1;
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
    int is_newline = check_prev_char_newline(doc, pos);
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
    int is_newline = check_prev_char_newline(doc, pos);
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
    const char *buf = "- ";
    const char *buf1 = "\n- ";
    int is_newline = check_prev_char_newline(doc, pos);
    if(is_newline == 1) {
        markdown_insert(doc, version, pos, buf);
    }else {
        markdown_insert(doc, version, pos, buf1);
    }
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
    int is_newline = check_prev_char_newline(doc, pos);
    if(is_newline == 1) {
        markdown_insert(doc, version, pos, buf);
    }else {
        markdown_insert(doc, version, pos, buf1);
    }
    return SUCCESS;
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url) {
    (void)doc; (void)version; (void)start; (void)end; (void)url;
    const char *buf = "[";
    int size = strlen(url) + 4;
    char* buf1 = malloc(size * sizeof(char));
    buf1[size]= '\0';
    snprintf(buf1, sizeof(buf), "](%s)", url);
    markdown_insert(doc, version, start-1, buf);
    markdown_insert(doc, version, end, buf1);
    free(buf1);
    return SUCCESS;
}

// === Utilities ===
void markdown_print(const document *doc, FILE *stream) {
    (void)doc; (void)stream;

}



char *markdown_flatten(const document *doc) {

    uint64_t size = markdown_get_size(doc);
    if(size == 0) {
        bufdoc = malloc(1);
        bufdoc[0] = '\0';
        return bufdoc;
    }
    bufdoc = malloc(size + 1);
    chunk* current = doc->head;
    size_t offset = 0;
    while(current){
        if(current->chunkversion == doc->version && (current->is_deleted == 0 || current->is_deleted > doc->version)) {
            for(size_t i = 0; i < current->chunksize; i++) {
                bufdoc[offset + i] = current->data[i];
            }
            offset += current->chunksize;
        }
        current = current->next;
    }
    bufdoc[size] = '\0';
    return bufdoc;
}

// update the chunck version
int markdown_update_chunk_version(document *doc) {
    chunk* current = doc->head;
    while(current != NULL) {
        if(current->is_deleted == 0) {
            current->chunkversion = doc->version;
        }
        current = current->next;
    }
    return SUCCESS;
}
// === Versioning ===
void markdown_increment_version(document *doc) {
    doc->version++;
    markdown_update_chunk_version(doc);

}

// int main(int argc, char** argv) {
//     char* result;
//     document* doc = markdown_init();
//     markdown_insert(doc, 0, 0, "hello world");
//     //printf("%ld,\n",doc->head->chunkversion);
//     markdown_increment_version(doc);
//     result = markdown_flatten(doc);
//     printf("result: %s\n", result);    
//     printf("---------------\n");

//     markdown_delete(doc, 1, 0, 11);
//     markdown_insert(doc, 1, 5, "!");
//     markdown_insert(doc, 1, 5, "aaa");
//     markdown_insert(doc, 1, 5, "bbb");
//     markdown_insert(doc, 1, 5, "ccc");
//     markdown_insert(doc, 1, 5, "ddd");
//     markdown_increment_version(doc);
//     result = markdown_flatten(doc);
//     printf("result: %s\n", result);
//     //printf("%d,%d,\n",doc->head->is_deleted,doc->head->next->is_deleted);
//     printf("---------------\n");
//     markdown_delete(doc, 2, 0, 1);
//     markdown_delete(doc, 2, 2, 4);   
//     markdown_increment_version(doc);
//     result = markdown_flatten(doc);
//     printf("version: %ld\n",doc->version);
//     printf("result: %s\n", result);    
    
//     // markdown_increment_version(doc);

//     free(result);
//     markdown_free(doc);
//     return 0;
// }
