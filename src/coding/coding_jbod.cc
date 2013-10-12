#include "coding_jbod.hh"
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
 * jbod_encoding: JBOD: non-stripped block allocation  (type=100)
 *
 * @param buf: buffer of data to be written to disk
 * @param size: size of the data buffer
 *
 * @return: assigned data block: disk id and block number.
 */
struct data_block_info Coding4Jbod::encode(const char* buf, int size)
{

	int retstat, disk_id, block_no, disk_total_num, block_size;
	//char *dev;
	//int datafd;
	int size_request, block_request;
	struct data_block_info block_written;
	int i;

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
	//here use jbod: first available block.

	block_no = 0;
	disk_id = -1;
	i = 0;
	while ((i < disk_total_num) && (disk_id == -1)){
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

		NCFS_DATA->free_offset[disk_id] = block_no + block_request;
		NCFS_DATA->free_size[disk_id]
			= NCFS_DATA->free_size[disk_id] - block_request;

		//Cache End
	}

	block_written.disk_id = disk_id;
	block_written.block_no = block_no;

	return block_written;
  
}



/*
 * decoding_jbod: Jbod decode
 *
 * @param disk_id: disk id               
 * @param buf: data buffer for reading data
 * @param size: size of the data buffer
 * @param offset: offset of data on disk
 *
 * @return: size of successful decoding (in bytes)
 */
int Coding4Jbod::decode(int disk_id, char* buf, long long size, long long offset)
{
  
	if(NCFS_DATA->disk_status[disk_id] == 0)
		return cacheLayer->readDisk(disk_id,buf,size,offset);
	else {
		printf("Raid %d: Disk %d failed\n",NCFS_DATA->disk_raid_type,disk_id);
		return -1;
	}
	AbnormalError();
	return -1;
	
}

/*
 * recovering_jbod: JBOD recover
 *
 * @param failed_disk_id: failed disk id
 * @param newdevice: new device to replace the failed disk
 * @return: 0: success ; -1: wrong
 */
int Coding4Jbod::recover(int failed_disk_id,char* newdevice)
{
	
	return -1;  

}

int Coding4Jbod::recover_enable_node_encode(int failed_disk_id, char* newdevice, int* node_socketid_array)
{
  
  return 0;
}

int Coding4Jbod::recover_using_multi_threads_nor(int failed_disk_id, char* newdevice, int total_sor_threads){
  
    return 0;
}

int Coding4Jbod::recover_using_multi_threads_sor(int failed_disk_id, char* newdevice
									, int total_sor_threads, int number_sor_thread){
  
    return 0;
}

int Coding4Jbod::recover_using_access_aggregation(int failed_disk_id, char* newdevice){
  
    return 0;
}

int Coding4Jbod::recover_conventional(int failed_disk_id, char* newdevice){
  
   return 0;
}