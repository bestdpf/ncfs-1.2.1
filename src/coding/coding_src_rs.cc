/*
 * coding_src_rs.cc
 * - Simple Regenerating Code atop Reed-Solomon
 *   Reference paper: 
 *   Dimitris S. Papailiopoulos, Jianqiang Luo, Alexandros G. Dimakis,
 *   Cheng Huang, Jin Li, "Simple Regenerating Codes: Network Coding for
 *   Cloud Storage", INFOCOM 2012.
 */
#include "coding_src_rs.hh"
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
 * Constructor for Reed Solomon
 */
Coding4SrcRS::Coding4SrcRS()
{

	int k=NCFS_DATA->data_disk_num;
	int m=NCFS_DATA->disk_total_num-NCFS_DATA->data_disk_num;
	int w=8;

	//generate the last m rows of the distribution matrix in GF(2^w) for reed solomon
	NCFS_DATA->generator_matrix=reed_sol_vandermonde_coding_matrix(k,m,w);

	//SRC - MSR
	src_f = configLayer->getConfigInt("FileSystem>Disk>Src>SrcF");
	if(src_f < 0)
		src_f = 0;
}

/*
 * src_rs_encoding: SRC-Reed Solomon: fault tolerance by stripped parity (type=130) and SRC
 * 
 * @param buf: buffer of data to be written to disk
 * @param size: size of the data buffer
 *
 * @return: assigned data block: disk id and block number.
 */
struct data_block_info Coding4SrcRS::encode(const char* buf, int size){

	int retstat, disk_id, block_no, disk_total_num,data_disk_num, block_size;

	int parity_block_location;

	int size_request, block_request, free_offset;
	struct data_block_info block_written;
	int i, j;
	int parity_disk_id;

	//buf_write is the data to be written to parity disks
	//buf_read is the origin data
	char *buf_temp, *buf_write, *buf_read;
	//char temp_char;
	int data_disk_coeff;

	int fault_tolerance;

	//n
	disk_total_num = NCFS_DATA->disk_total_num;

	//k
	data_disk_num = NCFS_DATA->data_disk_num;

	//m
	fault_tolerance=(disk_total_num-data_disk_num);


	block_size = NCFS_DATA->disk_block_size;

	size_request = fileSystemLayer->round_to_block_size(size);
	block_request = size_request / block_size;

	//implement disk write algorithm here.

	block_no = 0;
	disk_id = -1;
	free_offset = NCFS_DATA->free_offset[0];

	int tmp_free_offset=free_offset;

	//find the available data disk
	for (i=data_disk_num-1; i >= 0; i--){


		/*
		   if ((block_request <= (NCFS_DATA->free_size[i]))
		   && (free_offset >= (NCFS_DATA->free_offset[i])))
		   {
		   disk_id = i;
		   block_no = NCFS_DATA->free_offset[i];
		   free_offset = block_no;
		   }

		 */

		if(block_request <= (NCFS_DATA->free_size[i]))
		{


			if((NCFS_DATA->free_offset[i]%(data_disk_num-1))!=0)//if free_offset mode (data_disk_num-1) is nonzero, then the disk is writable
			{
				disk_id = i;
				block_no = NCFS_DATA->free_offset[i];
				tmp_free_offset = block_no;

			}else if(free_offset >(NCFS_DATA->free_offset[i]))// if free_offset is smaller than free_offset in disk0, then the disk is writable
			{
				disk_id = i;
				block_no = NCFS_DATA->free_offset[i];
				tmp_free_offset = block_no;

			}else if((i==0)&&(disk_id==-1)) // the special case: write disk 0 first
			{
				disk_id = i;
				block_no = NCFS_DATA->free_offset[i];
				tmp_free_offset = block_no;
			}

		}


	}

	free_offset=tmp_free_offset;


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


		//temp buffer
		buf_read = (char*)malloc(sizeof(char)*size_request);
		buf_temp = (char*)malloc(sizeof(char)*size_request);
		buf_write = (char*)malloc(sizeof(char)*size_request);
		memset(buf_write, 0, size_request);

		//allocate the available data block
		NCFS_DATA->free_offset[disk_id] = block_no + block_request;
		NCFS_DATA->free_size[disk_id]
			= NCFS_DATA->free_size[disk_id] - block_request;


		//read the original data block
		retstat = cacheLayer->readDisk(disk_id,buf_read,
				size_request,(block_no + block_no / src_f) * block_size);

