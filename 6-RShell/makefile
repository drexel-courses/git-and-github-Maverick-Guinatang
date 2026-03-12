# Makefile for dsh (Drexel Shell) Part 4 - Remote Shell

CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread  # For extra credit multi-threaded server
TARGET = dsh
SRCS = dsh_cli.c dshlib.c rsh_cli.c rsh_server.c
HDRS = dshlib.h rshlib.h
TEST_SCRIPT = test_dsh4.py
TEST_PORT = 12345

# Default target - build the shell
all: $(TARGET)

# Build the executable
$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Run automated tests
test: $(TARGET)
	@echo "Running pytest network tests..."
	@echo "Note: Tests start server automatically"
	@pytest $(TEST_SCRIPT) -v

# Run tests with detailed output
test-verbose: $(TARGET)
	@echo "Running pytest tests with detailed output..."
	@pytest $(TEST_SCRIPT) -vv --tb=long

# Run specific test class
test-class: $(TARGET)
	@echo "Run specific test class with: make test-class CLASS=TestSimpleCommands"
	@pytest $(TEST_SCRIPT)::$(CLASS) -v

# Run specific test function
test-one: $(TARGET)
	@echo "Run specific test with: make test-one TEST=test_echo_command"
	@pytest $(TEST_SCRIPT) -v -k "$(TEST)"

# Manual server/client testing
server: $(TARGET)
	@echo "Starting server on port $(TEST_PORT)..."
	@echo "Press Ctrl+C to stop"
	./$(TARGET) -s -p $(TEST_PORT)

# EXTRA CREDIT: Start server in multi-threaded mode
server-threaded: $(TARGET)
	@echo "Starting MULTI-THREADED server on port $(TEST_PORT)..."
	@echo "Multiple clients can connect simultaneously!"
	@echo "Press Ctrl+C to stop"
	./$(TARGET) -s -x -p $(TEST_PORT)

client: $(TARGET)
	@echo "Connecting to server on 127.0.0.1:$(TEST_PORT)..."
	./$(TARGET) -c -p $(TEST_PORT)

# Test local mode (non-networked)
test-local: $(TARGET)
	@echo "Testing local mode..."
	@echo -e "echo hello\npwd\nexit" | ./$(TARGET)

# Network analysis tools
tcpdump-capture: $(TARGET)
	@echo "Capturing network traffic on port $(TEST_PORT)..."
	@echo "In another terminal:"
	@echo "  Terminal 1: make server"
	@echo "  Terminal 2: make client"
	@echo "Press Ctrl+C to stop capture"
	sudo tcpdump -i lo -w remote_shell.pcap port $(TEST_PORT)

tcpdump-view:
	@echo "Viewing captured network traffic..."
	@if [ -f remote_shell.pcap ]; then \
		tcpdump -r remote_shell.pcap -X | less; \
	else \
		echo "Error: remote_shell.pcap not found. Run 'make tcpdump-capture' first."; \
	fi

wireshark:
	@echo "Opening Wireshark..."
	@if [ -f remote_shell.pcap ]; then \
		wireshark remote_shell.pcap & \
	else \
		echo "Error: remote_shell.pcap not found. Run 'make tcpdump-capture' first."; \
	fi

# strace network syscalls
strace-server: $(TARGET)
	@echo "Running server with strace (network syscalls)..."
	strace -e trace=socket,bind,listen,accept,send,recv,close ./$(TARGET) -s -p $(TEST_PORT)

strace-client: $(TARGET)
	@echo "Running client with strace (network syscalls)..."
	@echo "Make sure server is running first!"
	strace -e trace=socket,connect,send,recv,close ./$(TARGET) -c -p $(TEST_PORT)

# Check if port is in use
check-port:
	@echo "Checking if port $(TEST_PORT) is in use..."
	@if lsof -i :$(TEST_PORT) > /dev/null 2>&1; then \
		echo "Port $(TEST_PORT) is IN USE:"; \
		lsof -i :$(TEST_PORT); \
	else \
		echo "Port $(TEST_PORT) is available"; \
	fi

