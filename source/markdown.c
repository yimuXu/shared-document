#include "../libs/markdown.h"
#include <ctype.h>

#define SUCCESS 0 
#define INVALID_CURSOR_POS -1
#define DELETED_POSITION -2
#define OUTDATE_VERSION -3

char* bfdc;

// === Init and Free ===
document *markdown_init(void) {
    document *doc = (document*)malloc(sizeof(document));
    //doc->text = (char*)malloc(1024);
    doc->version = 0;
    doc->size = 0;
    doc->head = (chunk*)malloc(sizeof(chunk));
    doc->head->data = NULL;
    doc->head->chunksize = 0;
    doc->head->chunkversion = doc->version;
    doc->head->next = NULL;
    doc->head->prev = NULL;
    doc->head->is_deleted = 0;
    doc->head->order_num = 0;

    return doc;
}

void markdown_free(document *doc) {
    if (doc == NULL || doc->head->data == NULL) {
        return;
    }
    chunk *current = doc->head;
    while (current != NULL) {
        chunk *next = current->next;
        if(current->data != NULL){
            free(current->data); 
        }
        
        free(current);
        current = next;
    }
    //free(doc->text);
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
        if (current->chunkversion == doc->version && (current->is_deleted == 0 || current->is_deleted > doc->version)) {
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
    //printf("current_pos is %ld\n",logical_pos);

    if (current == NULL) {
        *out_offset = 0;
        return current;
    }
    return NULL; // insert at the beginning
}

//hepler function to check last element is newline
int check_prev_char_newline(document* doc, uint64_t pos) {
    uint64_t current_pos = 0;
    chunk* current = find_chunk_at_logical_pos(doc, pos, &current_pos);    
    if (pos == 0 || current == NULL){
        return 1;
    }
    while(current){
        if(current->chunkversion == doc->version && (current->is_deleted == 0 || current->is_deleted > doc->version)){
            break;    
        }
        current= current->prev;
    }
    if(current_pos == 0 && current->data != NULL){
        if(current->data[current->chunksize-1] == '\n') {
            return 1;
        }else {
            return 0;
        }
    }else if(current_pos > 0 && current->data != NULL) {
        if(current->data[current_pos-1] == '\n') {
            return 1;
        }else {
            return 0;
        }
    }///// 1 has \n, 0 not has \n
    return 0;
}

//helper function to check what followed element is
int check_next_char(document* doc, uint64_t version, uint64_t pos, char* content){
    (void)version;
    uint64_t current_pos = 0;
    chunk* current = find_chunk_at_logical_pos(doc,pos ,&current_pos);

    while(current){
        if(current->chunkversion == doc->version && (current->is_deleted == 0 || current->is_deleted > doc->version)){
            break;    
        }
        current= current->next;
    }
    if(current == NULL || current->data == NULL){
        return 0;
    }
    if(current_pos != 0 && current_pos < current->chunksize){
        
        if(current->data[current_pos] == content[0]){
            
            return 1;
        }else{
            return 0;
        }
    }else if(current_pos == 0){
        //printf("current next: %s\n",current->next);
        if (current->next == NULL || current->next->data == NULL){
            //printf("imein\n");
            return 0;

        }else if(current->next != NULL  && current->next->data == NULL){
            if(current->next->data[0] == content[0]){
                return 1;
            }else {
                return 0;
            }                
            
        }
        
    }
    return 0;
}

//hepler function split a chunk into two chunks
chunk* split_chunk(chunk* current, size_t pos, size_t len, document* doc) {
    //pos must be greater than 0!!!!!
    chunk* new_chunk = malloc(sizeof(chunk));
    new_chunk->is_deleted = current->is_deleted;
    new_chunk->order_num = 0;
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
            //printf("1:%s,2:%s,3:%s\n",current->data,new_chunk->data,new_chunk1->data);        
        }else if (pos == 0 && len > 0 && len < current->chunksize) { // split in two
            //printf("split in two\n");
            new_chunk->data = malloc(current->chunksize - len+1);
            memcpy(new_chunk->data, current->data + len, current->chunksize - len);
            new_chunk->data[current->chunksize - len] = '\0';
            new_chunk->chunksize = current->chunksize - len;    
            new_chunk->chunkversion = doc->version;
            new_chunk->prev = current;
            new_chunk->next = current->next;
            current->next = new_chunk;
            current->chunksize = len;
        }else if(pos == 0 && len == current->chunksize) { // split in two

        }
        
    }
    return current;
}
// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content) {
    //(void)doc; (void)version; (void)pos; (void)content;
    uint64_t size = markdown_get_size(doc);
    if(content == NULL) {
        //printf("content is NULL\n");
        return INVALID_CURSOR_POS                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 ;
    }
    // for order_list
    int ordernum = 0;
    if(strlen(content) == 2){
        if(isdigit((unsigned char)content[0]) && content[1] == '.'){
            ordernum = content[0] -'0';
        }
    }
    
    if(doc == NULL) {
        doc = markdown_init();
        doc->head->data = malloc(strlen(content)+1);
        doc->head->is_deleted = 0;
        strcpy(doc->head->data, content);
        doc->head->data[strlen(content)] = '\0';
        doc->head->chunksize = strlen(content);
        doc->size = strlen(content);
        doc->head->order_num = ordernum;
        return SUCCESS;
    }
    if(version != doc->version) {
        printf("version is out of date!%ld,%ld\n",version,doc->version);
        return OUTDATE_VERSION;
    }
    if(pos > size) {
        //printf("invalid cursor position!%ld,%ld\n",pos,size);
        return INVALID_CURSOR_POS;
;
    }
    // check the version no sure if necessary
    uint64_t current_pos;
    
    
    chunk* current = find_chunk_at_logical_pos(doc, pos, &current_pos);
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
        new_chunk->order_num = ordernum;
        //printf("version: %ld\n",new_chunk->chunkversion);
        if(current->next == NULL && pos != 0) {/////add to the end
            //printf("insert at the end\n");
            new_chunk->next = NULL;
            // find last chunk
            chunk* last_chunk = current;
            new_chunk->prev = last_chunk;
            last_chunk->next = new_chunk;
            new_chunk->next = NULL;
        }else if(current != NULL && pos !=0) {// insert in the middle
            // create a new chunk
            //printf("insert in the middle\n");
            new_chunk->next = current->next;
            new_chunk->prev = current;
            current->next->prev = new_chunk;
            current->next = new_chunk;
        }else if(pos == 0) {// insert at the beginning
            //printf("insert at the beginning\n");
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
        return INVALID_CURSOR_POS;
    }
    if(size == 0) {
        return INVALID_CURSOR_POS;
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
    //last_chunk = last_chunk->next;
   //printf("current data: %ld, current_pos: %ld, last_pos: %ld\n",len,current_pos,last_pos);
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
        last_chunk = last_chunk->next;
        // mark the chunk as deleted
        chunk* temp1 = current->next;
        chunk* temp2 = current->next;
        while(temp1 != last_chunk) {
            temp1 = temp1->next;
            
            if(temp2->chunkversion == doc->version) {
                temp2->is_deleted = doc->version + 1;
            }
            temp2 = temp1;
        }
        //printf("delete the whole chunk\n");
    }else if (current == last_chunk) {
        
        //current must be 3 parts 
        //printf("currentpos: %ld, len: %ld, size: %ld\n",current_pos,len,doc->size);

        current = split_chunk(current, current_pos, len, doc);
        current->next->is_deleted = doc->version + 1;            
        
 
    }
    return SUCCESS;
}