		//SRC - RS
		//Update SRC Code Block for data block
		int src_code_block_no = (src_f + 1) * (block_no / src_f + 1) -1;
		int src_code_disk_id = (disk_id - src_f + block_no % src_f + NCFS_DATA->disk_total_num) % NCFS_DATA->disk_total_num;
		char* buf_src_code = (char*)malloc(sizeof(char)*size);
		memset(buf_src_code,0,size);
		retstat = cacheLayer->readDisk(src_code_disk_id,buf_src_code,size,src_code_block_no * block_size);
		int* intbuf_src_code = (int*)buf_src_code;
		int* intbuf_read = (int*)buf_read;
		int* intbuf = (int*)buf;

		for(long long cnt = 0; cnt < (long long)(size * sizeof(char) / sizeof(int)); ++cnt){
			intbuf_src_code[cnt] = intbuf_src_code[cnt] ^ intbuf_read[cnt] ^ intbuf[cnt];
			intbuf_read[cnt] = intbuf[cnt] ^ intbuf_read[cnt];
		}
		retstat = cacheLayer->writeDisk(src_code_disk_id,buf_src_code,size,src_code_block_no * block_size);

		/*
		//calculate the XOR of buf and buf_read
		for (j=0; j < size_request; j++){

		buf_read[j] = buf[j] ^ buf_read[j];

		}
		 */

		// Cache Start
		//write the data block into disk
		retstat = cacheLayer->writeDisk(disk_id,buf,size,(block_no + block_no / src_f) * block_size);
		// Cache End

		//update the code disks one by one
		for(i=0;i<fault_tolerance;i++)
		{

			memset(buf_temp, 0, size_request);

			data_disk_coeff=NCFS_DATA->generator_matrix[i*data_disk_num+disk_id];

			for (j=0; j < size_request; j++){
				buf_temp[j]=galois_single_multiply((unsigned char)buf_read[j],data_disk_coeff,field_power);
			}

			//update the corresponding parity block in the appropriate location
			parity_disk_id=data_disk_num+i;

			parity_block_location=block_no*block_size;

			//retstat = cacheLayer->readDisk(parity_disk_id,buf_write,size_request,parity_block_location);   
			retstat = cacheLayer->readDisk(parity_disk_id,buf_write,size_request,(block_no + block_no / src_f) * block_size);

			//SRC-RS
			//Update SRC Code Block for RS Code Block
			int src_code_block_no = (src_f + 1) * (block_no / src_f + 1) - 1;
			int src_code_disk_id = (parity_disk_id - src_f + block_no % src_f + NCFS_DATA->disk_total_num) % NCFS_DATA->disk_total_num;

			retstat = cacheLayer->readDisk(src_code_disk_id,buf_src_code,size,src_code_block_no * block_size);
			int* intbuf_temp = (int*)buf_temp;
			int* intbuf_write = (int*)buf_write;
			for(long long cnt = 0; cnt < (long long)(size * sizeof(char) / sizeof(int)); ++cnt){
				intbuf_src_code[cnt] = intbuf_src_code[cnt] ^ intbuf_temp[cnt];
				intbuf_write[cnt] = intbuf_write[cnt] ^ intbuf_temp[cnt];	
			}

			retstat = cacheLayer->writeDisk(src_code_disk_id,buf_src_code,size,src_code_block_no * block_size);

			/*
			   for (j=0; j < size_request; j++){

			//Calculate parity block P
			buf_write[j] = buf_write[j] ^ buf_temp[j];

			} 
			 */

			retstat = cacheLayer->writeDisk(parity_disk_id,buf_write,size,(block_no + block_no / src_f) * block_size);

		}

		free(buf_temp);
		free(buf_read);
		free(buf_write);
	}


	block_written.disk_id = disk_id;
	block_written.block_no = block_no;

	return block_written;   


}



/*
 * decoding_src_rs: SRC-ReedSolomon decode (type = 130)
 *
 * @param disk_id: disk id               
 * @param buf: data buffer for reading data
 * @param size: size of the data buffer
 * @param offset: offset of data on disk
 *
 * @return: size of successful decoding (in bytes)
 */
