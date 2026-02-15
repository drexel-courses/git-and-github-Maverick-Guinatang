#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "dshlib.h"

//===================================================================
// HELPER FUNCTIONS - Memory Management (PROVIDED)
//===================================================================

/**
 * alloc_cmd_buff - Allocate memory for cmd_buff internal buffer
 * 
 * This function is provided for you. It allocates the _cmd_buffer
 * that will store the command string.
 */
int alloc_cmd_buff(cmd_buff_t *cmd_buff)
{
    cmd_buff->_cmd_buffer = malloc(SH_CMD_MAX);
    if (cmd_buff->_cmd_buffer == NULL) {
        return ERR_MEMORY;
    }
    cmd_buff->argc = 0;
    for (int i = 0; i < CMD_ARGV_MAX; i++) {
        cmd_buff->argv[i] = NULL;
    }
    return OK;
}

/**
 * free_cmd_buff - Free cmd_buff internal buffer
 * 
 * This function is provided for you. Call it when done with a cmd_buff.
 */
int free_cmd_buff(cmd_buff_t *cmd_buff)
{
    if (cmd_buff->_cmd_buffer != NULL) {
        free(cmd_buff->_cmd_buffer);
        cmd_buff->_cmd_buffer = NULL;
    }
    cmd_buff->argc = 0;
    return OK;
}

/**
 * clear_cmd_buff - Reset cmd_buff without freeing memory
 * 
 * This function is provided for you.
 */
int clear_cmd_buff(cmd_buff_t *cmd_buff)
{
    cmd_buff->argc = 0;
    for (int i = 0; i < CMD_ARGV_MAX; i++) {
        cmd_buff->argv[i] = NULL;
    }
    return OK;
}

/**
 * free_cmd_list - Free all cmd_buffs in a command list
 * 
 * This function is provided for you. It frees all allocated memory
 * in a command_list_t structure.
 */
int free_cmd_list(command_list_t *cmd_lst)
{
    for (int i = 0; i < cmd_lst->num; i++) {
        free_cmd_buff(&cmd_lst->commands[i]);
    }
    cmd_lst->num = 0;
    return OK;
}

//===================================================================
// PARSING FUNCTIONS - YOU IMPLEMENT THESE
//===================================================================

/*
 * Helper: trim leading/trailing whitespace in-place.
 * Returns pointer to first non-space character (may be inside the original string).
 */
static char *trim_ws_inplace(char *s)
{
    if (s == NULL) {
        return NULL;
    }

    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }

    if (*s == '\0') {
        return s;
    }

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return s;
}

/**
 * build_cmd_buff - Parse a single command string into cmd_buff_t
 * 
 * YOU NEED TO IMPLEMENT THIS FUNCTION!
 * 
 * This function takes a single command string (no pipes) and parses
 * it into argc/argv format.
 * 
 * Steps:
 *   1. Copy cmd_line into cmd_buff->_cmd_buffer
 *   2. Split the string by spaces into tokens
 *   3. Store each token pointer in cmd_buff->argv[]
 *   4. Set cmd_buff->argc to number of tokens
 *   5. Ensure cmd_buff->argv[argc] is NULL (required for execvp later)
 * 
 * Example:
 *   Input:  "ls -la /tmp"
 *   Output: argc=3, argv=["ls", "-la", "/tmp", NULL]
 * 
 * Hints:
 *   - Use strcpy() to copy cmd_line to _cmd_buffer
 *   - Use strtok() to split by spaces
 *   - Remember to trim leading/trailing whitespace
 *   - Handle multiple consecutive spaces
 * 
 * @param cmd_line: Command string to parse
 * @param cmd_buff: Allocated cmd_buff_t to populate
 * @return: OK on success, error code on failure
 */
