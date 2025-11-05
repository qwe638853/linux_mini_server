# Server-Client Application

A simple client-server application that can query system information or send emails.

## Quick Start

### Build

```bash
mkdir build && cd build
cmake ..
make
```

After building, the executables will be in `build/bin/`.

### Run

**Start Server:**
```bash
./build/bin/server
```

**Start Client:**
```bash
# Query system information
./build/bin/client

# Send email
./build/bin/client SENDMAIL recipient@example.com "Subject" "Body"
```

## Features

- Query system information (hostname, memory, disk, network interfaces, etc.)
- Send emails via SendGrid API
- Debug logging (compile-time and runtime control)

## Debug Mode

Enable at compile time:
```bash
cmake -DBUILD_DEBUG=ON ..
make
```

Enable at runtime:
```bash
DEBUG_LOG=1 ./build/bin/server
DEBUG_LOG=1 DEBUG_LOG_LEVEL=3 ./build/bin/client
```

## Project Structure

```
utility (libutility.so) - Shared library
  └── debug.c

server
  ├── server.c, sysinfo.c, smtp.c, env.c
  └── Links: utility, libcurl

client
  ├── client.c
  └── Links: utility
```

## Dependencies

- CMake 3.10+
- libcurl
- C99 compiler

## Notes

To send emails, you need a `.env` file in the project root directory:
```
SENDGRID_API_KEY=your_api_key
SENDGRID_FROM=your_email@example.com
```
