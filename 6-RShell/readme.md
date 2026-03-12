# Assignment: Custom Shell Part 4 - Remote Shell

This week we will add **network capability** to our `dsh` Drexel Shell, allowing you to run a shell server that accepts commands from remote clients over TCP!

## What is a Remote Shell?

A remote shell allows you to execute commands on a remote machine over a network. This is fundamental to systems administration - tools like `ssh`, `telnet`, and remote desktop all use similar client-server architectures.

In this assignment, you'll build:
- **Server mode:** Accepts client connections, executes commands, sends back results
- **Client mode:** Connects to server, sends commands, displays results
- **Local mode:** Works like Parts 1-3 (for testing)

## Network Architecture

```
┌─────────────────┐                  ┌─────────────────┐
│     Client      │                  │     Server      │
│                 │                  │                 │
│  ./dsh -c       │──── TCP/IP ─────▶│  ./dsh -s       │
│                 │                  │                 │
│  User types:    │                  │  Receives:      │
│  > ls           │──send("ls\0")──▶│  "ls\0"         │
│                 │                  │  Executes: ls   │
│  Displays:      │◀───send(data)────│  Sends output:  │
│  file1.txt      │                  │  "file1.txt...  │
│  file2.txt      │◀───send(EOF)─────│  ...file2.txt"  │
│                 │                  │  EOF (0x04)     │
└─────────────────┘                  └─────────────────┘
```

**Key concepts:**
- Client connects using IP address and port
- Commands sent as null-terminated strings
- Results sent back as stream with EOF marker (0x04)
- Server can handle multiple clients sequentially

---

## TCP Socket Programming

### Server-Side Flow

```c
// 1. Create socket
int svr_sock = socket(AF_INET, SOCK_STREAM, 0);

// 2. Bind to address/port
struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(port),
    .sin_addr.s_addr = INADDR_ANY
};
bind(svr_sock, (struct sockaddr*)&addr, sizeof(addr));

// 3. Listen for connections
listen(svr_sock, 20);

// 4. Accept client connection
int cli_sock = accept(svr_sock, NULL, NULL);

// 5. Communicate with client
recv(cli_sock, buffer, size, 0);  // Receive command
send(cli_sock, result, len, 0);   // Send result

// 6. Close client socket
close(cli_sock);
```

### Client-Side Flow

```c
// 1. Create socket
int cli_sock = socket(AF_INET, SOCK_STREAM, 0);

// 2. Connect to server
struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(port),
    .sin_addr.s_addr = inet_addr(server_ip)
};
connect(cli_sock, (struct sockaddr*)&addr, sizeof(addr));

// 3. Send command
send(cli_sock, "ls\0", 3, 0);

// 4. Receive results (loop until EOF)
while (recv(cli_sock, buffer, size, 0) > 0) {
    // Check for EOF marker (0x04)
    if (buffer[bytes-1] == 0x04) break;
}

// 5. Close socket
close(cli_sock);
```

---

## Protocol Design

### Message Format

**Client → Server:**
- Null-terminated C strings
- Example: `"ls -la\0"`
- Each command is one send()

**Server → Client:**
- Stream of data (no message boundaries in TCP!)
- Ends with EOF character (0x04)
- May take multiple recv() calls to get all data

**Example protocol exchange:**
```
Client: send("echo hello\0")
Server: recv() → "echo hello\0"
Server: executes echo hello
Server: send("hello\n")
Server: send("\x04")          ← EOF marker
Client: recv() → "hello\n\x04"
Client: sees EOF, stops receiving
```

### Why EOF Marker?

TCP is a **stream** protocol - it has no concept of message boundaries. If the server sends:
```c
send(sock, "hello\n", 6, 0);
```

The client might receive it as:
- One recv: `"hello\n"` ✓
- Two recvs: `"hel"` then `"lo\n"` 
- Combined with next: `"hello\nworld\n"`

We use `0x04` (ASCII EOF) as a delimiter to mark message end!

---

## Redirecting I/O to Socket

The server redirects command output to the socket using `dup2()`:

```c
// In child process after fork:
dup2(cli_socket, STDIN_FILENO);   // stdin from socket
dup2(cli_socket, STDOUT_FILENO);  // stdout to socket
dup2(cli_socket, STDERR_FILENO);  // stderr to socket
execvp("ls", argv);               // ls writes to socket!
```

**For pipelines:**
- First command: stdin from socket
- Last command: stdout/stderr to socket
- Middle commands: pipes like Part 3

