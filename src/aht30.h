#ifndef AHT30_H
#define AHT30_H

int  aht30_init(void);
int  aht30_read(float *temp, float *humi);
void aht30_close(void);

#endif
