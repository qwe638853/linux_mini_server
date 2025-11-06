#!/bin/bash
# 健壯性測試腳本
# 測試各種異常情況下的程序行為

set -e

# 顏色定義
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 測試結果計數
PASSED=0
FAILED=0

# 打印測試標題
print_test() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}測試: $1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# 打印成功
print_pass() {
    echo -e "${GREEN}✓ PASS: $1${NC}"
    ((PASSED++))
}

# 打印失敗
print_fail() {
    echo -e "${RED}✗ FAIL: $1${NC}"
    ((FAILED++))
}

# 清理函數
cleanup() {
    echo -e "\n${YELLOW}清理測試環境...${NC}"
    pkill -f "./server" 2>/dev/null || true
    pkill -f "nc 127.0.0.1 9734" 2>/dev/null || true
    sleep 1
}

# 設置清理陷阱
trap cleanup EXIT

# 檢查服務器是否在運行
check_server() {
    if pgrep -f "./server" > /dev/null; then
        return 0
    else
        return 1
    fi
}

# 啟動服務器（如果未運行）
start_server() {
    if ! check_server; then
        echo -e "${YELLOW}啟動服務器...${NC}"
        cd "$(dirname "$0")/build" || exit 1
        ./server > /tmp/server_test.log 2>&1 &
        SERVER_PID=$!
        sleep 2
        if check_server; then
            echo -e "${GREEN}服務器已啟動 (PID: $SERVER_PID)${NC}"
        else
            echo -e "${RED}服務器啟動失敗${NC}"
            cat /tmp/server_test.log
            exit 1
        fi
    fi
}

# 測試 1: 端口已被占用
test_port_already_in_use() {
    print_test "測試 1: 端口已被占用"
    
    cd "$(dirname "$0")/build" || exit 1
    
    # 啟動第一個服務器
    ./server > /tmp/server1.log 2>&1 &
    SERVER1_PID=$!
    sleep 2
    
    # 嘗試啟動第二個服務器（應該失敗）
    if ./server > /tmp/server2.log 2>&1; then
        print_fail "第二個服務器不應該成功啟動"
        kill $SERVER1_PID 2>/dev/null || true
        return
    fi
    
    # 檢查錯誤訊息
    if grep -q "bind.*failed\|Address already in use" /tmp/server2.log; then
        print_pass "正確檢測到端口已被占用並退出"
    else
        print_fail "未正確處理端口占用錯誤"
        cat /tmp/server2.log
    fi
    
    kill $SERVER1_PID 2>/dev/null || true
    sleep 1
}

# 測試 2: 客戶端突然斷開連接
test_client_disconnect() {
    print_test "測試 2: 客戶端突然斷開連接"
    
    start_server
    
    # 使用 nc 連接後立即斷開
    (echo "SYSINFO"; sleep 0.1) | nc 127.0.0.1 9734 > /dev/null 2>&1 &
    NC_PID=$!
    sleep 0.2
    kill $NC_PID 2>/dev/null || true
    
    sleep 2
    
    # 檢查服務器日誌
    if grep -q "Failed to write\|Client closed\|Error reading" /tmp/server_test.log; then
        print_pass "正確檢測並處理客戶端斷開連接"
    else
        print_fail "未正確處理客戶端斷開"
        tail -20 /tmp/server_test.log
    fi
    
    # 確認服務器仍在運行
    if check_server; then
        print_pass "服務器在客戶端斷開後繼續運行"
    else
        print_fail "服務器因客戶端斷開而崩潰"
    fi
}

# 測試 3: 超長輸入（緩衝區溢出防護）
test_long_input() {
    print_test "測試 3: 超長輸入（緩衝區溢出防護）"
    
    start_server
    
    # 生成超長命令（超過 256 字節）
    LONG_CMD=$(python3 -c "print('A' * 300)")
    
    cd "$(dirname "$0")/build" || exit 1
    echo -e "${LONG_CMD}\nSYSINFO" | timeout 5 ./client > /tmp/client_long.log 2>&1 || true
    
    sleep 2
    
    # 檢查服務器日誌
    if grep -q "Command line too long\|truncated" /tmp/server_test.log; then
        print_pass "正確檢測並處理超長輸入"
    else
        print_fail "未正確處理超長輸入"
        tail -20 /tmp/server_test.log
    fi
    
    # 確認服務器仍在運行
    if check_server; then
        print_pass "服務器在超長輸入後繼續運行"
    else
        print_fail "服務器因超長輸入而崩潰"
    fi
}

