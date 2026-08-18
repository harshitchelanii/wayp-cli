# Wayp-CLI

## Current Status

- TCP multi-client chat server
- Private messaging
- Command handler system
- `>users`
- `>help`
- `>whoami`
- `>clear`

A lightweight, plugin-based terminal chat application written in C.

## Features

- TCP client-server architecture
- Multi-client communication
- Username handshake (WIP)
- Thread-per-client server
- Plugin system (planned)

## Roadmap

- [x] TCP server
- [x] Multi-client support
- [x] Client broadcasting
- [ ] Username authentication
- [ ] Rooms
- [ ] Private messaging
- [ ] Plugin API
- [ ] End-to-end encryption

## Building

### Server

```bash
gcc server.c -o server -lpthread
```

### Client

```bash
gcc client.c -o client -lpthread
```

## Vision

Wayp-CLI aims to be a fast, extensible terminal chat application where users can customize functionality through plugins while keeping the core lightweight.