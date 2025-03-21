#!/bin/bash

# Description of the program
true '
Shell client-server program
'

: ' 
Programs must run without any warning or error!!!
You can add -Wextra and -Werror to make the compiler prohibit compilation in the presence of warnings:

gcc -Wall -Wextra -Werror -o program program.c
'

# Compile all source files and link them into the executable
gcc -Wall -Wextra -o shell main.c server.c client.c shell.c

# Run the program with server mode and port 33333
./shell -s -p 33333