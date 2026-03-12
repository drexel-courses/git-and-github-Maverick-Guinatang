# Network Protocol Analysis: TCP Remote Shell

**Assignment Component:** Required (10 points)  
**Difficulty:** Advanced Network Understanding  
**Skills:** Network Protocol Analysis, TCP Understanding, Self-Directed Learning

---

## The Challenge

You've built a client-server remote shell that communicates over TCP. But how do you **prove** your protocol works correctly? How do you see the actual TCP packets? How do you verify message boundaries with your EOF marker?

**Your task:** Use network analysis tools to examine your client-server communication. Understand TCP at the packet level, analyze your protocol design, and verify correct implementation.

**Specifically, you need to:**
1. Learn how to analyze network protocols
2. Capture and examine TCP communication between client and server
3. Analyze send/recv system calls
4. Understand message boundaries and EOF marker
5. Document your findings and learning process using AI tools

**The approach:** Use AI tools (ChatGPT, Claude, Gemini, etc.) to research network protocol analysis. This is a required component.

---

## Why This Matters

**In network programming:**
- Protocols are invisible - you can't see packets flowing
- TCP streams have no message boundaries
- Protocol bugs cause mysterious failures
- Analysis tools reveal what's actually happening

**Professional reality:**
- Every network application uses protocols
- Wireshark/tcpdump are industry-standard tools
- Protocol analysis is essential for debugging
- Understanding TCP is critical for systems programming

**For this assignment:**
- Validates your send/recv calls work correctly
- Verifies EOF marker delimits messages
- Shows TCP stream behavior (fragmentation, reassembly)
- Proves client-server communication

---

## Getting Started: Key Questions to Explore

Use AI tools to research and discover answers to these questions:

### Understanding Phase

1. **What is a network protocol?** How do applications communicate over networks?

2. **What is TCP?** How is it different from UDP?

3. **What are message boundaries?** Why doesn't TCP have them?

4. **How do you analyze network traffic?** What tools exist?

### Network Analysis Tools Phase

5. **What is tcpdump?** How do you capture packets with it?

6. **What is Wireshark?** How do you view captured packets?

7. **Can you use strace for network analysis?** What does it show?

8. **How do you filter for specific connections?** (By port, IP, etc.)

### Protocol Analysis Phase

9. **How do you see TCP connection establishment?** (3-way handshake)

10. **What do send() and recv() syscalls look like in strace?**

11. **How can you verify your EOF marker (0x04) is sent?**

12. **What happens if TCP fragments your messages?**

---

## Learning Strategy: Using AI Effectively

### Research Approach

1. **Start with concepts**: "What is TCP? How does it work?"
2. **Get tools**: "How do I use tcpdump to capture packets?"
3. **Analyze**: Capture your traffic, share with AI for help
4. **Understand**: Ask AI to explain packet contents
5. **Verify**: Confirm your protocol works correctly

### When You Get Stuck

- Share tcpdump/Wireshark output with AI
- Ask about specific packet fields you don't understand
- Request help interpreting hex dumps
- Compare successful vs failed connections

### Critical Thinking

**Remember:**
- TCP is connection-oriented (3-way handshake)
- TCP is a stream (no message boundaries)
- Your protocol uses null terminators and EOF markers
- send() and recv() don't guarantee complete messages

---

## What You Need to Deliver

### File: `network-protocol-analysis.md`

Create this file in your assignment directory with the following sections:

### 1. Learning Process (2 points)

Document how you learned network protocol analysis:
- What AI tools did you use?
- What questions did you ask? (Include 3-4 specific prompts)
- What resources did the AI point you to?
- What challenges did you encounter?

**Example:**
```
I used ChatGPT to learn network protocol analysis. I asked:
1. "How do I use tcpdump to capture TCP traffic on port 1234?"
2. "What is a 3-way handshake in TCP?"
3. "How can I see if my EOF character (0x04) is being sent?"

The AI recommended using tcpdump with `-X` flag to see hex dumps
of packet contents. The most challenging part was understanding
TCP sequence numbers and how fragmentation works.
```

### 2. Protocol Design Analysis (3 points)

Analyze and document your remote shell protocol:

#### A. Protocol Specification

Document YOUR protocol (the one you implemented):

**Client → Server:**
- Message format: (null-terminated string? fixed length? etc.)
- Encoding: (ASCII? Binary?)
- Example: `"ls -la\0"`

**Server → Client:**
- Message format: (how do you mark message end?)
- EOF marker: (0x04 character)
- Example: `"file1.txt\nfile2.txt\n\x04"`

**Explain why you use EOF marker:**
- Why is it needed with TCP?
- What would happen without it?

#### B. Message Boundary Problem

Explain the TCP message boundary issue:
- TCP is a stream protocol
- Multiple send() calls can be combined in one recv()
- One send() can be split across multiple recv()
- Your EOF marker solves this - how?

#### C. Protocol Limitations

Identify potential issues with your protocol:
- What if command output contains 0x04?
- What if network connection breaks mid-message?
- How would you improve it for production use?

### 3. Traffic Capture and Analysis (3 points)

Capture and analyze actual network traffic between client and server.

