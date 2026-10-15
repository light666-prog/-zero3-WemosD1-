#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <sqlite3.h>
#include "db.h"
#include <cjson/cJSON.h>
#include <errno.h>

#define SECRET_ID "你的ID"
#define SECRET_KEY "你的key"
#define HOST "iai.tencentcloudapi.com"ID
#define SERVICE "iai"
#define GROUP_ID "my_app_users" // 人员库ID，需先创建
#define VERSION "你的版本号"
#define REGION "你的区域"

char *file_to_base64(const char *filename);
char *build_authorization(const char *action, const char *payload, long ts_val);
size_t write_data(void *ptr, size_t size, size_t nmemb, void *userp);
bool db_update_face_id_by_person(const char *person_id, const char *face_id);
bool create_person(const char *image_path, const char *person_id, char *out_face_id);

struct Memory
{
    char *data;
    size_t size;
};

/* 返回：0=成功，1=未匹配，2=调用失败 */
int search_face(const char *image_path, char *out_name, double *out_score)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return 2;

    // 1. 图片转 Base64
    char *b64 = file_to_base64(image_path);
    if (!b64)
    {
        curl_easy_cleanup(curl);
        return 2;
    }

    // 2. 拼请求体
    size_t payload_len = strlen(b64) + 256;
    char *payload = malloc(payload_len);
    snprintf(payload, payload_len,
             "{\"GroupIds\":[\"%s\"],\"Image\":\"%s\",\"MaxFaceNum\":1}",
             GROUP_ID, b64);
    free(b64);

    // 3. 签名
    long ts_val = (long)time(NULL);
    char *auth = build_authorization("SearchFaces", payload, ts_val);

    char ts[16];
    snprintf(ts, sizeof(ts), "%ld", ts_val);
    char header_auth[700], header_ts[64];
    snprintf(header_auth, sizeof(header_auth), "Authorization: %s", auth);
    snprintf(header_ts, sizeof(header_ts), "X-TC-Timestamp: %s", ts);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, "Host: iai.tencentcloudapi.com");
    headers = curl_slist_append(headers, header_auth);
    headers = curl_slist_append(headers, header_ts);
    headers = curl_slist_append(headers, "X-TC-Action: SearchFaces");
    headers = curl_slist_append(headers, "X-TC-Version: 2018-03-01");
    headers = curl_slist_append(headers, "X-TC-Region: ap-nanjing");

    // 4. 发请求
    struct Memory chunk = {0};
    curl_easy_setopt(curl, CURLOPT_URL, "https://iai.tencentcloudapi.com");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    int ret = 2; // 默认失败

    if (res == CURLE_OK && chunk.data)
    {
        printf("响应: %s\n", chunk.data);

        cJSON *root = cJSON_Parse(chunk.data);
        if (root)
        {
            cJSON *response = cJSON_GetObjectItem(root, "Response");
            if (response)
            {
                cJSON *error = cJSON_GetObjectItem(response, "Error");
                if (error)
                {
                    cJSON *msg = cJSON_GetObjectItem(error, "Message");
                    fprintf(stderr, "API错误: %s\n", msg ? msg->valuestring : "?");
                }
                else
                {
                    cJSON *results = cJSON_GetObjectItem(response, "Results");
                    if (cJSON_IsArray(results) && cJSON_GetArraySize(results) > 0)
                    {
                        cJSON *first = cJSON_GetArrayItem(results, 0);
                        cJSON *candidates = cJSON_GetObjectItem(first, "Candidates");
                        if (cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0)
                        {
                            cJSON *top = cJSON_GetArrayItem(candidates, 0);
                            cJSON *pid = cJSON_GetObjectItem(top, "PersonId");
                            cJSON *score = cJSON_GetObjectItem(top, "Score");

                            if (pid && score)
                            {
                                *out_score = score->valuedouble;
                                printf("匹配到 PersonId=%s, Score=%.2f\n",
                                       pid->valuestring, *out_score);

                                if (*out_score >= 95.0)
                                {
                                    // ★★★ 在这里查数据库拿真实姓名 ★★★
                                    if (db_get_name_by_person_id(pid->valuestring, out_name))
                                    {
                                        printf("★★★ 查到姓名: %s\n", out_name);
                                        ret = 0;
                                    }
                                    else
                                    {
                                        // 数据库查不到，退而求其次用 person_id
                                        printf("★★★ 数据库查不到，用 person_id\n");
                                        strcpy(out_name, pid->valuestring);
                                        ret = 0;
                                    }
                                }
                                else
                                {
                                    ret = 1; // 分数太低
                                }
                            }
                            else
                            {
                                ret = 1;
                            }
                        }
                        else
                        {
                            ret = 1; // Candidates 为空
                        }
                    }
                    else
                    {
                        ret = 1; // Results 为空
                    }
                }
            }
            cJSON_Delete(root);
        }
    }
    else
    {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
    }
    free(chunk.data);
    free(payload);
    free(auth);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ret;
}

