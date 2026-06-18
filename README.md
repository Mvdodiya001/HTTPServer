# Secure High-Performance C++ Chat Server

A scalable, multi-threaded C++ chat server supporting HTTPS and Secure WebSockets (WSS). Built with Boost.Asio, Boost.Beast, and SQLite, optimized for concurrent performance.

## 🚀 Features

*   **Security**: Full SSL/TLS encryption for both HTTP (HTTPS) and WebSocket (WSS) connections.
*   **Performance**:
    *   **Hardware Concurrency**: Automatically detects and utilizes all available CPU cores (e.g., 12 threads).
    *   **Async Database**: Dedicated thread pool for non-blocking SQLite operations.
    *   **Keep-Alive**: Optimized connection handling with `SO_KEEPALIVE`.
*   **Storage**: SQLite3 database with Write-Ahead Logging (WAL) for reliability.
*   **API**: RESTful endpoints for user management and real-time WebSocket messaging.

## 🛠️ Build & Run

### Prerequisites
*   C++17 compatible compiler
*   CMake (3.15+)
*   Boost (System, Thread)
*   OpenSSL
*   SQLite3

### 1. Generate Certificates
For development/testing, generate self-signed certificates:
```bash
./generate_certs.sh
```
This creates `server.crt` and `server.key`.

### 2. Build
```bash
make
```
*(Or use `cmake . && make`)*

### 3. Run
```bash
./server
```
The server will start on port `8888`.

## 📊 Benchmarks & Performance

The system has been rigorously tested using `wrk` (modern HTTP benchmarking tool) on a 12-thread system.

**Target**: `https://localhost:8888/api/users` (SSL/TLS Encrypted)

### 🚀 Load Test Results (30s duration)

| Metric | Value | Notes |
| :--- | :--- | :--- |
| **Requests/Sec** | **1,795.16** | ~1.8K RPS with full SSL encryption |
| **Total Requests** | **54,004** | Zero failures |
| **Concurrency** | **400** | 400 active connections |
| **Avg Latency** | **55.96 ms** | Inclusive of TLS overhead |

### ⏱️ Latency Distribution
| Stat | Value |
| :--- | :--- |
| Stdev | 45.27 ms |
| Max | 243.38 ms |
| +/- Stdev | 68.45% |

### 🛡️ Functional Verification
*   **HTTPS**: Verified SSL handshake and data transfer (`HTTP 200 OK`).
*   **WSS**: Verified Secure WebSocket Upgrade (`101 Switching Protocols`).
*   **Frontend**: Verified browser access via `https://localhost:8888`.

### Architecture Specs
*   **Threading**: Uses `std::thread::hardware_concurrency()` (Tested on 12-thread system).
*   **Database**: Asynchronous worker pool pattern to prevent I/O blocking.

## 📂 Project Structure

*   `src/`: implementation files
*   `include/`: header files
*   `frontend/`: optimized web assets
*   `ChatRoom.cpp`: Manages active chat sessions
*   `HttpSession.cpp`: Handles HTTP API endpoints
*   `WSSession.cpp`: Handles WebSocket upgrades and messaging
*   `Database.cpp`: Async SQLite wrapper
