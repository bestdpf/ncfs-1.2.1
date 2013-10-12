#ifndef __CACHE_BASE_HH__
#define __CACHE_BASE_HH__

#include "cache_log.hh"

class CacheLayerBase{
	public:
		/** 
		 * Read from Disk through the Cache
		 *
		 * @param disk_id	ID of the disk to read
		 * @param buf	pointer to the buffer
		 * @param size	number of bytes to read
		 * @param offset	read offset
		 *
		 * @return number of bytes sucessfully read 
		 */
		virtual long long readDisk(int disk_id, char* buf, long long size, long long offset) = 0;

		/** 
		 * Write to Disk through the Cache
		 *
		 * @param disk_id	ID of the disk to write
		 * @param buf	pointer to the buffer
		 * @param size	number of bytes to write
		 * @param offset	write offset
		 *
		 * @return number of bytes sucessfully written
		 */
		virtual long long writeDisk(int disk_id, const char* buf, long long size,	long long offset) = 0;

		/**
		 * Set the Disk Name of a specified disk,
		 * for recovery usage
		 *
		 * @param disk_id	ID of the disk to be set
		 * @param newdevice	new Disk Name
		 */
		virtual void setDiskName(int disk_id, const char* newdevice) = 0;

		/**
		 * Create a File Info Cache for a file
		 *
		 * @param path 	path of the file
		 * @param id 	ID of the file
		 */
		virtual void createFileInfo(const char* path, int id) = 0;

		/**
		 * Read File Info through Cache
		 *
		 * @param id	ID of the file info cache
		 * @param buf	pointer to the buffer
		 * @param size	number of bytes to read
		 * @param offset 	read offset
		 *
		 * @return number of bytes sucessfully read
		 */
		virtual int readFileInfo(int id, char* buf,long long size, long long offset) = 0;

		/**
		 * Write File Info through Cache
		 *
		 * @param id 	ID of the file info cache
		 * @param buf	pointer to the buffer
		 * @param size 	number of bytes to write
		 * @param offset 	write offset
		 *
		 * @return number of bytes sucessfully written
		 */
		virtual int writeFileInfo(int id, const char* buf,long long size, long long offset) = 0;

		/**
		 * Search the File Info Cache ID of a File 
		 *
		 * @param path	path of the file
		 * 
		 * @return the File Info Cache ID, UNAVAILABLE indicates file is not cached
		 */
		virtual int searchFileInfo(const char* path) = 0;

		/**
		 * Remove a File Info Cache
		 *
		 * @param id	ID of the file
		 */
		virtual void destroyFileInfo(int id) = 0;

		/**
		 * Create a File Cache for a file
		 *
		 * @param path	path of the file
		 * @param id	ID of the file cache
		 */
		virtual void createFileCache(const char* path, int id) = 0;

		/**
		 * Read File through Cache
		 *
		 * @param id	ID of the file cache
		 * @param buf	pointer to the buffer
		 * @param size	number of bytes to read
		 * @param offset	read offset
		 *
		 * @return number of bytes sucessfully read
		 */
		virtual int readFileCache(int id, char* buf, long long size, long long offset) = 0;

		/**
		 * Write File through Cache
		 *
		 * @param id	ID of the file cache
		 * @param buf	pointer to the buffer
		 * @param size	number of bytes to write
		 * @param offset	write offset
		 *
		 * @return number of bytes sucessfully written
		 */
		virtual int writeFileCache(int id, const char* buf,long long size, long long offset) = 0;

		/**
		 * Remove a File Cache
		 *
		 * @param id	ID of the file cache
		 */
		virtual void destroyFileCache(int id) = 0;
};
#endif
