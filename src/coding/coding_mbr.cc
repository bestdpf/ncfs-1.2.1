#include "coding_mbr.hh"
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

extern "C"{
#include "../jerasure/galois.h"
#include "../jerasure/jerasure.h"
#include "../jerasure/reed_sol.h"
}

//NCFS context state
extern struct ncfs_state* NCFS_DATA;
extern FileSystemLayer* fileSystemLayer;
extern CacheLayer* cacheLayer;
extern DiskusageReport* diskusageLayer;

/*
 * Constructor funcation for MBR
 *
 */
Coding4Mbr::Coding4Mbr()
{
	int n,k,d;
	NCFS_DATA->mbr_n = NCFS_DATA->disk_total_num;
	NCFS_DATA->mbr_k = NCFS_DATA->data_disk_num;  //currently only support n=k-1 or n=k-2
	n = NCFS_DATA->mbr_n;
	k = NCFS_DATA->mbr_k;
	d = n - 1;
	NCFS_DATA->mbr_m = k*d - (int)(k*(k-1)/2);
	NCFS_DATA->mbr_c = (int)(n*(n-1)/2) - (int)(k*(2*n-k-1)/2);

	//each block (including encoded block) has a copy
	NCFS_DATA->mbr_segment_size = (int)(2*(NCFS_DATA->mbr_m + NCFS_DATA->mbr_c)/n);
	NCFS_DATA->segment_size = NCFS_DATA->mbr_segment_size;

	//Add by zhuyunfeng on 2011-07-11 begin.
	//Generate a coding matrix to generalize the e-mbr
	//Generate the last m rows of the distribution matrix in GF(2^w) for reed solomon
	field_power=8;
	NCFS_DATA->generator_matrix=reed_sol_vandermonde_coding_matrix(NCFS_DATA->mbr_m,NCFS_DATA->mbr_c,field_power);

	//Add by zhuyunfeng on 2011-07-11 end.

	printf("MBR parameters: n=%d, k=%d, m=%d, c=%d, segment_size=%d\n",    
			NCFS_DATA->mbr_n,NCFS_DATA->mbr_k,NCFS_DATA->mbr_m,NCFS_DATA->mbr_c,
			NCFS_DATA->mbr_segment_size);  

}


/*************************************************************************
 * MBR functions
 *************************************************************************/

/*
 * mbr_find_block_id: find block id for a block based on its disk id and block no
 *
 * @param disk_id: disk ID of the data block
 * @param block_no: block number of the data block
 * @param mbr_segment_size: the segment size for MBR coding
 *
 * @return: mbr block id
 */
int Coding4Mbr::mbr_find_block_id(int disk_id, int block_no, int mbr_segment_size)
{
	int mbr_block_id;
	int block_offset, mbr_offset;
	int counter, i;

	block_offset = block_no % mbr_segment_size;

	if (block_offset < disk_id){
		mbr_block_id = -1;
	}
	else{
		mbr_offset = block_offset - disk_id;
		mbr_block_id = 0;
		counter = mbr_segment_size;
		for (i=0; i<disk_id; i++){
			mbr_block_id = mbr_block_id + counter;
			counter--;
		}

		mbr_block_id = mbr_block_id + mbr_offset;
	}

	return mbr_block_id;
}

/*
 * mbr_find_dup_block_id: find block id for a duplicated block based on its disk id and block no
 *
 * @param disk_id: disk ID of the data block
 * @param block_no: block number of the data block
 * @param mbr_segment_size: the segment size for MBR coding
 *
 * @return: mbr block id
 */
int Coding4Mbr::mbr_find_dup_block_id(int disk_id, int block_no, int mbr_segment_size)
{
	int mbr_block_id;
	int block_offset, mbr_offset;
	int counter, i;

	block_offset = block_no % mbr_segment_size;

	if (block_offset >= disk_id){
		mbr_block_id = -1;
	}
	else{
		mbr_offset = disk_id - 1;
		mbr_block_id = 0;
		counter = mbr_segment_size;
		for (i=0; i<block_offset; i++){
			mbr_block_id = mbr_block_id + counter;
			counter--;
			mbr_offset--;
		}

		mbr_block_id = mbr_block_id + mbr_offset;
	}

	return mbr_block_id;
}


