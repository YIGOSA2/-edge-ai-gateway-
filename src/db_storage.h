#ifndef DB_STORAGE_H
#define DB_STORAGE_H

#include "collector.h"

int  db_init(const char *path);
void db_close(void);
int  db_insert(sensor_data_t *data);
int  db_query_recent(int count);

#endif
