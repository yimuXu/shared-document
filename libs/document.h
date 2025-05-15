#ifndef DOCUMENT_H

#define DOCUMENT_H
/**
 * This file is the header file for all the document functions. You will be tested on the functions inside markdown.h
 * You are allowed to and encouraged multiple helper functions and data structures, and make your code as modular as possible. 
 * Ensure you DO NOT change the name of document struct.
 */
//typedef struct chunk chunk;


typedef struct chunk{
    // TODO
    char* data;
    uint64_t chunksize;
    uint64_t chunkversion;// each time you modify the chunk, you need to update the version
    struct chunk* next;
    struct chunk* prev;
    int is_deleted;////1 deleted, 0 not deleted
} chunk;

typedef struct {
    // TODO
    chunk* head;
    uint64_t size;
    uint64_t version;
} document;

// Functions from here onwards.
#endif
