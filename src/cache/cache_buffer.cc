#include "cache_buffer.hh"
#include "../filesystem/filesystem_common.hh"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cache.hh"
#include <sys/time.h>

extern CacheLayer* cacheLayer;
extern struct ncfs_state* NCFS_DATA;

BufferTemplate::BufferTemplate()
{
	pthread_mutex_init(&bufferLock_,NULL);	
	fd_ = EMPTY;
	id_ = EMPTY;
}

void BufferTemplate::init(char* path, int fd, int id, bool doClear)
{
	logFunc("%d %s %d\n",id,path,fd);
	id_ = id;
	if(id == EMPTY)
		return;

	if(doClear){
		writeBack();
		waitJob();
		clearContent();
	}
	fd_ = fd;
	path_ = path;
}

void BufferTemplate::initSizes(long long blockSize, long long bufferSize, long long pageSize, bool doWrite)
{
	blockSize_ = blockSize;
	bufferSize_ = bufferSize;
	pageSize_ = pageSize;
	numOfBlock_ = bufferSize / blockSize;
	numOfPage_ = bufferSize / pageSize;
	content_ = (char*)malloc(bufferSize);
	dirtyBlock_ = new BlockStatus[numOfBlock_];
	pageStatus_ = new PageStatus[numOfPage_];
	clearContent();
	doWrite_ = doWrite;
}

long long BufferTemplate::readFromBuffer(char* buf, long long size, long long offset)
{
	long long readSize;
	pthread_mutex_lock(&bufferLock_);
	if(containOffset(offset)){
		checkReadAvailability(size,offset);
		readSize = min(size,((startAddress_+contentSize_) - offset));
		char* cacheAddress = content_ + (offset - startAddress_);
		memcpy(buf,cacheAddress,readSize);
	} else {
		pthread_mutex_unlock(&bufferLock_);
		return UNAVAILABLE;
	}
	pthread_mutex_unlock(&bufferLock_);
	return readSize;
}

long long BufferTemplate::writeToBuffer(const char* buf, long long size, long long offset)
{
	long long writeSize;
	long long dirtyStart;
	long long dirtyEnd;
	pthread_mutex_lock(&bufferLock_);
	if(containOffset(offset)){
		checkWriteAvailability(size,offset);
		writeSize = min(size,((startAddress_+contentSize_) - offset));
		char* cacheAddress = content_ + (offset - startAddress_);
		memcpy(cacheAddress,buf,writeSize);
		dirtyStart = (offset - startAddress_) / blockSize_;
		dirtyEnd = (offset - startAddress_ + writeSize) / blockSize_;
		if(((offset - startAddress_ + writeSize) % blockSize_) == 0)
			--dirtyEnd;
		for(long long i = dirtyStart; i <= dirtyEnd; ++i)
			dirtyBlock_[i] = DIRTY;
		dirtyBit_ = DIRTY;
	
	} else {
		pthread_mutex_unlock(&bufferLock_);
		return UNAVAILABLE;
	}
	pthread_mutex_unlock(&bufferLock_);
	return writeSize;
}

bool BufferTemplate::containOffset(long long offset)
{
	return ((offset >= startAddress_) && offset < startAddress_ + (contentSize_));
}

void BufferTemplate::writeBack()
{
	if(!doWrite_)
		return;
	if(id_ == EMPTY)
		return;
	if(dirtyBit_ == CLEAN)
		return;
	pthread_mutex_lock(&bufferLock_);
	cacheLayer->addThreadJob(this,WRITEBACK);
}

void BufferTemplate::prefetch(long long offset)
{
	pthread_mutex_lock(&bufferLock_);
	cacheLayer->addThreadJob(this,PREFETCH,offset);
}

void BufferTemplate::fetch(long long offset)
{
	pthread_mutex_lock(&bufferLock_);
	cacheLayer->addThreadJob(this,FETCH,offset);
	//logFunc("%lld\n",offset);
}