```
Socket ──→ cmd1 ─pipe─→ cmd2 ─pipe─→ cmd3 ──→ Socket
```

---

## Assignment Details

### Step 1 - Review [./rshlib.h](./rshlib.h)

Key constants and definitions:
- `RDSH_DEF_PORT` (1234) - default port
- `RDSH_COMM_BUFF_SZ` (64KB) - buffer size
- `RDSH_EOF_CHAR` (0x04) - message delimiter
- Error codes: `ERR_RDSH_*`
- Function prototypes

**Prompt changed to "dsh4>"** to indicate Part 4.

---

### Step 2 - Implement Client Functions in [./rsh_cli.c](./rsh_cli.c)

#### A. `start_client(server_ip, port)`

Creates client socket and connects to server.

**Algorithm:**
```c
1. Create socket: socket(AF_INET, SOCK_STREAM, 0)
2. Setup address structure with server_ip and port
3. Connect: connect(cli_sock, &addr, sizeof(addr))
4. Return cli_sock on success
```

**System calls:**
- `socket()` - create socket
- `connect()` - connect to server

#### B. `exec_remote_cmd_loop(server_ip, port)`

Main client loop - sends commands and receives results.

**Algorithm:**
```c
1. Allocate buffers (malloc)
2. Call start_client() to get cli_sock
3. While loop:
   a. Print prompt
   b. Read command with fgets()
   c. Send command: send(cli_sock, cmd, len, 0)
   d. Receive results in loop:
      - recv(cli_sock, buffer, size, 0)
      - Print received data
      - Check last byte for EOF (0x04)
      - If EOF, break recv loop
   e. Check for "exit" or "stop-server"
4. Close socket and free buffers
```

**Key points:**
- Send entire command including null terminator
- Loop on recv() until you see EOF marker
- Print using: `printf("%.*s", bytes_received, buffer)`
- Exit on "exit" command, stop server on "stop-server"

---

### Step 3 - Implement Server Functions in [./rsh_server.c](./rsh_server.c)

#### A. `boot_server(ifaces, port)`

Creates, binds, and starts listening on server socket.

**Algorithm:**
```c
1. Create socket: socket(AF_INET, SOCK_STREAM, 0)
2. Set SO_REUSEADDR option (helps with development)
3. Setup address structure with ifaces and port
4. Bind: bind(svr_sock, &addr, sizeof(addr))
5. Listen: listen(svr_sock, 20)
6. Return svr_sock
```

**Tip:** Use `setsockopt()` with `SO_REUSEADDR` before bind!

#### B. `process_cli_requests(svr_socket)`

Accept clients in loop, handle each connection.

**Algorithm:**
```c
while (1) {
    1. Accept connection: cli_sock = accept(svr_sock, NULL, NULL)
    2. Call exec_client_requests(cli_sock)
    3. If return code is negative (stop-server), break
    4. Otherwise loop to accept next client
}
```

#### C. `exec_client_requests(cli_socket)`

Receive commands from client, execute, send results back.

**Algorithm:**
```c
while (1) {
    1. Allocate command buffer
    2. Receive command: recv(cli_sock, cmd_buf, size, 0)
    3. If recv returns <= 0, client disconnected, break
    4. Parse command into command_list_t
    5. Check for built-ins:
       - "exit" → send EOF, close client, return OK
       - "stop-server" → send EOF, return OK_EXIT (negative)
       - "cd" → execute chdir(), send message
    6. If not built-in:
       - Call rsh_execute_pipeline(cli_sock, &clist)
       - Send EOF marker
    7. Free buffers and loop
}
```

#### D. `send_message_string(cli_socket, message)`

Send a string message followed by EOF.

**Algorithm:**
```c
1. Send message: send(cli_sock, message, strlen(message), 0)
2. Send EOF: send_message_eof(cli_sock)
```

#### E. `send_message_eof(cli_socket)`

Send EOF marker to signal end of transmission.

**Algorithm:**
```c
1. char eof = RDSH_EOF_CHAR;  // 0x04
2. send(cli_sock, &eof, 1, 0);
```

#### F. `rsh_execute_pipeline(cli_socket, command_list)`

Execute commands with socket as stdin/stdout/stderr.

**This is like Part 3 execute_pipeline(), but:**
- First command: `dup2(cli_socket, STDIN_FILENO)` for stdin
- Last command: `dup2(cli_socket, STDOUT_FILENO)` for stdout/stderr
- Last command: `dup2(cli_socket, STDERR_FILENO)` for stderr too

