#ifndef __CACHE_HH__
#define __CACHE_HH__

#define FILE_MAX	1024
#define DEFAULT_FILE_CACHE_NUM		10
#define DEFAULT_FILEINFO_CACHE_NUM	20

#include "cache_base.hh"
#include "cache_file.hh"
#include "cache_fileinfo.hh"
#include "cache_disk.hh"
#include "cache_threadpool.hh"
#include "cache_log.hh"
#include "cache_buffer.hh"

class CacheLayer : public CacheLayerBase{
	public:
		CacheLayer();
		CacheLayer(struct ncfs_state* ncfs_data);

		long long readDisk(int disk_id, char* buf, long long size, long long offset);
		long long writeDisk(int disk_id, const char* buf, long long size,	long long offset);
		void setDiskName(int disk_id, const char* newdevice);
		/*
		void flushDisks(long long offset);
		typedef struct{
			DiskCache* targetDiskCache_;
			long long offset_;
		} flushArg;
		static void* flushThread(void* data);
		*/
		void createFileInfo(const char* path, int id);
		int readFileInfo(int id, char* buf,long long size, long long offset);
		int writeFileInfo(int id, const char* buf,long long size, long long offset);
		int searchFileInfo(const char* path);
		void destroyFileInfo(int id);

		void createFileCache(const char* path, int id);
		int readFileCache(int id, char* buf, long long size, long long offset);
		int writeFileCache(int id, const char* buf,long long size, long long offset);
		void destroyFileCache(int id);

		void addThreadJob(BufferTemplate* targetBuffer, BufferJobType job,long long offset = -1);
		~CacheLayer();
	private:
		CacheThreadPool threadPool_;

		int numOfDisk_;
		int fileCacheIndex_[FILE_MAX];
		int fileInfoCacheIndex_[FILE_MAX];

		FileCache*		fileCache_;
		int numOfFileCache_;
		int lastAllocatedFileCache_;
		int searchFileCache(const char* path);
		int allocateFileCache();

		FileInfoCache* 	fileInfoCache_;
		int numOfFileInfoCache_;
		int lastAllocatedFileInfoCache_;
		int searchFileInfoCache(const char* path);
		int allocateFileInfoCache();

		DiskCache*	diskCache_;

		void init(struct ncfs_state* NCFS_DATA);
};
#endif
