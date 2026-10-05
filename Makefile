CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -Iinclude -pthread

COMMON = src/protocol.c src/net_utils.c

RBC_MODULES = src/rbc_context.c src/ma_generator.c
EVC_MODULES = src/evc_context.c

all: rbc_server evc_client

rbc_server:
	$(CC) $(CFLAGS) src/rbc_server.c $(COMMON) $(RBC_MODULES) -o rbc_server

evc_client:
	$(CC) $(CFLAGS) src/evc_client.c $(COMMON) $(EVC_MODULES) -o evc_client

clean:
	rm -f rbc_server evc_client
