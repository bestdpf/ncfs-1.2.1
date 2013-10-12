#include "blockstorage.hh"
#include "../filesystem/filesystem_common.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../gui/process_report.hh"
#include "../gui/diskusage_report.hh"
#include "../filesystem/filesystem_utils.hh"

extern struct ncfs_state* NCFS_DATA;
extern FileSystemLayer* fileSystemLayer;
extern ProcessReport *processReport;



/*************************************************************************
 * Private functions
 *************************************************************************/
/*       
 * search_disk_id: Search for disk id provided by device path
 *
 * @param path: device path
 *
 * @return: 
 */
int BlockStorage::search_disk_id(const char* path)
{
	int i;
	for(i = 0; i < NCFS_DATA->disk_total_num; ++i){                    
		if(strcmp(NCFS_DATA->dev_name[i],path) == 0)
			return i;             
	}            
	return -1;
}


/*************************************************************************
 * Public functions
 *************************************************************************/
/*
 * Constructor
 */
BlockStorage::BlockStorage() {
  
	//int i;

	disk_fd = (int*)malloc(NCFS_DATA->disk_total_num * sizeof(int));

	//for (i=0; i < NCFS_DATA->disk_total_num; i++){
	//	disk_fd[i] = open(NCFS_DATA->dev_name[i],O_RDWR);
	//	printf("***BlockStorage(): device=%s, fd=%d\n",NCFS_DATA->dev_name[i],disk_fd[i]);
	//}
}

/*
 * Destructor
 */
BlockStorage::~BlockStorage() {
        //int i;

        //for (i=0; i < NCFS_DATA->disk_total_num; i++){
        //        close(disk_fd[i]);
	//	printf("*** ~BlockStorage(): close fd: %d\n",disk_fd[i]);
        //}

	//free(disk_fd);
}



int BlockStorage::DiskRenew(int disk_id) {
	int fd;
	fd = open(NCFS_DATA->dev_name[disk_id],O_RDWR);
	disk_fd[disk_id] = fd;
	return fd;
}


/*
 * DiskRead: Read data from disk
 * 
 * @param path: file path
 * @param buf: data buffer
 * @param size: size
 * @param offset: offset
 * @return: size of successful read
 */

/*
int BlockStorage::DiskRead(const char* path, char* buf, long long size, 
		long long offset){
	//new ProcessItem(DISK2CACHE,true);
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,true);
	}

	//remarked for skip-ahead test
	//int fd = open(path,O_RDONLY);

	int i;
	int fd;
	int flag_data;

	flag_data = 0;
	//printf("StorageLayer:DiskRead path:%s\n",path);
	for (i=0; i < NCFS_DATA->disk_total_num; i++){
		if (strcmp(path,NCFS_DATA->dev_name[i]) == 0){
			fd = disk_fd[i];
			flag_data = 1;
			//printf("***DiskRead: fd = %d\n", fd);
		}
	}

	if (flag_data == 0){
		fd = open(path,O_RDONLY);
	}

	if(fd < 0){
		int id = search_disk_id(path);
		printf("///DiskRead %s failed\n",path);
		fileSystemLayer->set_device_status(id,1);
//		new ProcessItem(DISK2CACHE,false);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return fd;
	}
	int retstat = pread(fd,buf,size,offset);
	if(retstat <= 0){
		int id = search_disk_id(path);
		//NCFS_DATA->disk_status[id] = 1;
		fileSystemLayer->set_device_status(id,1);
		printf("///DiskRead %s failed\n",path);
		close(fd);
//		new ProcessItem(DISK2CACHE,false);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return retstat;
	}

	//remarked for skip-ahead test
	//close(fd);

	if (flag_data == 0){
		close(fd);
	}

	//printf("///DiskRead path %s, size %lld, offset %lld, retstat %d\n",path,size,offset,retstat);
//	new ProcessItem(DISK2CACHE,false);
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,false);
	}
	return retstat;
}
*/