int build_cmd_buff(char *cmd_line, cmd_buff_t *cmd_buff)
{
    if (cmd_line == NULL || cmd_buff == NULL || cmd_buff->_cmd_buffer == NULL) {
        return ERR_MEMORY;
    }

    // Trim leading/trailing whitespace on the command string
    char *src = trim_ws_inplace(cmd_line);

    // Empty after trimming
    if (src[0] == '\0') {
        return WARN_NO_CMDS;
    }

    // Reset argc/argv but keep allocated buffer
    clear_cmd_buff(cmd_buff);

    // Rebuild the string into _cmd_buffer with '\0' terminators between args
    char *dst = cmd_buff->_cmd_buffer;

    while (*src != '\0') {

        // Skip whitespace between tokens
        while (*src != '\0' && isspace((unsigned char)*src)) {
            src++;
        }
        if (*src == '\0') {
            break;
        }

        // Too many args?
        if (cmd_buff->argc >= CMD_ARGV_MAX - 1) {
            return ERR_CMD_OR_ARGS_TOO_BIG;
        }

        // Start of a new argument
        cmd_buff->argv[cmd_buff->argc] = dst;
        cmd_buff->argc++;

        // If token starts with a quote, consume until matching quote (quotes removed)
        if (*src == '"' || *src == '\'') {
            char quote_char = *src;
            src++; // skip opening quote

            while (*src != '\0' && *src != quote_char) {
                *dst = *src;
                dst++;
                src++;
            }

            // If we stopped on a closing quote, skip it
            if (*src == quote_char) {
                src++;
            }

            // End this argument
            *dst = '\0';
            dst++;

            continue;
        }

        // Normal (unquoted) token: copy until whitespace, but support quotes inside too
        while (*src != '\0' && !isspace((unsigned char)*src)) {

            // If we hit a quote mid-token, copy inside (quotes removed)
            if (*src == '"' || *src == '\'') {
                char quote_char = *src;
                src++; // skip opening quote

                while (*src != '\0' && *src != quote_char) {
                    *dst = *src;
                    dst++;
                    src++;
                }

                if (*src == quote_char) {
                    src++; // skip closing quote
                }

                continue;
            }

            *dst = *src;
            dst++;
            src++;
        }

        // End this argument
        *dst = '\0';
        dst++;
    }

    // NULL-terminate argv array
    cmd_buff->argv[cmd_buff->argc] = NULL;

    if (cmd_buff->argc == 0) {
        return WARN_NO_CMDS;
    }

    return OK;
}

/**
 * build_cmd_list - Parse command line with pipes into command_list_t
 * 
 * YOU NEED TO IMPLEMENT THIS FUNCTION! This is the main parsing function.
 * 
 * This function:
 *   1. Checks if input is empty/whitespace only
 *   2. Splits input by pipe character '|'
 *   3. For each segment, creates a cmd_buff_t
 *   4. Stores all cmd_buffs in command_list_t
 * 
 * Algorithm:
 *   1. Trim leading/trailing whitespace from cmd_line
 *   2. If empty, return WARN_NO_CMDS
 *   3. Count pipe characters
 *      - If count > CMD_MAX-1, return ERR_TOO_MANY_COMMANDS
 *   4. Split cmd_line by '|' character
 *      - For each segment:
 *        a. Allocate cmd_buff using alloc_cmd_buff()
 *        b. Parse segment using build_cmd_buff()
 *        c. Store in clist->commands[i]
 *   5. Set clist->num to number of commands
 *   6. Return OK
 * 
 * Example inputs and outputs:
 * 
 *   Input:  "cmd"
 *   Output: num=1, commands[0]={argc=1, argv=["cmd", NULL]}
 * 
 *   Input:  "cmd arg1 arg2"
 *   Output: num=1, commands[0]={argc=3, argv=["cmd", "arg1", "arg2", NULL]}
 * 
 *   Input:  "cmd1 | cmd2"
 *   Output: num=2, 
 *           commands[0]={argc=1, argv=["cmd1", NULL]}
 *           commands[1]={argc=1, argv=["cmd2", NULL]}
 * 
 *   Input:  "ls | grep txt | wc -l"
 *   Output: num=3,
 *           commands[0]={argc=1, argv=["ls", NULL]}
 *           commands[1]={argc=2, argv=["grep", "txt", NULL]}
 *           commands[2]={argc=2, argv=["wc", "-l", NULL]}
 * 
 * Error cases:
 *   Input:  ""  or "   "
 *   Return: WARN_NO_CMDS
 * 
 *   Input:  "c1|c2|c3|c4|c5|c6|c7|c8|c9"  (9 commands)
 *   Return: ERR_TOO_MANY_COMMANDS
 * 
 * Hints:
 *   - Use strchr() to find pipe characters
 *   - Use strtok() with PIPE_STRING to split by pipes
 *   - Remember to trim spaces around each command segment
 *   - Don't forget to set clist->num!
 *   - Call alloc_cmd_buff() for each command before build_cmd_buff()
 * 
 * Standard library functions you might use:
 *   - strlen(), strcpy(), strcmp()
 *   - strtok(), strchr()
 *   - isspace(), isalpha()
 * 
 * @param cmd_line: Full command line from user input
 * @param clist: Command list to populate
 * @return: OK on success, error code on failure
 */
