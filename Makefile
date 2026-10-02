CC     = gcc
CFLAGS = -std=gnu89 -w

all: abp gbn

abp: src/ABP_AbdulMuumin.c
	$(CC) $(CFLAGS) -o abp src/ABP_AbdulMuumin.c

gbn: src/GBN_AbdulMuumin.c
	$(CC) $(CFLAGS) -o gbn src/GBN_AbdulMuumin.c

clean:
	rm -f abp abp.exe gbn gbn.exe

.PHONY: all clean
