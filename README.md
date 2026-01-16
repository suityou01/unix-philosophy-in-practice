# Bell Labs Style Time Server

A pedagogical project demonstrating Unix systems programming, network protocols, 
and security concepts by building a simple time synchronization service from scratch.

## Philosophy

In the spirit of Bell Labs, when you need a tool, you build it. This project 
teaches fundamental concepts by implementing a network time service in C.

## Branch Progression

- `feature/01-basic-socket` - TCP server fundamentals
- `feature/02-get-time` - Implement GET_TIME command
- `feature/03-protocol-structure` - Binary protocol serialization
- `feature/04-set-time-unsafe` - SET_TIME without authentication (vulnerable!)
- `feature/05-add-authentication` - Token-based auth
- `feature/06-user-privileges` - System-level permission checking
- `feature/07-daemon-mode` - Daemonization
- `feature/08-systemd-service` - Production deployment

## Building
```bash
make
```

## Running
```bash
# Terminal 1
./server

# Terminal 2
./client
```

## Learning Objectives

- Socket programming (bind, listen, accept)
- Protocol design and serialization
- Client-server architecture
- Unix permissions and privileges
- Daemon processes
- Security vulnerabilities and mitigations


