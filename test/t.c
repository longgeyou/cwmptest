#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 解析单个键值对
void parse_header_line(const char *line, char **key, char **value) {
    char *copy = strdup(line);  // 复制字符串以避免修改原字符串
    char *colon = strchr(copy, ':');

    if (colon) {
        *colon = '\0';  // 截断字符串
        *key = copy;
        *value = colon + 1;

        // 去掉前后的空白字符
        while (**key == ' ' || **key == '\t') (*key)++;
        while (**value == ' ' || **value == '\t') (*value)++;
        while ((*value)[strlen(*value) - 1] == ' ' || (*value)[strlen(*value) - 1] == '\t') {
            (*value)[strlen(*value) - 1] = '\0';
        }
    } else {
        *key = copy;
        *value = NULL;
    }

    // 打印结果
    printf("Key: %s, Value: %s\n", *key, *value ? *value : "NULL");
}

int main() {
    const char *headers[] = {
        "Accept-Language: en-US,en;q=0.5",
        "Accept-Encoding: gzip, deflate",
        "Content-Type: application/x-www-form-urlencoded",
        "Content-Length: 27",
        "Connection: keep-alive"
    };
    int num_headers = sizeof(headers) / sizeof(headers[0]);

    for (int i = 0; i < num_headers; i++) {
        char *key, *value;
        parse_header_line(headers[i], &key, &value);

        // 释放分配的内存
        free(key);

        // 如果 value 不是 NULL，也释放它
        if (value && value != key) {
            free(value);
        }
    }

    return 0;
}