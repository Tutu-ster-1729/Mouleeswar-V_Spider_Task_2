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

# Level 2 (Socket Stream Radio)

## 1. Objective
To build an integrated point-to-point network utility (`nittalk`) capable of streaming files in memory safe chunks across local TCP endpoints using structured network headers 

## 2. Implementation
* **Strict Structure**: Enforced a strict 72-byte network envelope (`struct packed_header`) combining a 4-byte magic signature (`NIT\x00`), a 64-byte filename field, and a 4-byte integer for file size. Used `__attribute__((packed))` to disable compiler-specific structure padding across different computer architectures.
* **Socket Recycling**: Implemented `setsockopt()` with the `SO_REUSEADDR` flag to bypass the kernel's default 2-minute `TIME_WAIT` lock making the port rebinds instantly on reuse.
* **Endianness Protection**: Implemented against data corruption across contrasting CPU designs by using `htonl()` to swap host byte orders into network standard Big-Endian on transmit, and `ntohl()` to safely restore them on receive.
* **Chunking Loop**: Avoided system memory exhaustion by parsing binary files (`"rb"`, `"wb"`) through a fixed 4KB buffer (`CHUNK_SIZE`). Managed TCP short reads by calculating exact remaining payload limits to prevent reading into unexpected following data streams.
* **Integrated into shell**: Embedded `nittalk` directly within the primary shell parsing logic alongside `cd` and `exit`. If intercepted, the prompt bypasses the child process process-forking engine completely, routing data streams directly inside the parent process.

## 3. Results
* Seamless Transmission executed within two active `octosh_>` shells.
* Unauthorized network cnnection attempts or corrupted packets successfully dropped using `memcmp`
* Clean context restoration where both shells return instantly to shell prompts after file is transmitted.

## 4. Limitations
1. **Plaintext Vulnerability**: File payloads and metadata are completely unencrypted, leaving data vulnerable to network packet sniffers.
2. **Shell Blocking**: The network loops operate on the shell's main thread. The prompt remains totally frozen and unresponsive to user command entries while a transfer is underway.
3. **Single-Client Sequential Processing**: The listener queue only binds and negotiates a single connection at a time, lacking asynchronous multi-client file transfers.

---