**Algorithm:**
```c
1. Create N-1 pipes for N commands
2. Fork each command:
   a. If first command (i==0):
      - dup2(cli_socket, STDIN_FILENO)
   b. If not first command:
      - dup2(pipe[i-1][0], STDIN_FILENO)
   c. If not last command:
      - dup2(pipe[i][1], STDOUT_FILENO)
   d. If last command (i==N-1):
      - dup2(cli_socket, STDOUT_FILENO)
      - dup2(cli_socket, STDERR_FILENO)
   e. Close all pipes
   f. execvp()
3. Parent closes all pipes
4. Wait for all children
5. Return exit code of last command
```

---

### Step 4 - Network Protocol Analysis

**Points: 10 (REQUIRED)**

Once your remote shell works, analyze the network protocol using tools to understand TCP communication at a deep level.

**What You'll Do:**
1. Use AI tools to learn network protocol analysis
2. Analyze your client-server communication
3. Understand TCP streams, send/recv, and protocol design
4. Document your findings and learning process

**Why This Matters:**
- **Understand network protocols**: See how applications communicate
- **TCP stream behavior**: No message boundaries
- **Professional skill**: Essential for network programming
- **Validates implementation**: Verify your protocol works correctly

**Deliverable:**
Create a file `network-protocol-analysis.md` following the detailed instructions in [network-protocol-analysis.md](network-protocol-analysis.md).

**What You'll Analyze:**
- TCP connection establishment (3-way handshake)
- send() and recv() system calls
- Message fragmentation and reassembly
- EOF marker for message delimiting
- Protocol correctness

See [network-protocol-analysis.md](network-protocol-analysis.md) for complete instructions.

---

### Sample Run

**Start server:**
```bash
Terminal 1:
$ ./dsh -s
socket server mode:  addr:0.0.0.0:1234
-> Single-Threaded Mode
Server listening on port 1234...
```

**Start client:**
```bash
Terminal 2:
$ ./dsh -c
socket client mode:  addr:127.0.0.1:1234
Connected to server
dsh4> ls
makefile
dsh
dsh_cli.c
dshlib.c
dshlib.h
rsh_cli.c
rsh_server.c
rshlib.h
dsh4> echo "hello world"
hello world
dsh4> ls | grep makefile
makefile
dsh4> pwd
/home/student/04-ShellP4
dsh4> exit
exiting...
```

**Server sees:**
```
Client connected from 127.0.0.1
rdsh-exec: ls
rdsh-exec: echo "hello world"
rdsh-exec: ls | grep makefile
rdsh-exec: pwd
Client exited: getting next connection...
```

---

## Extra Credit: +10

Implement multi-threaded server to handle multiple clients simultaneously.

**Requirements:**

1. **Use -x flag** to enable threaded mode:
   ```bash
   ./dsh -s -x    # Multi-threaded server
   ```

2. **Create thread per client:**
   - When accept() returns, create new thread
   - Thread handles that client's exec_client_requests()
   - Main thread loops back to accept() immediately

3. **Thread implementation:**
   ```c
   pthread_t thread;
   pthread_create(&thread, NULL, handle_client, (void*)&cli_socket);
   pthread_detach(thread);  // Auto-cleanup when done
   ```

4. **Must support concurrent clients:**
   - Client 1 can send commands while Client 2 connects
   - Multiple clients can execute commands simultaneously
   - Each thread is isolated

**Why this matters:**
Production servers must handle multiple clients. Without threading, clients wait in queue!

---

## Grading Rubric

This assignment will be weighted **75 points**.

- **60 points**: Correct implementation of required functionality
  - Client implementation (15 points)
  - Server implementation (25 points)
  - Pipeline with socket I/O (15 points)
  - Built-in commands (5 points)
- **5 points**: Code quality (readable, well-commented, good design)
- **10 points**: Network protocol analysis (`network-protocol-analysis.md`)
- **10 points**: [EXTRA CREDIT] Multi-threaded server

**Total points achievable: 85/75**

---

## Submission Requirements

All files for this assignment should be placed in the `6-RShell` directory in your GitHub Classroom repository.

