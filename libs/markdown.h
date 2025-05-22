
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
//#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/wait.h>
//#include <bits/sigaction.h>
#ifndef MARKDOWN_H
#define MARKDOWN_H
#include "document.h" 

/**
 * The given file contains all the functions you will be required to complete. You are free to and encouraged to create
 * more helper functions to help assist you when creating the document. For the automated marking you can expect unit tests
 * for the following tests, verifying if the document functionalities are correctly implemented. All the commands are explained 
 * in detail in the assignment spec.
 */

// Return -1 if the cursor position is invalid

// Initialize and free a document
document * markdown_init(void);
void markdown_free(document *doc);

// === Edit Commands ===
int markdown_insert(document *doc, uint64_t version, size_t pos, const char *content);
int markdown_delete(document *doc, uint64_t version, size_t pos, size_t len);

// === Formatting Commands ===
int markdown_newline(document *doc, int version, int pos);
int markdown_heading(document *doc, uint64_t version, int level, size_t pos);
int markdown_bold(document *doc, uint64_t version, size_t start, size_t end);
int markdown_italic(document *doc, uint64_t version, size_t start, size_t end);
int markdown_blockquote(document *doc, uint64_t version, size_t pos);
int markdown_ordered_list(document *doc, uint64_t version, size_t pos);
int markdown_unordered_list(document *doc, uint64_t version, size_t pos);
int markdown_code(document *doc, uint64_t version, size_t start, size_t end);
int markdown_horizontal_rule(document *doc, uint64_t version, size_t pos);
int markdown_link(document *doc, uint64_t version, size_t start, size_t end, const char *url);

// === Utilities ===
void markdown_print(const document *doc, FILE *stream);
char *markdown_flatten(const document *doc);

// === Versioning ===
void markdown_increment_version(document *doc);
all_log* log_init();
int edit_doc(document* doc, char* data_copy);
void log_free(all_log* a_log);
#endif // MARKDOWN_H