# Kill processes using test port
kill-port:
	@echo "Killing processes on port $(TEST_PORT)..."
	@lsof -ti :$(TEST_PORT) | xargs -r kill -9
	@echo "Done"

# Valgrind memory check
valgrind: $(TARGET)
	@echo "Running valgrind memory check (local mode)..."
	@echo -e "echo hello\nexit" | valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./$(TARGET)

# Valgrind for network mode (server)
valgrind-server: $(TARGET)
	@echo "Running server with valgrind..."
	@echo "Connect with: make client (in another terminal)"
	valgrind --leak-check=full --track-fds=yes ./$(TARGET) -s -p $(TEST_PORT)

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o *.pcap

# Clean and rebuild
rebuild: clean all

# Install pytest if not present
install-pytest:
	@echo "Installing pytest..."
	pip3 install pytest --break-system-packages

# Demo: automated client-server test
demo: $(TARGET)
	@echo "Starting automated demo..."
	@echo "Starting server in background..."
	@./$(TARGET) -s -p $(TEST_PORT) > server.log 2>&1 & \
	SERVER_PID=$$!; \
	sleep 1; \
	echo "Running client commands..."; \
	echo -e "echo hello from client\nls\npwd\nexit" | ./$(TARGET) -c -p $(TEST_PORT); \
	echo "Stopping server..."; \
	kill $$SERVER_PID 2>/dev/null || true; \
	wait $$SERVER_PID 2>/dev/null || true; \
	echo "Demo complete!"; \
	cat server.log; \
	rm -f server.log

# Help target
help:
	@echo "dsh (Drexel Shell) Part 4 - Remote Shell Makefile"
	@echo ""
	@echo "Build Targets:"
	@echo "  all              - Build dsh (default)"
	@echo "  clean            - Remove built files"
	@echo "  rebuild          - Clean and rebuild"
	@echo ""
	@echo "Testing Targets:"
	@echo "  test             - Run all automated pytest tests"
	@echo "  test-verbose     - Run tests with detailed output"
	@echo "  test-class       - Run specific test class: make test-class CLASS=TestSimpleCommands"
	@echo "  test-one         - Run specific test: make test-one TEST=test_name"
	@echo "  test-local       - Test local (non-network) mode"
	@echo "  demo             - Run automated client-server demo"
	@echo ""
	@echo "Manual Testing:"
	@echo "  server           - Start server on port $(TEST_PORT)"
	@echo "  server-threaded  - Start MULTI-THREADED server (EXTRA CREDIT)"
	@echo "  client           - Connect client to localhost:$(TEST_PORT)"
	@echo "  check-port       - Check if port $(TEST_PORT) is in use"
	@echo "  kill-port        - Kill processes using port $(TEST_PORT)"
	@echo ""
	@echo "Network Analysis (for assignment):"
	@echo "  tcpdump-capture  - Capture network traffic to remote_shell.pcap"
	@echo "  tcpdump-view     - View captured traffic"
	@echo "  wireshark        - Open Wireshark with captured traffic"
	@echo "  strace-server    - Run server with strace (see network syscalls)"
	@echo "  strace-client    - Run client with strace (see network syscalls)"
	@echo ""
	@echo "Memory Checking:"
	@echo "  valgrind         - Run valgrind on local mode"
	@echo "  valgrind-server  - Run server with valgrind"
	@echo ""
	@echo "Other:"
	@echo "  install-pytest   - Install pytest"
	@echo "  help             - Show this help"
	@echo ""
	@echo "Typical workflow:"
	@echo "  1. make           # Build"
	@echo "  2. make test      # Run automated tests"
	@echo "  3. Terminal 1: make server"
	@echo "     Terminal 2: make client"
	@echo "  4. make tcpdump-capture  # For network analysis"

.PHONY: all test test-verbose test-class test-one server client test-local \
        tcpdump-capture tcpdump-view wireshark strace-server strace-client \
        check-port kill-port valgrind valgrind-server clean rebuild \
        install-pytest demo help
