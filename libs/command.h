#ifndef COMMAND_H
#define COMMAND_H

#include "markdown.h"

// general command interface
typedef struct {
    int (*execute)(document* doc, uint64_t version, char* args);
} Command;

//create function
Command* create_insert_command();
Command* create_delete_command();
Command* create_heading_command();
Command* create_bold_command();
Command* create_italic_command();
Command* create_blockquote_command();
Command* create_ordered_list_command();
Command* create_unordered_list_command();
Command* create_code_command();
Command* create_link_command();
Command* create_horizontal_rule_command();

// command distributor（替代 edit_doc）
int dispatch_command(document* doc, char* raw_command);
int edit_doc(document* doc, char* data);

#endif
