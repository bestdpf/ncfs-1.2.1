#include "cache_threadpool.hh"
#include "../config/config.hh"

extern ConfigLayer* configLayer;

void* CacheThreadPool::threadFunction(void* data)
{
	JobItem job;
	CacheThreadPool *This = (CacheThreadPool*)data;

	while(1)
	{
		if(!This->fetchOneJob(&job))
			break;
		else
			This->processJob(job.targetBuffer_,job.job_,job.offset_);
	}
	return NULL;
	logFunc("Thread ended\n");
}

CacheThreadPool::CacheThreadPool()
{
	numOfThread_ = configLayer->getConfigInt("Cache>ThreadPool>NumOfThread");
	if(numOfThread_ < 0)
		numOfThread_ = CACHE_THREADPOOL_DEFAULT_NUM;

	pthread_cond_init(&waitSignal_,NULL);
	pthread_mutex_init(&queueLock_,NULL);
	ended_ = false;
}

void CacheThreadPool::startThreadPool()
{
	tid_ = new pthread_t[4];
	int retstat;
	for(int i = 0; i < numOfThread_; ++i){
		retstat = pthread_create(&tid_[i],NULL,threadFunction,this);	
		logFunc("Thread Create %d %d\n",i,retstat);
		if(retstat < 0){
			perror("pthread_create()");
			exit(-1);
		}
	}
}

void CacheThreadPool::addJob(BufferTemplate* targetBuffer,BufferJobType job,long long offset)
{
	pthread_mutex_lock(&queueLock_);
	JobItem newItem;
	newItem.targetBuffer_ = targetBuffer;
	newItem.job_ = job;
	newItem.offset_ = offset;

	jobQueue_.push(newItem);
//	pthread_cond_broadcast(&waitSignal_);
	pthread_cond_signal(&waitSignal_);
	pthread_mutex_unlock(&queueLock_);
}

bool CacheThreadPool::fetchOneJob(JobItem* item)
{
	pthread_mutex_lock(&queueLock_);
	do{
		
		if(jobQueue_.empty())
			if(!ended_){
	//			logFunc("Wait Job\n");
				pthread_cond_wait(&waitSignal_,&queueLock_);
			}
	//	logFunc("Waken\n");

		//Waken up but no job = End
		if(jobQueue_.empty() && ended_){
			pthread_mutex_unlock(&queueLock_);
	//		logFunc("Ended\n");
			return false;
		} else if(jobQueue_.empty()){
	//		logFunc("Loop\n");
			continue;
		}
		else {
			item->targetBuffer_ = jobQueue_.front().targetBuffer_;
			item->job_ = jobQueue_.front().job_;
			item->offset_ = jobQueue_.front().offset_;
			jobQueue_.pop();
	//		logFunc("Get job %d %lld\n",item->job_,item->offset_);
			break;
		}
	}while(0);
	pthread_mutex_unlock(&queueLock_);
	return true;
}

void CacheThreadPool::processJob(BufferTemplate* targetBuffer, BufferJobType job,long long offset)
{
	//logFunc("Thread working\n");
	switch(job)
	{
		case PREFETCH:
			targetBuffer->doPrefetch(offset);
			break;
		case FETCH:
			targetBuffer->doFetch(offset);
			break;
		case WRITEBACK:
			targetBuffer->doWriteBack();
			break;
	}
}

CacheThreadPool::~CacheThreadPool()
{
	ended_ = true;
	pthread_cond_broadcast(&waitSignal_);
	for(int i = 0; i < numOfThread_; ++ i)
		pthread_join(tid_[i],NULL);
	delete [] tid_;
}
