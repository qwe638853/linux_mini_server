# Robustness Mechanisms - Execution Examples

本文件提供至少三個執行範例，展示有和沒有實作穩健性機制時程式行為的差異。

## 範例 1: accept() 失敗時的優雅降級

### 場景說明
當 Server 在處理大量連接請求時，可能會遇到暫時性的系統資源限制，導致 `accept()` 系統呼叫失敗。

### 有穩健性機制的行為（當前實作）

```c
// server.c:132-137
int cfd = accept(server_sockfd, (struct sockaddr *)&cli, &clilen);
if (cfd < 0) {
    ERROR_LOG(stderr, "accept() failed\n");
    perror("accept");
    continue;  // 關鍵：繼續等待下一個連接，不中斷服務
}
```

**執行結果：**
```
[INFO] Server starting...
server listening on 127.0.0.1:9734
[DEBUG] Waiting for client connection...
[ERROR] accept() failed
accept: Resource temporarily unavailable
[DEBUG] Waiting for client connection...  ← 繼續等待，服務未中斷
[INFO] Client connected from 127.0.0.1:54321
```

**關鍵行為：**
- ✅ 記錄錯誤訊息
- ✅ 使用 `continue` 繼續主循環
- ✅ Server 保持運行，繼續接受後續連接
- ✅ 單個連接失敗不影響整體服務

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
int cfd = accept(server_sockfd, (struct sockaddr *)&cli, &clilen);
// 沒有檢查返回值，直接使用
```

**可能的執行結果：**
```
[INFO] Server starting...
server listening on 127.0.0.1:9734
[DEBUG] Waiting for client connection...
Segmentation fault (core dumped)  ← 程式崩潰
```

或

```
[INFO] Server starting...
server listening on 127.0.0.1:9734
[DEBUG] Waiting for client connection...
[ERROR] Invalid file descriptor
[ERROR] Invalid file descriptor
... (無限循環，浪費 CPU 資源)
```

**問題：**
- ❌ 程式可能崩潰（如果對負數 fd 進行操作）
- ❌ 或進入無限錯誤循環
- ❌ 服務完全中斷，無法處理後續請求

---

## 範例 2: fork() 失敗時的資源清理

### 場景說明
當系統達到最大進程數限制時，`fork()` 會失敗。這種情況下，Server 必須正確清理已建立的客戶端 socket，避免資源洩漏。

### 有穩健性機制的行為（當前實作）

```c
// server.c:140-146
pid_t pid = fork();
if (pid < 0) {
    ERROR_LOG(stderr, "fork() failed\n");
    perror("fork");
    close(cfd);  // 關鍵：清理客戶端 socket
    continue;     // 關鍵：繼續服務其他連接
}
```

**執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[ERROR] fork() failed
fork: Cannot allocate memory
[DEBUG] Waiting for client connection...  ← 繼續等待新連接
[INFO] Client connected from 127.0.0.1:54322
[DEBUG] Child process started (PID: 12345)
```

**關鍵行為：**
- ✅ 正確關閉客戶端 socket (`close(cfd)`)
- ✅ 記錄錯誤訊息，便於診斷
- ✅ 繼續處理其他連接請求
- ✅ 無資源洩漏（socket 已關閉）

**驗證資源清理：**
```bash
# 在 fork() 失敗前後檢查 socket 數量
$ lsof -p <server_pid> | grep socket | wc -l
# 失敗前：例如 10 個
# 失敗後（有清理）：仍是 10 個（無增加）
# 失敗後（無清理）：11 個（socket 洩漏）
```

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
pid_t pid = fork();
if (pid == 0) {
    // 處理客戶端
} else {
    // 父進程繼續
}
// 如果 fork() 失敗（pid < 0），會進入 else 分支
// 但 cfd 未關閉，導致 socket 洩漏
```

**可能的執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[ERROR] fork() failed
fork: Cannot allocate memory
[DEBUG] Waiting for client connection...
# Socket 洩漏：cfd 沒有被關閉
# 每次 fork() 失敗都會洩漏一個 socket
# 最終導致 "Too many open files" 錯誤
```

**問題：**
- ❌ Socket 資源洩漏（每次失敗洩漏一個）
- ❌ 隨著時間累積，最終導致 `EMFILE` 錯誤（Too many open files）
- ❌ Server 無法再接受新連接

