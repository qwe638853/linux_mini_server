# 測試沒有保護機制的版本

要測試 `dd if=/dev/zero bs=1M count=100 | nc 127.0.0.1 9734` 在沒有保護機制時的行為，需要註解以下行並恢復原本的 `fgets()` 邏輯。

## 需要註解的行

在 `server.c` 中，註解掉以下行（227-256行）：

```c
// 限制讀取數據量，防止大量數據攻擊
// 使用 read() 直接讀取，最多讀取 256 字節（命令緩衝區大小）
char temp_buf[256];
ssize_t bytes_read = read(cfd, temp_buf, sizeof(temp_buf) - 1);
if (bytes_read <= 0) {
    WARN_LOG(stderr, "Failed to read command or connection closed\n");
    cleanup_and_exit(client_fp, cfd);
}
temp_buf[bytes_read] = '\0';

// 檢查是否有換行符
char *newline = strchr(temp_buf, '\n');

if (newline == NULL && bytes_read == sizeof(temp_buf) - 1) {
    // 沒有換行符且緩衝區滿了，可能是大量數據攻擊
    WARN_LOG(stderr, "Large data stream detected without newline, closing connection\n");
    cleanup_and_exit(client_fp, cfd);
}

// 將數據複製到 command 緩衝區
if (newline != NULL) {
    *newline = '\0';
}
// 移除回車符
char *carriage = strchr(temp_buf, '\r');
if (carriage != NULL) {
    *carriage = '\0';
}
strncpy(command, temp_buf, sizeof(command) - 1);
command[sizeof(command) - 1] = '\0';

// 讀取指令（socket 可讀，可以安全調用 fgets）
if (strlen(command) > 0) {
```

## 需要恢復的代碼

在註解掉上述代碼後，在相同位置（第258行之後）添加原本的 `fgets()` 邏輯：

```c
// 讀取指令（socket 可讀，可以安全調用 fgets）
if (fgets(command, sizeof(command), client_fp) != NULL) {
    // 檢查是否因為緩衝區太小而截斷（行太長）
    if(strlen(command) == sizeof(command) - 1 && command[sizeof(command) - 2] != '\n'){
        WARN_LOG(stderr, "Command line too long, may be truncated\n");
        // 清空輸入緩衝區，避免後續讀取錯誤
        int c;
        while ((c = fgetc(client_fp)) != EOF && c != '\n');
    }
    // 移除換行符
    command[strcspn(command, "\r\n")] = '\0';
    
    // 驗證命令不為空
    if (strlen(command) == 0) {
        WARN_LOG(stderr, "Empty command received\n");
        fprintf(client_fp, "Error: Empty command\n");
        fflush(client_fp);
        fclose(client_fp);
        close(cfd);
        exit(0);
    }
    
    INFO_LOG(stderr, "Received command: %s\n", command);
```

同時，需要修改 SENDMAIL 部分的讀取邏輯（265-276行），將 `read_line()` 改回 `fgets()`：

```c
// 讀取收件人
if (fgets(to, sizeof(to), client_fp) != NULL) {
    to[strcspn(to, "\r\n")] = '\0';
    DEBUG_LOG(stderr, "To: %s\n", to);
}
// 讀取主旨
if (fgets(subject, sizeof(subject), client_fp) != NULL) {
    subject[strcspn(subject, "\r\n")] = '\0';
    DEBUG_LOG(stderr, "Subject: %s\n", subject);
}
// 讀取內容
if (fgets(body, sizeof(body), client_fp) != NULL) {
    body[strcspn(body, "\r\n")] = '\0';
    DEBUG_LOG(stderr, "Body length: %zu\n", strlen(body));
}
```

最後，需要恢復原本的錯誤處理邏輯（306-310行），將：

```c
} else {
    // 如果沒有收到指令（可能是客戶端斷開或超時）
    WARN_LOG(stderr, "No command received: empty command or connection issue\n");
    cleanup_and_exit(client_fp, cfd);
}
```

改為：

```c
} else {
    // 如果沒有收到指令（可能是客戶端斷開或超時）
    if (ferror(client_fp)) {
        WARN_LOG(stderr, "No command received: error reading from client (client may have disconnected)\n");
    } else if (feof(client_fp)) {
        WARN_LOG(stderr, "No command received: client closed connection before sending command\n");
    } else {
        WARN_LOG(stderr, "No command received: timeout or empty input\n");
    }
    
    // 不發送系統資訊，直接清理並退出
    cleanup_and_exit(client_fp, cfd);
}
```

## 快速測試步驟

1. 註解掉保護機制代碼（227-256行）
2. 恢復原本的 `fgets()` 邏輯
3. 重新編譯：`cd build && make`
4. 啟動服務器：`./build/bin/server`
5. 在另一個終端執行：`dd if=/dev/zero bs=1M count=100 | nc 127.0.0.1 9734`
6. 觀察服務器的資源使用情況（內存、CPU）

## 預期觀察結果

- 服務器進程內存使用持續增加
- CPU 使用率可能升高
- 連接保持 ESTABLISHED 狀態
- 服務器可能無法響應其他客戶端

