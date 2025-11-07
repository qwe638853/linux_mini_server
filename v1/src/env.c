#include "env.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_env_file(const char *filename){
    INFO_LOG(stderr, "load_env_file: Loading from '%s'\n", filename ? filename : "NULL");
    // Check parameter validity
    if (filename == NULL) {
        ERROR_LOG(stderr, "load_env_file: filename cannot be NULL\n");
        fprintf(stderr, "Error: filename cannot be NULL\n");
        return -1;
    }
    
    if (filename[0] == '\0') {
        ERROR_LOG(stderr, "load_env_file: filename cannot be empty\n");
        fprintf(stderr, "Error: filename cannot be empty\n");
        return -1;
    }
    
    // Open file
    FILE *f = fopen(filename, "r");
    if (!f) {
        ERROR_LOG(stderr, "load_env_file: Failed to open file '%s'\n", filename);
        fprintf(stderr, "Error: Failed to open file '%s': ", filename);
        perror(NULL);
        return -1;
    }
    DEBUG_LOG(stderr, "load_env_file: File opened successfully\n");
    
    char line[1024];
    int line_num = 0;
    int success_count = 0;
    
    // Read line by line
    while (fgets(line, sizeof(line), f) != NULL) {
        line_num++;
        
        // Check if truncated due to buffer too small (line too long)
        if(strlen(line) == sizeof(line) - 1 && line[sizeof(line) - 2] != '\n'){
            WARN_LOG(stderr, "load_env_file: Line %d too long, may be truncated\n", line_num);
        }
        
        // Remove newline and carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }
        
        // Remove leading whitespace
        char *start = line;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        
        // Skip empty lines
        if (*start == '\0') {
            continue;
        }
        
        // Skip comment lines (starting with #)
        if (*start == '#') {
            continue;
        }
        
        // Find equals sign
        char *eq = strchr(start, '=');
        if (eq == NULL) {
            WARN_LOG(stderr, "load_env_file: Line %d: No '=' found, skipping\n", line_num);
            fprintf(stderr, "Warning: Line %d: No '=' found, skipping\n", line_num);
            continue;
        }
        
        // Split key and value
        *eq = '\0';
        char *key = start;
        char *value = eq + 1;
        
        // Remove trailing whitespace from key
        char *key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
            *key_end = '\0';
            key_end--;
        }
        
        // Check if key is empty
        if (key[0] == '\0') {
            WARN_LOG(stderr, "load_env_file: Line %d: Empty key, skipping\n", line_num);
            fprintf(stderr, "Warning: Line %d: Empty key, skipping\n", line_num);
            continue;
        }
        
        // Remove leading whitespace from value
        while (*value == ' ' || *value == '\t') {
            value++;
        }
        
        // Remove trailing whitespace from value
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
        
        // Handle quotes (remove leading and trailing quotes)
        if (value_len > 0) {
            if ((value[0] == '"' && value[value_len - 1] == '"') ||
                (value[0] == '\'' && value[value_len - 1] == '\'')) {
                value[value_len - 1] = '\0';
                value++;
            }
        }
        
        // Set environment variable (don't overwrite if exists, use 0)
        DEBUG_LOG(stderr, "load_env_file: Setting %s = %s\n", key, value);
        if (setenv(key, value, 0) != 0) {
            ERROR_LOG(stderr, "load_env_file: Failed to set environment variable '%s'\n", key);
            fprintf(stderr, "Error: Failed to set environment variable '%s': ", key);
            perror(NULL);
            fclose(f);
            return -1;
        }
        success_count++;
    }
    
    // Check for read errors
    if (ferror(f)) {
        ERROR_LOG(stderr, "load_env_file: Error reading file '%s'\n", filename);
        fprintf(stderr, "Error: Error reading file '%s'\n", filename);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    
    if (success_count == 0) {
        WARN_LOG(stderr, "load_env_file: No environment variables loaded from '%s'\n", filename);
        fprintf(stderr, "Warning: No environment variables loaded from '%s'\n", filename);
    } else {
        INFO_LOG(stderr, "load_env_file: Successfully loaded %d environment variables from '%s'\n", success_count, filename);
    }
    
    return 0;
}