/*
 * mbr_get_disk_id: get disk id for a block based on its MBR block ID
 *
 * @param mbr_block_id: mbr block ID of the data block
 * @param mbr_segment_size: the segment size for MBR coding
 *
 * @return: disk id
 */
int Coding4Mbr::mbr_get_disk_id(int mbr_block_id, int mbr_segment_size)
{
	int mbr_block_group, counter, temp;

	mbr_block_group = 0;
	counter = mbr_segment_size;
	temp = (mbr_block_id + 1) - counter;

	while (temp > 0){
		counter--;
		temp = temp - counter;
		(mbr_block_group)++;
	}

	return mbr_block_group;
}


/*
 * mbr_get_block_no: get block number for a block based on its disk id and MBR block number
 *
 * @param disk id: disk ID of the data block
 * @param mbr_block_id: mbr block ID of the data block
 * @param mbr_segment_size: the segment size for MBR coding
 *
 * @return: block number
 */
int Coding4Mbr::mbr_get_block_no(int disk_id, int mbr_block_id, int mbr_segment_size)
{
	int block_no;
	int mbr_offset, j;

	mbr_offset = mbr_block_id;
	for (j=0; j<disk_id; j++){
		mbr_offset = mbr_offset - (mbr_segment_size - j);
	}
	block_no = mbr_offset + disk_id;

	return block_no;
}


/*
 * mbr_get_dup_disk_id: get disk id for a duplicated block based on its MBR block ID
 *
 * @param mbr_block_id: mbr block ID of the data block
 * @param mbr_segment_size: the segment size for MBR coding
 *
 * @return: disk id
 */
int Coding4Mbr::mbr_get_dup_disk_id(int mbr_block_id, int mbr_segment_size)
{
	int dup_disk_id;
	int disk_id, mbr_offset;
	int j;

	disk_id = mbr_get_disk_id(mbr_block_id, mbr_segment_size);
	mbr_offset = mbr_block_id;
	for (j=0; j<disk_id; j++){
		mbr_offset = mbr_offset - (mbr_segment_size - j);
	}

	dup_disk_id = mbr_offset + 1 + disk_id;

	return dup_disk_id;
}


/*
 * mbr_get_dup_block_no: get block number for a duplicated block based on its disk id and MBR block number
 *
 * @param disk id: disk ID of the original data block
 * @param mbr_block_id: mbr block ID of the data block
 * @param mbr_segment_size: the segment size for MBR coding
 *
 * @return: block number
 */
int Coding4Mbr::mbr_get_dup_block_no(int disk_id, int mbr_block_id, int mbr_segment_size)
{
	int dup_block_no;

	dup_block_no = disk_id;

	return dup_block_no;
}

/*
 * msr_encoding: MSR Code: fault tolerance by stripped parity
 *
 * @param buf: buffer of data to be written to disk
 * @param size: size of the data buffer
 *
 * @return: assigned data block: disk id and block number.
 */
struct data_block_info Coding4Mbr::encode(const char* buf, int size)
{

	int retstat, disk_id, block_no, disk_total_num, block_size;
	int size_request, block_request, free_offset;
	struct data_block_info block_written;
	int i, j;
	char *buf2, *buf_read, *buf_temp;

	int mbr_segment_size;
	int mbr_segment_id;
	int mbr_native_block_id;
	int mbr_code_block_id;
	int dup_disk_id, dup_block_no, code_disk_id, code_block_no;

    ticks t1, t2, t3;

	mbr_segment_size = NCFS_DATA->mbr_segment_size;

	//test print
	//printf("***get_data_block_no 1: size=%d\n",size);

	disk_total_num = NCFS_DATA->disk_total_num;
	block_size = NCFS_DATA->disk_block_size;

	size_request = fileSystemLayer->round_to_block_size(size);
	block_request = size_request / block_size;

	//test print
	//printf("***get_data_block_no 2: size_request=%d, block_request=%d\n"
	//                ,size_request,block_request);