**Required Files:**
1. `rsh_cli.c` - Client implementation
2. `rsh_server.c` - Server implementation
3. `dshlib.c` - Reuse from Part 3
4. `dshlib.h` - Updated header
5. `rshlib.h` - Network header
6. `dsh_cli.c` - Provided
7. `network-protocol-analysis.md` - Your analysis
8. All provided files (`makefile`, `test_dsh4.py`, etc.)

**Submission Process:**

1. Ensure all files are in `6-RShell/` directory
2. Test compilation: `make clean && make`
3. Test functionality: Start server, connect client, run commands
4. Commit and push:
   ```bash
   git add 04-ShellP4/
   git commit -m "Complete shell part 4"
   git push origin main
   ```
5. Submit repository URL on Canvas

---

## Testing

Test your implementation manually:

**Terminal 1 (Server):**
```bash
make
./dsh -s
```

**Terminal 2 (Client):**
```bash
./dsh -c
dsh4> ls
dsh4> echo hello
dsh4> ls | grep makefile
dsh4> exit
```

**Test multiple clients:** Start multiple client terminals!

**Test network:** Use different machines (if available)

**Use the pytest files provided for automated testing**

---

## Important Concepts

### TCP vs UDP

**TCP (what we use):**
- Connection-oriented (establish connection first)
- Reliable (guaranteed delivery, in order)
- Stream-based (no message boundaries)
- Example: HTTP, SSH, FTP

**UDP:**
- Connectionless (just send packets)
- Unreliable (packets may be lost or reordered)
- Message-based (datagram boundaries preserved)
- Example: DNS, video streaming

### Socket System Calls

**Server:**
- `socket()` - create socket
- `bind()` - bind to address/port
- `listen()` - mark as passive (accept connections)
- `accept()` - accept incoming connection (blocks)
- `recv()` - receive data
- `send()` - send data
- `close()` - close socket

**Client:**
- `socket()` - create socket
- `connect()` - connect to server (blocks)
- `send()` - send data
- `recv()` - receive data
- `close()` - close socket

### Why dup2() for Sockets?

Commands expect stdin (fd 0), stdout (fd 1), stderr (fd 2).
Socket is a different file descriptor (e.g., fd 5).

```c
dup2(socket_fd, STDOUT_FILENO);  // Now fd 1 points to socket
execvp("ls", ...);                // ls writes to fd 1 → socket!
```

---

## Tips for Success

### Start Simple

1. **Client first:** Get start_client() and basic send/recv working
2. **Server basics:** Get boot_server() and accept working
3. **Simple command:** Just `echo hello` end-to-end
4. **Then complexity:** Add pipelines, built-ins

### Debug Network Code

**Use two terminals side-by-side:**
- Left: Server with debug prints
- Right: Client sending commands
- Watch both!

**Debug prints:**
```c
printf("[SERVER] Received command: %s\n", cmd);
printf("[SERVER] Sending %ld bytes\n", bytes_sent);
printf("[CLIENT] Sent command: %s\n", cmd);
printf("[CLIENT] Received %ld bytes\n", bytes_received);
```

### Common Mistakes

**1. Not sending null terminator:**
```c
// WRONG
send(sock, cmd, strlen(cmd), 0);

// RIGHT
send(sock, cmd, strlen(cmd)+1, 0);  // Include \0
```

**2. Not checking for EOF:**
```c
// Keep receiving until EOF marker
while (1) {
    bytes = recv(sock, buf, size, 0);
    if (bytes <= 0) break;
    if (buf[bytes-1] == RDSH_EOF_CHAR) break;  // Got EOF!
}
```

**3. Forgetting to redirect stderr:**
```c
// Last command needs both!
dup2(cli_socket, STDOUT_FILENO);
dup2(cli_socket, STDERR_FILENO);  // Don't forget this!
```

**4. Not closing socket in child:**
```c
// After dup2, child should close original socket fd
dup2(cli_socket, STDOUT_FILENO);
close(cli_socket);  // Still have stdout dup!
```

---

## Testing Checklist

- [ ] Server starts and listens
- [ ] Client connects to server
- [ ] Simple command works (echo hello)
- [ ] Command with output works (ls)
- [ ] Pipes work (ls | grep txt)
- [ ] cd command works
- [ ] exit command closes client
- [ ] stop-server command stops server
- [ ] Multiple sequential clients work
- [ ] EOF marker sent correctly
- [ ] No hangs or deadlocks

---

Good luck! Network programming is a critical skill for systems programmers. Once you complete this, you'll understand how **every networked application works** - from web servers to SSH to multiplayer games!
