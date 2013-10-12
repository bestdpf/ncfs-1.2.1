#include "cache.hh"
#include "../filesystem/filesystem_common.hh"
#include "../config/config.hh"

extern struct ncfs_state* NCFS_DATA;
extern ConfigLayer* configLayer;
/********************************
 *								*
 *	Cache Layer Constructors	*
 *								*
 ********************************/
CacheLayer::CacheLayer(){
	init(NCFS_DATA);
}

CacheLayer::CacheLayer(struct ncfs_state* ncfs_data){
	init(ncfs_data);
}

void CacheLayer::init(struct ncfs_state* ncfs_data)
{
	numOfDisk_ = ncfs_data->disk_total_num;
	diskCache_ = new DiskCache[numOfDisk_];
	for(int i = 0; i < numOfDisk_; ++i){
		logFunc("Disk Cache Init %d\n",i);
		diskCache_[i].init(ncfs_data->dev_name[i],i);
	}

	numOfFileCache_ = configLayer->getConfigInt("Cache>FileCache>NumOfCache");
	if(numOfFileCache_ == -1)
		numOfFileCache_ = DEFAULT_FILE_CACHE_NUM;

	numOfFileInfoCache_ = configLayer->getConfigInt("Cache>FileInfoCache>NumOfCache");
	if(numOfFileInfoCache_ == -1)
		numOfFileInfoCache_ = DEFAULT_FILEINFO_CACHE_NUM;

	fileInfoCache_ = new FileInfoCache[numOfFileInfoCache_];
	logFunc("Create File Info Cache\n");
	fileCache_ = new FileCache[numOfFileCache_];
	logFunc("Create File Cache\n");

	for(int i = 0; i < FILE_MAX; ++i){
		fileCacheIndex_[i] = EMPTY;
		fileInfoCacheIndex_[i] = EMPTY;
	}
	lastAllocatedFileCache_ = -1;
	lastAllocatedFileInfoCache_ = -1;
	threadPool_.startThreadPool();
}

long long CacheLayer::readDisk(int disk_id, char* buf, long long size, long long offset)
{
	if(disk_id < 0 || disk_id >= numOfDisk_)
		return FATALERROR;
		
	return diskCache_[disk_id].read(buf,size,offset);

}

long long CacheLayer::writeDisk(int disk_id, const char* buf, long long size, long long offset)
{
	if(disk_id < 0 || disk_id >= numOfDisk_)
		return FATALERROR;
	
	return diskCache_[disk_id].write(buf,size,offset);
	
	
}

void CacheLayer::setDiskName(int disk_id, const char* newdevice)
{
	diskCache_[disk_id].setPath(newdevice);
}
/*
void* CacheLayer::flushThread(void* data){
	flushArg* argument = (flushArg*)data;
	DiskCache* target = argument->targetDiskCache_;
	long long offset = argument->offset_;
	target->flushDisk(offset);
	return NULL;
}

void CacheLayer::flushDisks(long long offset)
{
	pthread_t tid[numOfDisk_];
	flushArg argument[numOfDisk_];
	for(int i = 0; i < numOfDisk_; ++i)
	{
		argument[i].targetDiskCache_ = &diskCache_[i];
		argument[i].offset_ = offset;
		pthread_create(&tid[i],NULL,flushThread,&argument[i]);
	}
	for(int i = 0; i < numOfDisk_; ++i)
	{
		pthread_join(tid[i],NULL);
	}
}
*/
/********************************
 *								*
 *	File Info Cache Operations	*
 *								*
 ********************************/
void CacheLayer::createFileInfo(const char* path, int id)
{
	if(id < 0 || id >= FILE_MAX){
		exit(-1);
	}
	int targetID = searchFileInfoCache(path);
	if(targetID == UNAVAILABLE)
		targetID = allocateFileInfoCache();
	if(targetID == FATALERROR){
		exit(-1);
	}
	fileInfoCacheIndex_[id] = targetID;
	//		if(msg_)
	//			fprintf(stderr,"File Info created %s with fd %d at %d\n",path,id,targetID);
	fileInfoCache_[targetID].init(path,id);
	//		if(msg_){
	//			fprintf(stderr,"ID %d\n",fileInfoCache_[targetID].getId());
	//			fprintf(stderr,"ID %d\n",fileInfoCache_[0].getId());
	//		}
}

	int CacheLayer::readFileInfo(int id, char* buf,long long size, long long offset)
{
		if(id < 0 || id >= FILE_MAX)
			return FATALERROR;
//		if(msg_)
//			fprintf(stderr,"File Info Read %d at %d\n",id,fileInfoCacheIndex_[id]);
		return fileInfoCache_[(fileInfoCacheIndex_[id])].read(buf,size,offset);
	}

	int CacheLayer::writeFileInfo(int id, const char* buf,long long size, long long offset)
{
		if(id < 0 || id >= FILE_MAX)
			return FATALERROR;
//		logFunc("%d @ %d\n",id,fileInfoCacheIndex_[id]);
		return fileInfoCache_[fileInfoCacheIndex_[id]].write(buf,size,offset);
	}

