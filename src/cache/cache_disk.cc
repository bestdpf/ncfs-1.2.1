#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "cache_disk.hh"
#include "../storage/storage.hh"
#include "cache.hh"
extern CacheLayer* cacheLayer;
extern StorageLayer* storageLayer;

DiskCache::DiskCache(const char* path, int id)
{
	long long cacheSize = configLayer->getConfigLong("Cache>DiskCache>CacheSize");
	long long blockSize = configLayer->getConfigLong("Cache>DiskCache>BlockSize");
	long long pageSize = configLayer->getConfigLong("Cache>DiskCache>PageSize");
	int numOfBuffer = configLayer->getConfigInt("Cache>DiskCache>NumOfBuffer");
	if(cacheSize < 0)
		cacheSize = DISK_CACHE_DEFAULT_SIZE;
	if(blockSize < 0)
		blockSize = DISK_CACHE_DEFAULT_BLOCK_SIZE;
	if(pageSize < 0)
		pageSize = DISK_CACHE_DEFAULT_PAGE_SIZE;
	if(numOfBuffer < 0)
		numOfBuffer = DISK_CACHE_DEFAULT_NUM_BUFFER;
	initSizes(blockSize,cacheSize,pageSize,numOfBuffer,true);
}

void DiskCache::initSizes(long long blockSize, long long bufferSize, long long pageSize, int numOfBuffer, bool doWrite)
{
	CacheTemplate::initSizes(blockSize,bufferSize,pageSize,numOfBuffer,doWrite);
	buffer_ = new DiskCacheBuffer[numOfBuffer];
	for(int i = 0; i < numOfBuffer; ++i){
		//logFunc("Creat Buffer %d\n",i);
		buffer_[i].initSizes(blockSize,bufferSize,pageSize,doWrite);
	}
	workingBuffer_ = &buffer_[0];
	currentPos_ = 0;
}

DiskCache::~DiskCache()
{
	emptyBuffer();
}

long long DiskCache::DiskCacheBuffer::doWrite(const char* buf, long long size, long long offset)
{
	logFunc("%s %lld %lld\n",path_,size,offset);
	long long retstat;
	retstat = storageLayer->writeDisk(id_,buf,size,offset);
	return retstat;
}

long long DiskCache::DiskCacheBuffer::doRead(char* buf, long long size, long long offset)
{
	logFunc("%s %lld %lld\n",path_,size,offset);
	long long retstat;
	retstat = storageLayer->readDisk(id_,buf,size,offset);
	return retstat;
}
/*
void DiskCache::flush(long long offset)
{
	cacheLayer->flushDisks(offset);
}

void DiskCache::flushDisk(long long offset)
{
	CacheTemplate::flush(offset);
}
 */
