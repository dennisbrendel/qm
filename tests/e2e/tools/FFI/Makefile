CC = gcc
CFLAGS = -Wall -g

VPATH=disk/QM:memory:shm:socket:sem:mq
QM_BIN=./bin/QM
ASIL_BIN=./bin/ASIL

all: cratetestqm cratetestasil shared-memory system_v_message_queues \
	posix_message_queues abstract-af_unix \
	system_v_semaphore named_posix_semaphore

cratetestqm: createqmbin file-allocate 90_percent_memory_eat

cratetestasil: createasilbin 20_percent_memory_eat

createqmbin:
	@mkdir -p $(QM_BIN)

createasilbin:
	@mkdir -p $(ASIL_BIN)

20_percent_memory_eat: memory_eat.c
	$(CC) $(CFLAGS) -DMEM_PERCENT=0.2 -o $(ASIL_BIN)/$@ $<

file-allocate: file-allocate.c
	$(CC) $(CFLAGS) -o $(QM_BIN)/$@ $<

90_percent_memory_eat: memory_eat.c
	$(CC) $(CFLAGS) -DMEM_PERCENT=0.9 -o $(QM_BIN)/$@ $<

shared-memory: shm.c
	$(CC) $(CFLAGS) -o $(ASIL_BIN)/$@ $<
	cp $(ASIL_BIN)/$@ $(QM_BIN)/$@

system_v_message_queues: system_v_message_queues.c
	$(CC) $(CFLAGS) -o $(ASIL_BIN)/$@ $<
	cp $(ASIL_BIN)/$@ $(QM_BIN)/$@

posix_message_queues: posix_message_queues.c
	$(CC) $(CFLAGS) -o $(ASIL_BIN)/$@ $<
	cp $(ASIL_BIN)/$@ $(QM_BIN)/$@

abstract-af_unix: af_unix.c
	$(CC) $(CFLAGS) -o $(ASIL_BIN)/$@ $<
	cp $(ASIL_BIN)/$@ $(QM_BIN)/$@

system_v_semaphore: system_v_semaphore.c
	$(CC) $(CFLAGS) -o $(ASIL_BIN)/$@ $<
	cp $(ASIL_BIN)/$@ $(QM_BIN)/$@

named_posix_semaphore: named_posix_semaphore.c
	$(CC) $(CFLAGS) -o $(ASIL_BIN)/$@ $<
	cp $(ASIL_BIN)/$@ $(QM_BIN)/$@

clean:
	rm -rf $(QM_BIN) $(ASIL_QM)

.PHONY: all clean createtestqm createtestasil
