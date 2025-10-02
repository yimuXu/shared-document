#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libs/command.h"

// === INSERT ===
typedef struct {
    int pos;
    char* content;
} InsertArgs;

InsertArgs* parse_insert_args(char* args) {
    InsertArgs* parsed = malloc(sizeof(InsertArgs));
    parsed->pos = atoi(strtok(args, " "));
    parsed->content = strtok(NULL, "");
    return parsed;
}

int insert_execute(document* doc, uint64_t version, char* args) {
    InsertArgs* parsed = parse_insert_args(args);
    int result = markdown_insert(doc, version, parsed->pos, parsed->content);
    free(parsed);
    return result;
}

Command* create_insert_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = insert_execute;
    return cmd;
}

// === DELETE ===
typedef struct {
    int pos;
    int len;
} DeleteArgs;

DeleteArgs* parse_delete_args(char* args) {
    DeleteArgs* parsed = malloc(sizeof(DeleteArgs));
    parsed->pos = atoi(strtok(args, " "));
    parsed->len = atoi(strtok(NULL, " "));
    return parsed;
}

int delete_execute(document* doc, uint64_t version, char* args) {
    DeleteArgs* parsed = parse_delete_args(args);
    int result = markdown_delete(doc, version, parsed->pos, parsed->len);
    free(parsed);
    return result;
}

Command* create_delete_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = delete_execute;
    return cmd;
}

// === HEADING ===
int heading_execute(document* doc, uint64_t version, char* args) {
    int level = atoi(strtok(args, " "));
    int pos = atoi(strtok(NULL, " "));
    return markdown_heading(doc, version, level, pos);
}

Command* create_heading_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = heading_execute;
    return cmd;
}

// === BOLD ===
int bold_execute(document* doc, uint64_t version, char* args) {
    int start = atoi(strtok(args, " "));
    int end = atoi(strtok(NULL, " "));
    return markdown_bold(doc, version, start, end);
}

Command* create_bold_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = bold_execute;
    return cmd;
}

// === ITALIC ===
int italic_execute(document* doc, uint64_t version, char* args) {
    int start = atoi(strtok(args, " "));
    int end = atoi(strtok(NULL, " "));
    return markdown_italic(doc, version, start, end);
}

Command* create_italic_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = italic_execute;
    return cmd;
}

// === BLOCKQUOTE ===
int blockquote_execute(document* doc, uint64_t version, char* args) {
    int pos = atoi(args);
    return markdown_blockquote(doc, version, pos);
}

Command* create_blockquote_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = blockquote_execute;
    return cmd;
}

// === ORDERED LIST ===
int ordered_list_execute(document* doc, uint64_t version, char* args) {
    int pos = atoi(args);
    return markdown_ordered_list(doc, version, pos);
}

Command* create_ordered_list_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = ordered_list_execute;
    return cmd;
}

// === UNORDERED LIST ===
int unordered_list_execute(document* doc, uint64_t version, char* args) {
    int pos = atoi(args);
    return markdown_unordered_list(doc, version, pos);
}

Command* create_unordered_list_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = unordered_list_execute;
    return cmd;
}

// === CODE ===
int code_execute(document* doc, uint64_t version, char* args) {
    int start = atoi(strtok(args, " "));
    int end = atoi(strtok(NULL, " "));
    return markdown_code(doc, version, start, end);
}

Command* create_code_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = code_execute;
    return cmd;
}

// === LINK ===
int link_execute(document* doc, uint64_t version, char* args) {
    int start = atoi(strtok(args, " "));
    int end = atoi(strtok(NULL, " "));
    char* url = strtok(NULL, "");
    return markdown_link(doc, version, start, end, url);
}

Command* create_link_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = link_execute;
    return cmd;
}

// === HORIZONTAL RULE ===
int hr_execute(document* doc, uint64_t version, char* args) {
    int pos = atoi(args);
    return markdown_horizontal_rule(doc, version, pos);
}

Command* create_horizontal_rule_command() {
    Command* cmd = malloc(sizeof(Command));
    cmd->execute = hr_execute;
    return cmd;
}

// === DISPATCHER ===
typedef struct {
    char* name;
    Command* (*create)();
} CommandEntry;

CommandEntry command_table[] = {
    {"INSERT", create_insert_command},
    {"DEL", create_delete_command},
    {"HEADING", create_heading_command},
    {"BOLD", create_bold_command},
    {"ITALIC", create_italic_command},
    {"BLOCKQUOTE", create_blockquote_command},
    {"ORDERED_LIST", create_ordered_list_command},
    {"UNORDERED_LIST", create_unordered_list_command},
    {"CODE", create_code_command},
    {"LINK", create_link_command},
    {"HORIZONTAL_RULE", create_horizontal_rule_command}
};

#define NUM_COMMANDS (sizeof(command_table) / sizeof(CommandEntry))

int dispatch_command(document* doc, char* raw_command) {
    char* copy = strdup(raw_command);
    char* commandtype = strtok(copy, " ");
    char* args = strtok(NULL, "");

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(commandtype, command_table[i].name) == 0) {
            Command* cmd = command_table[i].create();
            int result = cmd->execute(doc, doc->version, args);
            free(cmd);
            free(copy);
            return result;
        }
    }

    free(copy);
    return -1; // Unknown command
}

int edit_doc(document* doc, char* data){
    int result = dispatch_command(doc,data);
    return result;
}
