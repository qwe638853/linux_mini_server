# Robustness and Error Handling Mechanisms

本文件記錄程式中實作的各種異常和例外情況處理機制。

## 目錄

1. [系統呼叫錯誤處理](#系統呼叫錯誤處理)
2. [記憶體管理錯誤處理](#記憶體管理錯誤處理)
3. [檔案操作錯誤處理](#檔案操作錯誤處理)
4. [網路操作錯誤處理](#網路操作錯誤處理)
5. [信號處理](#信號處理)
6. [參數驗證](#參數驗證)
7. [外部 API 錯誤處理](#外部-api-錯誤處理)
8. [資源清理](#資源清理)
9. [錯誤日誌記錄](#錯誤日誌記錄)

---

## 系統呼叫錯誤處理

### Server (server.c)

#### Socket 操作
- **socket()** - 檢查返回值 < 0，失敗時記錄錯誤並返回
  ```c
  if (server_sockfd < 0) {
      ERROR_LOG(stderr, "socket() failed\n");
      perror("socket");
      return 1;
  }
  ```

- **inet_addr()** - 檢查返回值是否為 INADDR_NONE，失敗時記錄錯誤並返回
  ```c
  server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
  if(server_address.sin_addr.s_addr == INADDR_NONE){
      ERROR_LOG(stderr, "inet_addr() failed: invalid address\n");
      close(server_sockfd);
      return 1;
  }
  ```

- **bind()** - 檢查返回值 < 0，失敗時清理 socket 並返回
  ```c
  if (bind(server_sockfd, ...) < 0) {
      ERROR_LOG(stderr, "bind() failed\n");
      perror("bind");
      close(server_sockfd);
      return 1;
  }
  ```

- **listen()** - 檢查返回值 < 0，失敗時清理 socket 並返回
  ```c
  if (listen(server_sockfd, 10) < 0) {
      ERROR_LOG(stderr, "listen() failed\n");
      perror("listen");
      close(server_sockfd);
      return 1;
  }
  ```

- **accept()** - 檢查返回值 < 0，失敗時使用 `continue` 繼續等待下一個連接
  ```c
  if (cfd < 0) {
      ERROR_LOG(stderr, "accept() failed\n");
      perror("accept");
      continue;  // 不中斷服務，繼續等待
  }
  ```

- **fork()** - 檢查返回值 < 0，失敗時關閉客戶端 socket 並繼續
  ```c
  if (pid < 0) {
      ERROR_LOG(stderr, "fork() failed\n");
      perror("fork");
      close(cfd);
      continue;
  }
  ```

- **fdopen()** - 檢查返回值為 NULL，失敗時關閉 socket 並退出子進程
  ```c
  if (client_fp == NULL) {
      ERROR_LOG(stderr, "fdopen() failed\n");
      perror("fdopen");
      close(cfd);
      exit(1);
  }
  ```

#### 檔案 I/O 操作
- **fgets()** - 檢查緩衝區溢出（行太長）
  ```c
  if (fgets(command, sizeof(command), client_fp) != NULL) {
      // 檢查是否因為緩衝區太小而截斷（行太長）
      if(strlen(command) == sizeof(command) - 1 && command[sizeof(command) - 2] != '\n'){
          WARN_LOG(stderr, "Command line too long, may be truncated\n");
      }
  }
  ```

- **fprintf()** - 檢查返回值，失敗時記錄警告
  ```c
  if(fprintf(client_fp, "Command: %s\n", command) < 0 ||
     fprintf(client_fp, "To: %s\n", to) < 0){
      WARN_LOG(stderr, "Failed to write response to client\n");
  }
  ```

- **fflush()** - 檢查返回值，失敗時記錄警告
  ```c
  if(fflush(client_fp) != 0){
      WARN_LOG(stderr, "fflush() failed\n");
  }
  ```

- **fclose()** - 檢查返回值，失敗時記錄警告
  ```c
  if(fclose(client_fp) != 0){
      WARN_LOG(stderr, "fclose() failed\n");
  }
  ```

### Client (client.c)

#### Socket 操作
- **socket()** - 檢查返回值 < 0，失敗時記錄錯誤並退出
  ```c
  if (sockfd < 0) {
      ERROR_LOG(stderr, "socket() failed\n");
      perror("socket");
      exit(1);
  }
  ```

- **inet_addr()** - 檢查返回值是否為 INADDR_NONE，失敗時記錄錯誤並退出
  ```c
  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  if(address.sin_addr.s_addr == INADDR_NONE){
      ERROR_LOG(stderr, "inet_addr() failed: invalid address\n");
      close(sockfd);
      exit(1);
  }
  ```

- **connect()** - 檢查返回值 < 0，失敗時清理 socket 並退出
  ```c
  if (connect(sockfd, ...) < 0) {
      ERROR_LOG(stderr, "connect() failed\n");
      perror("connect");
      close(sockfd);
      exit(1);
  }
  ```

- **fdopen()** - 檢查返回值為 NULL，失敗時清理 socket 並退出
  ```c
  if (server_fp == NULL) {
      ERROR_LOG(stderr, "fdopen() failed\n");
      perror("fdopen");
      close(sockfd);
      exit(1);
  }
  ```

#### 檔案 I/O 操作
- **fprintf()** - 檢查返回值，失敗時記錄錯誤並退出
  ```c
  if(fprintf(server_fp, "SENDMAIL\n") < 0 ||
     fprintf(server_fp, "%s\n", to) < 0){
      ERROR_LOG(stderr, "Failed to send data to server\n");
      fclose(server_fp);
      exit(1);
  }
  ```

- **fflush()** - 檢查返回值，失敗時記錄錯誤並退出
  ```c
  if(fflush(server_fp) != 0){
      ERROR_LOG(stderr, "fflush() failed\n");
      fclose(server_fp);
      exit(1);
  }
  ```

- **ferror()** - 檢查檔案讀取錯誤
  ```c
  while (fgets(buffer, sizeof(buffer), server_fp) != NULL) {
      // 處理資料
  }
  if(ferror(server_fp)){
      ERROR_LOG(stderr, "Error reading from server\n");
  }
  ```

- **fclose()** - 檢查返回值，失敗時記錄警告
  ```c
  if(fclose(server_fp) != 0){
      WARN_LOG(stderr, "fclose() failed\n");
  }
  ```

### Sysinfo (sysinfo.c)

#### 系統資訊獲取
- **gethostname()** - 檢查返回值 < 0，失敗時記錄錯誤並返回 -1
  ```c
  if(gethostname(hostname, sizeof(hostname)) < 0){
      ERROR_LOG(fp, "gethostname() failed\n");
      perror("gethostname");
      return -1;
  }
  ```

- **time()** - 檢查返回值是否為 (time_t)-1，失敗時記錄錯誤並返回 -1
  ```c
  time_t now = time(NULL);
  if(now == (time_t)-1){
      ERROR_LOG(fp, "time() failed\n");
      perror("time");
      return -1;
  }
  ```

- **localtime()** - 檢查返回值為 NULL，失敗時記錄錯誤並返回 -1
  ```c
  struct tm *local_time = localtime(&now);
  if(local_time == NULL){
      ERROR_LOG(fp, "localtime() failed\n");
      perror("localtime");
      return -1;
  }
  ```

- **uname()** - 檢查返回值 < 0，失敗時記錄錯誤並返回 -1
  ```c
  if(uname(&name) < 0){
      ERROR_LOG(fp, "uname() failed\n");
      perror("uname");
      return -1;
  }
  ```

- **sysinfo()** - 檢查返回值 < 0，失敗時記錄錯誤並返回 -1。同時檢查除以零情況
  ```c
  if(sysinfo(&info) < 0){
      ERROR_LOG(fp, "sysinfo() failed\n");
      perror("sysinfo");
      return -1;
  }
  // 檢查除以零的情況
  if(info.totalram == 0){
      ERROR_LOG(fp, "sysinfo() returned zero total RAM\n");
      fprintf(fp, "Error: Invalid system information (zero total RAM)\n");
      return -1;
  }
  ```

- **getpwuid()** - 檢查返回值為 NULL，失敗時記錄錯誤並返回 -1
  ```c
  if(pw == NULL){
      ERROR_LOG(fp, "getpwuid() failed for UID: %d\n", uid);
      perror("getpwuid");
      return -1;
  }
  ```

- **statvfs()** - 檢查返回值 < 0，失敗時記錄錯誤並返回 -1。同時檢查除以零情況
  ```c
  if(statvfs("/", &info) < 0){
      ERROR_LOG(fp, "statvfs() failed for /\n");
      perror("statvfs");
      return -1;
  }
  // 檢查除以零的情況
  if(total_bytes == 0){
      ERROR_LOG(fp, "statvfs() returned zero total disk space\n");
      fprintf(fp, "Error: Invalid filesystem information (zero total space)\n");
      return -1;
  }
  ```

- **getifaddrs()** - 檢查返回值 < 0，失敗時記錄錯誤並返回 -1
  ```c
  if(getifaddrs(&ifaddr) < 0){
      ERROR_LOG(fp, "getifaddrs() failed\n");
      perror("getifaddrs");
      return -1;
  }
  ```

- **socket()** - 在網路介面處理中，檢查返回值 < 0，失敗時記錄警告並繼續處理下一個介面
  ```c
  if(fd < 0){
      WARN_LOG(fp, "  Failed to create socket for interface %s\n", ifa->ifa_name);
      continue;
  }
  ```

- **ioctl()** - 檢查返回值，失敗時記錄警告但不中斷處理
  ```c
  if(ioctl(fd, SIOCGIFHWADDR, &ifr) == 0){
      // 成功處理
  } else {
      WARN_LOG(fp, "  Failed to get MAC address for %s\n", ifa->ifa_name);
  }
  ```

- **strncpy()** - 確保字串以 null 終止
  ```c
  strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ-1);
  ifr.ifr_name[IFNAMSIZ-1] = '\0';  // 確保 null-terminated
  ```

---

## 記憶體管理錯誤處理

### SMTP (smtp.c)

#### 記憶體分配
- **strdup()** - 檢查返回值為 NULL
  ```c
  char *empty = strdup("");
  if(empty == NULL){
      ERROR_LOG(stderr, "json_escape: strdup() failed for empty string\n");
  }
  ```

- **malloc()** - 在 json_escape() 中檢查返回值為 NULL
  ```c
  char *escaped = malloc(cap);
  if(escaped == NULL){
      ERROR_LOG(stderr, "json_escape: malloc() failed\n");
      perror("malloc");
      return NULL;
  }
  ```

- **malloc()** - 在 send_email() 中檢查 payload 分配失敗
  ```c
  char *payload = (char *)malloc(payload_size);
  if (payload == NULL) {
      ERROR_LOG(stderr, "malloc() failed for payload\n");
      perror("malloc");
      free(escaped_subject);
      free(escaped_body);
      return -1;
  }
  ```

#### 記憶體溢出檢查
- **字串長度溢出檢查** - 檢查相加是否超過 SIZE_MAX
  ```c
  const size_t MAX_SAFE_SIZE = SIZE_MAX - 256;
  if(subject_len > MAX_SAFE_SIZE || body_len > MAX_SAFE_SIZE || 
     recipient_len > MAX_SAFE_SIZE || from_len > MAX_SAFE_SIZE){
      ERROR_LOG(stderr, "String length too large, potential overflow\n");
      free(escaped_subject);
      free(escaped_body);
      return -1;
  }
  size_t payload_size = subject_len + body_len + recipient_len + from_len + 256;
  // 再次檢查相加後的結果
  if(payload_size < subject_len + body_len + recipient_len + from_len){
      ERROR_LOG(stderr, "Payload size calculation overflow\n");
      return -1;
  }
  ```

#### 記憶體清理
- 所有失敗路徑都正確釋放已分配的記憶體
- 使用 `free()` 釋放 `malloc()` 分配的記憶體
- 在錯誤情況下確保資源清理

---

## 檔案操作錯誤處理

### Environment Variables (env.c)

#### 檔案開啟
- **fopen()** - 檢查返回值為 NULL，失敗時記錄錯誤並返回
  ```c
  FILE *f = fopen(filename, "r");
  if (!f) {
      ERROR_LOG(stderr, "load_env_file: Failed to open file '%s'\n", filename);
      fprintf(stderr, "Error: Failed to open file '%s': ", filename);
      perror(NULL);
      return -1;
  }
  ```

#### 檔案讀取錯誤
- **fgets()** - 檢查緩衝區溢出（行太長）
  ```c
  while (fgets(line, sizeof(line), f) != NULL) {
      // 檢查是否因為緩衝區太小而截斷（行太長）
      if(strlen(line) == sizeof(line) - 1 && line[sizeof(line) - 2] != '\n'){
          WARN_LOG(stderr, "load_env_file: Line %d too long, may be truncated\n", line_num);
      }
  }
  ```

- **ferror()** - 檢查檔案讀取錯誤
  ```c
  if (ferror(f)) {
      ERROR_LOG(stderr, "load_env_file: Error reading file '%s'\n", filename);
      fprintf(stderr, "Error: Error reading file '%s'\n", filename);
      fclose(f);
      return -1;
  }
  ```

#### 檔案格式驗證
- 檢查行格式（缺少等號）- 記錄警告並跳過
  ```c
  if (eq == NULL) {
      WARN_LOG(stderr, "load_env_file: Line %d: No '=' found, skipping\n", line_num);
      fprintf(stderr, "Warning: Line %d: No '=' found, skipping\n", line_num);
      continue;
  }
  ```

- 檢查空鍵 - 記錄警告並跳過
  ```c
  if (key[0] == '\0') {
      WARN_LOG(stderr, "load_env_file: Line %d: Empty key, skipping\n", line_num);
      fprintf(stderr, "Warning: Line %d: Empty key, skipping\n", line_num);
      continue;
  }
  ```

---

## 網路操作錯誤處理

### Server (server.c)

#### 讀取操作
- **read_line()** - 檢查 read() 返回值 <= 0，正確處理 EOF 和錯誤
  ```c
  ssize_t r = read(fd, &c, 1);
  if (r <= 0) {
      DEBUG_LOG(stderr, "read_line: read returned %zd\n", r);
      break;
  }
  ```

#### 客戶端連接處理
- 使用 `continue` 在 accept() 或 fork() 失敗時繼續服務，不中斷主循環

---

## 信號處理

### Server (server.c)

#### SIGCHLD 處理
- 設定信號處理器避免殭屍進程，檢查 sigaction() 返回值
  ```c
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
  if(sigaction(SIGCHLD, &sa, NULL) < 0){
      WARN_LOG(stderr, "sigaction(SIGCHLD) failed\n");
      perror("sigaction");
      // 非致命錯誤，繼續執行
  }
  ```

#### SIGPIPE 處理
- 忽略 SIGPIPE 信號，避免寫入已關閉的 socket 時程式崩潰，檢查 sigaction() 返回值
  ```c
  struct sigaction sa_pipe;
  memset(&sa_pipe, 0, sizeof(sa_pipe));
  sa_pipe.sa_handler = SIG_IGN;
  if(sigaction(SIGPIPE, &sa_pipe, NULL) < 0){
      WARN_LOG(stderr, "sigaction(SIGPIPE) failed\n");
      perror("sigaction");
      // 非致命錯誤，繼續執行
  }
  ```

---

## 參數驗證

### Environment Variables (env.c)

#### NULL 檢查
- 檢查 filename 參數是否為 NULL
  ```c
  if (filename == NULL) {
      ERROR_LOG(stderr, "load_env_file: filename cannot be NULL\n");
      fprintf(stderr, "Error: filename cannot be NULL\n");
      return -1;
  }
  ```

#### 空字串檢查
- 檢查 filename 是否為空字串
  ```c
  if (filename[0] == '\0') {
      ERROR_LOG(stderr, "load_env_file: filename cannot be empty\n");
      fprintf(stderr, "Error: filename cannot be empty\n");
      return -1;
  }
  ```

### SMTP (smtp.c)

#### 參數驗證
- **NULL 檢查** - 檢查 recipient, subject, body 是否為 NULL
  ```c
  if(recipient == NULL || subject == NULL || body == NULL){
      ERROR_LOG(stderr, "send_email: NULL parameter (recipient, subject, or body)\n");
      fprintf(stderr, "Error: recipient, subject, and body cannot be NULL\n");
      return -1;
  }
  ```

#### 環境變數檢查
- 檢查必要的環境變數是否存在
  ```c
  if (sendgrid_api_key == NULL || from_email == NULL) {
      ERROR_LOG(stderr, "SENDGRID_API_KEY or SENDGRID_FROM not set in .env file\n");
      fprintf(stderr, "Error: SENDGRID_API_KEY or SENDGRID_FROM not set in .env file\n");
      return -1;
  }
  ```

#### .env 檔案查找
- 嘗試多個路徑查找 .env 檔案，失敗時記錄錯誤
  ```c
  const char *env_paths[] = { "../../.env", "../.env", ".env" };
  int found = 0;
  for (size_t i = 0; i < sizeof(env_paths)/sizeof(env_paths[0]); ++i) {
      if (load_env_file(env_paths[i]) == 0) {
          found = 1;
          break;
      }
  }
  if (!found) {
      ERROR_LOG(stderr, "Cannot find .env file...\n");
      return -1;
  }
  ```

---

## 外部 API 錯誤處理

### SMTP (smtp.c)

#### CURL 初始化
- **curl_easy_init()** - 檢查返回值為 NULL
  ```c
  CURL *curl = curl_easy_init();
  if(curl == NULL){
      ERROR_LOG(stderr, "curl_easy_init() failed\n");
      perror("curl_easy_init");
      free(payload);
      return -1;
  }
  ```

#### CURL 執行錯誤
- **curl_easy_perform()** - 檢查 CURLcode 返回值
  ```c
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK){
      ERROR_LOG(stderr, "curl_easy_perform failed: %s\n", errbuf);
      fprintf(stderr, "curl_easy_perform failed: %s\n", errbuf);
  }
  ```

- **curl_easy_getinfo()** - 檢查返回值，失敗時使用預設值
  ```c
  CURLcode getinfo_res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  if(getinfo_res != CURLE_OK){
      WARN_LOG(stderr, "curl_easy_getinfo() failed, using default http_code 0\n");
      http_code = 0;
  }
  ```

- **curl_slist_append()** - 檢查返回值，失敗時清理資源並返回
  ```c
  headers = curl_slist_append(headers, auth_header);
  if(headers == NULL){
      ERROR_LOG(stderr, "curl_slist_append() failed for auth header\n");
      curl_easy_cleanup(curl);
      free(payload);
      return -1;
  }
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if(headers == NULL){
      ERROR_LOG(stderr, "curl_slist_append() failed for Content-Type header\n");
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      free(payload);
      return -1;
  }
  ```

#### 字串格式化安全
- **snprintf()** - 檢查返回值，避免截斷或錯誤
  ```c
  int snprintf_result = snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", sendgrid_api_key);
  if(snprintf_result < 0 || snprintf_result >= (int)sizeof(auth_header)){
      ERROR_LOG(stderr, "snprintf() failed for auth header (truncated or error)\n");
      curl_easy_cleanup(curl);
      free(payload);
      return -1;
  }
  ```

- **sprintf() → snprintf()** - 在 json_escape() 中使用安全的 snprintf
  ```c
  int written = snprintf(p, 7, "\\u%04X", c);
  if (written < 0 || written >= 7) {
      // 如果 snprintf 失敗或截斷，釋放記憶體並返回錯誤
      free(escaped);
      return NULL;
  }
  p += written;
  ```

#### HTTP 回應碼檢查
- 檢查 SendGrid API 回應碼是否為 202（接受）
  ```c
  if (http_code != 202) {
      ERROR_LOG(stderr, "SendGrid API returned error: %d\n", http_code);
      fprintf(stderr, "SendGrid API returned error: %d\n", http_code);
      return -1;
  }
  ```

#### CURL 超時設定
- 設定連接和執行超時，避免長時間等待
  ```c
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  ```

---

## 資源清理

### Server (server.c)

#### Socket 清理
- 在所有錯誤路徑中正確關閉 socket
  ```c
  close(server_sockfd);  // bind/listen 失敗時
  close(cfd);            // fork 失敗時
  ```

#### 檔案流清理
- 正確關閉檔案流和 socket
  ```c
  fflush(client_fp);
  fclose(client_fp);
  close(cfd);
  ```

#### 子進程清理
- 子進程正確關閉 server socket（不需要）
  ```c
  close(server_sockfd);  // 子進程中
  ```

### SMTP (smtp.c)

#### CURL 資源清理
- 確保所有 CURL 資源都被正確釋放
  ```c
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  free(payload);
  ```

#### 記憶體清理
- 在錯誤路徑中釋放已分配的記憶體
  ```c
  free(escaped_subject);
  free(escaped_body);
  ```

### Environment Variables (env.c)

#### 檔案清理
- 在所有路徑（成功和失敗）中關閉檔案
  ```c
  fclose(f);
  ```

### Sysinfo (sysinfo.c)

#### 網路資源清理
- 釋放 getifaddrs() 分配的記憶體
  ```c
  freeifaddrs(ifaddr);
  ```

- 關閉每個介面處理中創建的 socket
  ```c
  close(fd);
  ```

---

## 錯誤日誌記錄

### 分級錯誤日誌系統

程式使用四級錯誤日誌系統：

1. **ERROR_LOG** - 嚴重錯誤（總是輸出）
   - 系統呼叫失敗
   - 記憶體分配失敗
   - 檔案操作失敗
   - API 錯誤

2. **WARN_LOG** - 警告（需要編譯時啟用 DEBUG）
   - 非致命但需要注意的問題
   - 格式錯誤但可跳過的情況
   - 部分操作失敗但不影響整體

3. **INFO_LOG** - 資訊（需要編譯時啟用 DEBUG）
   - 函數進入點
   - 關鍵操作成功
   - 執行摘要

4. **DEBUG_LOG** - 除錯資訊（需要編譯時啟用 DEBUG）
   - 詳細執行過程
   - 內部狀態資訊

### 錯誤記錄位置

所有錯誤都使用以下方式記錄：
- `ERROR_LOG()` / `WARN_LOG()` - 記錄到日誌系統
- `perror()` - 輸出系統錯誤訊息
- `fprintf(stderr, ...)` - 輸出自訂錯誤訊息

---

## 異常情況處理總結

### 已實作的機制

✅ **系統呼叫錯誤檢查** - 所有系統呼叫都檢查返回值（socket, bind, listen, accept, fork, gethostname, time, localtime, uname, sysinfo, getpwuid, statvfs, getifaddrs, ioctl, sigaction, inet_addr）  
✅ **記憶體分配失敗處理** - 所有 malloc() 和 strdup() 都檢查返回值  
✅ **記憶體溢出檢查** - 檢查字串長度相加是否超過 SIZE_MAX，防止整數溢出  
✅ **檔案操作錯誤處理** - fopen(), fgets(), ferror(), fprintf(), fflush(), fclose() 都有錯誤處理  
✅ **緩衝區溢出防護** - fgets() 後檢查行是否太長被截斷，strncpy() 確保 null-terminated  
✅ **網路操作錯誤處理** - socket, bind, listen, accept, connect, inet_addr 都有錯誤處理  
✅ **檔案 I/O 錯誤處理** - fprintf(), fflush(), fclose() 都檢查返回值，ferror() 檢查讀取錯誤  
✅ **信號處理** - SIGCHLD 和 SIGPIPE 都有適當處理，sigaction() 檢查返回值  
✅ **參數驗證** - NULL 檢查、空字串檢查（filename, recipient, subject, body）  
✅ **數學運算安全** - 除以零檢查（記憶體使用率、磁碟使用率計算）  
✅ **字串格式化安全** - 使用 snprintf() 替代 sprintf()，檢查返回值避免截斷  
✅ **外部 API 錯誤處理** - CURL 錯誤（curl_easy_init, curl_easy_perform, curl_easy_getinfo, curl_slist_append）和 HTTP 回應碼檢查  
✅ **資源清理** - 所有分配的資源都在錯誤路徑中正確清理  
✅ **錯誤日誌記錄** - 使用分級日誌系統記錄所有錯誤  
✅ **優雅降級** - 部分失敗不影響整體服務（如網路介面處理、信號處理失敗）  

### 設計原則

1. **Fail Fast** - 致命錯誤立即返回，不繼續執行
2. **資源清理** - 所有錯誤路徑都正確清理已分配的資源
3. **優雅降級** - 非致命錯誤記錄警告但不中斷服務
4. **詳細日誌** - 所有錯誤都記錄詳細資訊，便於除錯
5. **用戶友好** - 錯誤訊息清楚說明問題和可能的解決方案

---

## 改進建議

### 可考慮的增強

1. **重試機制** - 對於暫時性錯誤（如網路超時）可以實作重試
2. **錯誤恢復** - 某些錯誤可以嘗試恢復（如自動尋找替代配置）
3. **更詳細的錯誤碼** - 使用錯誤碼系統而非簡單的 -1 返回值
4. **統計資訊** - 記錄錯誤發生頻率，用於監控
5. **健康檢查** - 定期檢查系統狀態，提前發現問題

---

最後更新：2024年12月

## 新增錯誤處理機制（2024年12月更新）

### 系統呼叫擴展檢查
- ✅ `gethostname()` - 檢查返回值
- ✅ `time()` - 檢查返回值是否為 (time_t)-1
- ✅ `localtime()` - 檢查返回值為 NULL
- ✅ `inet_addr()` - 檢查返回值為 INADDR_NONE（server.c 和 client.c）
- ✅ `sigaction()` - 檢查返回值（SIGCHLD 和 SIGPIPE）

### 字串與記憶體安全
- ✅ `strncpy()` - 確保 null-terminated
- ✅ `sprintf()` → `snprintf()` - 在 json_escape() 中使用安全的 snprintf
- ✅ `snprintf()` - 檢查返回值，避免截斷或錯誤（多處）
- ✅ `strdup()` - 檢查返回值
- ✅ 記憶體溢出檢查 - 使用 SIZE_MAX 檢查字串長度相加是否溢出

### 檔案 I/O 擴展檢查
- ✅ `fprintf()` - 檢查返回值（網路寫入時）
- ✅ `fflush()` - 檢查返回值
- ✅ `fclose()` - 檢查返回值
- ✅ `fgets()` - 檢查緩衝區溢出（行太長警告）
- ✅ `ferror()` - 檢查檔案讀取錯誤（client.c）

### 數學運算安全
- ✅ 除以零檢查 - `get_memory_usage()` 檢查 totalram 是否為 0
- ✅ 除以零檢查 - `get_disk_info()` 檢查 total_bytes 是否為 0

### 外部 API 擴展檢查
- ✅ `curl_slist_append()` - 檢查返回值（兩次調用）
- ✅ `curl_easy_getinfo()` - 檢查返回值，失敗時使用預設值

### 參數驗證擴展
- ✅ `send_email()` - 檢查 recipient, subject, body 是否為 NULL