# 測試 4: 客戶端連接但不發送數據（超時處理）
test_client_timeout() {
    print_test "測試 4: 客戶端連接但不發送數據（超時處理）"
    
    start_server
    
    # 連接但不發送任何數據
    nc 127.0.0.1 9734 > /dev/null 2>&1 &
    NC_PID=$!
    sleep 35  # 等待超時（30秒 + 緩衝）
    kill $NC_PID 2>/dev/null || true
    
    sleep 2
    
    # 檢查服務器日誌
    if grep -q "No command received\|timeout\|Client closed" /tmp/server_test.log; then
        print_pass "正確處理客戶端超時"
    else
        print_fail "未正確處理客戶端超時"
        tail -20 /tmp/server_test.log
    fi
    
    # 確認服務器仍在運行
    if check_server; then
        print_pass "服務器在超時後繼續運行"
    else
        print_fail "服務器因超時而崩潰"
    fi
}

# 測試 5: 空命令處理
test_empty_command() {
    print_test "測試 5: 空命令處理"
    
    start_server
    
    cd "$(dirname "$0")/build" || exit 1
    echo -e "\nSYSINFO" | timeout 5 ./client > /tmp/client_empty.log 2>&1 || true
    
    sleep 2
    
    # 檢查服務器日誌
    if grep -q "Empty command\|No command received" /tmp/server_test.log; then
        print_pass "正確處理空命令"
    else
        print_fail "未正確處理空命令"
        tail -20 /tmp/server_test.log
    fi
    
    # 確認服務器仍在運行
    if check_server; then
        print_pass "服務器在空命令後繼續運行"
    else
        print_fail "服務器因空命令而崩潰"
    fi
}

# 測試 6: 多個客戶端同時連接
test_multiple_clients() {
    print_test "測試 6: 多個客戶端同時連接"
    
    start_server
    
    cd "$(dirname "$0")/build" || exit 1
    
    # 同時啟動多個客戶端
    for i in {1..5}; do
        (echo "SYSINFO"; sleep 0.1) | timeout 5 ./client > /tmp/client_$i.log 2>&1 &
    done
    
    wait
    
    sleep 2
    
    # 確認服務器仍在運行
    if check_server; then
        print_pass "服務器成功處理多個同時連接"
    else
        print_fail "服務器無法處理多個同時連接"
    fi
    
    # 檢查所有客戶端都收到回應
    SUCCESS_COUNT=0
    for i in {1..5}; do
        if grep -q "System Info\|Hostname" /tmp/client_$i.log; then
            ((SUCCESS_COUNT++))
        fi
    done
    
    if [ $SUCCESS_COUNT -ge 4 ]; then
        print_pass "大部分客戶端成功收到回應 ($SUCCESS_COUNT/5)"
    else
        print_fail "太多客戶端未收到回應 ($SUCCESS_COUNT/5)"
    fi
}

# 主測試流程
main() {
    echo -e "${GREEN}"
    echo "=========================================="
    echo "  健壯性機制測試套件"
    echo "=========================================="
    echo -e "${NC}"
    
    # 確保在正確目錄
    cd "$(dirname "$0")" || exit 1
    
    # 檢查是否已編譯
    if [ ! -f "build/server" ] || [ ! -f "build/client" ]; then
        echo -e "${RED}錯誤: 請先編譯程序 (cd build && cmake .. && make)${NC}"
        exit 1
    fi
    
    # 運行測試
    test_port_already_in_use
    test_client_disconnect
    test_long_input
    test_client_timeout
    test_empty_command
    test_multiple_clients
    
    # 打印總結
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}測試總結${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}通過: $PASSED${NC}"
    echo -e "${RED}失敗: $FAILED${NC}"
    echo -e "${BLUE}總計: $((PASSED + FAILED))${NC}"
    
    if [ $FAILED -eq 0 ]; then
        echo -e "\n${GREEN}所有測試通過！${NC}"
        exit 0
    else
        echo -e "\n${RED}部分測試失敗${NC}"
        exit 1
    fi
}

# 運行主函數
main

