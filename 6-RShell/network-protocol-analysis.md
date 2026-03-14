
# Network Protocol Analysis: TCP Remote Shell

## 1. Learning Process

To learn how to analyze network protocol behavior, I used ChatGPT to understand both TCP communication and the tools used to inspect it. I used AI to help me learn how to trace socket system calls, how TCP differs from message-based protocols, and how to verify that my custom EOF marker was being transmitted correctly.

Some of the prompts I used were:

1. "How do I use strace to trace socket, connect, send, and recv calls?"
2. "What is the TCP 3-way handshake?"
3. "Why does TCP not preserve message boundaries?"
4. "How can I verify that my EOF character 0x04 is being sent?"

The AI explained that TCP is stream-based, so my shell protocol needed an application-level delimiter. It also suggested using `strace` to observe the exact `sendto()` and `recvfrom()` calls being made by both the client and the server.

The most challenging part was understanding why my first traces did not show the actual command traffic. I learned that on Linux, socket communication often appears in `strace` as `sendto()` and `recvfrom()` instead of only `send()` and `recv()`. Once I traced those calls as well, I was able to capture the full protocol exchange.

---

## 2. Protocol Design Analysis

### Client → Server

The client sends commands as **null-terminated ASCII strings**.

Example:
```
"echo hello\0"
```

The null terminator ensures the server receives a valid C string.

### Server → Client

The server sends command output followed by an **EOF marker** to signal the end of the message.

Example:
```
"hello\n"
"\x04"
```

The EOF marker is byte `0x04`.

### Why the EOF Marker is Needed

TCP is a **stream protocol**, meaning it does not preserve message boundaries. A single `send()` may be received in multiple `recv()` calls, or multiple sends may arrive together.

Because of this, the client keeps receiving data until it detects the EOF marker `0x04`, which signals the end of the server response.

### Protocol Limitations

This simple protocol has several limitations:

- If command output contains byte `0x04`, it could terminate the message early.
- There is no message length prefix.
- There is no encryption or authentication.
- If the connection drops mid‑message there is no recovery.

---

## 3. Traffic Capture and Analysis

I used **strace** to analyze the client-server communication.

### Commands Used

Server:
```bash
strace -f -s 1000 -e trace=socket,bind,listen,accept,send,recv,sendto,recvfrom -o server_trace.txt ./dsh -s
```

Client:
```bash
printf "echo hello\nexit\n" | strace -f -s 1000 -e trace=socket,connect,send,recv,sendto,recvfrom -o client_trace.txt ./dsh -c
```

### Client Trace

```
2036353 socket(AF_INET, SOCK_STREAM, IPPROTO_IP) = 3
2036353 connect(3, {sa_family=AF_INET, sin_port=htons(1234), sin_addr=inet_addr("127.0.0.1")}, 16) = 0
2036353 sendto(3, "echo hello\0", 11, 0, NULL, 0) = 11
2036353 recvfrom(3, "hello\n", 65536, 0, NULL, NULL) = 6
2036353 recvfrom(3, "\4", 65536, 0, NULL, NULL) = 1
2036353 sendto(3, "exit\0", 5, 0, NULL, 0) = 5
2036353 recvfrom(3, "\4", 65536, 0, NULL, NULL) = 1
2036353 +++ exited with 0 +++
```

### Server Trace

```
2036244 socket(AF_INET, SOCK_STREAM, IPPROTO_IP) = 3
2036244 bind(3, {sa_family=AF_INET, sin_port=htons(1234), sin_addr=inet_addr("0.0.0.0")}, 16) = 0
2036244 listen(3, 20) = 0
2036244 accept(3, NULL, NULL) = 4
2036244 recvfrom(4, "echo hello\0", 65535, 0, NULL, NULL) = 11
2036244 sendto(4, "\4", 1, 0, NULL, 0) = 1
2036244 recvfrom(4, "exit\0", 65535, 0, NULL, NULL) = 5
2036244 sendto(4, "\4", 1, 0, NULL, 0) = 1
```

### Analysis

The trace confirms:

- The client sends the command `"echo hello\0"`
- The server receives the command
- The server sends the command output `"hello\n"`
- The server sends EOF marker `0x04`
- The client detects EOF and stops receiving

This confirms the protocol is functioning correctly.

---

## 4. TCP Connection Verification

Checklist:

- [x] TCP socket created
- [x] Client connects to server
- [x] Server accepts connection
- [x] Commands sent as null‑terminated strings
- [x] Server responses end with EOF marker `0x04`
- [x] Client stops receiving when EOF marker appears

### TCP Connection Establishment

TCP uses a **3‑way handshake**:

1. SYN (client → server)
2. SYN‑ACK (server → client)
3. ACK (client → server)

Although `strace` does not show packet flags directly, the successful `connect()` and `accept()` calls confirm the handshake completed successfully.

### Handling of send() and recv()

The trace shows that the command response was received in **two separate recv calls**:

```
recvfrom(... "hello\n")
recvfrom(... "\4")
```

This demonstrates TCP stream behavior and why an application-level delimiter is necessary.

### Exit Command Behavior

The client sends:

```
sendto(... "exit\0")
```

The server receives the exit command and returns a final EOF marker. After that the client exits normally.

---

## Conclusion

This analysis confirms that the remote shell protocol works correctly over TCP.

The traces show:

- The TCP connection is successfully established.
- Commands are transmitted as null‑terminated strings.
- Server responses are streamed back to the client.
- An EOF marker (`0x04`) properly indicates the end of each response.

The EOF marker solves the TCP message boundary problem for this assignment, though a more robust production protocol would include message length fields and stronger error handling.