	//implement disk write algorithm here.
	//here use mbr.
	//approach: write original data block, duplicated block and coded block
	for (i=0; i < disk_total_num; i++){
		mbr_native_block_id = mbr_find_block_id(i,NCFS_DATA->free_offset[i],mbr_segment_size);
		if ( (mbr_native_block_id == -1) || (mbr_native_block_id >= NCFS_DATA->mbr_m) ){
			(NCFS_DATA->free_offset[i])++;
			(NCFS_DATA->free_size[i])--;
		}
	}

	block_no = 0;
	disk_id = -1;
	free_offset = NCFS_DATA->free_offset[0];
	for (i=disk_total_num-1; i >= 0; i--){
		mbr_native_block_id = mbr_find_block_id(i,NCFS_DATA->free_offset[i],mbr_segment_size);
		if ((block_request <= (NCFS_DATA->free_size[i]))
				&& (free_offset >= (NCFS_DATA->free_offset[i]))
				&& (mbr_native_block_id != -1)
				&& (mbr_native_block_id < NCFS_DATA->mbr_m))
		{
			disk_id = i;
			block_no = NCFS_DATA->free_offset[i];
			free_offset = block_no;
		}
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
		mbr_segment_id = (int)(block_no / mbr_segment_size);

		//write original data block
		buf_read = (char*)malloc(sizeof(char)*size_request);
		buf2 = (char*)malloc(sizeof(char)*size_request);
		buf_temp = (char*)malloc(sizeof(char)*size_request);

		//attention 1, read origin data block XOR new written block
		if (NCFS_DATA->mbr_c > 0){

			if (NCFS_DATA->run_experiment == 1){
                t1 = getticks();
			}

			//Cache Start
			retstat = cacheLayer->readDisk(disk_id,buf2,size_request,block_no*block_size);
			//Cache End

			if (NCFS_DATA->run_experiment == 1){
                t2 = getticks();
			}

			for (j=0; j < size_request; j++){
				buf2[j] = buf2[j] ^ buf[j];
			}

			if (NCFS_DATA->run_experiment == 1){
                t3 = getticks()

                NCFS_DATA->diskread_ticks += (t2 - t1);
                NCFS_DATA->encoding_ticks += (t3 - t2);
			}
		}

		NCFS_DATA->free_offset[disk_id] = block_no + block_request;
		NCFS_DATA->free_size[disk_id]
			= NCFS_DATA->free_size[disk_id] - block_request;

		if (NCFS_DATA->run_experiment == 1){
            t1 = getticks();
		}

		//write duplicated block
		mbr_native_block_id = mbr_find_block_id(disk_id, block_no, mbr_segment_size);
		dup_disk_id = mbr_get_dup_disk_id(mbr_native_block_id, mbr_segment_size);
		dup_block_no = mbr_get_dup_block_no(disk_id, mbr_native_block_id, mbr_segment_size);

		//printf("native block id is %d, disk is %d and dup block disk is %d, block no is %d\n",mbr_native_block_id,
		//       disk_id,dup_disk_id,dup_block_no);

		dup_block_no = dup_block_no + mbr_segment_id * mbr_segment_size;

		if (NCFS_DATA->run_experiment == 1){
            t2 = getticks();
		}

		//Cache Start
		//write data
		retstat = cacheLayer->writeDisk(disk_id,buf,size,block_no*block_size);
		//write duplicate data
		retstat = cacheLayer->writeDisk(dup_disk_id,buf,size,dup_block_no*block_size);
		//Cache End

		if (NCFS_DATA->run_experiment == 1){
            t3 = getticks();

            NCFS_DATA->encoding_ticks += (t2 - t1);
            NCFS_DATA->diskwrite_ticks += (t3 - t2);
		}


		if (NCFS_DATA->no_gui == 0){
			diskusageLayer->Update(dup_disk_id,dup_block_no,1);
		}

		//attention 2, make the new encoded block and write the encoded block
		//if (NCFS_DATA->mbr_c > 0){
		for (i=0; i < NCFS_DATA->mbr_c; i++){

			memset(buf_temp, 0, size_request); 
			memset(buf_read, 0, size_request); 
			//generate coded block data

			//use the coded block one by one.
			mbr_code_block_id = (NCFS_DATA->mbr_m)+i;
			code_disk_id = mbr_get_disk_id(mbr_code_block_id, mbr_segment_size);

			code_block_no = mbr_get_block_no(code_disk_id, mbr_code_block_id, mbr_segment_size);

			//printf("code block id is %d, disk is %d and block_no is %d\n",mbr_code_block_id,code_disk_id,code_block_no);

			code_block_no = code_block_no + mbr_segment_id * mbr_segment_size;

			//duplicate coded block
			dup_disk_id = mbr_get_dup_disk_id(mbr_code_block_id, mbr_segment_size);
			dup_block_no = mbr_get_dup_block_no(code_disk_id, mbr_code_block_id, mbr_segment_size);
			dup_block_no = dup_block_no + mbr_segment_id * mbr_segment_size;


			if (NCFS_DATA->run_experiment == 1){
                t1 = getticks();
			}

			//Cache Start
			retstat = cacheLayer->readDisk(code_disk_id,buf_read,size_request,
					code_block_no*block_size);
			//Cache End


			if (NCFS_DATA->run_experiment == 1){
                t2 = getticks();
			}


			//Calculate the code block on the Gauss Field
			int coefficient=NCFS_DATA->generator_matrix[i*NCFS_DATA->mbr_m+mbr_native_block_id];

			for (j=0; j < size_request; j++){
				buf_temp[j]=galois_single_multiply((unsigned char)buf2[j],coefficient,field_power);
			}

			for (j=0; j < size_request; j++){
				buf_read[j]=buf_read[j] ^ buf_temp[j];
			}

			if (NCFS_DATA->run_experiment == 1){
                t3 = getticks();

                NCFS_DATA->diskread_ticks += (t2 - t1);
                NCFS_DATA->encoding_ticks += (t3 - t2);
			}

			if (NCFS_DATA->run_experiment == 1){
                t1 = getticks();
			}

			// Cache Start
			//write coded block
			retstat = cacheLayer->writeDisk(code_disk_id,buf_read,size,code_block_no*block_size);
			//write duplicate coded block
			retstat = cacheLayer->writeDisk(dup_disk_id,buf_read,size,dup_block_no*block_size);
			// Cache End

			if (NCFS_DATA->run_experiment == 1){
                t2 = getticks();

                NCFS_DATA->diskwrite_ticks += (t2 - t1);
			}

		}

		free(buf_temp);
		free(buf_read);
		free(buf2);
	}

