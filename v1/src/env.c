#include "env.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_env_file(const char *filename){
    // 檢查參數有效性
    if (filename == NULL) {
        fprintf(stderr, "Error: filename cannot be NULL\n");
        return -1;
    }
    
    if (filename[0] == '\0') {
        fprintf(stderr, "Error: filename cannot be empty\n");
        return -1;
    }
    
    // 打開檔案
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Failed to open file '%s': ", filename);
        perror(NULL);
        return -1;
    }
    
    char line[1024];
    int line_num = 0;
    int success_count = 0;
    
    // 逐行讀取
    while (fgets(line, sizeof(line), f) != NULL) {
        line_num++;
        
        // 移除換行符和回車符
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }
        
        // 移除前導空白字符
        char *start = line;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        
        // 跳過空行
        if (*start == '\0') {
            continue;
        }
        
        // 跳過註釋行（以 # 開頭）
        if (*start == '#') {
            continue;
        }
        
        // 尋找等號
        char *eq = strchr(start, '=');
        if (eq == NULL) {
            fprintf(stderr, "Warning: Line %d: No '=' found, skipping\n", line_num);
            continue;
        }
        
        // 分割鍵和值
        *eq = '\0';
        char *key = start;
        char *value = eq + 1;
        
        // 移除鍵尾部的空白字符
        char *key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
            *key_end = '\0';
            key_end--;
        }
        
        // 檢查鍵是否為空
        if (key[0] == '\0') {
            fprintf(stderr, "Warning: Line %d: Empty key, skipping\n", line_num);
            continue;
        }
        
        // 移除值前導空白字符
        while (*value == ' ' || *value == '\t') {
            value++;
        }
        
        // 移除值尾部的空白字符
        size_t value_len = strlen(value);
        if (value_len > 0) {
            char *value_end = value + value_len - 1;
            while (value_end > value && (*value_end == ' ' || *value_end == '\t' || 
                                         *value_end == '\n' || *value_end == '\r')) {
                *value_end = '\0';
                value_end--;
            }
            value_len = strlen(value);
        }
        
        // 處理引號（移除前後的引號）
        if (value_len > 0) {
            if ((value[0] == '"' && value[value_len - 1] == '"') ||
                (value[0] == '\'' && value[value_len - 1] == '\'')) {
                value[value_len - 1] = '\0';
                value++;
            }
        }
        
        // 設置環境變數（如果已存在則不覆蓋，使用 0）
        if (setenv(key, value, 0) != 0) {
            fprintf(stderr, "Error: Failed to set environment variable '%s': ", key);
            perror(NULL);
            fclose(f);
            return -1;
        }
        success_count++;
    }
    
    // 檢查讀取錯誤
    if (ferror(f)) {
        fprintf(stderr, "Error: Error reading file '%s'\n", filename);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    
    if (success_count == 0) {
        fprintf(stderr, "Warning: No environment variables loaded from '%s'\n", filename);
    }
    
    return 0;
}

