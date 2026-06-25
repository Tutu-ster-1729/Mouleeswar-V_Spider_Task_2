#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>								// for fork(), execvp(), chdir()
#include <sys/wait.h>							// for waitpid()
#include <stdint.h>								// for fixed-width integer types like uint32_t
#include <sys/stat.h>							// for stat()
#include <arpa/inet.h>							// Network socket architecture

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define DELIMITERS " \t\r\n"
#define DEFAULT_PORT 4443
#define LOOPBACK_IP "127.0.0.1"
#define CHUNK_SIZE 4096							// 4KB chunk size for optimized file transfer

// 72 byte Strict Network Envelope Definition
struct packed_header {			                // 4 + 64 + 4 = 72 bytes
    char magic[4];
    char filename[64];
    uint32_t payload_size;
} __attribute__((packed));

// NITTALK UTILITY MODULES
void listener(const char *output_filename, int port) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    struct packed_header header;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[nittalk] Socket creation failed");
        return;
    }
    // Force attaching socket to the port to prevent "Address already in use" errors
    // Kernel implements a 2 min timer to used socket to catch loose packets 
    // SO_REUSEADDR bypasses that timer
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;		        // IPv4
    address.sin_addr.s_addr = INADDR_ANY;	    // Bind to any local IP interface (WiFi, Ethernet, etc)
    address.sin_port = htons(port);		        // Host-to-Network Short (endianness conversion)

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[nittalk] Bind failed");
        return;
    }
    listen(server_fd, 3);			            // Queue upto 3 connections before rejecting them

    printf("[nittalk] Standing by for  radio connection on port %d...\n", port);
    client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    printf("[nittalk] Link established.\n");

    // Read and validate exactly 72 bytes into struct memory
    if (recv(client_socket, &header, sizeof(struct packed_header), 0) != sizeof(struct packed_header)) {
        fprintf(stderr, "[nittalk] Error receiving authentication envelope.\n");
        close(client_socket);
        close(server_fd);
        return;
    }
  
    if (memcmp(header.magic, "NIT\x00", 4) != 0) {
	fprintf(stderr, "[nittalk SECURITY] Invalid magic signature. Connection dropped.\n");
	close(client_socket);
	close(server_fd);
	return;
    }

    // Decode payload size using Network-to-Host conversion
    uint32_t total_payload_bytes = ntohl(header.payload_size);
    printf("[nittalk] Header verified. Streaming: %s (%u bytes)\n", header.filename, total_payload_bytes);

    // Stream file payload data to disk in 4KB chunks
    FILE *dest_file = fopen(output_filename, "wb");
    if (!dest_file) {
        perror("[nittalk] Output file open failed");
        close(client_socket);
        close(server_fd);
        return;
    }
    
    char chunk_buffer[CHUNK_SIZE];
    uint32_t total_bytes_received = 0;
    int bytes_read;

    while (total_bytes_received < total_payload_bytes) {
        uint32_t remaining = total_payload_bytes - total_bytes_received;
        uint32_t target_read = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        bytes_read = recv(client_socket, chunk_buffer, target_read, 0);
        if (bytes_read <= 0) {
            fprintf(stderr, "[nittalk] Connection dropped by remote sender.\n");
            break;
        }

        fwrite(chunk_buffer, 1, bytes_read, dest_file);
        total_bytes_received += bytes_read;
    }

	if (total_bytes_received == total_payload_bytes) {
		printf("[nittalk] File transfer complete. Saved %u bytes.\n", total_bytes_received);
	} else {
		fprintf(stderr, "[nittalk ERROR] Transfer incomplete!. Received %u of %u bytes.\n", total_bytes_received, total_payload_bytes);
	}

    fclose(dest_file);
    close(client_socket);
    close(server_fd);
}

