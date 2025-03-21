true '
Shell client-server program
'

: ' 
Programs must run without any warning or error!!!
You can add -Wextra and -Werror to make the compiler prohibit compilation in the presence of warnings:

gcc -Wall -Wextra -Werror -o program program.c
'

gcc -Wall -o shell shell.c
./shell -s -p 33333