---

## 範例 3: fdopen() 失敗時的子進程清理

### 場景說明
在某些極端情況下（如系統資源耗盡），將 socket 轉換為 FILE* 流可能會失敗。子進程必須正確清理資源並退出。

### 有穩健性機制的行為（當前實作）

```c
// server.c:153-159
FILE *client_fp = fdopen(cfd, "r+");
if (client_fp == NULL) {
    ERROR_LOG(stderr, "fdopen() failed\n");
    perror("fdopen");
    close(cfd);  // 關鍵：清理 socket
    exit(1);     // 關鍵：子進程正確退出
}
```

**執行結果：**
```
[DEBUG] Child process started (PID: 12345)
[ERROR] fdopen() failed
fdopen: Cannot allocate memory
# 子進程退出，資源已清理
# 父進程繼續運行，無影響
```

**關鍵行為：**
- ✅ 檢查 `fdopen()` 返回值
- ✅ 失敗時清理 socket (`close(cfd)`)
- ✅ 子進程正確退出 (`exit(1)`)
- ✅ 父進程不受影響，繼續服務

**驗證清理：**
```bash
# 檢查子進程是否正確退出
$ ps aux | grep server
# 只有父進程在運行，子進程已退出

# 檢查是否有殭屍進程
$ ps aux | grep '<defunct>'
# 無殭屍進程（因為有 SIGCHLD 處理）
```

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
FILE *client_fp = fdopen(cfd, "r+");
// 沒有檢查，直接使用
fgets(command, sizeof(command), client_fp);  // ← 可能崩潰
```

**可能的執行結果：**
```
[DEBUG] Child process started (PID: 12345)
Segmentation fault (core dumped)  ← 子進程崩潰
# 或
[ERROR] Invalid file pointer
# 子進程可能變成殭屍進程
# 或導致父進程收到 SIGCHLD 但無法正確處理
```

**問題：**
- ❌ 子進程崩潰（段錯誤）
- ❌ Socket 資源洩漏（未關閉）
- ❌ 可能產生殭屍進程
- ❌ 父進程可能收到意外信號

---

## 範例 4: fprintf() 寫入失敗時的優雅處理

### 場景說明
當客戶端突然斷開連接時，向客戶端寫入資料會失敗。Server 應該檢測並記錄這種情況，但不應崩潰。

### 有穩健性機制的行為（當前實作）

```c
// server.c:186-191
if(fprintf(client_fp, "Command: %s\n", command) < 0 ||
   fprintf(client_fp, "To: %s\n", to) < 0){
    WARN_LOG(stderr, "Failed to write response to client\n");
    // 繼續執行，嘗試完成其他操作
}
```

**執行結果：**
```
[INFO] Processing SENDMAIL command
[WARN] Failed to write response to client
[INFO] Sending email to user@example.com
[INFO] Email sent successfully to user@example.com
# Server 繼續運行，嘗試完成郵件發送
# 即使客戶端已斷開，郵件仍可能成功發送
```

**關鍵行為：**
- ✅ 檢測寫入失敗（`fprintf()` 返回值 < 0）
- ✅ 記錄警告，但不中斷處理
- ✅ 繼續執行關鍵操作（如發送郵件）
- ✅ Server 保持穩定

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
fprintf(client_fp, "Command: %s\n", command);
fprintf(client_fp, "To: %s\n", to);
// 沒有檢查返回值
// 如果客戶端已斷開，可能收到 SIGPIPE 信號
```

**可能的執行結果：**
```
[INFO] Processing SENDMAIL command
Broken pipe  ← 收到 SIGPIPE 信號
# 如果沒有處理 SIGPIPE，程式可能崩潰
# 或
# 程式繼續執行但不知道寫入失敗
# 導致郵件發送但客戶端無法收到確認
```

**問題：**
- ❌ 可能收到 `SIGPIPE` 導致程式終止（如果未處理）
- ❌ 無法知道操作是否成功傳達給客戶端
- ❌ 無法進行適當的錯誤處理

---

## 範例 5: SIGPIPE 信號處理（防止寫入已關閉的 Socket 崩潰）

### 場景說明
當向已關閉的 socket 寫入資料時，系統會發送 `SIGPIPE` 信號。如果沒有處理，程式會立即終止。