**You can use EITHER approach:**
- **Option A:** tcpdump/Wireshark (packet-level analysis)
- **Option B:** strace (syscall-level analysis)
- **Best:** Use BOTH!

#### Option A: Using tcpdump/Wireshark

**Capture traffic:**
```bash
# Terminal 1: Start capture
sudo tcpdump -i lo -w remote_shell.pcap port 1234

# Terminal 2: Start server
./dsh -s

# Terminal 3: Start client, run commands
./dsh -c
dsh4> echo hello
dsh4> exit

# Terminal 1: Stop capture (Ctrl+C)
```

**Analyze with Wireshark:**
```bash
wireshark remote_shell.pcap
```

**Or view with tcpdump:**
```bash
tcpdump -r remote_shell.pcap -X
```

**Provide:**
- Screenshot or text of packet capture
- Identify TCP 3-way handshake packets
- Find packets containing your commands
- Locate EOF marker (0x04) in hex dump
- Explain sequence and acknowledgment numbers

#### Option B: Using strace

**Trace client:**
```bash
strace -e trace=socket,connect,send,recv -o client_trace.txt ./dsh -c
# Run some commands, then exit
```

**Trace server:**
```bash
strace -e trace=socket,bind,listen,accept,send,recv -o server_trace.txt ./dsh -s
# Wait for client to connect and run commands
```

**Provide:**
- Relevant strace output from both sides
- Identify socket(), connect(), accept() calls
- Show send() calls with command data
- Show recv() calls with response data
- Verify EOF character is sent

#### What to Analyze:

**For command: "echo hello"**

1. **Client sends:**
   - Identify send() call or TCP packet
   - Show hex dump of "echo hello\0"

2. **Server receives:**
   - Identify recv() call or TCP packet
   - Verify it received "echo hello\0"

3. **Server sends response:**
   - Multiple send() calls or packets
   - Response data: "hello\n"
   - EOF marker: 0x04

4. **Client receives:**
   - May take multiple recv() calls
   - Verify received all data
   - Verify EOF marker terminates receive loop

### 4. TCP Connection Verification (2 points)

Verify the TCP connection works correctly:

**Checklist:**
- [ ] TCP 3-way handshake occurs (SYN, SYN-ACK, ACK)
- [ ] Client connects to server successfully
- [ ] Commands are sent correctly (null-terminated)
- [ ] Server responses include EOF marker (0x04)
- [ ] Connection closes gracefully (FIN)

**Questions to answer:**
1. How many TCP packets for connection establishment?
2. How does TCP handle your send() calls? (One packet per send? Combined?)
3. Can you see the EOF character in packet/syscall dumps?
4. What happens on "exit" command? (Connection teardown)

If you found issues, describe what was wrong and how you fixed it.

---

## Technical Requirements

### Using tcpdump

**Capture on loopback (local testing):**
```bash
sudo tcpdump -i lo -X -s0 port 1234
```

**Capture to file:**
```bash
sudo tcpdump -i lo -w capture.pcap port 1234
```

**Read from file:**
```bash
tcpdump -r capture.pcap -X
```

**Filter by IP and port:**
```bash
sudo tcpdump -i lo 'tcp port 1234' -X
```

### Using Wireshark

**Start capture:**
1. Select loopback interface (lo)
2. Filter: `tcp.port == 1234`
3. Start capture
4. Run your client/server
5. Stop capture

**Analyze:**
- Right-click packet → Follow → TCP Stream
- View as: ASCII or Hex Dump
- Look for your commands and responses

### Using strace

**Trace network syscalls:**
```bash
strace -e trace=socket,connect,bind,listen,accept,send,recv ./dsh -c
```

**Save to file:**
```bash
strace -e trace=network -o trace.txt ./dsh -s
```

**Trace with string output:**
```bash
strace -s 1000 -e trace=send,recv ./dsh -c
```

---

## Grading Rubric

**10 points total:**

**Learning Process (2 points)**
- 2 pts: Clear documentation of AI-assisted learning with specific examples
- 1 pt: Vague description of learning process
- 0 pts: No evidence of learning process

**Protocol Design Analysis (3 points)**
- 3 pts: Thorough protocol documentation with message boundary explanation
- 2 pts: Good documentation, minor gaps
- 1 pt: Basic documentation, significant gaps
- 0 pts: No meaningful analysis

**Traffic Capture and Analysis (3 points)**
- 3 pts: Clear capture showing commands, responses, EOF marker
- 2 pts: Capture present but incomplete analysis
- 1 pt: Minimal capture or analysis
- 0 pts: No capture or analysis

**TCP Connection Verification (2 points)**
- 2 pts: Thorough verification with checklist completed
- 1 pt: Basic verification, incomplete
- 0 pts: No verification

---

## Hints for Success

### Running tcpdump

**Permission needed:**
```bash
# Option 1: Run as root
sudo tcpdump -i lo port 1234

# Option 2: Give tcpdump capabilities (one-time setup)
sudo setcap cap_net_raw,cap_net_admin=eip /usr/bin/tcpdump
tcpdump -i lo port 1234
```

