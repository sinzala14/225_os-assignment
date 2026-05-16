CC     = gcc
CFLAGS = -Wall -O2 -DPLATFORM_DESKTOP
LIBS   = -lraylib -lm -lpthread -ldl -lrt -lX11

TARGET = serc_mini_os

all: $(TARGET)

$(TARGET): mini_os.c
	$(CC) $(CFLAGS) mini_os.c -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) serc_log.txt

.PHONY: all clean