int build_cmd_list(char *cmd_line, command_list_t *clist)
{
    if (cmd_line == NULL || clist == NULL) {
        return ERR_MEMORY;
    }

    clist->num = 0;

    // 1. Trim leading/trailing whitespace from cmd_line
    char *line_trimmed = trim_ws_inplace(cmd_line);

    // 2. If empty, return WARN_NO_CMDS
    if (line_trimmed[0] == '\0') {
        return WARN_NO_CMDS;
    }

    // 3. Count pipe characters
    int pipe_count = 0;
    for (char *p = line_trimmed; *p != '\0'; p++) {
        if (*p == PIPE_CHAR) {
            pipe_count++;
        }
    }

    // If count > CMD_MAX-1, return ERR_TOO_MANY_COMMANDS
    if (pipe_count + 1 > CMD_MAX) {
        return ERR_TOO_MANY_COMMANDS;
    }

    // 4. Split cmd_line by '|' character
    char delim[2] = { PIPE_CHAR, '\0' };
    char *save_ptr = NULL;
    char *segment = strtok_r(line_trimmed, delim, &save_ptr);

    while (segment != NULL) {
        if (clist->num >= CMD_MAX) {
            free_cmd_list(clist);
            return ERR_TOO_MANY_COMMANDS;
        }

        char *seg_trimmed = trim_ws_inplace(segment);

        if (seg_trimmed[0] == '\0') {
            free_cmd_list(clist);
            return WARN_NO_CMDS;
        }

        // Allocate cmd_buff for this command
        int rc = alloc_cmd_buff(&clist->commands[clist->num]);
        if (rc != OK) {
            free_cmd_list(clist);
            return rc;
        }

        // Parse segment into argc/argv
        rc = build_cmd_buff(seg_trimmed, &clist->commands[clist->num]);
        if (rc != OK) {
            free_cmd_list(clist);
            return rc;
        }

        clist->num++;

        segment = strtok_r(NULL, delim, &save_ptr);
    }

    if (clist->num == 0) {
        return WARN_NO_CMDS;
    }

    return OK;
}

//===================================================================
// BUILT-IN COMMAND FUNCTIONS (PROVIDED FOR PART 1)
//===================================================================

/**
 * match_command - Check if input is a built-in command
 * 
 * This function is provided for you.
 */
Built_In_Cmds match_command(const char *input)
{
    if (strcmp(input, EXIT_CMD) == 0) {
        return BI_CMD_EXIT;
    }
    if (strcmp(input, "dragon") == 0) {
        return BI_CMD_DRAGON;
    }

    // NOTE: cd is NOT a built-in for Part 1 tests.
    // It should be parsed and printed like a normal command.
    return BI_NOT_BI;
}