// === Formatting Commands ===
int markdown_newline(document *doc, int version, int pos) {
    //(void)doc; (void)version; (void)pos;
    int result = markdown_insert(doc, version, pos, "\n");
    if(result==-1){
        return INVALID_CURSOR_POS;
    }else if(result == -2){
        return DELETED_POSITION;
    }else if(result ==  -3){
        return OUTDATE_VERSION;
    }
    return SUCCESS;
}

int markdown_heading(document *doc, uint64_t version, int level, size_t pos) {
    int result;
    char* buf = malloc((level + 2) * sizeof(char));
    for(int i = 0; i < level; i++) {
        buf[i] = '#';
        
    }
    buf[level] = ' ';
    buf[level+1] = '\0';
    int is_newline = check_prev_char_newline(doc, pos);
    result = markdown_insert(doc, version, pos, buf);
    if(result==-1){
        return INVALID_CURSOR_POS;
    }else if(result == -2){
        return DELETED_POSITION;
    }else if(result ==  -3){
        return OUTDATE_VERSION;
    }
    if(is_newline != 1) {
        result = markdown_newline(doc, version, pos);
        if(result==-1){
            return INVALID_CURSOR_POS;
        }else if(result == -2){
            return DELETED_POSITION;
        }else if(result ==  -3){
            return OUTDATE_VERSION;
        }
    }
    free(buf);


    return SUCCESS;
}

