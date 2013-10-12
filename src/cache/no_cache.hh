#ifndef __CACHE_DUMMY_HH__
#define __CACHE_DUMMY_HH__

#define FILE_MAX	1024
#include "cache_base.hh"
#include "limits.h"

class NoCache : public CacheLayerBase{
	public:
		NoCache();
		NoCache(struct ncfs_state* ncfs_data);

		long long readDisk(int disk_id, char* buf, long long size, long long offset);
		long long writeDisk(int disk_id, const char* buf, long long size,	long long offset);
		void setDiskName(int disk_id, const char* newdevice);
		void createFileInfo(const char* path, int id);
		int readFileInfo(int id, char* buf,long long size, long long offset);
		int writeFileInfo(int id, const char* buf,long long size, long long offset);
		int searchFileInfo(const char* path);
		void destroyFileInfo(int id);

		void createFileCache(const char* path, int id);
		int readFileCache(int id, char* buf, long long size, long long offset);
		int writeFileCache(int id, const char* buf,long long size, long long offset);
		void destroyFileCache(int id);

		~NoCache();
	protected:
		char fileInfoPath_[FILE_MAX][PATH_MAX];
		void init(struct ncfs_state* NCFS_DATA);
};
#endif
