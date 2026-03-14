#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <fcntl.h>

#include "dshlib.h"
#include "rshlib.h"

/*
 * exec_remote_cmd_loop - Main client loop
 * 
 * Connects to server, sends commands, receives and displays results.
 * 
 * Protocol:
 *   Client→Server: Null-terminated command string
 *   Server→Client: Data stream + EOF marker (0x04)
 * 
 * Algorithm:
 *   1. Allocate buffers
 *   2. Connect via start_client()
 *   3. Loop:
 *      a. Print prompt
 *      b. Read command with fgets()
 *      c. Send command (with null terminator!)
 *      d. Receive response in loop until EOF marker
 *      e. Display response
 *      f. Check for exit/stop-server
 *   4. Cleanup and close socket
 * 
 * @param address: Server IP address
 * @param port: Server port number
 * @return: OK on normal exit, error code on failure
 */
int exec_remote_cmd_loop(char *address, int port)
{
    char *cmd_buff = NULL;
    char *rsp_buff = NULL;
    int cli_socket = -1;
    ssize_t io_size;
    int is_eof;

    cmd_buff = malloc(SH_CMD_MAX);
    if (cmd_buff == NULL) {
        return client_cleanup(cli_socket, cmd_buff, rsp_buff, ERR_MEMORY);
    }

    rsp_buff = malloc(RDSH_COMM_BUFF_SZ);
    if (rsp_buff == NULL) {
        return client_cleanup(cli_socket, cmd_buff, rsp_buff, ERR_MEMORY);
    }

    cli_socket = start_client(address, port);
    if (cli_socket < 0) {
        perror("start_client");
        return client_cleanup(cli_socket, cmd_buff, rsp_buff, ERR_RDSH_CLIENT);
    }

    while (1)
    {
        printf("%s", SH_PROMPT);
        fflush(stdout);

        if (fgets(cmd_buff, SH_CMD_MAX, stdin) == NULL){
            printf("\n");
            break;
        }

        cmd_buff[strcspn(cmd_buff,"\n")] = '\0';

        io_size = send(cli_socket, cmd_buff, strlen(cmd_buff) + 1, 0);
        if (io_size < 0){
            perror("send");
            return client_cleanup(cli_socket, cmd_buff, rsp_buff, ERR_RDSH_COMMUNICATION);
        }

        while (1) {
            io_size = recv(cli_socket, rsp_buff, RDSH_COMM_BUFF_SZ, 0);

            if (io_size < 0){
                perror("recv");
                return client_cleanup(cli_socket, cmd_buff, rsp_buff, ERR_RDSH_COMMUNICATION);
            }

            if (io_size == 0){
                printf("%s", RCMD_SERVER_EXITED);
                return client_cleanup(cli_socket, cmd_buff, rsp_buff, OK);
            }

            is_eof = (rsp_buff[io_size - 1] == RDSH_EOF_CHAR);

            int printable_len = (int)io_size;
            if (is_eof) {
                printable_len--;
            }

            for (int i = 0; i < printable_len; i++) {
                unsigned char ch = (unsigned char)rsp_buff[i];
                if ((ch >= 32 && ch <= 126) || ch == '\n' || ch == '\t' || ch == '\r') {
                    putchar(ch);
                } else {
                    putchar('?');
                }
            }
            fflush(stdout);

            if (is_eof){
                break;
            }
        }

        if (strcmp(cmd_buff, EXIT_CMD) == 0 || strcmp(cmd_buff, "stop-server") == 0){
            break;
        }
    }

    return client_cleanup(cli_socket, cmd_buff, rsp_buff, OK);
}


/*
 * start_client - Connect to remote shell server
 * 
 * Creates socket and connects to server at specified address/port.
 * 
 * Algorithm:
 *   1. socket(AF_INET, SOCK_STREAM, 0)
 *   2. Setup sockaddr_in with server_ip and port
 *   3. connect() to server
 *   4. Return socket fd
 * 
 * @param address: Server IP address (e.g., "127.0.0.1")
 * @param port: Server port number (e.g., 1234)
 * @return: Socket fd on success, ERR_RDSH_CLIENT on failure
 */
int start_client(char *address, int port){
    struct sockaddr_in addr;
    int cli_socket;

    cli_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_socket < 0){
        return ERR_RDSH_CLIENT;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0){
        close(cli_socket);
        return ERR_RDSH_CLIENT;
    }

    if (connect(cli_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        close(cli_socket);
        return ERR_RDSH_CLIENT;
    }

    return cli_socket;
}



/*
 * client_cleanup - Clean up client resources
 * 
 * Helper function to close socket and free buffers.
 * Provided for you - handles cleanup on exit.
 * 
 * @param cli_socket: Client socket to close
 * @param cmd_buff: Command buffer to free
 * @param rsp_buff: Response buffer to free
 * @param rc: Return code to pass through
 * @return: The rc parameter (for easy return statements)
 */
int client_cleanup(int cli_socket, char *cmd_buff, char *rsp_buff, int rc){
    if(cli_socket > 0){
        close(cli_socket);
    }

    free(cmd_buff);
    free(rsp_buff);

    return rc;
}