void sender(const char *source_filename, const char *target_ip, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    struct packed_header header;
	struct stat file_stat;

    if (stat(source_filename, &file_stat) != 0) {
        perror("[nittalk] Local file lookup failed");
        return;
    }
    uint32_t file_size = file_stat.st_size;
	
    FILE *src_file = fopen(source_filename, "rb");
    if (!src_file) {
        perror("[nittalk] Source file open failed");
        return;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[nittalk] Socket creation failed");
		fclose(src_file);
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    // Convert IPv4 address from text to binary
    inet_pton(AF_INET, target_ip, &serv_addr.sin_addr);

    printf("[nittalk] Connecting to radio link address %s:%d...\n", target_ip, port);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[nittalk] Connection refused");
        fclose(src_file);
        close(sock);
        return;
    }

    // Populating and Transmit Header metadata
    memcpy(header.magic, "NIT\x00", 4);
    strncpy(header.filename, source_filename, 64);
    header.filename[63] = '\0';						// Ensure null termination
    // Safeguard integer size using Host-to-Network conversion before sending
    header.payload_size = htonl(file_size);

    if (send(sock, &header, sizeof(struct packed_header), 0) < 0) {
        perror("[nittalk] Failed to transmit authentication envelope");
        fclose(src_file);
        close(sock);
        return;
    }

    // Stream file payload data chunks
    char chunk_buffer[CHUNK_SIZE];
    size_t bytes_read;
    uint32_t total_bytes_sent = 0;

    while ((bytes_read = fread(chunk_buffer, 1, CHUNK_SIZE, src_file)) > 0) {
        send(sock, chunk_buffer, bytes_read, 0);
        total_bytes_sent += bytes_read;
    }

    printf("[nittalk] Transmission complete. Sent %u bytes.\n", total_bytes_sent);
    fclose(src_file);
    close(sock);
}

int main(){
    char input[MAX_INPUT_SIZE];
    char *args[MAX_ARGS];						// Array of character pointers (strings)
    
	while (1) {
    	printf("octosh_> ");
		// Force stdout to flush immediately so the prompt displays
		fflush(stdout);
		// Read input safely and handle EOF
		if (fgets(input, sizeof(input), stdin) == NULL){
			printf("\n");
			break;
		}
		// fgets keeps '\n'. Stripping it -> strcspn() returns the position of \n
		input[strcspn(input, "\n")] = '\0';
		int i = 0;
		char *token = strtok(input, DELIMITERS);// strtok splits the string based on delimiters
		while (token != NULL && i < MAX_ARGS -1){
			args[i] = token;
			i++;
			token = strtok(NULL, DELIMITERS);	// NULL tells it to keep chopping the last passed string 
		}
		args[i] = NULL; 						// Arrays must be NULL-terminated

		if (args[0] == NULL){
			continue;
		} else if (strcmp(args[0], "exit") == 0){
			break;
		} else if (strcmp(args[0], "cd") == 0) {
			if (args[1] == NULL){
			fprintf(stderr, "octosh: expected argument to \"cd\"\n");
			} else {
			if (chdir(args[1]) != 0) {			// System call that changes calling process directory
				perror("octosh cd");			// returns 0 on success and -1 if the path doesnt exist
			}
			}
			continue;							// Skip the forking and other logic if cd 
		} else if (strcmp(args[0], "nittalk") == 0) {
			if (args[1] == NULL || args[2] == NULL){
				fprintf(stderr, "Usage: nittalk -l <output_file> [port] OR nittalk -s <source_file> <target_ip> [port]\n");
			} else if (strcmp(args[1], "-l") == 0) {
				int port = (args[3] != NULL) ? atoi(args[3]) : DEFAULT_PORT; // Optional port argument
				listener(args[2], port);
			} else if (strcmp(args[1], "-s") == 0) {
				if (args[3] == NULL) {
					fprintf(stderr, "Usage: nittalk -s <source_file> <target_ip> [port]\n");
				} else {
					int port = (args[4] != NULL) ? atoi(args[4]) : DEFAULT_PORT; // Optional port argument
					sender(args[2], args[3], port);
				}
			} else fprintf(stderr, "[nittalk] Invalid runtime parameter format.\n");
			continue;							// Skip the forking and other logic if nittalk
		}
		
		pid_t pid = fork();						// Clones the running shell process
		// fork() gives child the same code as parent and is differentiated by what it returns at the end
		// pid returns -ve number if child process failed 
		// fork returns 0 if it is the child process, returns the pid of child if it is the parent process
		if (pid < 0){					
			perror("Fork failed"); 
		} else if (pid == 0){					// Child process
			// Inside the child process, execvp takes the command and its arguements
			// If it succeeds, it never returns here. It becomes a new program
			if (execvp(args[0], args) == -1){	// Throws the shell code and executes the command given
			perror("octosh");					// Prints the reason for error
			}
			exit(EXIT_FAILURE);					// Kill the child process if execvp fails
		} else {								// Parent Process
			// The parent process (shell) must wait until child finishes
			int status;
			// kernel writes the child's exit data to status and waitpid compares and holds parent until child is done executing
			waitpid(pid, &status, 0);			
		}
    }
    
	printf("Shutting down octo-shell cleanly...\n");
    return 0;
}
