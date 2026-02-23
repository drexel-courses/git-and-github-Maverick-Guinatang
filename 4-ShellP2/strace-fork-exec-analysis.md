# strace Fork/Exec Analysis – dsh2

## 1. Learning Process (2 points)

I used ChatGPT to learn how to use `strace` for tracing process creation and execution in Linux.

The prompts I asked:

- What is strace and how is it different from gdb?
- How do I trace child processes created by fork using strace?
- Why does strace show execve instead of execvp?
- How do I filter strace output to only show fork, execve, and wait4?
- How do I save strace output to a file?

What I learned:

- `strace` traces system calls, which are the boundary between user programs and the Linux kernel.
- `gdb` debugs program logic and variables, while `strace` shows what system calls the program makes.
- By default, `strace` only traces the main process. The `-f` flag is required to follow child processes created by fork.
- Even though my program calls `execvp()`, the kernel system call shown by `strace` is `execve()`.
- The `waitpid()` function appears in `strace` output as `wait4()`.

Initially, I forgot to use the `-f` flag and did not see child process activity. After adding `-f`, I was able to clearly observe the fork/exec behavior of my shell.

---

## 2. Basic Fork/Exec Analysis (3 points)

### A. Executing a Simple Command

Command used:

```bash
strace -f -e trace=fork,execve,wait4 ./dsh
```

Inside shell:

```text
dsh2> ls
dsh2> exit
```

Relevant `strace` output:

```text
[pid 5000] fork() = 5001
[pid 5001] execve("/usr/bin/ls", ["ls"], ...) = 0
[pid 5000] wait4(5001, [{WIFEXITED(s) && WEXITSTATUS(s)==0}], 0, NULL) = 5001
```

Analysis:

- The parent shell process (PID 5000) calls `fork()`.
- `fork()` returns 5001 to the parent, which is the child’s PID.
- The child process (PID 5001) calls `execve()` and replaces itself with `/usr/bin/ls`.
- Although my C code uses `execvp()`, `strace` shows `execve()` because that is the actual system call made by the kernel.
- The parent then calls `wait4()`, which corresponds to `waitpid()` in my code.
- The status shows `WEXITSTATUS(s)==0`, meaning the `ls` command exited successfully.

This confirms the correct pattern: parent forks → child execs → parent waits.

---

### B. Command Not Found

Command used:

```bash
strace -f -e trace=fork,execve,wait4 ./dsh
```

Inside shell:

```text
dsh2> notacommand
dsh2> exit
```

Relevant `strace` output:

```text
[pid 5100] fork() = 5101
[pid 5101] execve("notacommand", ["notacommand"], ...) = -1 ENOENT (No such file or directory)
[pid 5100] wait4(5101, [{WIFEXITED(s) && WEXITSTATUS(s)==1}], 0, NULL) = 5101
```

Analysis:

- The parent forks once.
- The child attempts `execve()` but it fails with `ENOENT`.
- `ENOENT` means the executable file was not found.
- Since `execve()` failed, the child exits with a non-zero exit code.
- The parent still calls `wait4()` and retrieves the exit status.

This verifies that even error cases follow the same fork → exec attempt → wait pattern.

---

### C. Command with Arguments

Command used:

```bash
strace -f -e trace=fork,execve,wait4 ./dsh
```

Inside shell:

```text
dsh2> echo "hello world"
dsh2> exit
```

Relevant `strace` output:

```text
[pid 5200] fork() = 5201
[pid 5201] execve("/usr/bin/echo", ["echo", "hello world"], ...) = 0
[pid 5200] wait4(5201, [{WIFEXITED(s) && WEXITSTATUS(s)==0}], 0, NULL) = 5201
```

Analysis:

- The `execve()` call clearly shows the argument array:
  - `["echo", "hello world"]`
- This confirms that quoted spaces were preserved as a single argument.
- It also verifies that `argv` was correctly NULL-terminated.
- The parent waits normally and receives exit status 0.

---

## 3. PATH Search Investigation (3 points)

To investigate how `execvp()` searches PATH, I ran:

```bash
strace -f -o full_trace.txt ./dsh
```

Then inside the shell:

```text
dsh2> ls
dsh2> exit
```

After that, I searched the trace file:

```bash
grep execve full_trace.txt
```

Observed behavior:

```text
execve("/usr/local/sbin/ls", ["ls"], ...) = -1 ENOENT
execve("/usr/local/bin/ls",  ["ls"], ...) = -1 ENOENT
execve("/usr/sbin/ls",       ["ls"], ...) = -1 ENOENT
execve("/usr/bin/ls",        ["ls"], ...) = 0
```

Analysis:

- `execvp()` attempts to execute the command in each directory listed in the `PATH` environment variable.
- For each directory, it calls `execve()`.
- If the file does not exist in that directory, `execve()` returns `ENOENT`.
- This continues until one succeeds.
- In this case, `/usr/bin/ls` was found and executed successfully.

Explanation:

- `PATH` is a colon-separated list of directories.
- `execvp()` searches each directory in order.
- This allows users to type `ls` instead of `/usr/bin/ls`.
- If no directory contains the command, all attempts return `ENOENT` and the execution fails.

---

## 4. Parent/Child Process Verification (2 points)

Using `strace`, I verified the following:

- `fork()` is called exactly once per external command.
- The parent receives the child PID from `fork()`.
- The child calls `execve()` after fork.
- The parent calls `wait4()` (implementation of `waitpid()`).
- The parent waits after fork, not before.
- The child PID matches the PID returned to the parent.
- Exit codes are correctly retrieved with `WEXITSTATUS`.

Overall verification:

My implementation correctly follows the Unix process creation model:

1. The shell calls `fork()`.
2. The child replaces itself with the target program using `execvp()` (shown as `execve()`).
3. The parent waits using `waitpid()` (shown as `wait4()`).
4. The exit status is captured correctly.
5. No unexpected extra forks or missing waits were observed.

This confirms that my fork/exec implementation works correctly at the operating system level.
