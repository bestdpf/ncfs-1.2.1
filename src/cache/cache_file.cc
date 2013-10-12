#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "cache_file.hh"
#include "../filesystem/filesystem_common.hh"
#include "../filesystem/filesystem_utils.hh"
#include "cache.hh"

extern struct ncfs_state* NCFS_DATA;
extern CacheLayerBase* cacheLayer;
extern FileSystemLayer* fileSystemLayer;

FileCache::FileCache(const char* path, int id)
{
	long long cacheSize = configLayer->getConfigLong("Cache>FileCache>CacheSize");
	long long blockSize = configLayer->getConfigLong("Cache>FileCache>BlockSize");
	long long pageSize = configLayer->getConfigLong("Cache>FileCache>PageSize");
	int numOfBuffer = configLayer->getConfigInt("Cache>FileCache>NumOfBuffer");
	if(cacheSize < 0)
		cacheSize = FILE_CACHE_DEFAULT_SIZE;
	if(blockSize < 0)
		blockSize = FILE_CACHE_DEFAULT_BLOCK_SIZE;
	if(pageSize < 0)
		pageSize = FILE_CACHE_DEFAULT_PAGE_SIZE;
	if(numOfBuffer < 0)
		numOfBuffer = FILE_CACHE_DEFAULT_NUM_BUFFER;
	initSizes(blockSize,cacheSize,pageSize,numOfBuffer,false);
}

void FileCache::initSizes(long long blockSize, long long bufferSize, long long pageSize, int numOfBuffer, bool doWrite)
{
	CacheTemplate::initSizes(blockSize,bufferSize,pageSize,numOfBuffer,doWrite);
	buffer_ = new FileCacheBuffer[numOfBuffer];
	for(int i = 0; i < numOfBuffer; ++i)
		buffer_[i].initSizes(blockSize,bufferSize,pageSize,doWrite);
	workingBuffer_ = &buffer_[0];
	currentPos_ = 0;
}

FileCache::~FileCache()
{
	emptyBuffer();
}

long long FileCache::FileCacheBuffer::doWrite(const char* buf, long long size, long long offset)
{
	return size;
}

long long FileCache::FileCacheBuffer::doRead(char* buf, long long size, long long offset)
{
	logFunc("%s %lld %lld\n",path_,size,offset);
	//to-do: read FileInfo and then Data from Cache
	long long finishedSize = 0;
	long long startBlock = offset / NCFS_DATA->disk_block_size;
	struct data_block_info blockInfo;
	long long blockInfoOffset = (startBlock + 1) * sizeof(struct data_block_info); 
	long long retstat = cacheLayer->readFileInfo(id_,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;
	long long copySize = NCFS_DATA->disk_block_size - (offset % NCFS_DATA->disk_block_size);
	long long readOffset = NCFS_DATA->disk_block_size - copySize;
	retstat = fileSystemLayer->codingLayer->decode(blockInfo.disk_id,buf,copySize,blockInfo.block_no * NCFS_DATA->disk_block_size + readOffset);
		if(retstat < 0)return FATALERROR;
	long long startBlockOffset = retstat;
	startBlock++;
	finishedSize += retstat;
	long long endBlock = (offset + size) / NCFS_DATA->disk_block_size;
	if((offset % NCFS_DATA->disk_block_size) == 0)
		--endBlock;
	if(endBlock <= startBlock)
		return finishedSize;

	char* bufAddress;
	long long count = 0;
	copySize = NCFS_DATA->disk_block_size;
	for(long long i = startBlock; i <= endBlock - 1; ++i){
		blockInfoOffset = (i + 1) * sizeof(struct data_block_info);
		retstat = cacheLayer->readFileInfo(id_,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;
		bufAddress = buf + startBlockOffset + (count * NCFS_DATA->disk_block_size); 
		retstat = cacheLayer->readDisk(blockInfo.disk_id,bufAddress,copySize,blockInfo.block_no*NCFS_DATA->disk_block_size);
		if(retstat < 0)return FATALERROR;
		++count;
	}

	blockInfoOffset = (endBlock + 1) * sizeof(struct data_block_info);
	retstat = cacheLayer->readFileInfo(id_,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;
	copySize = (offset + size) % NCFS_DATA->disk_block_size;
	if(copySize == 0) copySize = NCFS_DATA->disk_block_size;
	bufAddress = buf + size - copySize;
	retstat = cacheLayer->readDisk(blockInfo.disk_id,bufAddress,copySize,blockInfo.block_no*NCFS_DATA->disk_block_size);
		if(retstat < 0)return FATALERROR;
	finishedSize += retstat;

	return finishedSize;
}
