#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "markdown.h"

void run_tests() {
    document *doc;
    int result;

    // Test 1: Deleting from an empty document
    doc = NULL;
    result = markdown_delete(doc, 0, 0, 10);
    printf("Test 1: %s\n", result == SUCCESS ? "PASS" : "FAIL");

    // Test 2: Deleting the entire document
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 0, 0, doc->size);
    printf("Test 2: %s\n", (result == SUCCESS && doc->head == NULL && doc->size == 0) ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 3: Deleting within a single chunk
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 0, 7, 5);
    printf("Test 3: %s\n", (result == SUCCESS && strcmp(doc->head->data, "Hello, !") == 0) ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 4: Deleting across multiple chunks
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, ");
    markdown_insert(doc, 0, 7, "world");
    markdown_insert(doc, 0, 12, "!");
    result = markdown_delete(doc, 0, 5, 7);
    printf("Test 4: %s\n", (result == SUCCESS && strcmp(doc->head->data, "Hell!") == 0) ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 5: Deleting the first part of the document
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 0, 0, 5);
    printf("Test 5: %s\n", (result == SUCCESS && strcmp(doc->head->data, ", world!") == 0) ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 6: Deleting the last part of the document
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 0, 7, 6);
    printf("Test 6: %s\n", (result == SUCCESS && strcmp(doc->head->data, "Hello, ") == 0) ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 7: Deleting with len == 0
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 0, 5, 0);
    printf("Test 7: %s\n", (result == SUCCESS && strcmp(doc->head->data, "Hello, world!") == 0) ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 8: Deleting with pos > doc->size
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 0, 20, 5);
    printf("Test 8: %s\n", result == INVALID_CURSOR_POS ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 9: Deleting with pos + len > doc->size
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 0, 5, 20);
    printf("Test 9: %s\n", (result == SUCCESS && strcmp(doc->head->data, "Hello") == 0) ? "PASS" : "FAIL");
    markdown_free(doc);

    // Test 10: Deleting with version mismatch
    doc = markdown_init();
    markdown_insert(doc, 0, 0, "Hello, world!");
    result = markdown_delete(doc, 1, 0, 5); // doc->version is 0
    printf("Test 10: %s\n", result == OUTDATE_VERSION ? "PASS" : "FAIL");
    markdown_free(doc);
}

int main() {
    run_tests();
    return 0;
}