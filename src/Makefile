CC = ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpthread -lsqlite3

# SDK 中的头文件和库路径
SDK_SYSROOT = ~/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf/sysroot
CFLAGS += -I$(SDK_SYSROOT)/usr/include
LDFLAGS += -L$(SDK_SYSROOT)/usr/lib

TARGET = gateway

SRCS = main.c sensor_thread.c storage_thread.c ai_thread.c mqtt_thread.c \
       aht30.c mpu6050.c db_storage.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

BOARD_IP = 192.168.137.108
deploy: $(TARGET)
	scp $(TARGET) root@$(BOARD_IP):/root/

.PHONY: all clean deploy
