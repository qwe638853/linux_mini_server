# Server 判斷邏輯說明

本文檔詳細說明 Server 如何判斷和處理不同的客戶端請求。

## 整體流程圖

```
客戶端連接
    ↓
fork() 創建子進程
    ↓
設置 30 秒讀取超時
    ↓
fdopen() 轉換 socket 為 FILE*
    ↓
fgets() 讀取命令
    ↓
    ├─→ fgets() 成功 (返回 != NULL)
    │       ↓
    │   檢查命令長度（是否截斷）
    │       ↓
    │   移除換行符
    │       ↓
    │   檢查命令是否為空
    │       ├─→ 空命令 → 發送錯誤訊息並退出
    │       │
    │       └─→ 非空命令
    │               ↓
    │           strcmp(command, "SENDMAIL")
    │               ├─→ 是 "SENDMAIL" → 處理郵件發送
    │               │       ↓
    │               │   讀取 to, subject, body
    │               │       ↓
    │               │   調用 send_email()
    │               │       ↓
    │               │   發送結果給客戶端
    │               │       ↓
    │               │   退出
    │               │
    │               └─→ 其他命令
    │                       ↓
    │                   是 "SYSINFO"？
    │                       ├─→ 是 → 明確處理系統資訊
    │                       │       ↓
    │                       │   日誌：Processing SYSINFO command
    │                       │       ↓
    │                       │   發送系統資訊
    │                       │       ↓
    │                       │   退出
    │                       │
    │                       └─→ 否 → 未知命令
    │                               ↓
    │                           日誌：Unknown command
    │                               ↓
    │                           發送系統資訊（預設行為）
    │                               ↓
    │                           退出
    │
    └─→ fgets() 失敗 (返回 NULL)
            ↓
        檢查錯誤類型
            ├─→ ferror() → 讀取錯誤（客戶端可能斷開）
            ├─→ feof() → 客戶端關閉連接
            └─→ 其他（超時或空輸入）
                    ↓
            檢查連接是否仍然有效
                    ├─→ 有效 → 發送系統資訊
                    └─→ 無效 → 不發送，直接清理
                            ↓
                        退出
```

## 詳細判斷邏輯

### 第一層判斷：fgets() 是否成功

```c
if (fgets(command, sizeof(command), client_fp) != NULL) {
    // 路徑 A: 成功讀取到數據
} else {
    // 路徑 B: 讀取失敗（NULL）
}
```

#### 路徑 A: fgets() 成功（讀取到數據）

**情況 1: 超長輸入（緩衝區溢出風險）**
```c
if(strlen(command) == sizeof(command) - 1 && command[sizeof(command) - 2] != '\n'){
    // 命令太長，被截斷
    // 清空輸入緩衝區
}
```

**情況 2: 空命令（只有換行符）**
```c
if (strlen(command) == 0) {
    // 發送錯誤訊息
    // 退出
}
```

**情況 3: 正常命令**
```c
// 根據命令類型處理
if (strcmp(command, "SENDMAIL") == 0) {
    // 處理郵件發送
} else if (strcmp(command, "SYSINFO") == 0) {
    // 明確處理 SYSINFO 命令
} else {
    // 處理其他未知命令（預設：發送系統資訊）
}
```

#### 路徑 B: fgets() 失敗（返回 NULL）

**判斷失敗原因：**

```c
if (ferror(client_fp)) {
    // 情況 1: 讀取錯誤（客戶端可能突然斷開）
} else if (feof(client_fp)) {
    // 情況 2: 客戶端關閉連接（EOF）
} else {
    // 情況 3: 超時或空輸入
}
```

**決定是否發送回應：**

```c
if (!ferror(client_fp) && !feof(client_fp)) {
    // 連接仍然有效 → 發送系統資訊
} else {
    // 連接無效 → 不發送，直接清理
}
```

## 具體判斷條件

### 1. 命令類型判斷

| 條件 | 行為 | 日誌訊息 |
|------|------|---------|
| `strcmp(command, "SENDMAIL") == 0` | 處理郵件發送 | `[INFO] Processing SENDMAIL command` |
| `strcmp(command, "SYSINFO") == 0` | 發送系統資訊 | `[INFO] Processing SYSINFO command` |
| 其他任何命令（如 "TEST" 等） | 發送系統資訊（預設行為） | `[WARN] Unknown command: XXX, sending system info as default` |

**重要：** Server 明確識別兩個命令：
- `"SENDMAIL"` - 處理郵件發送
- `"SYSINFO"` - 明確處理系統資訊請求
- 其他命令 - 作為未知命令處理，但仍發送系統資訊作為預設行為

