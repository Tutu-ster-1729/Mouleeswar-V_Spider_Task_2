# Level 1 (The core Shell)

## 1. Objective 
To Build a functional multi-process CLI in pure C capable of parsing arguements, executing external binaries and managing local environment mutations

## 2. Implementation
* **REPL Engine**: Used an infinite loop driven by `fgets()` to enforce strict buffer conditions. `strcspn()` was used to strip trailing newline.
* **String Tokenization**: Used `strtok()` to partition input commands by specified delimiters into NULL-terminated array of character pointers.
* **Process Handling**: `fork()` is used to clone the shell. The child process handles the commands using `execvp()`, while the parent uses `waitpid()` to pause until child finishes execution.
* **Crictical commands**: commands such as `cd`, `exit` are handled before forking a child shell. `cd` uses `chdir()` to alter parent process's environment directly

## 3. Results 
* Basic commands (`ls`, `pwd`, `gcc`) executed perfectly with correct arguements.
* Empty inputs and white spaces are handled 
* Successful directory navigation from inside the shell

## 4. Limitations
1. Lacks Background execution. Long running child processes completely freeze user controls of parent shell
2. Cannot handle standard stream redirection (`<`, `>`, `|`, etc)
3. Hardcoded input size and total arguement limits 

---
