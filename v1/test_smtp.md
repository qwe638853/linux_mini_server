# SMTP 測試說明

## 前置需求

1. 安裝 libcurl 開發庫：
```bash
sudo apt-get install libcurl4-openssl-dev
# 或
sudo apt-get install libcurl-dev
```

2. 安裝 pkg-config（如果尚未安裝）：
```bash
sudo apt-get install pkg-config
```

## 設定環境變數

在專案根目錄建立 `.env` 檔案：

```bash
# 在 v1 目錄下
cat > .env << EOF
SENDGRID_API_KEY=your_sendgrid_api_key_here
SENDGRID_FROM=noreply@example.com
EOF
```

請替換為您的實際值：
- `SENDGRID_API_KEY`: 從 https://app.sendgrid.com/settings/api_keys 取得
- `SENDGRID_FROM`: 必須是在 SendGrid 中驗證過的發送信箱

## 編譯

使用 CMake 編譯：

```bash
cd /home/linux_mini_server/v1
mkdir -p build
cd build
cmake ..
make smtp_test
```

或者使用 gcc 直接編譯：

```bash
cd /home/linux_mini_server/v1
gcc -Wall -Wextra -o smtp_test src/smtp.c src/env.c -Iinclude -lcurl
```

## 執行測試

```bash
# 在 v1 目錄下執行
./build/bin/smtp_test

# 或如果直接編譯
./smtp_test
```

## 修改測試參數

編輯 `src/smtp.c` 中的 `main()` 函數來修改收件人、主旨和內容。

