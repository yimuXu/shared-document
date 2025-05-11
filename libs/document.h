#ifndef DOCUMENT_H

#define DOCUMENT_H
/**
 * This file is the header file for all the document functions. You will be tested on the functions inside markdown.h
 * You are allowed to and encouraged multiple helper functions and data structures, and make your code as modular as possible. 
 * Ensure you DO NOT change the name of document struct.
 */

typedef struct{
    // TODO
    char* data;
    uint64_t chunksize;
    chunk* next;
    chunk* prev;
} chunk;

typedef struct {
    // TODO
    chunk* head;
    uint64_t size;
} document;

// Functions from here onwards.
#endif
