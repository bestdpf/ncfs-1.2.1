#include "cache_template.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

CacheTemplate::CacheTemplate()
{
	memset(path_,0,PATH_MAX);
	id_ = EMPTY;
	fd_ = EMPTY;
	numOfUser_ = 0;
	numOfBuffer_ = 0;

	pthread_mutex_init(&cacheLock_,NULL);
	preOpen_ = false;
}

void CacheTemplate::initSizes(long long blockSize, long long bufferSize, long long pageSize, int numOfBuffer, bool doWrite)
{
	numOfBuffer_ = numOfBuffer;
	bufferSize_ = bufferSize;
}

void CacheTemplate::init(const char* path, int id)
{
	pthread_mutex_lock(&cacheLock_);
	setPath(path);
	if(id != EMPTY){
		++numOfUser_;
	}
	if(id_ != id){
		emptyBuffer();
	}
	if((id!= EMPTY) && preOpen_)
		preOpen();

	id_ = id;
	for(int i = 0; i < numOfBuffer_; ++i)
		buffer_[i].init(path_,fd_,id,false);
	pthread_mutex_unlock(&cacheLock_);
}

long long CacheTemplate::write(const char* buf, long long size, long long offset)
{
	long long writtenSize = 0;
	long long retstat;
	const char* bufAddress;
	lockMutex();
	while(writtenSize < size){
		bufAddress = buf + writtenSize;
		retstat = workingBuffer_->writeToBuffer(bufAddress,size - writtenSize,offset + writtenSize);
		if(retstat == UNAVAILABLE)
			flush(offset + writtenSize);
		else if(retstat < 0)
			logErr("%d: %lld %lld\n",id_,size,offset);
		else if(retstat < (size - writtenSize))
			flush(offset + writtenSize + retstat);
		if(retstat > 0)
			writtenSize += retstat;
	}
	unlockMutex();
	return writtenSize;
}

long long CacheTemplate::read(char* buf, long long size, long long offset)
{
	long long readSize = 0;
	long long retstat;
	char* bufAddress;
	lockMutex();
	while(readSize < size){
		bufAddress = buf + readSize;
		retstat = workingBuffer_->readFromBuffer(bufAddress,size - readSize,offset + readSize);
		if(retstat == UNAVAILABLE)
			flush(offset + readSize);
		else if(retstat < 0)
			logErr("%d: %lld %lld\n",id_,size,offset);
		else if(retstat < (size - readSize))
			flush(offset + readSize + retstat);
		if(retstat > 0)
			readSize += retstat;
	}
	unlockMutex();
	return readSize;
}

void CacheTemplate::setEmpty(){
	pthread_mutex_lock(&cacheLock_);
	--numOfUser_;
	if(numOfUser_ == 0){
		flushBuffer();
		id_ = EMPTY;
	}
	pthread_mutex_unlock(&cacheLock_);
}

bool CacheTemplate::isEmpty(){
	return id_ == EMPTY;
}

int CacheTemplate::getId(){
	return id_;
}

char* CacheTemplate::getPath(){
	return path_;
}

void CacheTemplate::setPath(const char* path)
{
	memset(path_,0,PATH_MAX);
	strncpy(path_,path,PATH_MAX);
}

CacheTemplate::~CacheTemplate()
{
	emptyBuffer();
	delete [] buffer_;
}

void CacheTemplate::preOpen()
{
//	logFunc("%p %s\n",path_,path_);
	fd_ = open(path_,O_RDWR | O_CREAT, 0666);
	if(fd_ < 0){
		fd_ = FATALERROR;
		logFunc("Cannot pre-open %s\n",path_);	
		exit(-1);
	}
}

void CacheTemplate::emptyBuffer()
{
	//Write Back all buffers and wait unit finished
	flushBuffer();
	for(int i = 0; i < numOfBuffer_; ++i)
		buffer_[i].waitJob();

	if(fd_ != EMPTY){
		close(fd_);
	}
}

void CacheTemplate::flushBuffer()
{
	for(int i = 0; i < numOfBuffer_; ++i)
		buffer_[i].writeBack();
}

void CacheTemplate::flush(long long offset)
{
	logFunc("%s offset %lld\n",path_,offset);
	workingBuffer_->writeBack();
	bool found = false;
	int oldPos = currentPos_;
	for(int i = 1; i < numOfBuffer_; ++i){
		currentPos_ = (oldPos + i) % numOfBuffer_;
		workingBuffer_ = &buffer_[currentPos_];
		if(workingBuffer_->containOffset(offset)){
			found = true;
			break;
		}
	}
	logFunc("found ? %d\n",found);
	if(!found){
		currentPos_ = (oldPos + 1) % numOfBuffer_;
		workingBuffer_ = &buffer_[currentPos_];
		workingBuffer_->fetch(offset);
	}
	logFunc("Wait\n");

	workingBuffer_->waitJob();

	BufferTemplate* prefetchBuffer = &buffer_[((currentPos_+1) % numOfBuffer_)];
	if(!prefetchBuffer->containOffset(offset + bufferSize_))
		prefetchBuffer->prefetch(offset + bufferSize_);
	logFunc("done\n");
}

void CacheTemplate::lockMutex(){
	pthread_mutex_lock(&cacheLock_);
}

void CacheTemplate::unlockMutex(){
	pthread_mutex_unlock(&cacheLock_);
}