void BufferTemplate::doWriteBack()
{
	long long retstat;
	long long i,j;
	long long dirtyStart = -1;
	char* contentAddress;
	struct timeval t1, t2;
	double duration;
	for(i = 0; i < numOfBlock_; ++i)
	{
		if(dirtyBlock_[i] == DIRTY){
			if(dirtyStart == -1)
				dirtyStart = i;
			dirtyBlock_[i] = PROCESSING;
		} else {
			if(dirtyStart == -1) continue;
			contentAddress = content_ + (blockSize_ * dirtyStart);
			if (NCFS_DATA->run_experiment == 1){
				gettimeofday(&t1,NULL);
			}

			retstat = doWrite(contentAddress, (i - dirtyStart) * blockSize_, startAddress_ + (blockSize_ * dirtyStart));

			if (NCFS_DATA->run_experiment == 1){
				gettimeofday(&t2,NULL);

				duration = (t2.tv_sec - t1.tv_sec) +
					(t2.tv_usec - t1.tv_usec)/1000000.0;
				NCFS_DATA->diskread_time += duration;
			}


				for( j = dirtyStart; j < i; ++j)
					dirtyBlock_[j] = CLEAN;
				dirtyStart = -1;
		}
	}

	i = numOfBlock_;
	if(dirtyStart != -1){
		contentAddress = (char*)content_ +
			(blockSize_ * dirtyStart);

		if (NCFS_DATA->run_experiment == 1){
			gettimeofday(&t1,NULL);
		}

		doWrite(contentAddress, (i - dirtyStart) * blockSize_, startAddress_ + (blockSize_ * dirtyStart));

		if (NCFS_DATA->run_experiment == 1){
			gettimeofday(&t2,NULL);

			duration = (t2.tv_sec - t1.tv_sec) +
				(t2.tv_usec - t1.tv_usec)/1000000.0;
			NCFS_DATA->diskread_time += duration;
		}

		for( j = dirtyStart; j < i; ++j)
			dirtyBlock_[j] = CLEAN;
		dirtyStart = -1;

	}
	dirtyBit_ = CLEAN;
	pthread_mutex_unlock(&bufferLock_);
}

void BufferTemplate::doPrefetch(long long offset){
	clearContent();
	bufferAlignment(offset);
	fetchContent();
	pthread_mutex_unlock(&bufferLock_);
}

void BufferTemplate::doFetch(long long offset){
	doPrefetch(offset);
}

void BufferTemplate::waitJob()
{
	pthread_mutex_lock(&bufferLock_);
	pthread_mutex_unlock(&bufferLock_);
}

BufferTemplate::~BufferTemplate()
{
	writeBack();
	waitJob();
	free(content_);
	delete [] dirtyBlock_;
	delete [] pageStatus_;
}

void BufferTemplate::clearContent()
{
	memset(content_,0,bufferSize_);
	dirtyBit_ = CLEAN;
	for(int i = 0; i < numOfBlock_; ++i)
		dirtyBlock_[i] = CLEAN;
	for(int i = 0; i < numOfPage_; ++i)
		pageStatus_[i] = UNFETCHED;
	startAddress_ = -1;
	contentSize_ = 0;
}

void BufferTemplate::checkReadAvailability(long long size, long long offset)
{
	checkAvailability(size,offset,true);
}

void BufferTemplate::checkWriteAvailability(long long size, long long offset)
{
	checkAvailability(size,offset,false);
	//checkAvailability(size,offset,true);
}

void BufferTemplate::checkAvailability(long long size, long long offset, bool alwaysRead)
{
	long long startPage = (offset - startAddress_) / pageSize_;
	long long endAddress = min(offset + size, startAddress_ + contentSize_);
	long long endPage = (endAddress - startAddress_)/ pageSize_;
	if((endAddress % pageSize_) == 0)
		--endPage;
	if((offset == (startAddress_ + startPage * pageSize_))&&(size >= pageSize_))
		fetchOnePage(startPage,alwaysRead);
	else
		fetchOnePage(startPage,true);
	if(endPage > startPage){
		if((endAddress % pageSize_) == 0)
			fetchOnePage(endPage,alwaysRead);
		else
			fetchOnePage(endPage,true);
		fetchPages(startPage + 1, endPage - 1, alwaysRead);
	}
}

long long BufferTemplate::doRead(char* buf, long long size, long long offset)
{
	logFunc("%s %lld %lld\n",path_,size,offset);
	if(fd_ < 0)
		return FATALERROR;
	return pread(fd_,buf,size,offset);
}

long long BufferTemplate::doWrite(const char* buf, long long size, long long offset)
{
	logFunc("%s %lld %lld\n",path_,size,offset);
	if(fd_ < 0)
		return FATALERROR;
	long long retstat;
	retstat =  pwrite(fd_,buf,size,offset);
	return retstat;
}

long long BufferTemplate::fetchPages(long long startPage,long long endPage, bool readContent)
{
	long long count = 0;
	bool successful;

	for(long long i = startPage; i <= endPage; ++i){
		successful = fetchOnePage(i,readContent);
		if(successful)
			++count;
		else
			return FATALERROR;
	}
	return count;
}

bool BufferTemplate::fetchOnePage(long long pageNum, bool readContent)
{
	if(pageStatus_[pageNum] == FETCHED)
		return true;
	if(readContent){
		char* cacheAddress = content_ + (pageNum * pageSize_);
		long long retstat = doRead(cacheAddress,pageSize_,startAddress_ + (pageNum * pageSize_));
		if(retstat < 0){
			exit(-1);
			return false;
		}
	}
	pageStatus_[pageNum] = FETCHED;
	return true;
}

void BufferTemplate::bufferAlignment(long long offset)
{
	startAddress_ = (offset / bufferSize_) * bufferSize_;
}

void BufferTemplate::fetchContent()
{
	contentSize_ = bufferSize_;
}
