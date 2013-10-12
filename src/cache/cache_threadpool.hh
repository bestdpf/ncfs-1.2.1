#ifndef __CACHE__THREADPOOL_HH__
#define __CACHE__THREADPOOL_HH__

#include <pthread.h>
#include <queue>
using namespace std;
#include "cache_buffer.hh"

#define CACHE_THREADPOOL_DEFAULT_NUM	4

typedef enum {PREFETCH,WRITEBACK,FETCH} BufferJobType; 

class CacheThreadPool{
	public:
		CacheThreadPool();
		typedef struct {
			BufferTemplate* targetBuffer_;
			BufferJobType job_;
			long long offset_;
		} JobItem;

		static void* threadFunction(void* data);
		void startThreadPool();
		void addJob(BufferTemplate* targetBuffer, BufferJobType job,long long offset = -1);
		bool fetchOneJob(JobItem* item);
		~CacheThreadPool();
	private:
		void processJob(BufferTemplate* targetBuffer, BufferJobType job, long long offset = -1);
		queue<JobItem> jobQueue_;
		pthread_t* tid_;
		pthread_mutex_t queueLock_;
		pthread_cond_t waitSignal_;
		int numOfThread_;
		bool ended_;
};
#endif
