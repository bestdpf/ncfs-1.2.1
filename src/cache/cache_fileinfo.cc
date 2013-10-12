#include "cache_fileinfo.hh"

FileInfoCache::FileInfoCache(const char* path, int id)
{
	long long cacheSize = configLayer->getConfigLong("Cache>FileInfoCache>CacheSize");
	long long blockSize = configLayer->getConfigLong("Cache>FileInfoCache>BlockSize");
	long long pageSize = configLayer->getConfigLong("Cache>FileInfoCache>PageSize");
	int numOfBuffer = configLayer->getConfigInt("Cache>FileInfoCache>NumOfBuffer");
	if(cacheSize < 0)
		cacheSize = FILEINFO_CACHE_DEFAULT_SIZE;
	if(blockSize < 0)
		blockSize = FILEINFO_CACHE_DEFAULT_BLOCK_SIZE;
	if(pageSize < 0)
		pageSize = FILEINFO_CACHE_DEFAULT_PAGE_SIZE;
	if(numOfBuffer < 0)
		numOfBuffer = FILEINFO_CACHE_DEFAULT_NUM_BUFFER;
	preOpen_ = true;
	initSizes(blockSize,cacheSize,pageSize,numOfBuffer,true);
}

void FileInfoCache::initSizes(long long blockSize, long long bufferSize, long long pageSize, int numOfBuffer, bool doWrite)
{
	CacheTemplate::initSizes(blockSize,bufferSize,pageSize,numOfBuffer,doWrite);
	buffer_ = new FileInfoCacheBuffer[numOfBuffer];
	for(int i = 0; i < numOfBuffer; ++i)
		buffer_[i].initSizes(blockSize,bufferSize,pageSize,doWrite);
	workingBuffer_ = &buffer_[0];
	currentPos_ = 0;
}

FileInfoCache::~FileInfoCache()
{
	emptyBuffer();
}

