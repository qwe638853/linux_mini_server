# Server Architecture

## Overview

The server uses a **fork-based multi-process architecture** to handle multiple clients concurrently. Each client connection is processed in a separate child process, providing isolation and fault tolerance.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    Server Process (Parent)                       │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  Initialization Phase                                     │  │
│  │  • socket() - Create listening socket                    │  │
│  │  • bind() - Bind to 127.0.0.1:9734                      │  │
│  │  • listen() - Set backlog to 10                         │  │
│  │  • Signal handlers (SIGCHLD, SIGPIPE)                   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                           │                                      │
│                           ▼                                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  Main Loop (while(1))                                     │  │
│  │                                                           │  │
│  │  1. accept() - Wait for client connection                │  │
│  │  2. fork() - Create child process                        │  │
│  │  3. Parent: close(cfd), continue loop                    │  │
│  │  4. Child: Process client request                        │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                           │
                           │ fork()
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ Child Process│  │ Child Process│  │ Child Process│
│   (Client 1) │  │   (Client 2) │  │   (Client N) │
└──────────────┘  └──────────────┘  └──────────────┘
         │                 │                 │
         └─────────────────┼─────────────────┘
                           │
                           ▼
         ┌─────────────────────────────────────┐
         │  Child Process (Per Client)         │
         │                                     │
         │  1. close(server_sockfd)            │
         │  2. fdopen(cfd) - Convert to FILE* │
         │  3. Read command from client        │
         │  4. Process request:                │
         │     • SENDMAIL → smtp.c            │
         │     • Other → sysinfo.c            │
         │  5. Send response                   │
         │  6. fclose(), close(), exit(0)      │
         └─────────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│   sysinfo.c  │  │    smtp.c    │  │    env.c     │
│              │  │              │  │              │
│ • Hostname   │  │ • SendGrid   │  │ • Load .env  │
│ • Memory     │  │   API        │  │ • Parse vars │
│ • Disk       │  │ • JSON       │  │              │
│ • Network    │  │   escape     │  │              │
│ • OS Info    │  │ • CURL       │  │              │
└──────────────┘  └──────────────┘  └──────────────┘
         │                 │                 │
         └─────────────────┼─────────────────┘
                           │
                           ▼
              ┌──────────────────────┐
              │  libutility.so       │
              │  (Shared Library)     │
              │                       │
              │  • debug.c           │
              │    - ERROR_LOG       │
              │    - WARN_LOG        │
              │    - INFO_LOG        │
              │    - DEBUG_LOG       │
              └──────────────────────┘
```

## Process Flow

### 1. Server Startup
```
socket() → bind() → listen() → signal handlers → main loop
```

### 2. Client Connection Handling
```
accept() → fork() → [Parent: continue] [Child: process request]
```

### 3. Request Processing
```
Read command → Parse → Route to module → Execute → Send response
```

## Component Details

### Parent Process
- **Responsibilities**:
  - Socket initialization and binding
  - Signal handling (SIGCHLD, SIGPIPE)
  - Accepting new connections
  - Forking child processes
  - Resource cleanup (closing client socket in parent)

### Child Process
- **Responsibilities**:
  - Handle individual client request
  - Read command from client
  - Execute appropriate module (sysinfo/smtp)
  - Send response back to client
  - Clean up and exit

### Modules

#### sysinfo.c
Provides system information functions:
- `get_hostname()` - System hostname
- `get_local_time()` - Current time
- `get_os_info()` - OS information
- `get_memory_usage()` - Memory statistics
- `get_user_info()` - User information
- `get_disk_info()` - Disk usage
- `get_env_info()` - Environment variables
- `get_network_info()` - Network interfaces

#### smtp.c
Handles email sending via SendGrid API:
- `send_email()` - Main email sending function
- `json_escape()` - JSON string escaping
- Uses libcurl for HTTP requests

#### env.c
Manages environment variable loading:
- `load_env_file()` - Load .env file
- Supports multiple path searching
- Validates file format

#### debug.c (utility library)
Provides debug logging infrastructure:
- Runtime enable/disable control
- Log level management
- Four log levels: ERROR, WARN, INFO, DEBUG

## Dependencies

```
server
├── libutility.so (shared library)
│   └── debug.c
├── libcurl (external library)
│   └── For SendGrid API communication
└── System libraries
    ├── socket, bind, listen, accept
    ├── fork, signal handling
    └── FILE I/O operations
```

## Concurrency Model

- **Process-based**: Each client gets its own process
- **True Parallelism**: Multiple processes can run simultaneously
- **Isolation**: Process crashes don't affect others
- **Resource Management**: Each process manages its own resources

## Signal Handling

- **SIGCHLD**: Handled to prevent zombie processes
  - `SA_NOCLDSTOP | SA_NOCLDWAIT` flags used
- **SIGPIPE**: Ignored to prevent crashes on broken pipe
  - Writing to closed socket returns error instead of signal

## Error Handling

All system calls include error checking:
- Socket operations: check return values
- Fork operations: handle failures gracefully
- File operations: validate FILE* pointers
- Resource cleanup: all error paths clean up properly

For detailed error handling mechanisms, see [ROBUSTNESS.md](ROBUSTNESS.md).

