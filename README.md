## Poll-Based TCP Chat Server
>A high-performance, single-threaded TCP chat server implemented in C++. This project demonstrates I/O Multiplexing using the poll() system call to manage multiple concurrent client connections without the need for multi-threading or complex locking mechanisms.

### Features
>Multiplexed I/O: Efficiently monitors multiple file descriptors simultaneously.
>Dynamic Connection Management: Automatically expands the polling array as new clients join.
>Broadcast Mechanism: Relays messages from any single client to all other connected peers.
>O(1) Deletion: Uses a "swap-and-pop" strategy to remove disconnected clients from the watch list instantly.

### How it Works
>The server operates on an event-driven loop:
>The Watchlist: The server maintains an array of pollfd structures. Initially, it only contains the Listening Socket.
>The Wait: The poll() call blocks the program until activity is detected on any of the monitored sockets.

### Event Handling:
>New Connection: If the listening socket is ready, accept() is called and the new client is added to the array.
>Incoming Data: If a client socket is ready, the server reads the message and loops through the array to send() it to everyone else.
>Disconnection: If a client closes the socket, the server closes its end and cleans up the array.

### Usage Guide
1. Compilation: Compile the source code using g++:
>Bash
g++ -o chat_server main.cpp
./chat_server

2. Testing with Telnet: Open a new terminal and connect to the server:
>Bash
telnet localhost 3490

Anything you type and send will be broadcast to all other connected clients.

3. Testing with Netcat (nc): Netcat is often the preferred tool for terminal-based testing:
>Bash
nc localhost 3490