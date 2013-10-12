CC = g++
CC_JERASURE = gcc
CC_ZPACR = gcc
CFLAGS_NCFS = -g -Wall -pthread `pkg-config fuse --libs`
CFLAGS_OBJ = -g -Wall -pthread `pkg-config fuse --cflags`
CFLAGS_UTIL = -g -Wall
LIB = -lpthread

dir_src = src
dir_filesystem = $(dir_src)/filesystem
dir_cache = $(dir_src)/cache
dir_coding = $(dir_src)/coding
dir_storage = $(dir_src)/storage
dir_utility= $(dir_src)/utility
dir_network = $(dir_src)/network
dir_gui = $(dir_src)/gui
dir_config = $(dir_src)/config
dir_jerasure = $(dir_src)/jerasure

#dir_objs defines the directory of objects. objs defines object paths.
dir_objs = $(dir_filesystem) $(dir_cache) $(dir_coding) $(dir_storage) $(dir_gui) $(dir_network) $(dir_config) $(dir_utility)
objs = $(dir_filesystem)/*.o $(dir_cache)/*.o $(dir_storage)/*.o $(dir_coding)/*.o \
		$(dir_network)/*.o $(dir_gui)/*.o $(dir_config)/*.o \
		$(dir_jerasure)/*.o
repair_objs = $(dir_utility)/recovery.o $(dir_filesystem)/filesystem_utils.o $(dir_cache)/*.o $(dir_storage)/*.o \
		$(dir_coding)/*.o $(dir_network)/*.o $(dir_gui)/*.o $(dir_config)/*.o \
		$(dir_jerasure)/*.o

executer_objs = $(dir_utility)/taskexecuter.o $(dir_filesystem)/filesystem_utils.o $(dir_cache)/*.o $(dir_storage)/*.o \
		$(dir_coding)/*.o $(dir_network)/*.o $(dir_gui)/*.o $(dir_config)/*.o \
		$(dir_jerasure)/*.o

tracer_objs = $(dir_utility)/tracer.o $(dir_filesystem)/filesystem_utils.o $(dir_cache)/*.o $(dir_storage)/*.o \
		$(dir_coding)/*.o $(dir_network)/*.o $(dir_gui)/*.o $(dir_config)/*.o \
		$(dir_jerasure)/*.o

default:
	@for i in $(dir_objs); do \
	(echo $$i; cd $$i; make); done
	$(CC_JERASURE) $(CFLAGS_OBJ) -c $(dir_jerasure)/cauchy.c -o $(dir_jerasure)/cauchy.o
	$(CC_JERASURE) $(CFLAGS_OBJ) -c $(dir_jerasure)/reed_sol.c -o $(dir_jerasure)/reed_sol.o
	$(CC_JERASURE) $(CFLAGS_OBJ) -c $(dir_jerasure)/jerasure.c -o $(dir_jerasure)/jerasure.o
	$(CC_JERASURE) $(CFLAGS_OBJ) -c $(dir_jerasure)/galois.c -o $(dir_jerasure)/galois.o

	@echo "Compiling ncfs"
	$(CC) -o ncfs $(objs) $(CFLAGS_NCFS)

setup: $(dir_utility)/setup.c
	$(CC) $(CFLAGS_UTIL) $(dir_utility)/setup.c -o setup

recover: $(dir_utility)/recovery.cc
	$(CC) $(CFLAGS_UTIL) $(repair_objs) -o recover $(LIB)

scheduler: $(dir_utility)/scheduler.cc
	$(CC) $(CFLAGS_UTIL) $(dir_utility)/scheduler.o -o scheduler

notify: $(dir_utility)/notify.cc
	$(CC) $(CFLAGS_UTIL) $(dir_utility)/notify.o -o notify

executer: $(dir_utility)/taskexecuter.cc
	$(CC) $(CFLAGS_UTIL) $(executer_objs) -o executer $(LIB)

tracer: $(dir_utility)/tracer.cc
	$(CC) $(CFLAGS_UTIL) $(tracer_objs) -o tracer $(LIB)

analyzer: $(dir_utility)/analyze.cc
	$(CC) $(CFLAGS_UTIL) $(dir_utility)/analyze.o -o analyze

read: $(dir_utility)/readperformance.cc
	$(CC) $(CFLAGS_UTIL) $(dir_utility)/readperformance.o -o read

remap: $(dir_utility)/remap.c
	$(CC) $(CFLAG_UTIL) $(dir_utility)/remap.c -o remap

benchmark: $(dir_utility)/benchmark.c
	$(CC) $(CFLAG_UTIL) $(dir_utility)/benchmark.c -o benchmark

clean:
	@echo "Deleting ncfs"
	rm -f ncfs *.o *~
	@echo "Deleting recover"
	rm -f recover
	rm -f $(dir_utility)/recovery.o
	@echo "Deleting scheduler"
	rm -f scheduler
	rm -f $(dir_utility)/scheduler.o
	@echo "Deleting executer"
	rm -f executer
	rm -f $(dir_utility)/taskexecuter.o
	@echo "Deleting tracer"
	rm -f tracer
	rm -f $(dir_utility)/tracer.o
	@echo "Deleting analyzer"
	rm -f analyze
	rm -f $(dir_utility)/analyze.o
	@echo "Deleting readperformance"
	rm -f read
	rm -f $(dir_utility)/readperformance.o
	@for i in $(objs); do \
	echo "Deleting $$i"; \
	(rm $$i); done
