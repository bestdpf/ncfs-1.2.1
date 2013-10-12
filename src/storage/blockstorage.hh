#ifndef __BLOCKSTORAGE_HH__
#define __BLOCKSTORAGE_HH__

#include "storage.hh"

//Block-based storage
class BlockStorage : public StorageLayer {

private:
	int search_disk_id(const char* path);
public:
	BlockStorage();
	~BlockStorage();
	int DiskRenew(int disk_id);
	int DiskRead(const char* path, char* buf, long long size, long long offset);
	int DiskWrite(const char* path, const char* buf, long long size, 
			long long offset);
	long long readDisk(int id, char* buf, long long size, long long offset);
	long long writeDisk(int id, const char* buf, long long size, long long offset);
	int *disk_fd;
};

#endif