**Reading hex dumps:**
```
0x0000:  4500 003c 1234 4000 4006 2345 7f00 0001  E..<.4@.@.#E....
0x0010:  7f00 0001 04d2 9876 1234 5678 9abc def0  .......v.4Vx....
0x0020:  8018 0156 fe30 0000 0101 080a 0012 3456  ...V.0........4V
0x0030:  0012 3456 6563 686f 2068 656c 6c6f 0a    ..4Vecho.hello.
                                    ^^^^^^^^^^^^
                                    "echo hello\n"
```

### Finding EOF Character

**In hex dump:**
- EOF = 0x04
- Look for `04` in hex output
- Usually at end of server response

**Example:**
```
0x0040:  6865 6c6c 6f0a 04              hello..
                      ^^
                      EOF (0x04)
```

### Verifying with strace

**Look for send() calls:**
```
[pid 1001] send(3, "echo hello\0", 11, 0) = 11
```
- Socket fd = 3
- Sent 11 bytes
- Includes null terminator

**Look for recv() calls:**
```
[pid 1001] recv(3, "hello\n\4", 1024, 0) = 7
```
- Received 7 bytes
- Includes EOF (0x04 or \4)

---

## Example: What Good Analysis Looks Like

Here's what a strong protocol analysis might include:

### Protocol Design

**My Remote Shell Protocol:**

**Client → Server:**
- Format: Null-terminated ASCII string
- Example: "ls -la\0" (7 bytes including \0)
- Encoding: ASCII for compatibility
- One command per send() call

**Server → Client:**
- Format: Variable-length ASCII stream
- Delimiter: EOF character (0x04) at end
- Example: "file1.txt\nfile2.txt\n\x04"
- May require multiple recv() calls

**Why EOF Marker:**
TCP is a stream protocol with no message boundaries. If I send():
```c
send(sock, "hello\n", 6, 0);
send(sock, "\x04", 1, 0);
```

The client might recv():
- Both in one call: "hello\n\x04" (7 bytes)
- Split: "hello" (5 bytes), then "\n\x04" (2 bytes)

The EOF marker (0x04) tells the client: "This is the last byte of this message, stop receiving."

**Limitations:**
1. If command output contains 0x04, it would break protocol
2. No length prefix, so client doesn't know how much data to expect
3. No error checking or checksums

### Traffic Capture (using tcpdump)

**Captured command: "echo hello"**

**Client sends (TCP packet):**
```
0x0000:  4500 0033 1234 4000 4006 0000 7f00 0001  E..3.4@.@.......
0x0010:  7f00 0001 c3a4 04d2 0000 0001 0000 0002  ................
0x0020:  8018 0156 0000 0000 0101 080a 0000 0001  ...V............
0x0030:  6563 686f 2068 656c 6c6f 00              echo.hello.
          ^^^^^^^^^^^^^^^^^^^^^^^
          "echo hello\0" (11 bytes)
```

**Server responds (TCP packet 1 - data):**
```
0x0000:  4500 002f 1234 4000 4006 0000 7f00 0001  E../.4@.@.......
0x0010:  7f00 0001 04d2 c3a4 0000 0002 0000 000d  ................
0x0020:  8018 0156 0000 0000 0101 080a 0000 0002  ...V............
0x0030:  6865 6c6c 6f0a                          hello.
          ^^^^^^^^^^
          "hello\n" (6 bytes)
```

**Server responds (TCP packet 2 - EOF):**
```
0x0000:  4500 0029 1234 4000 4006 0000 7f00 0001  E..).4@.@.......
0x0010:  7f00 0001 04d2 c3a4 0000 0008 0000 000d  ................
0x0020:  8018 0156 0000 0000 0101 080a 0000 0003  ...V............
0x0030:  04                                      .
          ^^
          EOF (0x04)
```

**Analysis:**
- Client sent command with null terminator (\0)
- Server sent response in TWO packets
- First packet: actual output "hello\n"
- Second packet: EOF marker (0x04)
- Client's recv() loop would get both, see EOF, and stop

### Verification

✓ TCP connection established (saw 3-way handshake: SYN, SYN-ACK, ACK)
✓ Client sends null-terminated command
✓ Server sends response data
✓ Server sends EOF marker (0x04) at end
✓ Client can detect EOF and stop receiving
✓ Connection closes gracefully with FIN packets

---

## Resources

- `man tcpdump` - tcpdump documentation
- `man 7 tcp` - TCP protocol manual
- `man 2 socket` - socket system calls
- Wireshark User Guide
- Your AI tool of choice (ChatGPT, Claude, Gemini, etc.)

---

## Final Thought

Network protocols are invisible - you can't see the packets flying between client and server. Tools like tcpdump, Wireshark, and strace make them visible, showing you exactly what's happening on the wire and in system calls.

The goal isn't just to capture some packets - it's to **understand how your protocol works at the deepest level**. When you see your commands and responses in hex dumps, you understand TCP, socket programming, and network communication in a way that reading documentation never achieves.

This knowledge applies to **every networked application** - web browsers, SSH, databases, games - they all use TCP/UDP with custom protocols. You're learning the fundamental skill of network programming!

**Good luck with your analysis!**