/**
 * Drexel dragon ASCII art (extra credit).
 * Spaces must be preserved exactly; store as an array of strings.
 */
static const char *drexel_dragon[] = {
"                                                                        @%%%%                       ",
"                                                                     %%%%%%                         ",
"                                                                    %%%%%%                          ",
"                                                                 % %%%%%%%           @              ",
"                                                                %%%%%%%%%%        %%%%%%%           ",
"                                       %%%%%%%  %%%%@         %%%%%%%%%%%%@    %%%%%%  @%%%%       ",
"                                  %%%%%%%%%%%%%%%%%%%%%%      %%%%%%%%%%%%%%%%%%%%%%%%%%%%         ",
"                                %%%%%%%%%%%%%%%%%%%%%%%%%%   %%%%%%%%%%%% %%%%%%%%%%%%%%%          ",
"                               %%%%%%%%%%%%%%%%%%%%%%%%%%%%% %%%%%%%%%%%%%%%%%%%     %%%           ",
"                             %%%%%%%%%%%%%%%%%%%%%%%%%%%%@ @%%%%%%%%%%%%%%%%%%        %%           ",
"                            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% %%%%%%%%%%%%%%%%%%%%%%               ",
"                            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%             ",
"                            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%@%%%%%%@             ",
"      %%%%%%%%@           %%%%%%%%%%%%%%%%        %%%%%%%%%%%%%%%%%%%%%%%%%%      %%               ",
"    %%%%%%%%%%%%%         %%@%%%%%%%%%%%%           %%%%%%%%%%% %%%%%%%%%%%%      @%               ",
"  %%%%%%%%%%   %%%        %%%%%%%%%%%%%%            %%%%%%%%%%%%%%%%%%%%%%%%                       ",
" %%%%%%%%%       %         %%%%%%%%%%%%%             %%%%%%%%%%%%@%%%%%%%%%%%                      ",
"%%%%%%%%%@                % %%%%%%%%%%%%%            @%%%%%%%%%%%%%%%%%%%%%%%%%                    ",
"%%%%%%%%@                 %%@%%%%%%%%%%%%            @%%%%%%%%%%%%%%%%%%%%%%%%%%%%                 ",
"%%%%%%%@                   %%%%%%%%%%%%%%%           %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%             ",
"%%%%%%%%%%                  %%%%%%%%%%%%%%%          %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%      %%%% ",
"%%%%%%%%%@                   @%%%%%%%%%%%%%%         %%%%%%%%%%%%@ %%%% %%%%%%%%%%%%%%%%%   %%%%%%%%",
"%%%%%%%%%%                  %%%%%%%%%%%%%%%%%        %%%%%%%%%%%%%      %%%%%%%%%%%%%%%%%% %%%%%%%%%",
"%%%%%%%%%@%%@                %%%%%%%%%%%%%%%%@       %%%%%%%%%%%%%%     %%%%%%%%%%%%%%%%%%%%%%%%  %% ",
" %%%%%%%%%%                  % %%%%%%%%%%%%%%@        %%%%%%%%%%%%%%   %%%%%%%%%%%%%%%%%%%%%%%%%% %% ",
"  %%%%%%%%%%%%  @           %%%%%%%%%%%%%%%%%%        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  %%%",
"   %%%%%%%%%%%%% %%  %  %@ %%%%%%%%%%%%%%%%%%          %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    %%%",
"    %%%%%%%%%%%%%%%%%% %%%%%%%%%%%%%%%%%%%%%%           @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%    %%%%%%%",
"     %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%              %%%%%%%%%%%%%%%%%%%%%%%%%%%%        %%%  ",
"      @%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                  %%%%%%%%%%%%%%%%%%%%%%%%%              ",
"        %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                      %%%%%%%%%%%%%%%%%%%  %%%%%%%         ",
"           %%%%%%%%%%%%%%%%%%%%%%%%%%                           %%%%%%%%%%%%%%%  @%%%%%%%%%        ",
"              %%%%%%%%%%%%%%%%%%%%           @%@%                  @%%%%%%%%%%%%%%%%%%   %%%       ",
"                  %%%%%%%%%%%%%%%        %%%%%%%%%%                    %%%%%%%%%%%%%%%    %        ",
"                %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%                      %%%%%%%%%%%%%%           ",
"                %%%%%%%%%%%%%%%%%%%%%%%%%%  %%%% %%%                      %%%%%%%%%%  %%%@         ",
"                     %%%%%%%%%%%%%%%%%%% %%%%%% %%                          %%%%%%%%%%%%%@         ",
"                                                                                 %%%%%%%@          "
};

