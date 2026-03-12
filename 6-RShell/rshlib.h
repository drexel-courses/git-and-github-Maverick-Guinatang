#ifndef __RSH_LIB_H__
#define __RSH_LIB_H__

#include "dshlib.h"

//===================================================================
// REMOTE SHELL CONSTANTS AND DEFINITIONS
//===================================================================

// Network Configuration
#define RDSH_DEF_PORT           1234        // Default port number
#define RDSH_DEF_SVR_INTFACE    "0.0.0.0"   // Bind to all interfaces
#define RDSH_DEF_CLI_CONNECT    "127.0.0.1" // Default: localhost

// Buffer Sizes
#define RDSH_COMM_BUFF_SZ       (1024*64)   // 64KB buffer for network I/O

// Protocol Constants
// EOF character marks end of server response
// Critical: TCP is a stream, we need message boundaries!
static const char RDSH_EOF_CHAR = 0x04;    // ASCII EOF (Ctrl-D)

// Special Return Codes
#define STOP_SERVER_SC          200         // Command requests server stop

// Error Codes
#define ERR_RDSH_COMMUNICATION  -50     // send()/recv() failed
#define ERR_RDSH_SERVER         -51     // Server setup/operation failed
#define ERR_RDSH_CLIENT         -52     // Client connection failed
#define ERR_RDSH_CMD_EXEC       -53     // Command execution failed
#define WARN_RDSH_NOT_IMPL      -99     // Function not implemented

//===================================================================
// OUTPUT MESSAGE CONSTANTS
//===================================================================

// Error Messages
#define CMD_ERR_RDSH_COMM   "rdsh-error: communications error\n"
#define CMD_ERR_RDSH_EXEC   "rdsh-error: command execution error\n"
#define CMD_ERR_RDSH_ITRNL  "rdsh-error: internal server error - %d\n"
#define CMD_ERR_RDSH_SEND   "rdsh-error: partial send. Sent %d, expected %d\n"

// Client Messages
#define RCMD_SERVER_EXITED  "server appeared to terminate - exiting\n"

// Server Messages
#define RCMD_MSG_CLIENT_EXITED  "client exited: getting next connection...\n"
#define RCMD_MSG_SVR_STOP_REQ   "client requested server to stop, stopping...\n"
#define RCMD_MSG_SVR_EXEC_REQ   "rdsh-exec: %s\n"
#define RCMD_MSG_SVR_RC_CMD     "rdsh-exec: rc = %d\n"

//===================================================================
// FUNCTION PROTOTYPES - CLIENT (rsh_cli.c)
//===================================================================

/**
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
int start_client(char *address, int port);

/**
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
int exec_remote_cmd_loop(char *address, int port);

/**
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
int client_cleanup(int cli_socket, char *cmd_buff, char *rsp_buff, int rc);

//===================================================================
// FUNCTION PROTOTYPES - SERVER (rsh_server.c)
//===================================================================

/**
 * start_server - Main server entry point
 * 
 * Boots server, processes client requests, stops on exit.
 * Provided - calls boot_server() and process_cli_requests().
 * 
 * @param ifaces: Interface to bind (e.g., "0.0.0.0")
 * @param port: Port to listen on
 * @param is_threaded: 1 for multi-threaded (EC), 0 for single-threaded
 * @return: OK_EXIT on normal shutdown, error code on failure
 */
int start_server(char *ifaces, int port, int is_threaded);

/**
 * boot_server - Initialize and start server socket
 * 
 * Creates socket, binds to address/port, starts listening.
 * 
 * Algorithm:
 *   1. socket(AF_INET, SOCK_STREAM, 0)
 *   2. setsockopt(SO_REUSEADDR) - helps with development
 *   3. Setup sockaddr_in with ifaces and port
 *   4. bind() socket to address
 *   5. listen() with backlog of 20
 *   6. Return socket fd
 * 
 * @param ifaces: Interface IP to bind to
 * @param port: Port number to bind to
 * @return: Server socket fd on success, ERR_RDSH_COMMUNICATION on failure
 */
int boot_server(char *ifaces, int port);

/**
 * stop_server - Shut down server socket
 * 
 * Provided - simply closes server socket.
 * 
 * @param svr_socket: Server socket to close
 * @return: Result of close()
 */
