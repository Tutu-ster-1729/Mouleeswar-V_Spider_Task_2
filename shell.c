#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>					// for fork(), execvp()
#include <sys/wait.h>					// for waitpid()

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define DELIMITERS " \t\r\n"

int main(){
    char input[MAX_INPUT_SIZE];
    char *args[MAX_ARGS];				// Array of character pointers (strings)
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
	char *token = strtok(input, DELIMITERS);	// strtok splits the string based on delimiters
	while (token != NULL && i < MAX_ARGS -1){
	    args[i] = token;
	    i++;
	    token = strtok(NULL, DELIMITERS);		// NULL tells it to keep chopping the last passed string 
	}
	args[i] = NULL; 				// Arrays must be NULL-terminated
	if (args[0] == NULL){
	    continue;
	} else if (strcmp(args[0], "exit") == 0){
	    break;
	} else if (strcmp(args[0], "cd") == 0) {
	    if (args[1] == NULL){
		fprintf(stderr, "octosh: expected argument to \"cd\"\n");
	    } else {
		if (chdir(args[1]) != 0) {		// System call that changes calling process directory
		    perror("octosh cd");		// returns 0 on success and -1 if the path doesnt exist
		}
	    }
	    continue;					// Skip the forking and other logic if cd 
	}
	pid_t pid = fork();				// Clones the running shell process
	// fork() gives child the same code as parent and is differentiated by what it returns at the end
	// pid returns -ve number if child process failed 
	// fork returns 0 if it is the child process, returns the pid of child if it is the parent process
	if (pid < 0){					
	    perror("Fork failed"); 
	} else if (pid == 0){				// Child process
	    // Inside the child process, execvp takes the command and its arguements
	    // If it succeeds, it never returns here. It becomes a new program
	    if (execvp(args[0], args) == -1){		// Throws the shell code and executes the command given
		perror("octosh");			// Prints the reason for error
	    }
	    exit(EXIT_FAILURE);				// Kill the child process if execvp fails
	} else {					// Parent Process
	    // The parent process (shell) must wait until child finishes
	    int status;
	    // kernel writes the child's exit data to status and waitpid compares and holds parent until child is done executing
	    waitpid(pid, &status, 0);			
	}
    }
    printf("Shutting down octo-shell cleanly...\n");
    return 0;
}