int BlockStorage::DiskRead(const char* path, char* buf, long long size, 
		long long offset){
  
	//new ProcessItem(DISK2CACHE,true);
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,true);
	}

	int fd = open(path,O_RDONLY);

	if(fd < 0){
		int id = search_disk_id(path);
		fileSystemLayer->set_device_status(id,1);

		printf("///DiskRead %s open failed\n",path);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return fd;
	}
	
	int retstat = pread(fd,buf,size,offset);
	if(retstat <= 0){
		int id = search_disk_id(path);
		fileSystemLayer->set_device_status(id,1);
		printf("///DiskRead %s failed\n",path);
		close(fd);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return retstat;
	}

	close(fd);

	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,false);
	}
	return retstat;
}
/*
 * DiskWrite: Write data from disk
 * 
 * @param path: file path
 * @param buf: data buffer
 * @param size: size
 * @param offset: offset
 * @return: size of successful write
 */

/*
int BlockStorage::DiskWrite(const char* path, const char* buf, long long size, 
		long long offset){
//	new ProcessItem(DISK2CACHE,true);
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,true);
	}

	//remarked for skip-ahead test
	//int fd = open(path,O_WRONLY);

	int i;
        int fd;
	int flag_data;

	flag_data = 0;
//	printf("StorageLayer: DiskWrite\n");
        for (i=0; i < NCFS_DATA->disk_total_num; i++){
                if (strcmp(path,NCFS_DATA->dev_name[i]) == 0){
                        fd = disk_fd[i];
			flag_data = 1;
                        //printf("***DiskWrite: fd = %d\n", fd);
                }
        }

	if (flag_data == 0){
		fd = open(path,O_WRONLY);
	}

	if(fd < 0){
		int id = search_disk_id(path);
		fileSystemLayer->set_device_status(id,1);
//		NCFS_DATA->disk_status[id] = 1;
		printf("///DiskWrite %s open failed\n",path);
//		new ProcessItem(DISK2CACHE,false);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return fd;
	}
	int retstat = pwrite(fd,buf,size,offset);
	if(retstat <= 0){
		int id = search_disk_id(path);
//		NCFS_DATA->disk_status[id] = 1;
		fileSystemLayer->set_device_status(id,1);
		printf("///DiskWrite %s write failed\n",path);
		close(fd);
//		new ProcessItem(DISK2CACHE,false);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return retstat;
	}

	//remarked for skip-ahead test
	//close(fd);

	if (flag_data == 0){
		close(fd);
	}

	//printf("///DiskWrite path %s, size %lld, offset %lld, retstat %d\n",
	//		path,size,offset,retstat);
//	new ProcessItem(DISK2CACHE,false);
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,false);
	}
	return retstat;
}
*/
int BlockStorage::DiskWrite(const char* path, const char* buf, long long size, 
		long long offset){

	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,true);
	}

        int fd = open(path,O_WRONLY);

	if(fd < 0){
		int id = search_disk_id(path);
		fileSystemLayer->set_device_status(id,1);

		printf("///DiskWrite %s open failed\n",path);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return fd;
	}
	int retstat = pwrite(fd,buf,size,offset);
	if(retstat <= 0){
		int id = search_disk_id(path);
		fileSystemLayer->set_device_status(id,1);
		printf("///DiskWrite %s write failed\n",path);
		close(fd);
		if (NCFS_DATA->no_gui == 0){
			processReport->Update(DISK2CACHE,false);
		}
		return retstat;
	}

        //Add by zhuyunfeng on Sep 4, 2011 begin.
	//Write data to the disk immediately
	fsync(fd);
	//Add by zhuyunfeng on Sep 4, 2011 end.
	

	close(fd);

	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,false);
	}
	return retstat;
}

long long BlockStorage::readDisk(int id, char* buf, long long size, long long offset)
{
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,true);
	}
	int fd = disk_fd[id];
	int retstat = pread(fd,buf,size,offset);
	if(retstat <= 0){
		fileSystemLayer->set_device_status(id,1);
		close(fd);
	}
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,false);
	}
	return retstat;
}

long long BlockStorage::writeDisk(int id, const char* buf, long long size, long long offset)
{
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,true);
	}
	int fd = disk_fd[id];
	int retstat = pwrite(fd,buf,size,offset);
	if(retstat <= 0){
		fileSystemLayer->set_device_status(id,1);
		close(fd);
	}
	if (NCFS_DATA->no_gui == 0){
		processReport->Update(DISK2CACHE,false);
	}
	return retstat;
}