### 有穩健性機制的行為（當前實作）

```c
// server.c:116-122
struct sigaction sa_pipe;
memset(&sa_pipe, 0, sizeof(sa_pipe));
sa_pipe.sa_handler = SIG_IGN;  // 關鍵：忽略 SIGPIPE
if(sigaction(SIGPIPE, &sa_pipe, NULL) < 0){
    WARN_LOG(stderr, "sigaction(SIGPIPE) failed\n");
    perror("sigaction");
    // 非致命錯誤，繼續執行
}
```

**測試場景：**
```bash
# 終端 1：啟動 Server
$ ./server
server listening on 127.0.0.1:9734

# 終端 2：連接並立即斷開
$ nc 127.0.0.1 9734 &
$ killall nc  # 立即斷開連接

# Server 嘗試寫入時的行為
```

**執行結果（有機制）：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
[WARN] Failed to write response to client
# Server 繼續運行，無崩潰
# fprintf() 返回錯誤，但程式繼續執行
```

**執行結果（無機制）：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
Broken pipe
[1] 12345 broken pipe  ./server
# 程式因 SIGPIPE 而終止
```

**關鍵行為：**
- ✅ 忽略 `SIGPIPE` 信號，防止程式崩潰
- ✅ 寫入失敗由 `fprintf()` 返回值檢測
- ✅ 可以進行適當的錯誤處理和記錄
- ✅ Server 保持穩定運行

---

---

## 範例 6: 客戶端連接但不發送數據（超時處理）⭐ 容易測試

### 場景說明
客戶端連接到服務器後，可能因為網路問題或程式錯誤而不發送任何數據。服務器應該設置超時，避免無限期等待。

### 有穩健性機制的行為（當前實作）

```c
// server.c:155-163
// 設置 socket 讀取超時（30秒），防止客戶端連接後不發送數據
struct timeval timeout;
timeout.tv_sec = 30;
timeout.tv_usec = 0;
if (setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    WARN_LOG(stderr, "setsockopt(SO_RCVTIMEO) failed\n");
    perror("setsockopt");
    // 非致命錯誤，繼續執行
}
```

```c
// server.c:281-289
} else {
    // 如果沒有收到指令（可能是客戶端斷開或超時）
    if (ferror(client_fp)) {
        ERROR_LOG(stderr, "Error reading from client (client may have disconnected)\n");
    } else if (feof(client_fp)) {
        WARN_LOG(stderr, "Client closed connection before sending command\n");
    } else {
        WARN_LOG(stderr, "No command received (timeout or empty input), sending default system info\n");
    }
```

**測試方法：**
```bash
# 終端 1：啟動服務器
$ cd build && ./server
server listening on 127.0.0.1:9734

# 終端 2：連接但不發送任何數據（等待 30 秒以上）
$ nc 127.0.0.1 9734
# 不輸入任何內容，等待...

# 服務器日誌（30秒後）：
[WARN] No command received (timeout or empty input), sending default system info
# 服務器繼續運行，可以處理下一個連接
```

**執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
[WARN] No command received (timeout or empty input), sending default system info
# 服務器在超時後繼續運行，可以接受新連接
```

**關鍵行為：**
- ✅ 設置 30 秒讀取超時
- ✅ 檢測超時情況並記錄
- ✅ 服務器繼續運行，不阻塞
- ✅ 可以處理後續連接請求

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
FILE *client_fp = fdopen(cfd, "r+");
fgets(command, sizeof(command), client_fp);  // 無限期等待
```

