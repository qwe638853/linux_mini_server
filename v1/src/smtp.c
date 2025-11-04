#include "smtp.h"
#include "env.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *json_escape(const char *str){
    if(!str) return strdup("");

    size_t len = strlen(str);
    size_t cap = len * 6 + 1;
    char *escaped = malloc(cap);
    if(escaped == NULL){
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
                    sprintf(p, "\\u%04X", c);
                    p += 6;
                } else {
                    *p++ = (char)c;
                }
        }
    }
    *p = '\0';
    return escaped;
}
int send_email(const char *recipient, const char *subject, const char *body){
    // 固定從專案根目錄載入 .env 檔案
    // 嘗試載入常見的 .env 位置，只要有一個成功即可，否則回傳錯誤
    
    const char *env_paths[] = { "../../.env", "../.env", ".env" };
    int found = 0;
    for (size_t i = 0; i < sizeof(env_paths)/sizeof(env_paths[0]); ++i) {
        if (load_env_file(env_paths[i]) == 0) {
            found = 1;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "Error: Cannot find .env file in project root directory or parent directories\n");
        return -1;
    }
    const char *sendgrid_api_key = getenv("SENDGRID_API_KEY");
    const char *from_email = getenv("SENDGRID_FROM");
    
    if (sendgrid_api_key == NULL || from_email == NULL) {
        fprintf(stderr, "Error: SENDGRID_API_KEY or SENDGRID_FROM not set in .env file\n");
        return -1;
    }

    char *escaped_subject = json_escape(subject);
    char *escaped_body = json_escape(body);
    if(!escaped_subject|| !escaped_body){
        perror("json_escape");
        free(escaped_subject);
        free(escaped_body);
        return -1;
    }

    size_t payload_size = strlen(escaped_subject) + strlen(escaped_body) + strlen(recipient) + strlen(from_email) + 256;
    char *payload = (char *)malloc(payload_size);
    if (payload == NULL) {
        perror("malloc");
        free(escaped_subject);
        free(escaped_body);
        return -1;
    }
    snprintf(payload, payload_size,
        "{"
          "\"personalizations\":[{\"to\":[{\"email\":\"%s\"}]}],"
          "\"from\":{\"email\":\"%s\"},"
          "\"subject\":\"%s\","
          "\"content\":[{\"type\":\"text/plain\",\"value\":\"%s\"}]"
        "}",
        recipient, from_email, escaped_subject, escaped_body
    );

    free(escaped_subject); 
    free(escaped_body);

    CURL *curl = curl_easy_init();
    if(curl == NULL){
        perror("curl_easy_init");
        free(payload);
        return -1;
    }

    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", sendgrid_api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char errbuf[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.sendgrid.com/v3/mail/send");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(payload));
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    
    int http_code = 0;
    if (res != CURLE_OK){
        fprintf(stderr, "curl_easy_perform failed: %s\n", errbuf);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(payload);

    if (res != CURLE_OK) {
        return -1;
    }

    if (http_code != 202) {
        fprintf(stderr, "SendGrid API returned error: %d\n", http_code);
        return -1;
    }


    return 0;
    
}
