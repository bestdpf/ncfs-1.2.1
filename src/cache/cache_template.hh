#ifndef __CACHE_TEMPLATE_HH__
#define __CACHE_TEMPLATE_HH__

#include <limits.h>
#include "cache_buffer.hh"
#include "cache_log.hh"
#include "../config/config.hh"

#define CACHE_DEFAULT_SIZE			4*1024*1024
#define CACHE_DEFAULT_BLOCK_SIZE	4*1024
#define CACHE_DEFAULT_PAGE_SIZE		4*1024
#define CACHE_DEFAULT_BUFFER_SIZE	4	

extern ConfigLayer* configLayer;

class CacheTemplate{
	public:
		CacheTemplate();

		virtual void initSizes(long long blockSize,	long long cacheSize, long long pageSizeE, int numOfBuffer, bool doWrite) = 0;

		void init(const char* path, int id);

		long long write(const char* buf, long long size,	long long offset);
		long long read(char* buf, long long size, long long offset);

		void setEmpty();
		bool isEmpty();
		int getId();

		char* getPath(void);
		void setPath(const char* path);

		~CacheTemplate();
	protected:
		void preOpen();

		void emptyBuffer();
		void flushBuffer();

		void flush(long long offset);
		
		virtual void lockMutex();
		virtual void unlockMutex();

		char path_[PATH_MAX];
		int id_;
		int fd_;
		int numOfUser_;
		int numOfBuffer_;
		long long bufferSize_;

		bool preOpen_;

		pthread_mutex_t cacheLock_;

		BufferTemplate* buffer_;
		BufferTemplate* workingBuffer_;
		int currentPos_;
};
#endif
