Simple Webserver simulation using sockets.

1. Concurrent Requests handled by forking child process to handle the request after listening for the request. After handling the load to the child, parent process will continue listening in the same port.
2. Two operations GET,PUT implemented. Get serves the content to the client. PUT accepts content from the client & creates a copy in the server.

Run instructions:
gcc webServer.c -o webServer
./webServer
