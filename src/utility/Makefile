CC = gcc
CFLAGS_UTIL = -g -Wall 

dir_utility= .

default:
	g++ $(CFLAGS_UTIL) -c *.cc

setup: $(dir_utility)/setup.c
	$(CC) $(CFLAGS_UTIL) $(dir_utility)/setup.c -o setup

recovery: $(dir_utility)/recovery.c
	$(CC) $(CFLAGS_UTIL) $(dir_utility)/recovery.c -o recovery

remap: $(dir_utility)/remap.c
	$(CC) $(CFLAG_UTIL) $(dir_utility)/remap.c -o remap

benchmark: $(dir_utility)/benchmark.c
	$(CC) $(CFLAG_UTIL) $(dir_utility)/benchmark.c -o benchmark

clean:
	@echo "Deleting recovery"
	rm -f recovery.o
	@echo "Deleting repair task scheduler"
	rm -f scheduler.o
	@echo "Deleting repair task executer"
	rm -f taskexecuter.o
	@echo "Deleting trace executer"
	rm -f tracer.o
	@echo "Deleting experiment result analyzer"
	rm -f analyze.o
	@echo "Deleting read performance"
	rm -f readperformance.o