/**
 * exec_built_in_cmd - Execute built-in commands
 * 
 * This function is provided for you, but incomplete.
 * You can add dragon command here for extra credit.
 */
Built_In_Cmds exec_built_in_cmd(cmd_buff_t *cmd)
{
    Built_In_Cmds bi_cmd = match_command(cmd->argv[0]);
    
    switch (bi_cmd) {
        case BI_CMD_EXIT:
            // Exit is handled in main loop
            return BI_CMD_EXIT;
            
        case BI_CMD_DRAGON:
            // Extra credit - implement dragon here
            for (int i = 0; i < (int)(sizeof(drexel_dragon) / sizeof(drexel_dragon[0])); i++) {
                printf("%s\n", drexel_dragon[i]);
            }
            return BI_EXECUTED;
            
        case BI_CMD_CD:
            // CD will be implemented in Part 2
            printf("cd not implemented yet!\n");
            return BI_EXECUTED;
            
        default:
            return BI_NOT_BI;
    }
}

//===================================================================
// MAIN SHELL LOOP - YOU IMPLEMENT THIS
//===================================================================

/**
 * exec_local_cmd_loop - Main shell loop
 * 
 * YOU NEED TO IMPLEMENT THIS FUNCTION! This is your shell's main loop.
 * 
 * This function should:
 *   1. Loop forever (while(1))
 *   2. Print the shell prompt (SH_PROMPT)
 *   3. Read a line of input using fgets()
 *   4. Remove trailing newline
 *   5. Check for exit command - if found, print "exiting..." and break
 *   6. Parse the command line using build_cmd_list()
 *   7. Handle return codes:
 *      - WARN_NO_CMDS: print CMD_WARN_NO_CMD
 *      - ERR_TOO_MANY_COMMANDS: print CMD_ERR_PIPE_LIMIT with CMD_MAX
 *      - OK: print the parsed commands (see below)
 *   8. Free the command list using free_cmd_list()
 *   9. Loop back to step 2
 * 
 * Output format when commands are parsed successfully:
 * 
 *   First line:
 *     printf(CMD_OK_HEADER, clist.num);
 * 
 *   For each command:
 *     printf("<%d> %s", i+1, clist.commands[i].argv[0]);
 *     if (clist.commands[i].argc > 1) {
 *         printf(" [");
 *         for (int j = 1; j < clist.commands[i].argc; j++) {
 *             printf("%s", clist.commands[i].argv[j]);
 *             if (j < clist.commands[i].argc - 1) {
 *                 printf(" ");
 *             }
 *         }
 *         printf("]");
 *     }
 *     printf("\n");
 * 
 * Examples of expected output:
 * 
 *   dsh> cmd
 *   PARSED COMMAND LINE - TOTAL COMMANDS 1
 *   <1> cmd
 * 
 *   dsh> cmd arg1 arg2
 *   PARSED COMMAND LINE - TOTAL COMMANDS 1
 *   <1> cmd [arg1 arg2]
 * 
 *   dsh> cmd1 | cmd2 | cmd3
 *   PARSED COMMAND LINE - TOTAL COMMANDS 3
 *   <1> cmd1
 *   <2> cmd2
 *   <3> cmd3
 * 
 *   dsh> 
 *   warning: no commands provided
 * 
 *   dsh> c1|c2|c3|c4|c5|c6|c7|c8|c9
 *   error: piping limited to 8 commands
 * 
 *   dsh> exit
 *   exiting...
 * 
 * Starter code to help you get going:
 * 
 *   char cmd_line[SH_CMD_MAX];
 *   command_list_t clist;
 *   int rc;
 *   
 *   while (1) {
 *       printf("%s", SH_PROMPT);
 *       
 *       if (fgets(cmd_line, SH_CMD_MAX, stdin) == NULL) {
 *           printf("\n");
 *           break;
 *       }
 *       
 *       // Remove trailing newline
 *       cmd_line[strcspn(cmd_line, "\n")] = '\0';
 *       
 *       // Check for exit command
 *       // TODO: implement exit check
 *       
 *       // Parse the command line
 *       rc = build_cmd_list(cmd_line, &clist);
 *       
 *       // Handle return codes and print output
 *       // TODO: implement return code handling
 *       
 *       // Free memory
 *       if (rc == OK) {
 *           free_cmd_list(&clist);
 *       }
 *   }
 *   
 *   return OK;
 * 
 * @return: OK on success
 */
