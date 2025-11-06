# Client vs nc 的差異說明

## 問題

用戶發現 `./client` 和 `nc` 在預設行為上的差異，以及註釋與實際行為的不一致。

## 行為對比

### 使用 `./client`（不帶參數）

```bash
$ ./client
```

**實際行為：**
- Client 會發送 `"SYSINFO\n"` 指令
- Server 收到後進入第 251 行的 `else` 分支（非 SENDMAIL 指令）
- **立即**發送系統資訊回應
- 日誌顯示：`[INFO] Processing system info request`

**代碼路徑：**
```
client.c:165 → 發送 "SYSINFO\n"
server.c:183 → fgets() 成功讀取到 "SYSINFO\n"
server.c:207 → strcmp() 不是 "SENDMAIL"
server.c:251 → 進入 else 分支，發送系統資訊
```

### 使用 `nc`（不發送任何東西）

```bash
$ nc 127.0.0.1 9734
# 不輸入任何內容，等待...
```

**實際行為：**
- `nc` 不發送任何數據
- Server 的 `fgets()` 會阻塞等待（最多 30 秒超時）
- 30 秒後超時，`fgets()` 返回 `NULL`
- Server 進入第 282 行的 `else` 分支（fgets 返回 NULL）
- 發送系統資訊回應
- 日誌顯示：`[WARN] No command received (timeout or empty input), sending default system info`

**代碼路徑：**
```
server.c:183 → fgets() 等待數據（30秒超時）
server.c:282 → fgets() 返回 NULL（超時或 EOF）
server.c:289 → 記錄超時警告
server.c:293 → 檢查連接有效後發送系統資訊
```

## 關鍵差異

| 項目 | `./client` | `nc`（不發送） |
|------|-----------|---------------|
| **發送數據** | ✅ 發送 `"SYSINFO\n"` | ❌ 不發送任何東西 |
| **等待時間** | 立即回應 | 最多等待 30 秒（超時） |
| **Server 日誌** | `Processing system info request` | `No command received (timeout...)` |
| **代碼路徑** | 第 251 行 else 分支 | 第 282 行 else 分支 |
| **用戶體驗** | 快速回應 | 需要等待超時 |

## 為什麼 Client 要發送 "SYSINFO"？

### 優點
1. **立即回應** - 不需要等待 30 秒超時
2. **明確意圖** - 清楚表達要獲取系統資訊
3. **更好的用戶體驗** - 快速獲得結果

### 缺點
1. **與註釋不一致** - 註釋說「不發送任何指令」，但實際發送了
2. **與 nc 行為不同** - 可能造成混淆

## 如果 Client 真的不發送任何東西會怎樣？

如果修改 client.c，讓它真的不發送任何數據：

```c
// 假設的修改
} else {
    // 真的不發送任何東西
    // 不調用 fprintf()
}
```

**結果：**
- Server 會等待 30 秒超時
- 用戶體驗變差（需要等待）
- 但行為與 `nc` 一致

## 建議

### 選項 1: 保持現狀（推薦）
- 修正註釋，說明實際行為
- 保持快速回應的優點
- 在文檔中說明與 `nc` 的差異

### 選項 2: 讓 Client 真的不發送
- 修改代碼，真的不發送任何數據
- 與 `nc` 行為一致
- 但用戶體驗變差（需要等待超時）

### 選項 3: 添加選項
- 添加 `--no-send` 選項，讓用戶選擇是否發送
- 預設行為：發送 `SYSINFO`（快速）
- 可選行為：不發送（測試超時機制）

## 當前實作（已修正註釋）

當前實作採用**選項 1**：
- Client 預設發送 `"SYSINFO\n"` 以獲得快速回應
- 註釋已修正，說明實際行為
- 在文檔中說明與 `nc` 的差異

這樣既保持了良好的用戶體驗，又明確了行為差異。

