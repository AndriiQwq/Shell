#!/bin/bash

# Description of the program
true '
Shell client-server program
'

# gcc -Wall -Wextra -Werror -o program program.c ...

: ' 
Programs must run without any warning or error!!!
You can add -Wextra and -Werror to make the compiler prohibit compilation in the presence of warnings:

make CFLAGS="-Wall -Wextra -Werror
'

# Compile all source files and link them into the executable
make CFLAGS="-Wall -Wextra -Werror"

# Run the program with server mode and port 8072
make run ARGS="-s -p 8072"