**可能的執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
# 子進程永遠阻塞在 fgets()，等待客戶端發送數據
# 資源被佔用，無法處理其他連接
# 如果多個客戶端都這樣，服務器會耗盡進程資源
```

**問題：**
- ❌ 子進程無限期阻塞
- ❌ 資源（進程、socket）被佔用
- ❌ 無法處理其他連接請求
- ❌ 可能導致服務器資源耗盡

---

## 範例 7: 超長輸入處理（緩衝區溢出防護）⭐ 容易測試

### 場景說明
客戶端可能發送超過緩衝區大小的命令。服務器應該檢測並處理這種情況，防止緩衝區溢出。

### 有穩健性機制的行為（當前實作）

```c
// server.c:182-189
if (fgets(command, sizeof(command), client_fp) != NULL) {
    // 檢查是否因為緩衝區太小而截斷（行太長）
    if(strlen(command) == sizeof(command) - 1 && command[sizeof(command) - 2] != '\n'){
        WARN_LOG(stderr, "Command line too long, may be truncated\n");
        // 清空輸入緩衝區，避免後續讀取錯誤
        int c;
        while ((c = fgetc(client_fp)) != EOF && c != '\n');
    }
```

**測試方法：**
```bash
# 終端 1：啟動服務器
$ cd build && ./server

# 終端 2：發送超長命令（超過 256 字節）
$ python3 -c "print('A' * 300)" | nc 127.0.0.1 9734

# 服務器日誌：
[WARN] Command line too long, may be truncated
# 服務器繼續運行，不會崩潰
```

**執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
[WARN] Command line too long, may be truncated
# 命令被截斷，但服務器繼續運行
# 清空剩餘輸入，避免影響後續讀取
```

**關鍵行為：**
- ✅ 檢測超長輸入（緩衝區溢出風險）
- ✅ 記錄警告訊息
- ✅ 清空輸入緩衝區，避免後續錯誤
- ✅ 服務器繼續運行，不會崩潰

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
char command[256];
fgets(command, sizeof(command), client_fp);
// 沒有檢查是否截斷
strcmp(command, "SENDMAIL");  // 可能使用截斷的命令
```

**可能的執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
# 命令被截斷但未檢測
# 後續讀取可能讀到錯誤的數據
# 可能導致邏輯錯誤或安全問題
```

**問題：**
- ❌ 未檢測緩衝區溢出風險
- ❌ 可能導致邏輯錯誤
- ❌ 後續讀取可能讀到錯誤數據
- ❌ 潛在的安全風險

---

## 範例 8: 空命令驗證 ⭐ 容易測試

### 場景說明
客戶端可能發送空命令（只有換行符）。服務器應該驗證並拒絕空命令。

### 有穩健性機制的行為（當前實作）

```c
// server.c:193-201
// 驗證命令不為空
if (strlen(command) == 0) {
    WARN_LOG(stderr, "Empty command received\n");
    fprintf(client_fp, "Error: Empty command\n");
    fflush(client_fp);
    fclose(client_fp);
    close(cfd);
    exit(0);
}
```

**測試方法：**
```bash
# 終端 1：啟動服務器
$ cd build && ./server

# 終端 2：發送空命令
$ echo "" | nc 127.0.0.1 9734
# 或
$ printf "\n" | nc 127.0.0.1 9734

# 服務器回應：
Error: Empty command
```

**執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
[WARN] Empty command received
# 發送錯誤訊息給客戶端
# 子進程正確退出
# 服務器繼續運行
```

**關鍵行為：**
- ✅ 驗證命令不為空
- ✅ 發送錯誤訊息給客戶端
- ✅ 正確清理資源並退出
- ✅ 服務器繼續運行

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
char command[256];
fgets(command, sizeof(command), client_fp);
if (strcmp(command, "SENDMAIL") == 0) {
    // 處理郵件
} else {
    // 處理系統資訊
}
// 如果 command 為空，會進入 else 分支，可能導致意外行為
```

**可能的執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
# 空命令被當作普通命令處理
# 可能導致意外的系統資訊請求
# 或邏輯錯誤
```

**問題：**
- ❌ 未驗證空命令
- ❌ 可能導致意外的行為
- ❌ 浪費資源處理無效請求

---

## 範例 9: 客戶端斷開連接檢測 ⭐ 容易測試

### 場景說明
客戶端可能在服務器讀取或寫入時突然斷開連接。服務器應該檢測並優雅處理。

### 有穩健性機制的行為（當前實作）

```c
// server.c:281-309
} else {
    // 如果沒有收到指令（可能是客戶端斷開或超時）
    if (ferror(client_fp)) {
        ERROR_LOG(stderr, "Error reading from client (client may have disconnected)\n");
    } else if (feof(client_fp)) {
        WARN_LOG(stderr, "Client closed connection before sending command\n");
    } else {
        WARN_LOG(stderr, "No command received (timeout or empty input), sending default system info\n");
    }
    
    // 只有在連接仍然有效時才發送回應
    if (!ferror(client_fp) && !feof(client_fp)) {
        // 發送回應
    }
}
```

**測試方法：**
```bash
# 終端 1：啟動服務器
$ cd build && ./server