	block_written.disk_id = disk_id;
	block_written.block_no = block_no;

	return block_written; 

} 



//implement the decoding function for MBR Code
/*
 * decoding_MBR Code: MBR Code decode
 *
 * @param disk_id: disk id               
 * @param buf: data buffer for reading data
 * @param size: size of the data buffer
 * @param offset: offset of data on disk
 *
 * @return: size of successful decoding (in bytes)
 */

int Coding4Mbr::decode(int disk_id, char* buf, long long size, long long offset)
{

	int retstat;
	ticks t1, t2, t3, t4;

	if(NCFS_DATA->disk_status[disk_id] == 0){
		if (NCFS_DATA->run_experiment == 1){
            t1 = getticks();
		}

		retstat = cacheLayer->readDisk(disk_id,buf,size,offset);

		if (NCFS_DATA->run_experiment == 1){
            t2 = getticks();

			NCFS_DATA->diskread_ticks += (t2 - t1);
		}

		return retstat;
	}
	else {
		//printf("Degraded Read\n");
		char* temp_buf;
		char* buf_temp;

		int i;
		long long j;
		temp_buf = (char*)malloc(sizeof(char)*size);
		memset(temp_buf, 0, size);

		memset(buf, 0, size);

		buf_temp = (char*)malloc(sizeof(char)*size);
		memset(buf_temp, 0, size);

		//int* inttemp_buf = (int*)temp_buf;
		int* intbuf = (int*)buf;
		int* intbuf_temp = (int*)buf_temp;


		int *erased; //record the surviving or fail of each disk

		int *decoding_matrix;
		int *dm_ids; //record the ids of the selected disks

		int mbr_segment_size;
		int mbr_segment_id;
		int mbr_block_id;
		long long block_no;
		int dup_disk_id, dup_block_no;
		int temp_mbr_block_id, temp_disk_id, temp_dup_disk_id, temp_block_no;
		int flag_dup;
		int disk_failed_no, disk_another_failed_id;
		long long offset_read;
		int temp;

		//check the number of failed disks
		disk_failed_no = 0;
		disk_another_failed_id = -1;
		for (i=0; i < NCFS_DATA->disk_total_num; i++){
			if (NCFS_DATA->disk_status[i] == 1){
				(disk_failed_no)++;
				if (i != disk_id){
					disk_another_failed_id = i;
				}
			}
		}

		mbr_segment_size = NCFS_DATA->mbr_segment_size;
		block_no = (long long)(offset / NCFS_DATA->disk_block_size);
		mbr_segment_id = (int)(block_no / mbr_segment_size);

		if (NCFS_DATA->run_experiment == 1){
            t1 = getticks();
		}

		flag_dup = 0;
		mbr_block_id = mbr_find_block_id(disk_id, block_no, mbr_segment_size);
		if (mbr_block_id == -1){
			flag_dup = 1;
			mbr_block_id = mbr_find_dup_block_id(disk_id, block_no, mbr_segment_size);
		}

		if (flag_dup == 0){
			dup_disk_id = mbr_get_dup_disk_id(mbr_block_id,mbr_segment_size);
			dup_block_no = mbr_get_dup_block_no(disk_id,mbr_block_id,mbr_segment_size);
			dup_block_no = dup_block_no + mbr_segment_id * mbr_segment_size;
		}
		else{ //if flag_dup = 1
			dup_disk_id = mbr_get_disk_id(mbr_block_id,mbr_segment_size);
			dup_block_no = mbr_get_block_no(dup_disk_id,mbr_block_id,mbr_segment_size);
			dup_block_no = dup_block_no + mbr_segment_id * mbr_segment_size;
		}

		if (NCFS_DATA->run_experiment == 1){
            t2 = getticks();

			NCFS_DATA->decoding_ticks += (t2 - t1);
		}


		//Generalized E-MBR, you may have multiplie failures
		if(NCFS_DATA->disk_status[dup_disk_id] == 1){

			if (NCFS_DATA->run_experiment == 1){
                t1 = getticks();
			}

			//record the disk health information
			erased=(int *)malloc(sizeof(int)*(NCFS_DATA->mbr_m+NCFS_DATA->mbr_c));

			//record the first m healthy blocks
			dm_ids=(int *)malloc(sizeof(int)*(NCFS_DATA->mbr_m));

			//Use coded block to decode
			//printf("***MBR: decode by coded block. mbr_block_id=%d\n",mbr_block_id);
			for (i=0; i<((NCFS_DATA->mbr_m) + (NCFS_DATA->mbr_c)); i++){

				temp_mbr_block_id = i;
				temp_disk_id = mbr_get_disk_id(temp_mbr_block_id, mbr_segment_size);
				temp_dup_disk_id = mbr_get_dup_disk_id(temp_mbr_block_id, 
						mbr_segment_size);		  

				//the block doesn't exist
				if((NCFS_DATA->disk_status[temp_disk_id] == 1)&&(NCFS_DATA->disk_status[temp_dup_disk_id] == 1)){
					erased[i]=1;
				}else
				{
					erased[i]=0;
				}
			}

			//allocate k*k space for decoding matrix
			decoding_matrix = (int *)malloc(sizeof(int)*(NCFS_DATA->mbr_m)*(NCFS_DATA->mbr_m));

			if (jerasure_make_decoding_matrix((NCFS_DATA->mbr_m), (NCFS_DATA->mbr_c),field_power,
						NCFS_DATA->generator_matrix, 
						erased, decoding_matrix, dm_ids) < 0) {
				free(erased);
				free(dm_ids);
				free(decoding_matrix);
				return -1;
			}


			if (NCFS_DATA->run_experiment == 1){
                t2 = getticks();

				NCFS_DATA->decoding_ticks += (t2 - t1);
			}


			//travel the selected surviving blocks
			for(i=0;i<(NCFS_DATA->mbr_m);i++)
			{

				if (NCFS_DATA->run_experiment == 1){
                    t1 = getticks();
				}

				memset(temp_buf, 0, size);

				//decoding efficient for this surviving block
				int efficient=decoding_matrix[NCFS_DATA->mbr_m*mbr_block_id+i];

				//real block id
				temp_mbr_block_id= dm_ids[i];

				temp_disk_id = mbr_get_disk_id(temp_mbr_block_id, mbr_segment_size);
				temp_block_no = mbr_get_block_no(temp_disk_id, temp_mbr_block_id,
						mbr_segment_size);

				if (NCFS_DATA->disk_status[temp_disk_id] == 1){
					//original data block fails, use duplicated block
					temp = temp_disk_id;
					temp_disk_id = mbr_get_dup_disk_id(temp_mbr_block_id, 
							mbr_segment_size);
					temp_block_no = mbr_get_dup_block_no(temp, temp_mbr_block_id,
							mbr_segment_size);
				}			

				temp_block_no = temp_block_no + mbr_segment_id * mbr_segment_size;
				offset_read = (long long)(temp_block_no * (NCFS_DATA->disk_block_size));

				if (NCFS_DATA->run_experiment == 1){
                    t2 = getticks();
				}

				retstat = cacheLayer->readDisk(temp_disk_id,temp_buf,size,offset_read);

				if (NCFS_DATA->run_experiment == 1){
                    t3 = getticks();
				}


				if (retstat < 0 ){
					return -1;
				}

				for (j = 0; j < size; j++){
					buf_temp[j]=galois_single_multiply((unsigned char)temp_buf[j],efficient,field_power);
				}

				for(j = 0; j < (long long)(size * sizeof(char) / sizeof(int)); ++j){
					intbuf[j] = intbuf[j] ^ intbuf_temp[j];
				}

				if (NCFS_DATA->run_experiment == 1){
                    t4 = getticks();

					NCFS_DATA->decoding_ticks += (t2 - t1);

					NCFS_DATA->diskread_ticks += (t3 - t2);

					NCFS_DATA->decoding_ticks += (t4 - t3);
				} 

			}

		}
		else{
			//Use duplicated block to decode

			offset_read = (long long)(dup_block_no * (NCFS_DATA->disk_block_size));

			if (NCFS_DATA->run_experiment == 1){
                t1 = getticks();             
			}

			retstat = cacheLayer->readDisk(dup_disk_id,buf,size,offset_read);

			if (NCFS_DATA->run_experiment == 1){
                t2 = getticks();

				NCFS_DATA->diskread_ticks += (t2 - t1);
			}

			return retstat;
		}


		free(erased);
		free(dm_ids);
		free(decoding_matrix);
		free(temp_buf);
		return size;
	}


	AbnormalError();
	return -1;  
}

