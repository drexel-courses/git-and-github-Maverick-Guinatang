# strace-pipe-analysis.md
**Course:** CS283 – Drexel Shell (dsh3)  
**Component:** System Call Analysis with `strace`  
**Author:** Maverick Guinatang  
**Date:** 2026-03-02  

---

# 1) Learning Process (2 pts)

To understand how pipes and file descriptors work at the system call level, I used ChatGPT to guide my research and experimentation with `strace`.

### Questions I asked:
1. How do I use `strace` to see `pipe()` and `dup2()` system calls?
2. Why do I need the `-f` flag when tracing pipelines?
3. What does `pipe2([3,4], 0)` mean?
4. Why does a process hang if I forget to close unused pipe ends?

### What I learned:
- `pipe()` (or `pipe2()`) creates **two file descriptors**:
  - read end
  - write end
- File descriptors 0, 1, 2 are reserved:
  - 0 = stdin  
  - 1 = stdout  
  - 2 = stderr  
  So pipes typically start at fd 3.
- `dup2(oldfd, newfd)` redirects input/output by duplicating file descriptors.
- Every process (parent and children) must close unused pipe ends.
- If any process keeps a write end open, the reader never receives EOF and can hang.
- The `-f` flag in `strace` is critical because pipelines create child processes, and without `-f` we would not see their `dup2()` or `execve()` calls.

This process helped me understand how my shell connects processes together at the OS level.

---

# 2) Basic Pipe Analysis (3 pts)

---

## A) Two-Command Pipeline: `ls | cat`

### Command used:
```bash
printf "ls | cat\nexit\n" | \
strace -f -e trace=pipe,pipe2,dup2,close,fork,vfork,clone,execve,wait4,waitpid ./dsh 2>&1 | \
grep -E "pipe2?\(|dup2\(|close\(|fork\(|vfork\(|clone\(|execve\(|wait4\(|waitpid\("
```

### Relevant Output (filtered):
```text
execve("./dsh", ["./dsh"], ...) = 0
pipe2([3, 4], 0)                        = 0
clone(...) = 4140485
clone(...) = 4140486

[pid 4140485] dup2(4, 1 ...)
[pid 4140486] dup2(3, 0 ...)

[pid 4140484] close(3 ...)
[pid 4140484] close(4 ...)

[pid 4140485] close(3 ...)
[pid 4140485] close(4 ...)

[pid 4140486] close(3 ...)
[pid 4140486] close(4 ...)

[pid 4140485] execve("/usr/bin/ls", ["ls"], ...)
[pid 4140486] execve("/usr/bin/cat", ["cat"], ...)

[pid 4140484] wait4(4140485, ...)
[pid 4140484] wait4(4140486, ...)
```

### Analysis

For 2 commands, we expect **N−1 = 1 pipe**.

**Pipe creation:**
```
pipe2([3, 4], 0) = 0
```
- fd 3 = read end  
- fd 4 = write end  

**Children created:**
- PID 4140485 → `ls`
- PID 4140486 → `cat`

**Child 1 (`ls`):**
```
dup2(4, 1)
```
- Redirects stdout to pipe write end.
- Output of `ls` goes into the pipe.

Closes unused fds:
- close(3)
- close(4) (safe after dup2)

**Child 2 (`cat`):**
```
dup2(3, 0)
```
- Redirects stdin from pipe read end.
- `cat` reads from pipe.

Closes unused fds:
- close(4)
- close(3) (safe after dup2)

**Parent:**
- Closes both pipe ends.
- Calls wait4() for both children.

**Data Flow:**
`ls` → pipe → `cat` → terminal

---

## B) Three-Command Pipeline: `ls | grep makefile | wc -l`

### Command used:
```bash
printf "ls | grep makefile | wc -l\nexit\n" | \
strace -f -e trace=pipe,pipe2,dup2,close,fork,vfork,clone,execve,wait4,waitpid ./dsh 2>&1 | \
grep -E "pipe2?\(|dup2\(|close\(|fork\(|vfork\(|clone\(|execve\(|wait4\(|waitpid\("
```

### Relevant Output (filtered):
```text
pipe2([3, 4], 0) = 0
pipe2([5, 6], 0) = 0

clone(...) = 4140559
clone(...) = 4140560
clone(...) = 4140561

[pid 4140559] dup2(4, 1 ...)
[pid 4140560] dup2(3, 0 ...)
[pid 4140560] dup2(6, 1 ...)
[pid 4140561] dup2(5, 0 ...)

wait4(...)
wait4(...)
wait4(...)
```

### Analysis

For 3 commands, we expect **2 pipes**.

**Pipe creation:**
```
pipe2([3, 4], 0)
pipe2([5, 6], 0)
```

- Pipe 1: 3 (read), 4 (write)
- Pipe 2: 5 (read), 6 (write)

**Children:**
- PID 4140559 → `ls`
- PID 4140560 → `grep makefile`
- PID 4140561 → `wc -l`

**`ls`:**
```
dup2(4, 1)
```
Writes into pipe 1.

**`grep` (middle command):**
```
dup2(3, 0)
dup2(6, 1)
```
- Reads from pipe 1.
- Writes into pipe 2.

Middle commands must redirect BOTH stdin and stdout.

**`wc -l`:**
```
dup2(5, 0)
```
Reads from pipe 2.

**Parent:**
Waits for all three children.

**Data Flow:**
`ls` → pipe1 → `grep` → pipe2 → `wc -l` → terminal

This confirms correct N−1 pipe logic and proper middle-stage handling.

---

# 3) File Descriptor Management (3 pts)

### When are pipes created?
Pipes are created **before forking** so children inherit them.

For N commands, N−1 pipes are created.

---

### Why do pipe fds start at 3?
Each process starts with:
- 0 = stdin
- 1 = stdout
- 2 = stderr

Next available fds start at 3.

---

### How does dup2() work?
Example:
```
dup2(4, 1)
```
- Copies fd 4 onto fd 1.
- stdout now writes to the pipe.
- fd 4 can be closed after duplication.

---

### Why must we close unused pipe ends?
If any process keeps a write end open:
- The reader never receives EOF.
- The reading process blocks forever.
- This causes pipeline hangs.

Closing unused ends ensures:
- No file descriptor leaks.
- Proper EOF behavior.
- Clean termination.

---

# 4) Pipeline Verification (2 pts)

✔ pipe() called N−1 times  
✔ Each child uses correct dup2() calls  
✔ Middle commands redirect both stdin and stdout  
✔ Parent closes all pipe fds  
✔ Parent waits for all children  
✔ No hanging behavior observed  

---

# Final Conclusion

Using `strace` made the invisible pipe mechanism visible. I verified:

- Correct pipe creation
- Proper file descriptor redirection
- Correct closing of unused pipe ends
- Proper process synchronization

The `strace` output confirms that my shell correctly implements Unix-style pipelines at the system call level.