/* ========== 图片转 Base64 ========== */
char *file_to_base64(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return NULL;
    fseek(fp, 0, SEEK_END);
    long filelen = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *buffer = malloc(filelen);
    if (!buffer)
    {
        fclose(fp);
        return NULL;
    }
    fread(buffer, 1, filelen, fp);
    fclose(fp);
    int encoded_len = 4 * ((filelen + 2) / 3);
    char *b64_out = malloc(encoded_len + 1);
    if (!b64_out)
    {
        free(buffer);
        return NULL;
    }
    EVP_EncodeBlock((unsigned char *)b64_out, buffer, filelen);
    b64_out[encoded_len] = '\0';
    free(buffer);
    return b64_out;
}

/* ========== SHA256 十六进制 ========== */
void sha256_hex(const unsigned char *data, size_t len, char *out)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data, len, hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[SHA256_DIGEST_LENGTH * 2] = '\0';
}

/* ========== HMAC-SHA256 ========== */
int hmac_sha256(const unsigned char *key, size_t key_len,
                const unsigned char *data, size_t data_len,
                unsigned char *out)
{
    unsigned int len = 0;
    HMAC(EVP_sha256(), key, key_len, data, data_len, out, &len);
    return len;
}

/* ========== 二进制转十六进制 ========== */
void bin2hex(const unsigned char *bin, size_t len, char *out)
{
    for (size_t i = 0; i < len; i++)
        sprintf(out + i * 2, "%02x", bin[i]);
    out[len * 2] = '\0';
}

/* ========== 生成签名 ========== */
char *build_authorization(const char *action, const char *payload, long ts_val)
{
    char timestamp[16];
    snprintf(timestamp, sizeof(timestamp), "%ld", ts_val);

    time_t t = (time_t)ts_val;
    struct tm *tm_utc = gmtime(&t);
    char date[11];
    strftime(date, 11, "%Y-%m-%d", tm_utc);

    char action_lower[64];
    snprintf(action_lower, sizeof(action_lower), "%s", action);
    for (int i = 0; action_lower[i]; i++)
        action_lower[i] = tolower((unsigned char)action_lower[i]);

    char hashed_payload[65] = {0};
    sha256_hex((const unsigned char *)payload, strlen(payload), hashed_payload);

    char canonical_headers[256];
    snprintf(canonical_headers, sizeof(canonical_headers),
             "content-type:application/json; charset=utf-8\n"
             "host:%s\n"
             "x-tc-action:%s\n",
             HOST, action_lower);

    const char *signed_headers = "content-type;host;x-tc-action";

    char canonical_request[2048];
    snprintf(canonical_request, sizeof(canonical_request),
             "POST\n/\n\n%s\n%s\n%s",
             canonical_headers, signed_headers, hashed_payload);

    char hashed_canonical[65] = {0};
    sha256_hex((const unsigned char *)canonical_request,
               strlen(canonical_request), hashed_canonical);

    char credential_scope[128];
    snprintf(credential_scope, sizeof(credential_scope),
             "%s/%s/tc3_request", date, SERVICE);

    char string_to_sign[1024];
    snprintf(string_to_sign, sizeof(string_to_sign),
             "TC3-HMAC-SHA256\n%s\n%s\n%s",
             timestamp, credential_scope, hashed_canonical);

    unsigned char secret_date[32], secret_service[32], secret_signing[32];
    char tc3_key[128];
    snprintf(tc3_key, sizeof(tc3_key), "TC3%s", SECRET_KEY);

    hmac_sha256((unsigned char *)tc3_key, strlen(tc3_key),
                (unsigned char *)date, strlen(date), secret_date);
    hmac_sha256(secret_date, 32,
                (unsigned char *)SERVICE, strlen(SERVICE), secret_service);
    hmac_sha256(secret_service, 32,
                (unsigned char *)"tc3_request", strlen("tc3_request"), secret_signing);

    unsigned char signature_bin[32];
    hmac_sha256(secret_signing, 32,
                (unsigned char *)string_to_sign, strlen(string_to_sign),
                signature_bin);
    char signature_hex[65] = {0};
    bin2hex(signature_bin, 32, signature_hex);

    char *auth = malloc(512);
    snprintf(auth, 512,
             "TC3-HMAC-SHA256 Credential=%s/%s, "
             "SignedHeaders=%s, Signature=%s",
             SECRET_ID, credential_scope, signed_headers, signature_hex);
            return auth;
}