/*
 * recovering_mbr: MBR recover
 *
 * @param failed_disk_id: failed disk id
 * @param newdevice: new device to replace the failed disk
 * @return: 0: success ; -1: wrong
 */
int Coding4Mbr::recover(int failed_disk_id,char* newdevice)
{

	//size of data to be rebuilded
	int __recoversize = NCFS_DATA->disk_size[failed_disk_id];
	int block_size = NCFS_DATA->disk_block_size;
	char buf[block_size];	

	ticks t1, t2;

	for(int i = 0; i < __recoversize; ++i){

		//reset the buf data
		memset(buf, 0, block_size);

		int offset = i * block_size;

		int retstat = fileSystemLayer->codingLayer->decode(failed_disk_id,buf,block_size,offset);

		if (NCFS_DATA->run_experiment == 1){
            t1 = getticks();
		}
		retstat = cacheLayer->writeDisk(failed_disk_id,buf,block_size,offset);

		if (NCFS_DATA->run_experiment == 1){
            t2 = getticks();

			NCFS_DATA->diskwrite_ticks += (t2 - t1);

		}
	}

	return 0;
}

int Coding4Mbr::recover_enable_node_encode(int failed_disk_id, char* newdevice, int* node_socketid_array)
{

	return 0;
}

int Coding4Mbr::recover_using_multi_threads_nor(int failed_disk_id, char* newdevice, int total_sor_threads){

	return 0;
}

int Coding4Mbr::recover_using_multi_threads_sor(int failed_disk_id, char* newdevice
		, int total_sor_threads, int number_sor_thread){

	return 0;
}

int Coding4Mbr::recover_using_access_aggregation(int failed_disk_id, char* newdevice){

	return 0;
}

int Coding4Mbr::recover_conventional(int failed_disk_id, char* newdevice){

	return 0;
}