int CacheLayer::searchFileInfoCache(const char* path)
{
	for(int i = 0; i < numOfFileInfoCache_; ++i){
		//		fprintf(stderr,"FileInfo %s at %d\n",fileInfoCache_[i].getPath(),i);
		if (strncmp(path, fileInfoCache_[i].getPath(),
					PATH_MAX) == 0){
//			if(msg_)
//				fprintf(stderr,"Cached FileInfo %s at %d\n",path,i);
			return i;
		}
	}
	return UNAVAILABLE;
}

int CacheLayer::allocateFileInfoCache()
{
	for(int i = 1; i <= numOfFileInfoCache_; ++i){
		int targetID = (lastAllocatedFileInfoCache_ + i ) % numOfFileInfoCache_;
		if(fileInfoCache_[targetID].isEmpty()){
			lastAllocatedFileInfoCache_ = targetID;
			return targetID;
		}
	}
	return FATALERROR;
}

int CacheLayer::searchFileInfo(const char* path)
{
	int targetID = searchFileInfoCache(path);
	if(targetID < 0)
		return targetID;
	else
		return fileInfoCache_[targetID].getId();
}	

void CacheLayer::destroyFileInfo(int id)
{
	fileInfoCache_[fileInfoCacheIndex_[id]].setEmpty();
//	fileInfoCacheIndex_[id] = EMPTY;
}

/********************************
 *								*
 *	File Cache Operations		*
 *								*
 ********************************/
	void CacheLayer::createFileCache(const char* path, int id)
{
		if(id < 0 || id >= FILE_MAX){
			exit(-1);
		}
		int targetID = searchFileCache(path);
		if(targetID == UNAVAILABLE)
			targetID = allocateFileCache();
		if(targetID == FATALERROR){
//			if(msg_)
//				fprintf(stderr,"Failed to allocate File Cache\n");
			exit(-1);
		}
		fileCacheIndex_[id] = targetID;
//		if(msg_)
//			fprintf(stderr,"%s with fd %d at %d\n",path,id,targetID);
		fileCache_[targetID].init(path,id);
	}

	int CacheLayer::readFileCache(int id, char* buf,long long size, long long offset)
{
		if(id < 0 || id >= FILE_MAX)
			return FATALERROR;
		return fileCache_[fileCacheIndex_[id]].read(buf,size,offset);
	}

	int CacheLayer::writeFileCache(int id, const char* buf,long long size, long long offset){
		if(id < 0 || id >= FILE_MAX)
			return FATALERROR;
		return fileCache_[fileCacheIndex_[id]].write(buf,size,offset);
	}

int CacheLayer::searchFileCache(const char* path){
	for(int i = 0; i < numOfFileCache_; ++i){
		if (strncmp(path, fileCache_[i].getPath(),
					PATH_MAX) == 0){
//			if(msg_)
//				fprintf(stderr,"Cached File %s at %d\n",path,i);
			return i;
		}
	}
	return UNAVAILABLE;
}

int CacheLayer::allocateFileCache(){
	for(int i = 1; i <= numOfFileCache_; ++i){
		int targetID = (lastAllocatedFileCache_ + i ) % numOfFileCache_;
		if(fileCache_[targetID].isEmpty()){
			lastAllocatedFileCache_ = targetID;
			return targetID;
		}
	}
	return FATALERROR;
}

void CacheLayer::destroyFileCache(int id){
	fileCache_[fileCacheIndex_[id]].setEmpty();
	//fileCacheIndex_[id] = EMPTY;
}

void CacheLayer::addThreadJob(BufferTemplate* targetBuffer, BufferJobType job,long long offset)
{
	threadPool_.addJob(targetBuffer,job,offset);
}

/********************************
 *								*
 *	Cache Layer Destructors		*
 *								*
 ********************************/
CacheLayer::~CacheLayer(){
	delete [] fileCache_;
	delete [] fileInfoCache_;
	delete [] diskCache_;
}
