#ifndef __CACHE_FILEINFO_HH__
#define __CACHE_FILEINFO_HH__
#include "cache_template.hh"

#define FILEINFO_CACHE_DEFAULT_SIZE			2*1024*1024
#define FILEINFO_CACHE_DEFAULT_BLOCK_SIZE	4*1024
#define FILEINFO_CACHE_DEFAULT_PAGE_SIZE	4*1024
#define FILEINFO_CACHE_DEFAULT_NUM_BUFFER	4

class FileInfoCache : public CacheTemplate{
	public:
		FileInfoCache(const char* path = "", int id = EMPTY);
		void initSizes(	long long blockSize, long long cacheSize, 
						long long pageSize, int numOfBuffer, bool doWrite);
		~FileInfoCache();
	protected:
		class FileInfoCacheBuffer : public BufferTemplate{
		};
};

#endif