# 終端 2：連接後立即斷開
$ (sleep 0.1; echo "SYSINFO") | nc 127.0.0.1 9734 &
$ sleep 0.2
$ killall nc  # 立即斷開

# 服務器日誌：
[WARN] Client closed connection before sending command
# 或
[ERROR] Error reading from client (client may have disconnected)
```

**執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
[WARN] Client closed connection before sending command
# 服務器檢測到斷開，不嘗試寫入
# 正確清理資源
# 服務器繼續運行
```

**關鍵行為：**
- ✅ 使用 `ferror()` 和 `feof()` 檢測連接狀態
- ✅ 只在連接有效時才寫入
- ✅ 正確清理資源
- ✅ 服務器繼續運行

### 沒有穩健性機制的行為（假設）

```c
// 假設的錯誤實作
FILE *client_fp = fdopen(cfd, "r+");
fgets(command, sizeof(command), client_fp);
// 如果客戶端已斷開，fgets 返回 NULL
// 但沒有檢查，繼續執行
fprintf(client_fp, "Response\n");  // 可能失敗或收到 SIGPIPE
```

**可能的執行結果：**
```
[INFO] Client connected from 127.0.0.1:54321
[DEBUG] Child process started (PID: 12345)
Broken pipe  # 收到 SIGPIPE 信號
# 如果沒有處理 SIGPIPE，程式可能崩潰
```

**問題：**
- ❌ 未檢測客戶端斷開
- ❌ 可能收到 SIGPIPE 導致崩潰
- ❌ 浪費資源嘗試寫入已關閉的連接

---

## 快速測試指南

### 使用自動化測試腳本

我們提供了一個自動化測試腳本，可以快速測試所有健壯性機制：

```bash
# 確保程序已編譯
cd build
cmake ..
make

# 運行測試腳本
cd ..
./test_robustness.sh
```

測試腳本會自動測試：
1. ✅ 端口已被占用
2. ✅ 客戶端突然斷開連接
3. ✅ 超長輸入處理
4. ✅ 客戶端連接超時
5. ✅ 空命令處理
6. ✅ 多個客戶端同時連接

### 手動測試方法

#### 測試 1: 端口占用（最容易測試）
```bash
# 終端 1
$ cd build && ./server

# 終端 2（應該失敗）
$ cd build && ./server
# 應該看到: bind() failed: Address already in use
```

#### 測試 2: 客戶端斷開（最容易測試）
```bash
# 終端 1
$ cd build && ./server

# 終端 2
$ nc 127.0.0.1 9734 &
$ sleep 0.1
$ killall nc
# 服務器應該繼續運行，不會崩潰
```

#### 測試 3: 超長輸入（最容易測試）
```bash
# 終端 1
$ cd build && ./server

# 終端 2
$ python3 -c "print('A' * 300)" | nc 127.0.0.1 9734
# 服務器應該記錄警告但繼續運行
```

---

## 總結

這些範例展示了穩健性機制的關鍵價值：

1. **優雅降級** - 單個操作失敗不影響整體服務
2. **資源清理** - 所有錯誤路徑都正確清理資源
3. **錯誤檢測** - 及時發現並記錄錯誤
4. **服務連續性** - Server 在各種錯誤情況下保持運行
5. **可維護性** - 詳細的錯誤日誌便於診斷問題
6. **超時處理** - 防止資源被無限期佔用
7. **輸入驗證** - 防止緩衝區溢出和無效輸入
8. **連接狀態檢測** - 優雅處理客戶端斷開

沒有這些機制時，程式可能在各種錯誤情況下崩潰、洩漏資源，或進入錯誤狀態，導致服務不可用。

### 最容易測試的場景（推薦）

1. ⭐⭐⭐ **端口占用** - 啟動兩個服務器，第二個應該失敗
2. ⭐⭐⭐ **客戶端斷開** - 使用 `nc` 連接後立即 `killall nc`
3. ⭐⭐ **超長輸入** - 發送超過 256 字節的命令
4. ⭐⭐ **空命令** - 發送只有換行符的命令
5. ⭐ **超時處理** - 連接後等待 30 秒不發送數據