### 2. 空命令判斷

```c
// 移除換行符後檢查
command[strcspn(command, "\r\n")] = '\0';
if (strlen(command) == 0) {
    // 空命令 → 發送錯誤並退出
}
```

**觸發情況：**
- 客戶端只發送換行符：`"\n"` 或 `"\r\n"`
- 客戶端發送空字串：`""`

### 3. 連接狀態判斷

```c
// 檢查三種狀態
ferror(client_fp)  // 讀取錯誤
feof(client_fp)     // 文件結束（客戶端關閉）
// 兩者都為 false → 連接有效
```

**判斷邏輯：**

| ferror() | feof() | 狀態 | 行為 |
|----------|--------|------|------|
| true | - | 讀取錯誤 | 不發送回應，記錄錯誤 |
| - | true | 客戶端關閉 | 不發送回應，記錄警告 |
| false | false | 連接有效 | 發送系統資訊 |

### 4. 超時判斷

超時由 `setsockopt(SO_RCVTIMEO)` 設置（30秒）。當超時發生時：
- `fgets()` 返回 `NULL`
- `ferror()` 和 `feof()` 都為 `false`
- 進入路徑 B 的 `else` 分支
- 連接仍然有效，發送系統資訊

## 實際案例

### 案例 1: `./client SENDMAIL`

```
1. fgets() 成功讀取 "SENDMAIL\n"
2. 移除換行符 → "SENDMAIL"
3. strcmp("SENDMAIL", "SENDMAIL") == 0 → true
4. 進入郵件處理分支
5. 讀取 to, subject, body
6. 調用 send_email()
7. 發送結果
8. 退出
```

### 案例 2: `./client` 或 `./client SYSINFO`

```
1. fgets() 成功讀取 "SYSINFO\n"
2. 移除換行符 → "SYSINFO"
3. strcmp("SYSINFO", "SENDMAIL") == 0 → false
4. strcmp("SYSINFO", "SYSINFO") == 0 → true
5. 進入 SYSINFO 分支（明確處理）
6. 日誌：`[INFO] Processing SYSINFO command`
7. 發送系統資訊
8. 退出
```

### 案例 2b: `./client TEST`（未知命令）

```
1. fgets() 成功讀取 "TEST\n"
2. 移除換行符 → "TEST"
3. strcmp("TEST", "SENDMAIL") == 0 → false
4. strcmp("TEST", "SYSINFO") == 0 → false
5. 進入 else 分支（未知命令）
6. 日誌：`[WARN] Unknown command: TEST, sending system info as default`
7. 發送系統資訊（預設行為）
8. 退出
```

### 案例 3: `nc` 連接但不發送數據

```
1. fgets() 等待數據（最多 30 秒）
2. 30 秒後超時，fgets() 返回 NULL
3. ferror() == false, feof() == false
4. 進入路徑 B 的 else 分支
5. 記錄超時警告
6. 連接有效 → 發送系統資訊
7. 退出
```

### 案例 4: 客戶端連接後立即斷開

```
1. fgets() 嘗試讀取
2. 客戶端已斷開，fgets() 返回 NULL
3. feof() == true（或 ferror() == true）
4. 進入路徑 B
5. 記錄警告/錯誤
6. 連接無效 → 不發送回應
7. 清理資源並退出
```

### 案例 5: 發送空命令

```
1. fgets() 成功讀取 "\n"
2. 移除換行符 → ""
3. strlen("") == 0 → true
4. 發送 "Error: Empty command"
5. 退出（不發送系統資訊）
```

## 關鍵判斷點總結

1. **fgets() 返回值** - 決定進入路徑 A 還是路徑 B
2. **命令是否為空** - 空命令直接拒絕
3. **命令是否為 "SENDMAIL"** - 決定處理郵件
4. **命令是否為 "SYSINFO"** - 明確處理系統資訊請求
5. **其他命令** - 作為未知命令處理，但仍發送系統資訊
6. **連接狀態** - 決定是否發送回應
7. **錯誤類型** - 區分不同類型的失敗原因

## 設計特點

1. **明確的命令識別** - 明確識別 SENDMAIL 和 SYSINFO 兩個命令
2. **預設行為友好** - 未知命令仍會獲得系統資訊（預設行為）
3. **清晰的日誌** - 不同命令類型有不同的日誌訊息
4. **超時保護** - 30 秒超時防止資源被無限期佔用
5. **錯誤檢測** - 區分不同類型的錯誤（斷開、超時、空命令）
6. **資源清理** - 所有路徑都正確清理資源
7. **優雅降級** - 連接無效時不嘗試發送，避免錯誤

