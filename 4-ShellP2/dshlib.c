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

int free_cmd_buff(cmd_buff_t *cmd_buff)
{
    if (cmd_buff->_cmd_buffer != NULL) {
        free(cmd_buff->_cmd_buffer);
        cmd_buff->_cmd_buffer = NULL;
    }
    cmd_buff->argc = 0;
    return OK;
}

int clear_cmd_buff(cmd_buff_t *cmd_buff)
{
    cmd_buff->argc = 0;
    for (int i = 0; i < CMD_ARGV_MAX; i++) {
        cmd_buff->argv[i] = NULL;
    }
    return OK;
}

//===================================================================
// PARSING FUNCTIONS - YOU IMPLEMENT THESE
//===================================================================

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

int build_cmd_buff(char *cmd_line, cmd_buff_t *cmd_buff)
{
    if (cmd_line == NULL || cmd_buff == NULL || cmd_buff->_cmd_buffer == NULL) {
        return ERR_MEMORY;
    }

    char *src = trim_ws_inplace(cmd_line);

    if (src[0] == '\0') {
        return WARN_NO_CMDS;
    }

    clear_cmd_buff(cmd_buff);

    char *dst = cmd_buff->_cmd_buffer;

    while (*src != '\0') {

        while (*src != '\0' && isspace((unsigned char)*src)) {
            src++;
        }
        if (*src == '\0') {
            break;
        }

        if (cmd_buff->argc >= CMD_ARGV_MAX - 1) {
            return ERR_CMD_OR_ARGS_TOO_BIG;
        }

        cmd_buff->argv[cmd_buff->argc] = dst;
        cmd_buff->argc++;

        if (*src == '"' || *src == '\'') {
            char quote_char = *src;
            src++;

            while (*src != '\0' && *src != quote_char) {
                *dst = *src;
                dst++;
                src++;
            }

            if (*src == quote_char) {
                src++;
            }

            *dst = '\0';
            dst++;

            continue;
        }

        while (*src != '\0' && !isspace((unsigned char)*src)) {

            if (*src == '"' || *src == '\'') {
                char quote_char = *src;
                src++;

                while (*src != '\0' && *src != quote_char) {
                    *dst = *src;
                    dst++;
                    src++;
                }

                if (*src == quote_char) {
                    src++;
                }

                continue;
            }

            *dst = *src;
            dst++;
            src++;
        }

        *dst = '\0';
        dst++;
    }

    cmd_buff->argv[cmd_buff->argc] = NULL;

    if (cmd_buff->argc == 0) {
        return WARN_NO_CMDS;
    }

    return OK;
}

//===================================================================
// BUILT-IN COMMAND FUNCTIONS
//===================================================================

static int last_return_code = 0;

Built_In_Cmds match_command(const char *input)
{
    if (strcmp(input, EXIT_CMD) == 0) {
        return BI_CMD_EXIT;
    }
    if (strcmp(input, "cd") == 0) {
        return BI_CMD_CD;
    }
    if (strcmp(input, "rc") == 0) {
        return BI_RC;
    }
    if (strcmp(input, "dragon") == 0) {
        return BI_CMD_DRAGON;
    }
    return BI_NOT_BI;
}

Built_In_Cmds exec_built_in_cmd(cmd_buff_t *cmd)
{
    Built_In_Cmds bi_cmd = match_command(cmd->argv[0]);

    switch (bi_cmd) {
        case BI_CMD_EXIT:
            return BI_CMD_EXIT;

        case BI_CMD_CD:
            if (cmd->argc < 2) {
                last_return_code = 0;
                return BI_EXECUTED;
            }
            if (chdir(cmd->argv[1]) != 0) {
                perror("cd");
                last_return_code = 1;
            } else {
                last_return_code = 0;
            }
            return BI_EXECUTED;

        case BI_RC:
            printf("%d\n", last_return_code);
            return BI_EXECUTED;

        case BI_CMD_DRAGON:
            return BI_NOT_BI;

        default:
            return BI_NOT_BI;
    }
}

//===================================================================
// EXECUTION FUNCTIONS
//===================================================================