int markdown_bold(document *doc, uint64_t version, size_t start, size_t end) {
    //(void)doc; (void)version; (void)start; (void)end;
    const char *buf = "**";
    int result1,result2;
    result1 = markdown_insert(doc, version, start, buf);
    result2 = markdown_insert(doc, version, end, buf);
    if(result1==-1 || result2 == -1){
        return INVALID_CURSOR_POS;
    }else if(result1==-2 || result2 == -2){
        return DELETED_POSITION;
    }else if(result1==-3 || result2 == -3){
        return OUTDATE_VERSION;
    }
    return SUCCESS;
}

int markdown_italic(document *doc, uint64_t version, size_t start, size_t end) {
    //(void)doc; (void)version; (void)start; (void)end;
    int result1,result2;
    const char *buf = "*";
    result1 = markdown_insert(doc, version, start, buf);
    result2 = markdown_insert(doc, version, end, buf);
    if(result1==-1 || result2 == -1){
        return INVALID_CURSOR_POS;
    }else if(result1==-2 || result2 == -2){
        return DELETED_POSITION;
    }else if(result1==-3 || result2 == -3){
        return OUTDATE_VERSION;
    }
    return SUCCESS;
}

int markdown_blockquote(document *doc, uint64_t version, size_t pos) {
    //(void)doc; (void)version; (void)pos;
    const char *buf = "> ";
    int result;
    int is_newline = check_prev_char_newline(doc, pos);
    result = markdown_insert(doc, version, pos, buf);
    if(result==-1){
        return INVALID_CURSOR_POS;
    }else if(result == -2){
        return DELETED_POSITION;
    }else if(result ==  -3){
        return OUTDATE_VERSION;
    }
    if(is_newline == 0) {
        result = markdown_newline(doc, version, pos);   
        if(result==-1){
            return INVALID_CURSOR_POS;
        }else if(result == -2){
            return DELETED_POSITION;
        }else if(result ==  -3){
            return OUTDATE_VERSION;
        }
    }

    return SUCCESS;
}

// find last newline before pos
int find_last_order_number(document* doc, uint64_t pos){
    size_t current_pos = 0;
    chunk* current = find_chunk_at_logical_pos(doc,pos,&current_pos);
    int new_line = check_prev_char_newline(doc, pos);

    if(current == NULL || pos == 0){
        return 0;
    }
    if(new_line == 1){
        current = current->prev->prev;
    }    
    chunk* temp = current;
    chunk* result = NULL;
    while(temp){
        //printf("imin ordernum:%ld\n",temp->chunkversion);
        if(temp->data && temp-> chunksize == 2 && temp->order_num != 0 &&
            temp->chunkversion == doc->version &&
            (temp->is_deleted == 0 || temp->is_deleted > doc->version)){
                //printf("i find a num\n");
                return temp->order_num;
            }    
        if(temp->data && temp-> chunksize == 1 && temp->data[0] == '\n' &&
            temp->chunkversion == doc->version &&
            (temp->is_deleted == 0 || temp->is_deleted > doc->version)){
            
            result = temp;
            break;
        }
        temp = temp->prev;
    }
    if(result == NULL || result->data == NULL){
        return 0;
    }else if(result->order_num != 0){
        return result->order_num;
    }
    return 0;
    

}

// modify followed order function
int modify_order_number(document* doc, uint64_t version, uint64_t pos, char start){
    size_t current_pos = 0;
    chunk* current = find_chunk_at_logical_pos(doc,pos,&current_pos);
    if(current == NULL || current->data == NULL){
        return 1;////ERROR
    }
    int count = 1;
    size_t modifypos = pos; // for modify order num
    while(current){
        if(current->order_num != 0 && current->chunkversion == doc->version&& (current->is_deleted == 0 || current->is_deleted > doc->version)){
            markdown_delete(doc, version, modifypos,2);
            char num[3];
            snprintf(num, 3,"%c.",start + count);
            markdown_insert(doc,version,modifypos, num);
            modifypos+= current->chunksize;
            count++;
        }
        current = current->next;
    }
    return 0;
}

