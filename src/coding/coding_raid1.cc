#include "coding_raid1.hh"
#include "../filesystem/filesystem_common.hh"
#include "../filesystem/filesystem_utils.hh"
#include "../cache/cache.hh"
#include "../gui/diskusage_report.hh"

#include <stdio.h>
#include <stdlib.h>
#include <fuse.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

//NCFS context state
extern struct ncfs_state* NCFS_DATA;
extern FileSystemLayer* fileSystemLayer;
extern CacheLayer* cacheLayer;
extern DiskusageReport* diskusageLayer;

/*
 * raid1_encoding: RAID 1: fault tolerance by mirror (type=1)
 *
 * @param buf: buffer of data to be written to disk
 * @param size: size of the data buffer
 *
 * @return: assigned data block: disk id and block number.
 */
struct data_block_info Coding4Raid1::encode(const char* buf, int size)
{

	int retstat, disk_id, block_no, disk_total_num, block_size;
	//char *dev, *dev2;
	//int datafd, datafd2;
	int size_request, block_request;
	struct data_block_info block_written;
	int i;
	int mirror_disk_id;

	//test print
	//printf("***get_data_block_no 1: size=%d\n",size);

	disk_total_num = NCFS_DATA->disk_total_num;
	block_size = NCFS_DATA->disk_block_size;

	size_request = fileSystemLayer->round_to_block_size(size);
	block_request = size_request / block_size;

	//test print
	//printf("***get_data_block_no 2: size_request=%d, block_request=%d\n"
	//		,size_request,block_request);


	//implement disk write algorithm here.
	//here use raid1: mirroring. (with jbod)

	block_no = 0;
	disk_id = -1;
	i = 0;
	//first n/2 disks are data disks
	while ((i < disk_total_num/2) && (disk_id == -1)){
		if (block_request <= (NCFS_DATA->free_size[i]))
		{
			disk_id = i;
			block_no = NCFS_DATA->free_offset[i];
		}
		i++;
	}


	//get block from space_list if no free block available
	if (disk_id == -1){
		if (NCFS_DATA->space_list_head != NULL){
			disk_id = NCFS_DATA->space_list_head->disk_id;
			block_no = NCFS_DATA->space_list_head->disk_block_no;
			fileSystemLayer->space_list_remove(disk_id,block_no);
		}
	}


	if (disk_id == -1){
		printf("***get_data_block_no: ERROR disk_id = -1\n");
	}
	else{
		//Cache Start
		retstat = cacheLayer->writeDisk(disk_id,buf,size,block_no*block_size);
		//Cache End

		NCFS_DATA->free_offset[disk_id] = block_no + block_request;
		NCFS_DATA->free_size[disk_id]
			= NCFS_DATA->free_size[disk_id] - block_request;

		//write data to mirror disk
		mirror_disk_id = disk_total_num - disk_id - 1;
		//Cache Start
		retstat = cacheLayer->writeDisk(mirror_disk_id,buf,size,block_no*block_size);
		//Cache End
	}

	block_written.disk_id = disk_id;
	block_written.block_no = block_no;

	return block_written;  
}


/*
 * decoding_raid1: RAID 1 decode
 *
 * @param disk_id: disk id
 * @param buf: data buffer for reading data
 * @param size: size of the data buffer
 * @param offset: offset of data on disk
 *
 * @return: size of successful decoding (in bytes)
 */
int Coding4Raid1::decode(int disk_id, char* buf, long long size, long long offset)
{

	if(NCFS_DATA->disk_status[disk_id] == 0)
		return cacheLayer->readDisk(disk_id,buf,size,offset);
	else {
		int mirror_disk_id = NCFS_DATA->disk_total_num - disk_id - 1;
		if(NCFS_DATA->disk_status[mirror_disk_id] == 0){
			return cacheLayer->readDisk(mirror_disk_id,buf,size,offset);
		} else {
			printf("Raid 1 both disk %d and mirror disk %d\n",disk_id,mirror_disk_id);
			return -1;
		}
	}
	AbnormalError();
	return -1;  
}

/*
 * recovering_raid1: RAID 1 recover
 *
 * @param failed_disk_id: failed disk id
 * @param newdevice: new device to replace the failed disk
 *
 * @return: 0: success ; -1: wrong
 */
int Coding4Raid1::recover(int failed_disk_id,char* newdevice)
{
        //size of data to be rebuilded
	int __recoversize = NCFS_DATA->disk_size[failed_disk_id];
	int block_size = NCFS_DATA->disk_block_size;
	char buf[block_size];
	
	for(int i = 0; i < __recoversize; ++i){

	  	//reset the buf data
	        memset(buf, 0, block_size);
		
		int offset = i * block_size;
		int retstat = fileSystemLayer->codingLayer->decode(failed_disk_id,buf,block_size,offset);
		retstat = cacheLayer->writeDisk(failed_disk_id,buf,block_size,offset);

	}

	return 0;
	
}

int Coding4Raid1::recover_enable_node_encode(int failed_disk_id, char* newdevice, int* node_socketid_array)
{
  
  return 0;
}

int Coding4Raid1::recover_using_multi_threads_nor(int failed_disk_id, char* newdevice, int total_sor_threads){
  
    return 0;
}

int Coding4Raid1::recover_using_multi_threads_sor(int failed_disk_id, char* newdevice
									, int total_sor_threads, int number_sor_thread){
  
    return 0;
}

int Coding4Raid1::recover_using_access_aggregation(int failed_disk_id, char* newdevice){
  
    return 0;
}

int Coding4Raid1::recover_conventional(int failed_disk_id, char* newdevice){
  
   return 0;
}