int exec_cmd(cmd_buff_t *cmd)
{
    pid_t pid;
    int status;

    if (cmd == NULL || cmd->argc == 0 || cmd->argv[0] == NULL) {
        return ERR_EXEC_CMD;
    }

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return ERR_EXEC_CMD;
    }

    if (pid == 0) {
        execvp(cmd->argv[0], cmd->argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return ERR_EXEC_CMD;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return ERR_EXEC_CMD;
}

/*
 * Implement your exec_local_cmd_loop function by building a loop that prompts the
 * user for input.  Use the SH_PROMPT constant from dshlib.h and then
 * use fgets to accept user input.
 *
 *      while(1){
 *        printf("%s", SH_PROMPT);
 *        if (fgets(cmd_buff, ARG_MAX, stdin) == NULL){
 *           printf("\n");
 *           break;
 *        }
 *        //remove the trailing \n from cmd_buff
 *        cmd_buff[strcspn(cmd_buff,"\n")] = '\0';
 *
 *        //IMPLEMENT THE REST OF THE REQUIREMENTS
 *      }
 *
 *   Also, use the constants in the dshlib.h in this code.
 *      SH_CMD_MAX              maximum buffer size for user input
 *      EXIT_CMD                constant that terminates the dsh program
 *      SH_PROMPT               the shell prompt
 *      OK                      the command was parsed properly
 *      WARN_NO_CMDS            the user command was empty
 *      ERR_TOO_MANY_COMMANDS   too many pipes used
 *      ERR_MEMORY              dynamic memory management failure
 *
 *   errors returned
 *      OK                     No error
 *      ERR_MEMORY             Dynamic memory management failure
 *      WARN_NO_CMDS           No commands parsed
 *      ERR_TOO_MANY_COMMANDS  too many pipes used
 *
 *   console messages
 *      CMD_WARN_NO_CMD        print on WARN_NO_CMDS
 *      CMD_ERR_PIPE_LIMIT     print on ERR_TOO_MANY_COMMANDS
 *      CMD_ERR_EXECUTE        print on execution failure of external command
 *
 *  Standard Library Functions You Might Want To Consider Using (assignment 1+)
 *      malloc(), free(), strlen(), fgets(), strcspn(), printf()
 *
 *  Standard Library Functions You Might Want To Consider Using (assignment 2+)
 *      fork(), execvp(), exit(), chdir()
 */
int exec_local_cmd_loop()
{
    char *cmd_buff;
    int rc = 0;
    cmd_buff_t cmd;

    // TODO IMPLEMENT MAIN LOOP

    cmd_buff = malloc(SH_CMD_MAX);
    if (cmd_buff == NULL) {
        return ERR_MEMORY;
    }

    rc = alloc_cmd_buff(&cmd);
    if (rc != OK) {
        free(cmd_buff);
        return ERR_MEMORY;
    }

    while(1){
        printf("%s", SH_PROMPT);
        if (fgets(cmd_buff, SH_CMD_MAX, stdin) == NULL){
           printf("\n");
           break;
        }
        //remove the trailing \n from cmd_buff
        cmd_buff[strcspn(cmd_buff,"\n")] = '\0';

        // TODO IMPLEMENT parsing input to cmd_buff_t *cmd_buff
        rc = build_cmd_buff(cmd_buff, &cmd);

        if (rc == WARN_NO_CMDS) {
            printf("%s", CMD_WARN_NO_CMD);
            continue;
        }

        if (rc == ERR_TOO_MANY_COMMANDS) {
            printf(CMD_ERR_PIPE_LIMIT, CMD_MAX);
            continue;
        }

        if (rc != OK) {
            continue;
        }

        // TODO IMPLEMENT if built-in command, execute builtin logic for exit, cd (extra credit: dragon)
        // the cd command should chdir to the provided directory; if no directory is provided, do nothing
        Built_In_Cmds bi = exec_built_in_cmd(&cmd);

        if (bi == BI_CMD_EXIT) {
            printf("exiting...\n");
            break;
        }

        if (bi == BI_EXECUTED) {
            continue;
        }

        // TODO IMPLEMENT if not built-in command, fork/exec as an external command
        // for example, if the user input is "ls -l", you would fork/exec the command "ls" with the arg "-l"
        rc = exec_cmd(&cmd);
        last_return_code = rc;
        if (rc == ERR_EXEC_CMD) {
            printf("error: execution failure\n");
        }
    }

    free_cmd_buff(&cmd);
    free(cmd_buff);
    return OK;
}
