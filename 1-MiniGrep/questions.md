# Questions

Answer the following questions about your minigrep implementation:

## 1. Pointer Arithmetic in Pattern Matching

In your `str_match()` function, you need to check if the pattern appears anywhere in the line, not just at the beginning. Explain your approach for checking all possible positions in the line. How do you use pointer arithmetic to advance through the line?

> For the first problem str_match(), I wanted to try to match the pattern starting at every start spot in the line. So, I use a pointer start that begins at the start of the line and I move it forward with start++. For each start, I compare the pattern and the line using two pointers. If the characters match, I move both the pointers forward. If something does not match, I stop it then move start to the next character. If I reach a point at the end of the pattern then a pattern was found and so I return 1. If I reach the end of the line and no patter was found I return 0.

## 2. Case-Insensitive Comparison

When implementing case-insensitive matching (the `-i` flag), you need to compare characters without worrying about case. Explain how you handle the case where the pattern is "error" but the line contains "ERROR" or "Error". What functions did you use and why?

> For -i, I make both characters the same case before comparing. I do this by using tolower() on the character from the line and the character from the pattern. That way "error" matches "ERROR", "Error", etc., because they all get compared in lowercase.

## 3. Memory Management

Your program allocates a line buffer using `malloc()`. Explain what would happen if you forgot to call `free()` before your program exits. Would this cause a problem for:
   - A program that runs once and exits?
   - A program that runs in a loop processing thousands of files?

> The use of malloc() is to to make space for the line buffer and so I should use free() when I am done. If I do not use free() that memory is “leaked.” If the program usually runs once and exit then it should not matter in the long run as the OS would usually clean it up but regardless it is still bad practice. If the program runs a lot then memory leaks can build up and waste memory and the program can slow down or crash.

## 4. Buffer Size Choice

The starter code defines `LINE_BUFFER_SZ` as 256 bytes. What happens if a line in the input file is longer than 256 characters? How does `fgets()` handle this situation? (You may need to look up the documentation for `fgets()` to answer this.)

> When a line is longer than 256 bytes, fgets() does not read the entire line. Instead, it reads up to the max byte it can within the line because its buffer size is limiting. Only the bytes that fills the buffer gets processed. So, when fgets() encounters unread characters after a newline, those leftovers stay in the file. Then, the next time it runs, it grabs whatever follows that newline. So if it hasn’t hit the newline yet, the entire line slips into the next read. Long inputs might spread across several calls before finishing.

## 5. Return Codes

The program uses different exit codes (0, 1, 2, 3, 4) for different situations. Why is it useful for command-line utilities to return different codes instead of always returning 0 or 1? Give a practical example of how you might use these return codes in a shell script.

> When running tools from the command line, using varied exit values makes sense since these numbers might explain the exit reason instead of just indicating success or failure. Take a shell script, for instance - it could examine the returned code to determine if data matched a pattern, if the file failed to open, or if arguments were entered incorrectly. Each situation could trigger distinct actions based on that single number.