int markdown_ordered_list(document *doc, uint64_t version, size_t pos) {
    (void)doc; (void)version; (void)pos;
    if(version != doc->version){
        printf("outdate version.\n");
        return OUTDATE_VERSION;
    }
    int is_newline = check_prev_char_newline(doc, pos);
    char buf[3];
    buf[2] = '\0';
    char i = '0';
    char* spa = " "; 
    //check 
    int o = find_last_order_number(doc,pos);
    
    i = o + '0' + 1;
    //printf(" i :%c\n",i);
    snprintf(buf, sizeof(buf), "%c.", i);
    //printf(" buf :%s\n",buf);
    modify_order_number(doc,version, pos, i);
    int result;
    int is_space = check_next_char(doc, version, pos, spa);
    if(is_space == 0){
        //printf("print spac\n");
        result = markdown_insert(doc, version, pos, spa);
        if(result==-1){
            return INVALID_CURSOR_POS;
        }else if(result == -2){
            return DELETED_POSITION;
        }else if(result ==  -3){
            return OUTDATE_VERSION;
        }
    }
    result = markdown_insert(doc, version, pos, buf);
    if(result==-1){
        return INVALID_CURSOR_POS;
    }else if(result == -2){
        return DELETED_POSITION;
    }else if(result ==  -3){
        return OUTDATE_VERSION;
    }
    if(is_newline != 1) {
        result = markdown_newline(doc, version, pos);
        if(result==-1){
            return INVALID_CURSOR_POS;
        }else if(result == -2){
            return DELETED_POSITION;
        }else if(result ==  -3){
            return OUTDATE_VERSION;
        }
    }



    return SUCCESS;
}

int markdown_unordered_list(document *doc, uint64_t version, size_t pos) {
    //(void)doc; (void)version; (void)pos;
    const char *buf = "- ";
    int result;
    int is_newline = check_prev_char_newline(doc, pos);
    result = markdown_insert(doc, version, pos, buf);
    if(result==-1){
        return INVALID_CURSOR_POS;
    }else if(result == -2){
        return DELETED_POSITION;
    }else if(result ==  -3){
        return OUTDATE_VERSION;
    }
    if(is_newline != 1) {
        result = markdown_newline(doc, version, pos);
        if(result==-1){
            return INVALID_CURSOR_POS;
        }else if(result == -2){
            return DELETED_POSITION;
        }else if(result ==  -3){
            return OUTDATE_VERSION;
        }
    }
    return SUCCESS;
}

int markdown_code(document *doc, uint64_t version, size_t start, size_t end) {
    //(void)doc; (void)version; (void)start; (void)end;
    const char *buf = "`";
    int result1,result2;
    result1 = markdown_insert(doc, version, start, buf);
    result2 = markdown_insert(doc, version, end, buf);
    if(result1==-1 || result2 == -1){
        return INVALID_CURSOR_POS;
    }else if(result1==-2 || result2 == -2){
        return DELETED_POSITION;
    }else if(result1==-3 || result2 == -3){
        return OUTDATE_VERSION;
    }
    return SUCCESS;
}

int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos) {
    //(void)doc; (void)version; (void)pos;
    const char *buf = "---";
    char* newline = "\n";
    int next_newline = check_next_char(doc, version, pos, newline);
    int result;
    printf("check char: %d",next_newline);
    if(next_newline == 0){
        result = markdown_insert(doc,version,pos,newline);
        if(result==-1){
            return INVALID_CURSOR_POS;
        }else if(result == -2){
            return DELETED_POSITION;
        }else if(result ==  -3){
            return OUTDATE_VERSION;
        }
    }
    result = markdown_insert(doc, version, pos, buf);
    if(result==-1){
        return INVALID_CURSOR_POS;
    }else if(result == -2){
        return DELETED_POSITION;
    }else if(result ==  -3){
        return OUTDATE_VERSION;
    }
    int is_newline = check_prev_char_newline(doc, pos);
    if(is_newline == 0) {
        result = markdown_insert(doc, version, pos, newline);
        if(result==-1){
            return INVALID_CURSOR_POS;
        }else if(result == -2){
            return DELETED_POSITION;
        }else if(result ==  -3){
            return OUTDATE_VERSION;
        }
    }
    return SUCCESS;
}

int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url) {
    //(void)doc; (void)version; (void)start; (void)end; (void)url;
    const char *buf = "[";
    size_t size = strlen(url) + 4;
    char* buf1 = malloc(size);
    buf1[size-1]= '\0';
    int result1,result2;
    snprintf(buf1, size, "](%s)", url);
    result1 = markdown_insert(doc, version, start, buf);
    result2 = markdown_insert(doc, version, end, buf1);
    if(result1==-1 || result2 == -1){
        return INVALID_CURSOR_POS;
    }else if(result1==-2 || result2 == -2){
        return DELETED_POSITION;
    }else if(result1==-3 || result2 == -3){
        return OUTDATE_VERSION;
    }
    free(buf1);
    return SUCCESS;
}

// === Utilities === 
void markdown_print(const document *doc, FILE *stream) {
    //(void)doc; (void)stream;
    
    char* doctext = markdown_flatten(doc);
    //uint64_t size = strlen(doctext);
    int x = fprintf(stream,"%s",doctext);
    if(x < 0 ){
        printf("fprint failed\n");
    }
    free(doctext);

}


