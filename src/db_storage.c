#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "db_storage.h"

static sqlite3 *db = NULL;

static const char *CREATE_TABLE_SQL =
    "CREATE TABLE IF NOT EXISTS readings ("
    "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  timestamp  INTEGER NOT NULL,"
    "  temp       REAL,"
    "  humi       REAL,"
    "  vib_x      INTEGER,"
    "  vib_y      INTEGER,"
    "  vib_z      INTEGER,"
    "  ai_result  INTEGER DEFAULT 0"
    ");";

int db_init(const char *path)
{
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] 打开失败: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_exec(db, CREATE_TABLE_SQL, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] 建表失败: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    printf("[DB] 数据库就绪: %s\n", path);
    return 0;
}

int db_insert(sensor_data_t *data)
{
    if (!db) return -1;

    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO readings(timestamp, temp, humi, vib_x, vib_y, vib_z) "
        "VALUES(%lu, %.2f, %.2f, %d, %d, %d);",
        (unsigned long)data->timestamp,
        data->temp, data->humi,
        (int)data->vib_x, (int)data->vib_y, (int)data->vib_z);

    int rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] 插入失败: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    return 0;
}

static int query_callback(void *not_used, int argc, char **argv, char **col_name)
{
    for (int i = 0; i < argc; i++) {
        printf("%s = %s  ", col_name[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

int db_query_recent(int count)
{
    if (!db) return -1;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM readings ORDER BY id DESC LIMIT %d;", count);

    int rc = sqlite3_exec(db, sql, query_callback, NULL, NULL);
    return (rc == SQLITE_OK) ? 0 : -1;
}

void db_close(void)
{
    if (db) {
        sqlite3_close(db);
        db = NULL;
        printf("[DB] 数据库已关闭\n");
    }
}