int Coding4SrcRS::decode(int disk_id, char* buf, long long size, long long offset){

	int retstat;
	long long src_offset = offset / NCFS_DATA->disk_block_size / src_f * NCFS_DATA->disk_block_size;

	//a healthy data disk
	if(NCFS_DATA->disk_status[disk_id] == 0){

		retstat = cacheLayer->readDisk(disk_id,buf,size,offset+src_offset);

		return retstat;
	}else{

		struct timeval starttime, endtime;

		char* temp_buf;
		char* buf_temp;
		int i;
		long long j;

		int *erased; //record the surviving or fail of each disk

		int *decoding_matrix;
		int *dm_ids; //record the ids of the selected disks

		int disk_total_num;
		int data_disk_num; 

		disk_total_num=NCFS_DATA->disk_total_num;
		data_disk_num=NCFS_DATA->data_disk_num;

		int block_size; 
		int block_no; 

		int blockoffset; 
		int diskblocklocation; // indicates the block location

		block_size=NCFS_DATA->disk_block_size; 
		block_no=offset/block_size; 
		blockoffset=offset-block_no*block_size; 

		//int* inttemp_buf = (int*)temp_buf;
		int* intbuf = (int*)buf;

		//Test SRC Disk
		bool available = true;
		long long src_block_to_read = block_no + block_no / src_f;
		long long src_disk_to_read = disk_id;
		char* buf_code = (char*)malloc(sizeof(char)*size);
		char* buf_read = (char*)malloc(sizeof(char)*size);
		memset(buf_code,0,size);
		memset(buf_read,0,size);

		for(int i = 0; i < src_f; ++i){
			++src_block_to_read;
			src_disk_to_read = (src_disk_to_read - 1 + NCFS_DATA->disk_total_num) % NCFS_DATA->disk_total_num;

			if(src_block_to_read % (src_f+1) == 0){
				src_block_to_read -= src_f + 1;
				src_disk_to_read = (src_disk_to_read + src_f + 1) % NCFS_DATA->disk_total_num;
			}

			if(NCFS_DATA->disk_status[src_disk_to_read] != 0){
				available = false;
				break;
			}
		}

		/*****************
		 *    Use SRC    *
		 *****************/
		if(available){
			src_block_to_read = block_no + block_no / src_f;
			src_disk_to_read = disk_id;
			int* intbuf_code = (int*)buf_code;
			int* intbuf_read = (int*)buf_read;
			for(int i = 0; i < src_f; ++i){
				++src_block_to_read;
				src_disk_to_read = (src_disk_to_read - 1 + NCFS_DATA->disk_total_num) % NCFS_DATA->disk_total_num;

				//Loop within the SRC Segment
				if(src_block_to_read % (src_f+1) == 0){
					src_block_to_read -= src_f + 1;
					src_disk_to_read = (src_disk_to_read + src_f + 1) % NCFS_DATA->disk_total_num;
				}

				//				fprintf(stderr,"Decode with %lld %lld\n",src_disk_to_read,src_block_to_read);

				if(NCFS_DATA->disk_status[src_disk_to_read] != 0){
					available = false;
					break;
				}

				gettimeofday(&starttime,NULL);

				retstat = cacheLayer->readDisk(src_disk_to_read,buf_read,size,src_block_to_read*NCFS_DATA->disk_block_size);

				gettimeofday(&endtime,NULL);

				NCFS_DATA->diskread_time=NCFS_DATA->diskread_time+
					endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;

				if(retstat < size){
					available = false;
					break;
				}
				gettimeofday(&starttime,NULL);
				//for(int j = 0; j < size; ++j){
				for(long long j = 0; j < (long long)(size * sizeof(char) / sizeof(int)); ++j){
					//	buf_code[j] = buf_code[j] ^ buf_read[j];
					intbuf_code[j] = intbuf_code[j] ^ intbuf_read[j];
				}
				gettimeofday(&endtime,NULL);

				NCFS_DATA->encoding_time=NCFS_DATA->encoding_time+
					endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;
			}
		}

		//Successfully decode with SRC
		if(available){
			memcpy(buf,buf_code,size);
			free(buf_code);
			free(buf_read);
			return size;
		}

		/*****************
		 *    Use RS     *
		 *****************/

		temp_buf = (char*)malloc(sizeof(char)*size);
		memset(temp_buf, 0, size);

		buf_temp = (char*)malloc(sizeof(char)*size);
		memset(buf_temp, 0, size);

		memset(buf, 0, size);

		int* intbuf_temp = (int*)buf_temp;

		//record the disk health information
		erased=(int *)malloc(sizeof(int)*(disk_total_num));

		//record the first k healthy disks
		dm_ids=(int *)malloc(sizeof(int)*(data_disk_num));

		//find all the failed disks
		for (i=0; i < disk_total_num; i++){
			if (NCFS_DATA->disk_status[i] == 1){

				erased[i]=1;
			}else{
				erased[i]=0;
			}
		}

		//allocate k*k space for decoding matrix
		decoding_matrix = (int *)malloc(sizeof(int)*data_disk_num*data_disk_num);

		if (jerasure_make_decoding_matrix(data_disk_num, (disk_total_num-data_disk_num),field_power,
					NCFS_DATA->generator_matrix, 
					erased, decoding_matrix, dm_ids) < 0) {
			free(erased);
			free(dm_ids);
			free(decoding_matrix);
			return -1;
		}


		//travel the selected surviving disks
		for(i=0;i<data_disk_num;i++){

			diskblocklocation=block_no*block_size+blockoffset;

			int efficient=decoding_matrix[data_disk_num*disk_id+i];

			gettimeofday(&starttime,NULL);

			//processed data block
			retstat = cacheLayer->readDisk(dm_ids[i],temp_buf,size,diskblocklocation+src_offset);

			gettimeofday(&endtime,NULL);

			NCFS_DATA->diskread_time=NCFS_DATA->diskread_time+
				endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;

			gettimeofday(&starttime,NULL);

			for (j=0; j < size; j++){
				buf_temp[j]=galois_single_multiply((unsigned char)temp_buf[j],efficient,field_power);
			}

			for(j = 0; j < (long long)(size * sizeof(char) / sizeof(int)); ++j){
				intbuf[j] = intbuf[j] ^ intbuf_temp[j];
			}

			gettimeofday(&endtime,NULL);

			NCFS_DATA->encoding_time=NCFS_DATA->encoding_time+
				endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;

			memset(temp_buf, 0, size);


		}


		free(erased);
		free(dm_ids);
		free(decoding_matrix);
		free(temp_buf);
		free(buf_temp);

		return size;
	}

	AbnormalError();
	return -1;

}


