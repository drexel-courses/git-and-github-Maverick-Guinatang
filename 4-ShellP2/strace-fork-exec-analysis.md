# strace Fork/Exec Analysis

## 1. Learning Process

I used ChatGPT to learn how to use `strace` to trace fork/exec behavior in my shell implementation.

Questions I asked:

- How do I use strace to trace fork and exec system calls?
- Why do I need the -f flag when tracing a shell?
- Why does strace show execve instead of execvp?
- How can I tell which process is the parent and which is the child?

What I learned:

- `strace` shows system calls made by a program to the Linux kernel.
- `gdb` debugs program logic and variables, but `strace` shows actual OS-level behavior.
- The `-f` flag is required to follow child processes created by `fork()`.
- Although my code calls `execvp()`, `strace` shows `execve()` because `execvp()` is a wrapper that searches PATH and then calls `execve()`.
- `waitpid()` appears as `wait4()` in Linux `strace` output.

At first I did not see the child process behavior clearly, but once I used `-f`, I could see both the parent and child activity.

---

## 2. Basic Fork/Exec Analysis

### A. Executing a Simple Command (`ls`)

Command used:

```bash
strace -f -e trace=fork,execve,wait4 -o trace_ls.txt ./dsh
```

Inside my shell:

```text
dsh2> ls
dsh2> exit
```

Relevant output:

```text
2802289 execve("./dsh", ["./dsh"], 0x7ffff26cd758 /* 26 vars */) = 0
2802289 wait4(2802292,  <unfinished ...>
2802292 execve("/home/gbm26/courses/software/bin/ls", ["ls"], 0x7ffd879f31d8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802292 execve("/usr/local/sbin/ls", ["ls"], 0x7ffd879f31d8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802292 execve("/usr/local/bin/ls", ["ls"], 0x7ffd879f31d8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802292 execve("/usr/sbin/ls", ["ls"], 0x7ffd879f31d8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802292 execve("/usr/bin/ls", ["ls"], 0x7ffd879f31d8 /* 26 vars */) = 0
2802292 +++ exited with 0 +++
2802289 <... wait4 resumed>[{WIFEXITED(s) && WEXITSTATUS(s) == 0}], 0, NULL) = 2802292
```

Analysis:

- The shell process (PID 2802289) starts by executing `./dsh`.
- The child process that runs `ls` is PID 2802292.
- The child attempts multiple `execve()` calls while searching through PATH.
- Each failed attempt returns `ENOENT` (file not found).
- The successful call is `execve("/usr/bin/ls", ["ls"], ...) = 0`.
- The parent process calls `wait4(2802292, ...)` and resumes after the child exits.
- `WEXITSTATUS(s) == 0` confirms that `ls` exited successfully.

This verifies that the parent waits for the child and that the child replaces itself with the `ls` program.

---

### B. Command Not Found (`notacommand`)

Command used:

```bash
strace -f -e trace=fork,execve,wait4 -o trace_notfound.txt ./dsh
```

Inside my shell:

```text
dsh2> notacommand
dsh2> exit
```

Relevant output:

```text
2802300 execve("./dsh", ["./dsh"], 0x7ffc79779fb8 /* 26 vars */) = 0
2802300 wait4(2802307,  <unfinished ...>
2802307 execve("/home/gbm26/courses/software/bin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/usr/local/sbin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/usr/local/bin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/usr/sbin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/usr/bin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/sbin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/bin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/usr/games/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/usr/local/games/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 execve("/snap/bin/notacommand", ["notacommand"], 0x7ffc2339f308 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802307 +++ exited with 1 +++
2802300 <... wait4 resumed>[{WIFEXITED(s) && WEXITSTATUS(s) == 1}], 0, NULL) = 2802307
```

Analysis:

- Parent PID is 2802300.
- Child PID is 2802307.
- The child attempts to execute `notacommand` in every PATH directory.
- Every attempt fails with `ENOENT`.
- Since no executable is found, the child exits with status 1.
- The parent collects this exit status using `wait4()`.

This confirms correct error handling when a command does not exist.

---

### C. Command with Arguments (`echo "hello world"`)

Command used:

```bash
strace -f -e trace=fork,execve,wait4 -o trace_echo.txt ./dsh
```

Inside my shell:

```text
dsh2> echo "hello world"
dsh2> exit
```

Relevant output:

```text
2802548 execve("./dsh", ["./dsh"], 0x7ffc02b50708 /* 26 vars */) = 0
2802548 wait4(2802553,  <unfinished ...>
2802553 execve("/home/gbm26/courses/software/bin/echo", ["echo", "hello world"], 0x7ffefd4c47a8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802553 execve("/usr/local/sbin/echo", ["echo", "hello world"], 0x7ffefd4c47a8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802553 execve("/usr/local/bin/echo", ["echo", "hello world"], 0x7ffefd4c47a8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802553 execve("/usr/sbin/echo", ["echo", "hello world"], 0x7ffefd4c47a8 /* 26 vars */) = -1 ENOENT (No such file or directory)
2802553 execve("/usr/bin/echo", ["echo", "hello world"], 0x7ffefd4c47a8 /* 26 vars */) = 0
2802553 +++ exited with 0 +++
2802548 <... wait4 resumed>[{WIFEXITED(s) && WEXITSTATUS(s) == 0}], 0, NULL) = 2802553
```

Analysis:

- The argument array shown in `execve()` is `["echo", "hello world"]`.
- This confirms that quoted spaces were preserved as a single argument.
- The successful executable is `/usr/bin/echo`.
- The child exits with status 0 and the parent waits correctly.

---

## 3. PATH Search Investigation

To observe PATH searching in more detail, I ran:

```bash
strace -f -o full_trace_ls.txt ./dsh
```

Then:

```bash
grep -n "execve" full_trace_ls.txt | head -n 80
```

Output:

```text
1:2802581 execve("./dsh", ["./dsh"], 0x7ffd7609ec28 /* 26 vars */) = 0
74:2802582 execve("/home/gbm26/courses/software/bin/ls", ["ls"], 0x7fffde708e98 /* 26 vars */) = -1 ENOENT (No such file or directory)
75:2802582 execve("/usr/local/sbin/ls", ["ls"], 0x7fffde708e98 /* 26 vars */) = -1 ENOENT (No such file or directory)
76:2802582 execve("/usr/local/bin/ls", ["ls"], 0x7fffde708e98 /* 26 vars */) = -1 ENOENT (No such file or directory)
77:2802582 execve("/usr/sbin/ls", ["ls"], 0x7fffde708e98 /* 26 vars */) = -1 ENOENT (No such file or directory)
78:2802582 execve("/usr/bin/ls", ["ls"], 0x7fffde708e98 /* 26 vars */) = 0
```

This shows that `execvp()` searches through PATH directories in order until it finds a match. Each failed attempt returns `ENOENT`. The successful execution occurs when `/usr/bin/ls` is found.

---

## 4. Parent/Child Verification

From the traces:

- A child process is created for each external command.
- The child performs the `execve()` calls.
- The parent waits using `wait4()`.
- Exit codes are correctly returned to the parent.
- Successful commands exit with 0.
- Failed commands exit with 1.

Conclusion:

Using `strace` confirms that my shell correctly implements the fork/exec/wait model used by Unix systems. Each command runs in a child process, the child replaces itself with the requested program, and the parent synchronizes correctly using `waitpid()`.
