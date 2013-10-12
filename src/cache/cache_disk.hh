#ifndef __CACHE_DISK_HH__
#define __CACHE_DISK_HH__
#include "cache_template.hh"

#define DISK_CACHE_DEFAULT_SIZE			4*1024*1024
#define DISK_CACHE_DEFAULT_BLOCK_SIZE	4*1024
#define DISK_CACHE_DEFAULT_PAGE_SIZE	4*1024
#define DISK_CACHE_DEFAULT_NUM_BUFFER	4

class DiskCache : public CacheTemplate{
	public:
		DiskCache(const char* path = "", int id = EMPTY);
		void initSizes(	long long blockSize, long long cacheSize, 
						long long pageSize, int numOfBuffer, bool doWrite);
		//void flushDisk(long long offset);
		~DiskCache();
	protected:
		//void flush(long long offset);
		class DiskCacheBuffer : public BufferTemplate{
			protected:
				long long doWrite(const char* buf, long long size, long long offset);
				long long doRead(char* buf, long long size, long long offset);
		};
};

#endif
