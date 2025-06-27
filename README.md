A configurable client-server application built in C for Linux. Clients can connect to the server and execute various commands, which are logged in a dedicated log file.
 The system supports environment variable loading, UNIX and TCP sockets, inactivity timeouts, and custom runtime settings.

### How to run, add args if need.:
 `make run ARGS="-s -p 8072"` — run server.
 `make run ARGS="-c -p 8072"` — run client.
- For more information, use the command `help`.
