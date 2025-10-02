README — Markdown Collaborative Editing System
Project Overview
This is a multi-user collaborative Markdown editing system built in C. It allows multiple clients to connect to a central server and issue commands to modify a shared Markdown document. The system supports rich formatting commands such as bold, italic, headings, blockquotes, lists, code blocks, and hyperlinks. It uses the Command and Strategy design patterns for modularity and extensibility.

Build Instructions
Make sure GCC is installed. Then run the following from the project root:

make clean
make

This will generate two executables:

./server — the server program

./client — the client program

how to run

./server <timeinterval>    in millisecond
./client <serverPID> <name>

type command in the terminal
command must follow strict syntax 
server and client should be in different terminal


Command Type	Format Example	Description
Insert Text	INSERT 12 Hello World	Insert text at position 12
Delete Text	DEL 5 10	Delete 10 characters from position 5
Bold	BOLD 5 10	Apply bold from position 5 to 10
Italic	ITALIC 3 8	Apply italic from position 3 to 8
Heading	HEADING 2 0	Insert level-2 heading at position 0
Blockquote	BLOCKQUOTE 0	Insert blockquote at position 0
Ordered List	ORDERED_LIST 0	Insert ordered list at position 0
Unordered List	UNORDERED_LIST 0	Insert unordered list at position 0
Code Block	CODE 5 15	Apply code formatting from 5 to 15
Hyperlink	LINK 5 10	Link text from 5 to 10 to a URL
Horizontal Rule	HORIZONTAL_RULE 20	Insert horizontal rule at position 20


Client Debugging Commands
• DOC?; Prints the entire document to the client’s terminal.
• PERM?; Requests and displays the client’s current role/permissions from the server.
• LOG?; Outputs a full log of all executed commands in order.

Server Debugging Commands
• DOC?; Prints the current document state to the server terminal.
• LOG?; Outputs a full log of all executed commands in order