/*
 * recovering_src_rs: SRC-ReedSolomon recover
 *
 * @param failed_disk_id: failed disk id
 * @param newdevice: new device to replace the failed disk
 *
 * @return: 0: success ; -1: wrong
 */
int Coding4SrcRS::recover(int failed_disk_id,char* newdevice){

	return recover_conventional(failed_disk_id,newdevice);
}

int Coding4SrcRS::recover_enable_node_encode(int failed_disk_id, char* newdevice, int* node_socketid_array){

	return 0;
}

int Coding4SrcRS::recover_using_multi_threads_nor(int failed_disk_id, char* newdevice, int total_sor_threads){

	return 0;
}

int Coding4SrcRS::recover_using_multi_threads_sor(int failed_disk_id, char* newdevice
		, int total_sor_threads, int number_sor_thread){

	return 0;
}

int Coding4SrcRS::recover_using_access_aggregation(int failed_disk_id, char* newdevice){

	return 0;
}

int Coding4SrcRS::recover_conventional(int failed_disk_id, char* newdevice){

	//size of data to be rebuilded
	int __recoversize = NCFS_DATA->disk_size[failed_disk_id];
	int block_size = NCFS_DATA->disk_block_size;
	char buf[block_size];	
	int* intbuf = (int*)buf;
	char* buf_code = (char*)malloc(sizeof(char) * block_size);
	int* intbuf_code = (int*)buf_code;

	struct timeval starttime, endtime;

	for(int i = 0; i < __recoversize; ++i){

		//reset the buf data
		memset(buf, 0, block_size);

		//Recover SRC Block
		if(((i + 1) % src_f) == 0){
			int src_block_to_read = i + (i+1) / src_f;
			int src_disk_to_read = failed_disk_id;
			for(int j = 0; j < src_f; ++j){
				--src_block_to_read;
				src_disk_to_read = (src_disk_to_read + 1) % (NCFS_DATA->disk_total_num);
				gettimeofday(&starttime,NULL);

				cacheLayer->readDisk(src_disk_to_read,buf_code,block_size,src_block_to_read*block_size);

				gettimeofday(&endtime,NULL);

				NCFS_DATA->diskread_time=NCFS_DATA->diskread_time+
					endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;

				gettimeofday(&starttime,NULL);

				for(long long cnt = 0; cnt < (long long)(block_size * sizeof(char) / sizeof(int)); ++cnt){
					intbuf[cnt] = intbuf[cnt] ^ intbuf_code[cnt];
				}

				gettimeofday(&endtime,NULL);
				NCFS_DATA->encoding_time=NCFS_DATA->encoding_time+
					endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;
			}
			gettimeofday(&starttime,NULL);

			cacheLayer->writeDisk(failed_disk_id,buf,block_size,(i + (i+1) / src_f) * block_size);

			gettimeofday(&endtime,NULL);
			NCFS_DATA->diskwrite_time=NCFS_DATA->diskwrite_time+
				endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;

		}

		long long offset = i * block_size;
		long long src_offset = offset / NCFS_DATA->disk_block_size / src_f * NCFS_DATA->disk_block_size;

		int retstat = fileSystemLayer->codingLayer->decode(failed_disk_id,buf,block_size,offset);

		gettimeofday(&starttime,NULL);

		retstat = cacheLayer->writeDisk(failed_disk_id,buf,block_size,offset+src_offset);

		gettimeofday(&endtime,NULL);

		NCFS_DATA->diskwrite_time=NCFS_DATA->diskwrite_time+
			endtime.tv_sec-starttime.tv_sec+(endtime.tv_usec - starttime.tv_usec)/1000000.0;


	}

	return 0;
}