char *markdown_flatten(const document *doc) {

    uint64_t size = markdown_get_size(doc);
    if(size == 0) {
        bfdc = malloc(1);
        bfdc[0] = '\0';
        return bfdc;
    }
    bfdc = malloc((size + 1));
    chunk* current = doc->head;
    size_t offset = 0;
    while(current){
        if(current->chunkversion == doc->version && (current->is_deleted == 0 || current->is_deleted > doc->version)) {
            for(size_t i = 0; i < current->chunksize; i++) {
                bfdc[offset + i] = current->data[i];
            }
            offset += current->chunksize;
        }
        current = current->next;
    }
    bfdc[size] = '\0';
    return bfdc;
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

all_log* log_init(){
    all_log* log = malloc(sizeof(all_log));
    log->size = 0;
    log->head = NULL;
    log->tail = NULL;
    log->last_start = NULL;
    log->last_end = NULL;
    return log;
}

int edit_doc(document* doc, char* data){

    int edit_result;
    char* data_copy = malloc(strlen(data)+1);
    strncpy(data_copy,data,strlen(data));
    data_copy[strlen(data)] ='\0';
    char* commandtype = strtok(data_copy, " ");
    if(strcmp(commandtype, "INSERT") == 0){
        // insert command
        int pos = atoi(strtok(NULL, " "));
        char* content = strtok(NULL, "");
        //content[strcspn(content, "\n")] = 0;
        // call the insert function //////////////////////
        //printf("insert %s at %d\n", content, pos);
        // call insert function
        edit_result = markdown_insert(doc,doc->version,pos, content);

    }else if(strcmp(commandtype, "DEL") == 0){
        // delete command
        int pos = atoi(strtok(NULL, " "));
        int len = atoi(strtok(NULL, " "));
        edit_result = markdown_delete(doc,doc->version,pos, len);
        //error handle
    }else if(strcmp(commandtype, "NEWLINE") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_newline(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "HEADING") == 0){
        int level = atoi(strtok(NULL, " "));
        int pos = atoi(strtok(NULL, " "));
        edit_result= markdown_heading(doc, doc->version,level, pos);
        //error handle
    }else if(strcmp(commandtype, "BOLD") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        edit_result = markdown_bold(doc, doc->version, start_pos,end_pos);
        //error handle
    }else if(strcmp(commandtype, "ITALIC") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        edit_result = markdown_italic(doc, doc->version, start_pos, end_pos);
        //error handle
    }else if(strcmp(commandtype, "BLOCKQUOTE") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_blockquote(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "ORDERED_LIST") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_ordered_list(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "UNORDERED_LIST") == 0){
        int pos = atoi(strtok(NULL, " "));
        edit_result = markdown_unordered_list(doc, doc->version, pos);
        //error handle
    }else if(strcmp(commandtype, "CODE") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        edit_result = markdown_code(doc, doc->version, start_pos,end_pos);
        //error handle
    }else if(strcmp(commandtype, "LINK") == 0){
        int start_pos = atoi(strtok(NULL, " "));
        int end_pos = atoi(strtok(NULL, " "));
        char* link = strtok(NULL, " ");
        edit_result = markdown_link(doc, doc->version, start_pos,end_pos,link);
        //error handle
    }else{
        free(data_copy);
        return -1;
    }
    free(data_copy);
    return edit_result;
}

void log_free(all_log* a_log){
    versionlog* cur = a_log->head;
    versionlog* temp = a_log->head;
    while(cur){
        temp = cur->next;
        free(cur->editlog);
        free(cur);
        cur = temp;
    }
    free(a_log);
}
// int main() {
//     document* doc = markdown_init();

//     // Insert three ordered list items
//     markdown_insert(doc,0,0,"rice noodle dumpling burger");
    
//     markdown_increment_version(doc);
//     char* result = markdown_flatten(doc);
//     printf("Ordered list result:\n%s\n", result);
//     markdown_ordered_list(doc, 1, 0);
//     markdown_horizontal_rule(doc,1,30);
//     markdown_increment_version(doc);
//     printf("------------\n");
//     markdown_ordered_list(doc, 2, 7);
//     markdown_increment_version(doc);

//     markdown_ordered_list(doc, doc->version, 18);
//     //markdown_increment_version(doc);
//     //markdown_ordered_list(doc, doc->version, 26);
//     markdown_increment_version(doc);
//     markdown_ordered_list(doc, doc->version, 30);
//     markdown_increment_version(doc);
//     markdown_delete(doc, doc->version,7,10);
//     markdown_increment_version(doc);
//     result = markdown_flatten(doc);
//     printf("Ordered list result:\n%s\n", result);

//     free(result);
//     markdown_free(doc);
//     return 0;
// }
