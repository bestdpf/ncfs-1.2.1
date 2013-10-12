#ifndef __STORAGE_HH__
#define __STORAGE_HH__

#include "../filesystem/filesystem_common.hh"

#define AbnormalError() printf("%s %s %d\n", __FILE__, __func__,__LINE__);

//Abstract class of storage layer
class StorageLayer {

  public:
        virtual int DiskRenew(int disk_id) = 0;
        virtual int DiskRead(const char* path, char* buf, long long size, long long offset) = 0;
        virtual int DiskWrite(const char* path, const char* buf, long long size, 
			long long offset) = 0;
	virtual long long readDisk(int id, char* buf, long long size, long long offset) = 0;
	virtual long long writeDisk(int id, const char* buf, long long size, long long offset) = 0;
	int *disk_fd;   //list of device descriptors
};

#endif
