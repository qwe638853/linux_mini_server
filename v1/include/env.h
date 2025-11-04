#pragma once

/**
 * 載入環境變數檔案
 * 
 * @param filename .env 檔案的路徑
 * @return 成功返回 0，失敗返回 -1
 * 
 * 功能說明：
 * - 讀取檔案中的 KEY=VALUE 格式
 * - 自動跳過空行和註釋行（以 # 開頭）
 * - 自動移除鍵和值的前後空白
 * - 支援引號包裹的值（單引號或雙引號）
 * - 使用 setenv() 設置環境變數（不覆蓋已存在的變數）
 */
int load_env_file(const char *filename);