int stop_server(int svr_socket);

/**
 * process_cli_requests - Accept and handle client connections
 * 
 * Main server loop - accepts clients, processes their requests.
 * 
 * Algorithm:
 *   while (1):
 *     1. accept() client connection
 *     2. Call exec_client_requests(cli_socket)
 *     3. If returns negative (stop-server), break
 *     4. Otherwise, loop to accept next client
 * 
 * @param svr_socket: Listening server socket
 * @return: OK_EXIT on stop-server, error code on failure
 */
int process_cli_requests(int svr_socket);

/**
 * exec_client_requests - Handle commands from one client
 * 
 * Receives commands from client, executes them, sends results back.
 * 
 * Algorithm:
 *   while (1):
 *     1. recv() command from client
 *     2. If recv <= 0, client disconnected, break
 *     3. Parse command into command_list_t
 *     4. Check for built-ins:
 *        - "exit": send EOF, return OK
 *        - "stop-server": send EOF, return OK_EXIT (negative)
 *        - "cd": execute chdir(), send message
 *     5. If not built-in:
 *        - Call rsh_execute_pipeline(cli_socket, &clist)
 *        - Send EOF marker
 *     6. Free buffers and loop
 * 
 * @param cli_socket: Client connection socket
 * @return: OK on client exit, OK_EXIT on stop-server, error on failure
 */
int exec_client_requests(int cli_socket);

/**
 * send_message_string - Send string message to client
 * 
 * Sends message followed by EOF marker.
 * 
 * @param cli_socket: Client socket
 * @param buff: Null-terminated message string
 * @return: OK on success, ERR_RDSH_COMMUNICATION on failure
 */
int send_message_string(int cli_socket, char *buff);

/**
 * send_message_eof - Send EOF marker to client
 * 
 * Sends RDSH_EOF_CHAR (0x04) to signal end of transmission.
 * 
 * @param cli_socket: Client socket
 * @return: OK on success, ERR_RDSH_COMMUNICATION on failure
 */
int send_message_eof(int cli_socket);

/**
 * rsh_execute_pipeline - Execute commands with socket I/O
 * 
 * Like execute_pipeline() from Part 3, but:
 *   - First command: stdin from cli_socket
 *   - Last command: stdout/stderr to cli_socket
 *   - Middle commands: piped normally
 * 
 * Algorithm:
 *   1. Create N-1 pipes for N commands
 *   2. Fork each command:
 *      - First cmd: dup2(cli_socket, STDIN)
 *      - Middle cmds: dup2 pipes normally
 *      - Last cmd: dup2(cli_socket, STDOUT) and STDERR
 *      - All: close all pipe fds
 *      - execvp()
 *   3. Parent closes all pipes
 *   4. Wait for all children
 *   5. Return exit code of last command
 * 
 * @param socket_fd: Client socket (for stdin/stdout/stderr)
 * @param clist: Command list to execute
 * @return: Exit code of last command, or special codes
 */
int rsh_execute_pipeline(int socket_fd, command_list_t *clist);

//===================================================================
// OPTIONAL HELPER FUNCTIONS
//===================================================================

/**
 * rsh_match_command - Check if command is built-in
 * 
 * Optional helper - maps command strings to Built_In_Cmds enum.
 * 
 * @param input: Command string
 * @return: Appropriate Built_In_Cmds value
 */
Built_In_Cmds rsh_match_command(const char *input);

/**
 * rsh_built_in_cmd - Execute built-in command
 * 
 * Optional helper - handles built-in command execution.
 * 
 * @param cmd: Parsed command
 * @return: Built_In_Cmds indicating what happened
 */
Built_In_Cmds rsh_built_in_cmd(cmd_buff_t *cmd);

//===================================================================
// EXTRA CREDIT - MULTI-THREADED SERVER
//===================================================================

/**
 * handle_client - Thread function for handling one client
 * 
 * For extra credit multi-threaded server.
 * Each client connection spawns a new thread running this function.
 * 
 * @param arg: Pointer to client socket fd
 * @return: NULL
 */
void *handle_client(void *arg);

#endif // __RSH_LIB_H__
