#ifndef __CACHE_BUFFER_HH__
#define __CACHE_BUFFER_HH__
#include "cache_log.hh"
#include <pthread.h>

typedef enum {CLEAN = 0, DIRTY, PROCESSING} BlockStatus;
typedef enum {UNFETCHED = 0, FETCHED} PageStatus;

static inline long long min (long long a, long long b){
	return a < b ? a : b;
}

class BufferTemplate{
	public:
		BufferTemplate();
		void init(char* path, int fd, int id, bool doClear);
		void initSizes(long long blockSize, long long bufferSize, long long pageSize, bool doWrite);

		long long readFromBuffer(char* buf, long long size, long long offset);
		long long writeToBuffer(const char* buf, long long size, long long offset);

		bool containOffset(long long offset);

		void writeBack();
		void prefetch(long long offset);
		void fetch(long long offset);

		void doWriteBack();
		void doPrefetch(long long offset);
		void doFetch(long long offset);

		void waitJob();

		~BufferTemplate();
	protected:
		void clearContent();

		void checkReadAvailability(long long size, long long offset);
		void checkWriteAvailability(long long size, long long offset);
		void checkAvailability(long long size, long long offset, bool alwaysRead);

		virtual long long doRead(char* buf, long long size, long long offset);
		virtual long long doWrite(const char* buf, long long size, long long offset);

		long long fetchPages(long long startPage, long long endPage, bool readContent);
		bool fetchOnePage(long long pageNum, bool readContent);

		void bufferAlignment(long long offset);
		void fetchContent();
		
		long long blockSize_;
		long long bufferSize_;
		long long pageSize_;
		long long numOfBlock_;
		long long numOfPage_;
		bool doWrite_;

		char* content_;
		BlockStatus dirtyBit_;
		BlockStatus* dirtyBlock_;
		PageStatus* pageStatus_;

		char* path_;
		int fd_;
		int id_;

		long long startAddress_;
		long long contentSize_;

		pthread_mutex_t bufferLock_;
};
#endif
