#include <stdio.h>
#include <unistd.h>
#include "collector.h"
#include "db_storage.h"

void *storage_thread(void *arg)
{
    printf("[存储线程] 启动\n");

    if (db_init("/root/sensor.db") != 0) {
        fprintf(stderr, "[存储线程] 数据库初始化失败\n");
        return NULL;
    }

    int count = 0;

    while (g_shared.running) {
        sensor_data_t local;

        pthread_mutex_lock(&g_shared.lock);
        local = g_shared.data;
        pthread_mutex_unlock(&g_shared.lock);

        if (db_insert(&local) == 0) {
            count++;
            if (count % 60 == 0) {
                printf("[存储线程] 已写入 %d 条记录\n", count);
            }
        }

        sleep(1);
    }

    db_close();
    printf("[存储线程] 退出，共写入 %d 条记录\n", count);
    return NULL;
}
