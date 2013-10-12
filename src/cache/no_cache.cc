#include "no_cache.hh"
#include "../storage/storage.hh"
#include "../filesystem/filesystem_common.hh"
#include "../filesystem/filesystem_utils.hh"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>


extern struct ncfs_state* NCFS_DATA;
extern StorageLayer* storageLayer;
extern CacheLayerBase* cacheLayer;
extern FileSystemLayer* fileSystemLayer;

NoCache::NoCache()
{
	init(NCFS_DATA);
}

NoCache::NoCache(struct ncfs_state* ncfs_data)
{
	init(ncfs_data);
}

long long NoCache::readDisk(int disk_id, char* buf, long long size, long long offset)
{
	return storageLayer->DiskRead(NCFS_DATA->dev_name[disk_id],buf,size,offset); 
}

long long NoCache::writeDisk(int disk_id, const char* buf, long long size, long long offset){
	return storageLayer->DiskWrite(NCFS_DATA->dev_name[disk_id],buf,size,offset);
}

void NoCache::setDiskName(int disk_id, const char* newdevice){
	return;
}

void NoCache::createFileInfo(const char* path, int id)
{
	strncpy(fileInfoPath_[id],path,PATH_MAX);
	return;
}

int NoCache::readFileInfo(int id, char* buf, long long size, long long offset)
{
	int fd = open(fileInfoPath_[id],O_RDONLY);
	if(fd < 0){
		perror("open()");
		exit(-1);
	}
	int retstat = pread(fd,buf,size,offset);
	close(fd);
	return retstat;
}

int NoCache::writeFileInfo(int id, const char* buf, long long size, long long offset)
{
	int fd = open(fileInfoPath_[id],O_WRONLY|O_CREAT,"0644");
	if(fd < 0){
		perror("open()");
		exit(-1);
	}
	int retstat = pwrite(fd,buf,size,offset);
	close(fd);
	return retstat;
}

int NoCache::searchFileInfo(const char* path)
{
	for(int i = 0; i < FILE_MAX; ++i){
		if(strncmp(fileInfoPath_[i],path,PATH_MAX) == 0)
			return i;
	}
	return UNAVAILABLE;
}

void NoCache::destroyFileInfo(int id){
	memset(fileInfoPath_[id],0,PATH_MAX);
	return;
}

void NoCache::createFileCache(const char* path, int id){
	return;
}


int NoCache::readFileCache(int id, char* buf, long long size, long long offset){

	int bigblocksize = NCFS_DATA->disk_block_size;
	int deployblocksize = bigblocksize;
	long long finishedSize = 0;
	long long startBlock = offset / deployblocksize;
	struct data_block_info blockInfo;

	memset(&blockInfo,0,sizeof(data_block_info));

	long long blockInfoOffset = (startBlock + 1) * sizeof(struct data_block_info); 
	long long retstat = cacheLayer->readFileInfo(id,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;

	long long copySize = NCFS_DATA->disk_block_size - (offset % deployblocksize);
	long long readOffset = deployblocksize - copySize;

	retstat = fileSystemLayer->codingLayer->decode(blockInfo.disk_id,buf,copySize,
						blockInfo.block_no * deployblocksize + readOffset);
		if(retstat < 0)return FATALERROR;

	long long startBlockOffset = retstat;

	finishedSize += retstat;

	long long endBlock = (offset + size) / deployblocksize;


	if(((offset + size)% deployblocksize) == 0)
		--endBlock;
//	fprintf(stderr,"%s %d %lld %lld",__FILE__,__LINE__,startBlock,endBlock);
	if(endBlock <= startBlock)
		return finishedSize;
	startBlock++;

	char* bufAddress;
	long long count = 0;

	copySize = deployblocksize;

	for(long long i = startBlock; i <= endBlock - 1; ++i){
		blockInfoOffset = (i + 1) * sizeof(struct data_block_info);
		memset(&blockInfo,0,sizeof(data_block_info));
		retstat = cacheLayer->readFileInfo(id,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;
		bufAddress = buf + startBlockOffset + (count * deployblocksize); 
		retstat = fileSystemLayer->codingLayer->decode(blockInfo.disk_id,
						bufAddress,copySize,blockInfo.block_no*deployblocksize);
		if(retstat < 0)return FATALERROR;
		++count;
	}

	blockInfoOffset = (endBlock + 1) * sizeof(struct data_block_info);
	memset(&blockInfo,0,sizeof(data_block_info));
	retstat = cacheLayer->readFileInfo(id,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;

	copySize = (offset + size) % deployblocksize;

	if(copySize == 0) copySize = deployblocksize;

	bufAddress = buf + size - copySize;
	retstat = fileSystemLayer->codingLayer->decode(blockInfo.disk_id,
				bufAddress,copySize,blockInfo.block_no*deployblocksize);
		if(retstat < 0)return FATALERROR;

	finishedSize += retstat;

	return finishedSize;
}

/*
int NoCache::readFileCache(int id, char* buf, long long size, long long offset){
	long long finishedSize = 0;
	long long startBlock = offset / NCFS_DATA->disk_block_size;
	struct data_block_info blockInfo;

	memset(&blockInfo,0,sizeof(data_block_info));

	long long blockInfoOffset = (startBlock + 1) * sizeof(struct data_block_info); 
	long long retstat = cacheLayer->readFileInfo(id,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
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
		memset(&blockInfo,0,sizeof(data_block_info));
		retstat = cacheLayer->readFileInfo(id,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;
		bufAddress = buf + startBlockOffset + (count * NCFS_DATA->disk_block_size); 
		retstat = fileSystemLayer->codingLayer->decode(blockInfo.disk_id,bufAddress,copySize,blockInfo.block_no*NCFS_DATA->disk_block_size);
		if(retstat < 0)return FATALERROR;
		++count;
	}

	blockInfoOffset = (endBlock + 1) * sizeof(struct data_block_info);
	memset(&blockInfo,0,sizeof(data_block_info));
	retstat = cacheLayer->readFileInfo(id,(char*)&blockInfo,sizeof(struct data_block_info),blockInfoOffset);
		if(retstat < 0)return FATALERROR;

	copySize = (offset + size) % NCFS_DATA->disk_block_size;

	if(copySize == 0) copySize = NCFS_DATA->disk_block_size;

	bufAddress = buf + size - copySize;
	retstat = fileSystemLayer->codingLayer->decode(blockInfo.disk_id,bufAddress,copySize,blockInfo.block_no*NCFS_DATA->disk_block_size);
		if(retstat < 0)return FATALERROR;

	finishedSize += retstat;

	return finishedSize;
}
*/

void NoCache::init(struct ncfs_state* ncfs_data)
{
	for(int i = 0; i < FILE_MAX; ++i)
		memset(fileInfoPath_[i],0,PATH_MAX);
	return;
}

int NoCache::writeFileCache(int id, const char* buf, long long size, long long offset)
{
	return size;
}

void NoCache::destroyFileCache(int id)
{
	return;
}