int exec_local_cmd_loop()
{
    char cmd_line[SH_CMD_MAX];
    command_list_t clist;
    int rc;

    while (1) {
        printf("%s", SH_PROMPT);

        if (fgets(cmd_line, SH_CMD_MAX, stdin) == NULL) {
            printf("\n");
            break;
        }

        // Remove trailing newline
        cmd_line[strcspn(cmd_line, "\n")] = '\0';

        // Parse the command line
        rc = build_cmd_list(cmd_line, &clist);

        // Handle return codes:
        if (rc == WARN_NO_CMDS) {
            printf(CMD_WARN_NO_CMD);
            continue;
        }

        if (rc == ERR_TOO_MANY_COMMANDS) {
            printf(CMD_ERR_PIPE_LIMIT, CMD_MAX);
            continue;
        }

        if (rc != OK) {
            // Other errors - do not print extra output (tests usually check only required messages)
            continue;
        }

        // Check built-in commands (exit, dragon)
        Built_In_Cmds bi = exec_built_in_cmd(&clist.commands[0]);

        if (bi == BI_CMD_EXIT) {
            printf("exiting...\n");
            free_cmd_list(&clist);
            break;
        }

        if (bi == BI_EXECUTED) {
            free_cmd_list(&clist);
            continue;
        }

        // Print the parsed commands (required format)
        printf(CMD_OK_HEADER, clist.num);

        for (int i = 0; i < clist.num; i++) {
            printf("<%d> %s", i + 1, clist.commands[i].argv[0]);

            if (clist.commands[i].argc > 1) {
                printf(" [");
                for (int j = 1; j < clist.commands[i].argc; j++) {
                    printf("%s", clist.commands[i].argv[j]);
                    if (j < clist.commands[i].argc - 1) {
                        printf(" ");
                    }
                }
                printf("]");
            }

            printf("\n");
        }

        // Free allocated memory before looping back
        free_cmd_list(&clist);
    }

    return OK;
}

//===================================================================
// EXECUTION FUNCTIONS - For future assignments
//===================================================================

/**
 * exec_cmd - Execute a single command (Part 2)
 * 
 * This will be implemented in Part 2 using fork/exec
 */
int exec_cmd(cmd_buff_t *cmd)
{
    (void)cmd; // suppress unused parameter warning for Part 1
    printf("exec_cmd not implemented yet (Part 2)\n");
    return OK;
}

/**
 * execute_pipeline - Execute piped commands (Part 3)
 * 
 * This will be implemented in Part 3 using pipes
 */
int execute_pipeline(command_list_t *clist)
{
    (void)clist; // suppress unused parameter warning for Part 1
    printf("execute_pipeline not implemented yet (Part 3)\n");
    return OK;
}

