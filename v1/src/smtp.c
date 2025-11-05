#include "smtp.h"
#include "env.h"
#include "debug.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static char *json_escape(const char *str){
    DEBUG_LOG(stderr, "json_escape: escaping string (length: %zu)\n", str ? strlen(str) : 0);
    if(!str) {
        char *empty = strdup("");
        if(empty == NULL){
            ERROR_LOG(stderr, "json_escape: strdup() failed for empty string\n");
        }
        return empty;
    }

    size_t len = strlen(str);
    size_t cap = len * 6 + 1;
    char *escaped = malloc(cap);
    if(escaped == NULL){
        ERROR_LOG(stderr, "json_escape: malloc() failed\n");
        perror("malloc");
        return NULL;
    }
    // loop through the string and escape the characters
    char *p = escaped;
    for(size_t i = 0; i < len; i++){
        unsigned char c = (unsigned char)str[i];
        switch(c){
            case '\"': *p++='\\'; *p++='\"'; break;
            case '\\': *p++='\\'; *p++='\\'; break;
            case '\b': *p++='\\'; *p++='b';  break;
            case '\f': *p++='\\'; *p++='f';  break;
            case '\n': *p++='\\'; *p++='n';  break;
            case '\r': *p++='\\'; *p++='r';  break;
            case '\t': *p++='\\'; *p++='t';  break;
            default:
                if (c < 0x20) {
                    /* 其他控制字元 */
                    int written = snprintf(p, 7, "\\u%04X", c);
                    if (written < 0 || written >= 7) {
                        // 如果 snprintf 失敗或截斷，釋放記憶體並返回錯誤
                        free(escaped);
                        return NULL;
                    }
                    p += written;
                } else {
                    *p++ = (char)c;
                }
        }
    }
    *p = '\0';
    return escaped;
}
int send_email(const char *recipient, const char *subject, const char *body){
    // 參數驗證
    if(recipient == NULL || subject == NULL || body == NULL){
        ERROR_LOG(stderr, "send_email: NULL parameter (recipient, subject, or body)\n");
        fprintf(stderr, "Error: recipient, subject, and body cannot be NULL\n");
        return -1;
    }
    
    INFO_LOG(stderr, "send_email: Preparing to send email to %s\n", recipient);
    // 固定從專案根目錄載入 .env 檔案
    // 嘗試載入常見的 .env 位置，只要有一個成功即可，否則回傳錯誤
    
    const char *env_paths[] = { "../../.env", "../.env", ".env" };
    int found = 0;
    DEBUG_LOG(stderr, "send_email: Looking for .env file...\n");
    for (size_t i = 0; i < sizeof(env_paths)/sizeof(env_paths[0]); ++i) {
        DEBUG_LOG(stderr, "  Trying: %s\n", env_paths[i]);
        if (load_env_file(env_paths[i]) == 0) {
            found = 1;
            INFO_LOG(stderr, "  Found .env file at: %s\n", env_paths[i]);
            break;
        }
    }
    if (!found) {
        ERROR_LOG(stderr, "Cannot find .env file in project root directory or parent directories\n");
        fprintf(stderr, "Error: Cannot find .env file in project root directory or parent directories\n");
        return -1;
    }
    const char *sendgrid_api_key = getenv("SENDGRID_API_KEY");
    const char *from_email = getenv("SENDGRID_FROM");
    
    if (sendgrid_api_key == NULL || from_email == NULL) {
        ERROR_LOG(stderr, "SENDGRID_API_KEY or SENDGRID_FROM not set in .env file\n");
        fprintf(stderr, "Error: SENDGRID_API_KEY or SENDGRID_FROM not set in .env file\n");
        return -1;
    }
    DEBUG_LOG(stderr, "send_email: Environment variables loaded, from: %s\n", from_email);

    DEBUG_LOG(stderr, "send_email: Escaping JSON strings\n");
    char *escaped_subject = json_escape(subject);
    char *escaped_body = json_escape(body);
    if(!escaped_subject|| !escaped_body){
        ERROR_LOG(stderr, "json_escape() failed\n");
        perror("json_escape");
        free(escaped_subject);
        free(escaped_body);
        return -1;
    }

    // 檢查字串長度，避免溢出
    size_t subject_len = strlen(escaped_subject);
    size_t body_len = strlen(escaped_body);
    size_t recipient_len = strlen(recipient);
    size_t from_len = strlen(from_email);
    
    // 檢查是否會溢出（檢查相加是否超過 SIZE_MAX）
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
        free(escaped_subject);
        free(escaped_body);
        return -1;
    }
    
    DEBUG_LOG(stderr, "send_email: Allocating payload buffer (size: %zu)\n", payload_size);
    char *payload = (char *)malloc(payload_size);
    if (payload == NULL) {
        ERROR_LOG(stderr, "malloc() failed for payload\n");
        perror("malloc");
        free(escaped_subject);
        free(escaped_body);
        return -1;
    }
    int snprintf_result = snprintf(payload, payload_size,
        "{"
          "\"personalizations\":[{\"to\":[{\"email\":\"%s\"}]}],"
          "\"from\":{\"email\":\"%s\"},"
          "\"subject\":\"%s\","
          "\"content\":[{\"type\":\"text/plain\",\"value\":\"%s\"}]"
        "}",
        recipient, from_email, escaped_subject, escaped_body
    );
    if(snprintf_result < 0 || snprintf_result >= (int)payload_size){
        ERROR_LOG(stderr, "snprintf() failed for payload (truncated or error)\n");
        free(payload);
        free(escaped_subject);
        free(escaped_body);
        return -1;
    }

    free(escaped_subject); 
    free(escaped_body);

    INFO_LOG(stderr, "send_email: Initializing CURL\n");
    CURL *curl = curl_easy_init();
    if(curl == NULL){
        ERROR_LOG(stderr, "curl_easy_init() failed\n");
        perror("curl_easy_init");
        free(payload);
        return -1;
    }

    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf_result = snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", sendgrid_api_key);
    if(snprintf_result < 0 || snprintf_result >= (int)sizeof(auth_header)){
        ERROR_LOG(stderr, "snprintf() failed for auth header (truncated or error)\n");
        curl_easy_cleanup(curl);
        free(payload);
        return -1;
    }
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
    DEBUG_LOG(stderr, "send_email: HTTP headers configured\n");

    char errbuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.sendgrid.com/v3/mail/send");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(payload));
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    DEBUG_LOG(stderr, "send_email: CURL options set, sending request...\n");

    CURLcode res = curl_easy_perform(curl);
    
    int http_code = 0;
    if (res != CURLE_OK){
        ERROR_LOG(stderr, "curl_easy_perform failed: %s\n", errbuf);
        fprintf(stderr, "curl_easy_perform failed: %s\n", errbuf);
    } else {
        CURLcode getinfo_res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if(getinfo_res != CURLE_OK){
            WARN_LOG(stderr, "curl_easy_getinfo() failed, using default http_code 0\n");
            http_code = 0;
        } else {
            INFO_LOG(stderr, "send_email: HTTP response code: %d\n", http_code);
        }
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(payload);

    if (res != CURLE_OK) {
        return -1;
    }

    if (http_code != 202) {
        ERROR_LOG(stderr, "SendGrid API returned error: %d\n", http_code);
        fprintf(stderr, "SendGrid API returned error: %d\n", http_code);
        return -1;
    }

    INFO_LOG(stderr, "send_email: Email sent successfully\n");
    return 0;
    
}