/* ========== 接收响应 ========== */
size_t write_data(void *ptr, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;

    char *new_data = realloc(mem->data, mem->size + total + 1);
    if (!new_data)
        return 0;

    mem->data = new_data;
    memcpy(mem->data + mem->size, ptr, total);
    mem->size += total;
    mem->data[mem->size] = '\0';

    return total;
}
void sqlite3_insert(sqlite3 *db, const char *face_id)
{
    const char *sql = "INSERT INTO SignPeople(face_id) VALUES(?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "prepare 失败: %s\n", sqlite3_errmsg(db));
        return;
    }

    // ★ 字符串用 bind_text
    sqlite3_bind_text(stmt, 1, face_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        printf("插入成功: face_id=%s\n", face_id);
    else
        fprintf(stderr, "插入失败: %s\n", sqlite3_errmsg(db));

    sqlite3_finalize(stmt);
}
bool create_person(const char *image_path, const char *person_id, char *out_face_id)
{
    printf("★★★ create_person 开始\n");
    fflush(stdout);

    /* ===== 1. 校验参数 ===== */
    if (!image_path || !person_id || !out_face_id)
    {
        printf("★★★ 参数为空\n");
        fflush(stdout);
        return false;
    }
    printf("★★★ image_path=[%s], person_id=[%s]\n", image_path, person_id);
    fflush(stdout);

    /* ===== 2. 校验图片文件存在 ===== */
    FILE *test = fopen(image_path, "rb");
    if (!test)
    {
        printf("★★★ 图片打不开: %s (errno=%d: %s)\n",
               image_path, errno, strerror(errno));
        fflush(stdout);
        return false;
    }
    fseek(test, 0, SEEK_END);
    long fsize = ftell(test);
    fclose(test);
    printf("★★★ 图片大小: %ld 字节\n", fsize);
    fflush(stdout);

    if (fsize <= 0)
    {
        printf("★★★ 图片为空\n");
        fflush(stdout);
        return false;
    }

    /* ===== 3. curl 初始化 ===== */
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        printf("★★★ curl_easy_init 失败\n");
        fflush(stdout);
        return false;
    }

    /* ===== 4. 图片转 base64 ===== */
    char *b64 = file_to_base64(image_path);
    if (!b64)
    {
        printf("★★★ file_to_base64 失败\n");
        fflush(stdout);
        curl_easy_cleanup(curl);
        return false;
    }
    printf("★★★ base64 长度: %zu\n", strlen(b64));
    fflush(stdout);

    /* ===== 5. 拼 payload ===== */
    size_t payload_len = strlen(b64) + 512;
    char *payload = malloc(payload_len);
    if (!payload)
    {
        printf("★★★ payload malloc 失败\n");
        fflush(stdout);
        free(b64);
        curl_easy_cleanup(curl);
        return false;
    }

    int written = snprintf(payload, payload_len,
                           "{\"GroupId\":\"%s\",\"PersonId\":\"%s\","
                           "\"PersonName\":\"%s\",\"Image\":\"%s\"}",
                           GROUP_ID, person_id, person_id, b64);
    free(b64);

    if (written >= (int)payload_len)
    {
        printf("★★★ payload 被截断，需要更大缓冲区\n");
        fflush(stdout);
        free(payload);
        curl_easy_cleanup(curl);
        return false;
    }
    printf("★★★ payload 长度: %d\n", written);
    fflush(stdout);

    /* ===== 6. 算签名 ===== */
    long ts_val = (long)time(NULL);
    char *auth = build_authorization("CreatePerson", payload, ts_val);
    if (!auth)
    {
        printf("★★★ 签名生成失败\n");
        fflush(stdout);
        free(payload);
        curl_easy_cleanup(curl);
        return false;
    }
    printf("★★★ 签名: %s\n", auth);
    fflush(stdout);

    /* ===== 7. 设置 Header ===== */
    char ts[16];
    snprintf(ts, sizeof(ts), "%ld", ts_val);
    char header_auth[800], header_ts[64];
    snprintf(header_auth, sizeof(header_auth), "Authorization: %s", auth);
    snprintf(header_ts, sizeof(header_ts), "X-TC-Timestamp: %s", ts);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, "Host: iai.tencentcloudapi.com");
    headers = curl_slist_append(headers, header_auth);
    headers = curl_slist_append(headers, header_ts);
    headers = curl_slist_append(headers, "X-TC-Action: CreatePerson");
    headers = curl_slist_append(headers, "X-TC-Version: 2018-03-01");
    headers = curl_slist_append(headers, "X-TC-Region: ap-nanjing");

    /* ===== 8. 发请求 ===== */
    struct Memory chunk = {0};
    curl_easy_setopt(curl, CURLOPT_URL, "https://iai.tencentcloudapi.com");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    /* ===== 9. 检查 curl 结果 ===== */
    if (res != CURLE_OK)
    {
        printf("★★★ curl 失败: %s\n", curl_easy_strerror(res));
        fflush(stdout);
        free(chunk.data);
        free(payload);
        free(auth);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    printf("★★★ HTTP 状态码: %ld\n", http_code);
    fflush(stdout);

    /* ===== 10. 检查响应是否为空 ===== */
    if (!chunk.data || chunk.size == 0)
    {
        printf("★★★ 响应为空\n");
        fflush(stdout);
        free(chunk.data);
        free(payload);
        free(auth);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    printf("★★★ 腾讯云响应: %s\n", chunk.data);
    fflush(stdout);

    /* ===== 11. 解析 JSON ===== */
    bool success = false;
    cJSON *root = cJSON_Parse(chunk.data);
    if (!root)
    {
        printf("★★★ JSON 解析失败\n");
        fflush(stdout);
    }
    else
    {
        cJSON *response = cJSON_GetObjectItem(root, "Response");
        if (!response)
        {
            printf("★★★ 没有 Response 字段\n");
            fflush(stdout);
        }
        else
        {
            /* 检查是否有 Error */
            cJSON *error = cJSON_GetObjectItem(response, "Error");
            if (error)
            {
                cJSON *code = cJSON_GetObjectItem(error, "Code");
                cJSON *msg = cJSON_GetObjectItem(error, "Message");

                printf("★★★ API 错误\n");
                fflush(stdout);
                printf("     Code: %s\n", code ? code->valuestring : "?");
                printf("     Message: %s\n", msg ? msg->valuestring : "?");

                /* 特殊处理：已存在 */
                if (code && strcmp(code->valuestring,
                                   "InvalidParameterValue.PersonIdAlreadyExist") == 0)
                {
                    printf("★★★ PersonId 已存在，跳过注册\n");
                    fflush(stdout);
                    strcpy(out_face_id, "EXISTS");
                    success = true; // 当作成功
                }
            }
            else
            {
                /* 取 FaceId */
                cJSON *face_id = cJSON_GetObjectItem(response, "FaceId");
                if (face_id && face_id->valuestring && strlen(face_id->valuestring) > 0)
                {
                    strcpy(out_face_id, face_id->valuestring);
                    printf("★★★ FaceId = %s\n", out_face_id);
                    fflush(stdout);
                    success = true;
                }
                else
                {
                    printf("★★★ 响应里没有 FaceId\n");
                    fflush(stdout);

                    /* 有时 CreatePerson 返回 FaceIds 数组 */
                    cJSON *face_ids = cJSON_GetObjectItem(response, "FaceIds");
                    if (cJSON_IsArray(face_ids) && cJSON_GetArraySize(face_ids) > 0)
                    {
                        cJSON *first = cJSON_GetArrayItem(face_ids, 0);
                        if (first && first->valuestring)
                        {
                            strcpy(out_face_id, first->valuestring);
                            printf("★★★ FaceId(从数组)= %s\n", out_face_id);
                            fflush(stdout);
                            success = true;
                        }
                    }
                }
            }
        }
        cJSON_Delete(root);
    }

    /* ===== 12. 清理 ===== */
    free(chunk.data);
    free(payload);
    free(auth);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    printf("★★★ create_person 结束: success=%d\n", success);
    fflush(stdout);
    return success;
}
/* ========== main ========== */
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("用法:\n");
        printf("  注册: %s register <图片> <person_id>\n", argv[0]);
        printf("  登录: %s login <图片>\n", argv[0]);
        return 1;
    }

    sqlite3_open("./Sign.db", &db);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (strcmp(argv[1], "register") == 0)
    {
        if (argc < 4)
        {
            printf("缺少 person_id\n");
            return 1;
        }
        const char *image_path = argv[2]; // ★ 图片在前
        const char *person_id = argv[3];  // ★ person_id 在后

        char face_id[128] = {0};
        bool ok = create_person(image_path, person_id, face_id); // ★ 顺序对

        if (ok)
        {
            // 输出给 TCP 服务端读
            printf("FACEID:%s\n", face_id);
            db_update_face_id_by_person(person_id, face_id);
        }
        else
        {
            printf("REGISTER_FAIL\n");
        }
    }
    else if (strcmp(argv[1], "login") == 0)
    {
        char name[64] = {0};
        double score = 0.0;

        int ret = search_face(argv[2], name, &score);

        printf("★★★ search_face 返回: ret=%d, name=[%s], score=%.2f\n",
               ret, name, score);
        fflush(stdout);

        if (ret == 0)
        {
            printf("NAME:%s\n", name);
            printf("SCORE:%.2f\n", score);
            fflush(stdout);
        }
        else
        {
            printf("NO_MATCH\n");
            fflush(stdout);
        }
    }

    curl_global_cleanup();
    sqlite3_close(db);
    return 0;
}