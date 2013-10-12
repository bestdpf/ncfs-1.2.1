#ifndef __CACHE_FILE_HH__
#define __CACHE_FILE_HH__
#include "cache_template.hh"

#define FILE_CACHE_DEFAULT_SIZE			4*1024*1024
#define FILE_CACHE_DEFAULT_BLOCK_SIZE	4*1024
#define FILE_CACHE_DEFAULT_PAGE_SIZE	32*1024
#define FILE_CACHE_DEFAULT_NUM_BUFFER	4

class FileCache : public CacheTemplate{
	public:
		FileCache(const char* path = "", int id = EMPTY);
//		void init(const char* path, int id);
		void initSizes(	long long blockSize, long long cacheSize, 
						long long pageSize, int numOfBuffer, bool doWrite);
		~FileCache();
	protected:
		class FileCacheBuffer : public BufferTemplate{
			protected:
				long long doWrite(const char* buf, long long size, long long offset);
				long long doRead(char* buf, long long size, long long offset);
		};
};

#endif
