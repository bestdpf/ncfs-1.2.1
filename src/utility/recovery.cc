#include "recovery.hh"
#include <sys/time.h>
#include <ctime>

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "../filesystem/filesystem_common.hh"
#include "../filesystem/filesystem_utils.hh"

#include "../cache/cache.hh"
#include "../cache/no_cache.hh"

#include "../storage/storage.hh"
#include "../storage/blockstorage.hh"

#include "../gui/report.hh"
#include "../gui/diskusage_report.hh"
#include "../gui/process_report.hh"

#include "../config/config.hh"

#include "../coding/coding.hh"
#include "../coding/coding_jbod.hh"
#include "../coding/coding_raid0.hh"
#include "../coding/coding_raid1.hh"
#include "../coding/coding_raid4.hh"
#include "../coding/coding_raid5.hh"
#include "../coding/coding_raid6.hh"
#include "../coding/coding_rs.hh"
#include "../coding/coding_src_rs.hh"
#include "../coding/coding_mbr.hh"



struct ncfs_state* NCFS_DATA;
FileSystemLayer* fileSystemLayer;
ConfigLayer* configLayer;
CacheLayerBase* cacheLayer;
StorageLayer* storageLayer;
ReportLayer* reportLayer;
DiskusageReport* diskusageLayer;
ProcessReport *processReport;

#define MAX_CONNECTION_NUM 50
#define PORT 54321

char _newdevice[PATH_MAX];
int _failed_disk_id;
int _rebuild_method;

int _total_sor_threads;
int _number_sor_thread;

int _socketid[MAX_CONNECTION_NUM];

/*
 * If Proxy can connect to all the deamons on surviving storage nodes,
 * then return 1, else return 0 
 */
int TestConnectionToStorageNode(int failed_disk_id){

	int retstat=1;

	//create socket connections to surviving storage nodes.
	//storage connection id for each storage node
	int disk_total_num=NCFS_DATA->disk_total_num;

	//socket address for deamon on storage node
	struct sockaddr_in servaddr;

	int i;

	//for EVENODD, we only need the last two node with encoding capability
	for(i=(disk_total_num-2);i<disk_total_num;i++){

		if(i!=failed_disk_id){

			if ((_socketid[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				retstat=0;
				break;
			}else{
				printf("Create connection socket for storage node %d successfully and the socket id is %d.\n",
						i,_socketid[i]);
			}

			memset(&servaddr, 0, sizeof(servaddr));
			servaddr.sin_family = AF_INET;

			//storage node ip address
			servaddr.sin_addr.s_addr = inet_addr(NCFS_DATA->ip_addr[i]);
			servaddr.sin_port = htons(PORT);

			/* associate the opened socket with the destination's address */
			if (connect(_socketid[i], (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {

				printf("There is no deamon on storage node %d.\n",i);
				retstat=0;
				break;
			}	      

		}

	}


	return retstat;
}


/*
 * traditional recover method, storage node without encoding capability
 * recover method id :0
 */
int ConventionalRecover(int fail_disk_id){

	//Not Cauchy RS Code And Not EVENODD And STAR Code
	if((NCFS_DATA->disk_raid_type!=12)&&(NCFS_DATA->disk_raid_type!=11)&&(NCFS_DATA->disk_raid_type!=9)
			&&(NCFS_DATA->disk_raid_type!=8)&&(NCFS_DATA->disk_raid_type!=13)&&(NCFS_DATA->disk_raid_type!=130)){

		NCFS_DATA->process_state = 1;
		//int __recoversize = NCFS_DATA->free_offset[fail_disk_id];
		int __recoversize = NCFS_DATA->disk_size[fail_disk_id];

		struct timeval starttime, endtime;
		double duration;
		double data_size;

		gettimeofday(&starttime,NULL);
		NCFS_DATA->dev_name[fail_disk_id] = (char*)realloc(NCFS_DATA->dev_name[fail_disk_id],strlen(_newdevice));

		memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice)+1);
		strncpy(NCFS_DATA->dev_name[fail_disk_id],_newdevice,strlen(_newdevice));
		cacheLayer->setDiskName(fail_disk_id,_newdevice);

		//storageLayer->disk_fd[fail_disk_id] = open(NCFS_DATA->dev_name[fail_disk_id],O_RDWR);
		storageLayer->DiskRenew(fail_disk_id);

		//number of blocks in the failed disk
		int total_block_num=__recoversize;

		//Do not support the incomplement segment now...
		if(NCFS_DATA->disk_raid_type==7){//X-Code

			//number of segment in the failed disk
			int last_segment_blocks=total_block_num%(NCFS_DATA->data_disk_num);

			total_block_num=total_block_num-last_segment_blocks;


		}else if((NCFS_DATA->disk_raid_type==8)||(NCFS_DATA->disk_raid_type==11)){//EVENODD and STAR Code

			//number of segment in the failed disk
			int last_segment_blocks=total_block_num%(NCFS_DATA->data_disk_num-1);

			total_block_num=total_block_num-last_segment_blocks;


		}else if(NCFS_DATA->disk_raid_type==9){//RDP

			//number of segment in the failed disk
			int last_segment_blocks=total_block_num%(NCFS_DATA->data_disk_num);

			total_block_num=total_block_num-last_segment_blocks;


		}

		for(int i = 0; i < total_block_num; ++i){
			int block_size = NCFS_DATA->disk_block_size;
			int offset = i * block_size;
			//(bug@@)char *buf = (char*)calloc(block_size,0);
			char buf[block_size];
			int retstat = fileSystemLayer->codingLayer->decode(fail_disk_id,buf,block_size,offset);

			retstat = cacheLayer->writeDisk(fail_disk_id,buf,block_size,offset);

		}



		//NCFS_DATA->disk_status[fail_disk_id] = 0;
		fileSystemLayer->set_device_status(fail_disk_id,0);

		gettimeofday(&endtime,NULL);

		fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",fail_disk_id,_newdevice);

		duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
		data_size  = __recoversize * (NCFS_DATA->disk_block_size) / (1024 * 1024);

		printf("Read time = %fs\n", NCFS_DATA->diskread_time);
		printf("Write time = %fs\n", NCFS_DATA->diskwrite_time);
		printf("Encoding time = %fs\n", NCFS_DATA->encoding_time);
		printf("Elapsed Time = %fs\n", duration);
		printf("Repair Throughput = %f MB/s\n", (float)(data_size / duration));
		printf("Storage Node Size = %f MB\n", (float)data_size);
		printf("Block Size = %d B\n", NCFS_DATA->disk_block_size);

		//Add By Zhuyunfeng on Aug 23,2011 begin. 
		//record the experiment results
		FILE *result_record_file;

		time_t tTime = time(NULL);

		//record experiment results
		if ((result_record_file = fopen("ExperimentResult","a")) != NULL){

			fprintf(result_record_file,"\nRecord Timestamp = %s\n", asctime(localtime(&tTime)));
			fprintf(result_record_file,"Read time = %fs\n", NCFS_DATA->diskread_time);
			fprintf(result_record_file,"Write time = %fs\n", NCFS_DATA->diskwrite_time);
			fprintf(result_record_file,"Encoding time = %fs\n", NCFS_DATA->encoding_time);
			fprintf(result_record_file,"Elapsed Time = %fs\n", duration);
			fprintf(result_record_file,"Repair Throughput = %f MB/s\n", (float)(data_size / duration));
			fprintf(result_record_file,"Storage Node Size = %f MB\n", (float)data_size);
			fprintf(result_record_file,"Block Size = %d B\n", NCFS_DATA->disk_block_size);
			fclose(result_record_file);
		}
		//Add By Zhuyunfeng on Aug 23,2011 end. 

		NCFS_DATA->process_state = 0;
		return 0;

	}else{

		NCFS_DATA->process_state = 1;
		//int __recoversize = NCFS_DATA->free_offset[fail_disk_id];
		int __recoversize = NCFS_DATA->disk_size[fail_disk_id];

		struct timeval starttime, endtime;
		double duration;
		double data_size;

		gettimeofday(&starttime,NULL);
		NCFS_DATA->dev_name[fail_disk_id] = (char*)realloc(NCFS_DATA->dev_name[fail_disk_id],strlen(_newdevice));

		//memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice));
		memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice)+1);
		strncpy(NCFS_DATA->dev_name[fail_disk_id],_newdevice,strlen(_newdevice));
		cacheLayer->setDiskName(fail_disk_id,_newdevice);
		storageLayer->DiskRenew(fail_disk_id);

		int retstat = fileSystemLayer->codingLayer->recover_conventional(fail_disk_id,_newdevice);

		//NCFS_DATA->disk_status[fail_disk_id] = 0;
		fileSystemLayer->set_device_status(fail_disk_id,0);

		gettimeofday(&endtime,NULL);

		fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",fail_disk_id,_newdevice);

		duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
		data_size  = __recoversize * (NCFS_DATA->disk_block_size) / (1024 * 1024);

		printf("Read time = %fs\n", NCFS_DATA->diskread_time);
		printf("Write time = %fs\n", NCFS_DATA->diskwrite_time);
		printf("Encoding time = %fs\n", NCFS_DATA->encoding_time);
		printf("Elapsed time = %fs\n",duration);
		printf("Throughput = %f MB/s\n",(float)(data_size / duration));
		printf("Data size = %f MB\n",(float)data_size);

		//Add By Zhuyunfeng on Aug 23,2011 begin. 
		//record the experiment results
		FILE *result_record_file;

		time_t tTime = time(NULL);

		//record experiment results
		if ((result_record_file = fopen("ExperimentResult","a")) != NULL){

			fprintf(result_record_file,"\nRecord Timestamp = %s\n", asctime(localtime(&tTime)));
			fprintf(result_record_file,"Read time = %fs\n", NCFS_DATA->diskread_time);
			fprintf(result_record_file,"Write time = %fs\n", NCFS_DATA->diskwrite_time);
			fprintf(result_record_file,"Encoding time = %fs\n", NCFS_DATA->encoding_time);
			fprintf(result_record_file,"Elapsed Time = %fs\n", duration);
			fprintf(result_record_file,"Repair Throughput = %f MB/s\n", (float)(data_size / duration));
			fprintf(result_record_file,"Data Size = %f MB\n", (float)data_size);
			fprintf(result_record_file,"Block Size = %d B\n", NCFS_DATA->disk_block_size);
			fclose(result_record_file);
		}
		//Add By Zhuyunfeng on Aug 23,2011 end. 

		NCFS_DATA->process_state = 0;
		return retstat;
	}
}

/*
 * Dimakis recover method, storage node without encoding capability
 * recover method id :1
 */
int DimakisRecoverWithNoEncodingCapability(int fail_disk_id){

	NCFS_DATA->process_state = 1;
	//int __recoversize = NCFS_DATA->free_offset[fail_disk_id];
	int __recoversize = NCFS_DATA->disk_size[fail_disk_id];

	struct timeval starttime, endtime;
	double duration;
	double data_size;

	gettimeofday(&starttime,NULL);
	NCFS_DATA->dev_name[fail_disk_id] = (char*)realloc(NCFS_DATA->dev_name[fail_disk_id],strlen(_newdevice));

	//memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice));
	memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice)+1);
	strncpy(NCFS_DATA->dev_name[fail_disk_id],_newdevice,strlen(_newdevice));
	cacheLayer->setDiskName(fail_disk_id,_newdevice);
	storageLayer->DiskRenew(fail_disk_id);

	int retstat = fileSystemLayer->codingLayer->recover(fail_disk_id,_newdevice);

	//NCFS_DATA->disk_status[fail_disk_id] = 0;
	fileSystemLayer->set_device_status(fail_disk_id,0);

	gettimeofday(&endtime,NULL);

	fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",fail_disk_id,_newdevice);

	duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
	data_size  = __recoversize * (NCFS_DATA->disk_block_size) / (1024 * 1024);

	printf("Read time = %fs\n", NCFS_DATA->diskread_time);
	printf("Write time = %fs\n", NCFS_DATA->diskwrite_time);
	printf("Encoding time = %fs\n", NCFS_DATA->encoding_time);
	printf("Elapsed Time = %fs\n", duration);
	printf("Repair Throughput = %f MB/s\n", (float)(data_size / duration));
	printf("Storage Node Size = %f MB\n", (float)data_size);
	printf("Block Size = %d B\n", NCFS_DATA->disk_block_size);

	//Add By Zhuyunfeng on Aug 23,2011 begin. 
	//record the experiment results
	FILE *result_record_file;

	time_t tTime = time(NULL);

	//record experiment results
	if ((result_record_file = fopen("ExperimentResult","a")) != NULL){

		fprintf(result_record_file,"\nRecord Timestamp = %s\n", asctime(localtime(&tTime)));
		fprintf(result_record_file,"Read time = %fs\n", NCFS_DATA->diskread_time);
		fprintf(result_record_file,"Write time = %fs\n", NCFS_DATA->diskwrite_time);
		fprintf(result_record_file,"Encoding time = %fs\n", NCFS_DATA->encoding_time);
		fprintf(result_record_file,"Elapsed Time = %fs\n", duration);
		fprintf(result_record_file,"Repair Throughput = %f MB/s\n", (float)(data_size / duration));
		fprintf(result_record_file,"Storage Node Size = %f MB\n", (float)data_size);
		fprintf(result_record_file,"Block Size = %d B\n", NCFS_DATA->disk_block_size);
		fclose(result_record_file);
	}
	//Add By Zhuyunfeng on Aug 23,2011 end. 

	NCFS_DATA->process_state = 0;
	return retstat;
}

/*
 * recover method, storage node with encoding capability
 * recover method id :2
 */
int RecoverWithEncodingCapability(int fail_disk_id){

	NCFS_DATA->process_state = 1;
	//int __recoversize = NCFS_DATA->free_offset[fail_disk_id];
	int __recoversize = NCFS_DATA->disk_size[fail_disk_id];

	struct timeval starttime, endtime;
	double duration;
	double data_size;

	gettimeofday(&starttime,NULL);
	NCFS_DATA->dev_name[fail_disk_id] = (char*)realloc(NCFS_DATA->dev_name[fail_disk_id],strlen(_newdevice));
	memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice)+1);
	strncpy(NCFS_DATA->dev_name[fail_disk_id],_newdevice,strlen(_newdevice));
	cacheLayer->setDiskName(fail_disk_id,_newdevice);
	storageLayer->DiskRenew(fail_disk_id);


	int retstat = fileSystemLayer->codingLayer->recover_enable_node_encode(fail_disk_id,_newdevice,_socketid);

	//NCFS_DATA->disk_status[fail_disk_id] = 0;
	fileSystemLayer->set_device_status(fail_disk_id,0);

	gettimeofday(&endtime,NULL);

	fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",fail_disk_id,_newdevice);

	duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
	data_size  = __recoversize * (NCFS_DATA->disk_block_size) / (1024 * 1024);
	printf("Elapsed time = %fs\n",duration);
	printf("Throughput = %f MB/s\n",(float)(data_size / duration));
	printf("Data size = %f MB\n",(float)data_size);

	NCFS_DATA->process_state = 0;
	return retstat;
}

/*
 * recover failed storage node using access aggregation methods
 * recover method id :3
 */
int RecoverUsingAccessAggregation(int fail_disk_id){

	NCFS_DATA->process_state = 1;

	int __recoversize = NCFS_DATA->disk_size[fail_disk_id];

	struct timeval starttime, endtime;
	double duration;
	double data_size;

	gettimeofday(&starttime,NULL);
	NCFS_DATA->dev_name[fail_disk_id] = (char*)realloc(NCFS_DATA->dev_name[fail_disk_id],strlen(_newdevice));
	memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice)+1);
	strncpy(NCFS_DATA->dev_name[fail_disk_id],_newdevice,strlen(_newdevice));
	cacheLayer->setDiskName(fail_disk_id,_newdevice);
	storageLayer->DiskRenew(fail_disk_id);


	int retstat = fileSystemLayer->codingLayer->recover_using_access_aggregation(fail_disk_id,_newdevice);

	//NCFS_DATA->disk_status[fail_disk_id] = 0;
	fileSystemLayer->set_device_status(fail_disk_id,0);

	gettimeofday(&endtime,NULL);

	fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",fail_disk_id,_newdevice);

	duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
	data_size  = __recoversize * (NCFS_DATA->disk_block_size) / (1024 * 1024);
	printf("Elapsed time = %fs\n",duration);
	printf("Throughput = %f MB/s\n",(float)(data_size / duration));
	printf("Data size = %f MB\n",(float)data_size);

	//Add By Zhuyunfeng on Aug 23,2011 begin. 
	//record the experiment results
	FILE *result_record_file;

	time_t tTime = time(NULL);

	//record experiment results
	if ((result_record_file = fopen("ExperimentResult","a")) != NULL){

		fprintf(result_record_file,"\nRecord Timestamp = %s\n", asctime(localtime(&tTime)));
		fprintf(result_record_file,"Read time = %fs\n", NCFS_DATA->diskread_time);
		fprintf(result_record_file,"Write time = %fs\n", NCFS_DATA->diskwrite_time);
		fprintf(result_record_file,"Encoding time = %fs\n", NCFS_DATA->encoding_time);
		fprintf(result_record_file,"Elapsed Time = %fs\n", duration);
		fprintf(result_record_file,"Repair Throughput = %f MB/s\n", (float)(data_size / duration));
		fprintf(result_record_file,"Storage Node Size = %f MB\n", (float)data_size);
		fprintf(result_record_file,"Block Size = %d B\n", NCFS_DATA->disk_block_size);
		fclose(result_record_file);
	}
	//Add By Zhuyunfeng on Aug 23,2011 end. 

	NCFS_DATA->process_state = 0;
	return retstat;
}

/*
 * Recover Failed Storage Node Using Stripe-Oriented Multithreading 
 * recover method id :5
 */
int RecoverUsingMultiThreads_NOR(int fail_disk_id){

	NCFS_DATA->process_state = 1;

	int __recoversize = NCFS_DATA->disk_size[fail_disk_id];

	struct timeval starttime, endtime;
	double duration;
	double data_size;

	gettimeofday(&starttime,NULL);
	NCFS_DATA->dev_name[fail_disk_id] = (char*)realloc(NCFS_DATA->dev_name[fail_disk_id],strlen(_newdevice));
	memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice)+1);
	strncpy(NCFS_DATA->dev_name[fail_disk_id],_newdevice,strlen(_newdevice));
	cacheLayer->setDiskName(fail_disk_id,_newdevice);
	storageLayer->DiskRenew(fail_disk_id);


	int retstat = fileSystemLayer->codingLayer->recover_using_multi_threads_nor(fail_disk_id,_newdevice,_total_sor_threads);

	//NCFS_DATA->disk_status[fail_disk_id] = 0;
	fileSystemLayer->set_device_status(fail_disk_id,0);

	gettimeofday(&endtime,NULL);

	fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",fail_disk_id,_newdevice);

	duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
	data_size  = __recoversize * (NCFS_DATA->disk_block_size) / (1024 * 1024);
	printf("Elapsed time = %fs\n",duration);
	printf("Throughput = %f MB/s\n",(float)(data_size / duration));
	printf("Data size = %f MB\n",(float)data_size);

	//Add By Zhuyunfeng on Aug 23,2011 begin. 
	//record the experiment results
	FILE *result_record_file;

	time_t tTime = time(NULL);

	//record experiment results
	if ((result_record_file = fopen("ExperimentResult","a")) != NULL){

		fprintf(result_record_file,"\nRecord Timestamp = %s\n", asctime(localtime(&tTime)));
		//fprintf(result_record_file,"Read time = %fs\n", NCFS_DATA->diskread_time);
		//fprintf(result_record_file,"Write time = %fs\n", NCFS_DATA->diskwrite_time);
		//fprintf(result_record_file,"Encoding time = %fs\n", NCFS_DATA->encoding_time);
		fprintf(result_record_file,"Elapsed Time = %fs\n", duration);
		fprintf(result_record_file,"Repair Throughput = %f MB/s\n", (float)(data_size / duration));
		fprintf(result_record_file,"Storage Node Size = %f MB\n", (float)data_size);
		fprintf(result_record_file,"Block Size = %d B\n", NCFS_DATA->disk_block_size);
		fclose(result_record_file);
	}
	//Add By Zhuyunfeng on Aug 23,2011 end. 

	NCFS_DATA->process_state = 0;
	return retstat;
}

/*
 * recover failed storage node using Stripe-Oriented Multi-Proxy Recovering
 * recover method id :4
 */
int RecoverUsingMultiThreads_SOR(int fail_disk_id){

	NCFS_DATA->process_state = 1;

	int __recoversize = NCFS_DATA->disk_size[fail_disk_id];

	struct timeval starttime, endtime;
	double duration;
	double data_size;

	gettimeofday(&starttime,NULL);
	NCFS_DATA->dev_name[fail_disk_id] = (char*)realloc(NCFS_DATA->dev_name[fail_disk_id],strlen(_newdevice));
	memset(NCFS_DATA->dev_name[fail_disk_id],0,strlen(_newdevice)+1);
	strncpy(NCFS_DATA->dev_name[fail_disk_id],_newdevice,strlen(_newdevice));
	cacheLayer->setDiskName(fail_disk_id,_newdevice);
	storageLayer->DiskRenew(fail_disk_id);


	int retstat = fileSystemLayer->codingLayer->recover_using_multi_threads_sor(fail_disk_id,_newdevice
			,_total_sor_threads,_number_sor_thread);

	//NCFS_DATA->disk_status[fail_disk_id] = 0;
	fileSystemLayer->set_device_status(fail_disk_id,0);

	gettimeofday(&endtime,NULL);

	fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",fail_disk_id,_newdevice);

	duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
	data_size  = __recoversize * (NCFS_DATA->disk_block_size) / (1024 * 1024);
	printf("Elapsed time = %fs\n",duration);
	printf("Throughput = %f MB/s\n",(float)(data_size / duration));
	printf("Data size = %f MB\n",(float)data_size);

	//Add By Zhuyunfeng on Aug 23,2011 begin. 
	//record the experiment results
	FILE *result_record_file;

	time_t tTime = time(NULL);

	//record experiment results
	if ((result_record_file = fopen("ExperimentResult","a")) != NULL){

		fprintf(result_record_file,"\nRecord Timestamp = %s\n", asctime(localtime(&tTime)));
		fprintf(result_record_file,"Read time = %fs\n", NCFS_DATA->diskread_time);
		fprintf(result_record_file,"Write time = %fs\n", NCFS_DATA->diskwrite_time);
		fprintf(result_record_file,"Encoding time = %fs\n", NCFS_DATA->encoding_time);
		fprintf(result_record_file,"Elapsed Time = %fs\n", duration);
		fprintf(result_record_file,"Repair Throughput = %f MB/s\n", (float)(data_size / duration));
		fprintf(result_record_file,"Storage Node Size = %f MB\n", (float)data_size);
		fprintf(result_record_file,"Block Size = %d B\n", NCFS_DATA->disk_block_size);
		fclose(result_record_file);
	}
	//Add By Zhuyunfeng on Aug 23,2011 end. 

	NCFS_DATA->process_state = 0;
	return retstat;
}


void RecoveryTool::recover(){
	int coding_type = NCFS_DATA->disk_raid_type;
	int disk_total_num = NCFS_DATA->disk_total_num;
	int fail_disk_num = 0;
	int fail_disk_id = 0;


	for(int i = disk_total_num-1; i >= 0; --i){
		if(NCFS_DATA->disk_status[i] !=0){
			++fail_disk_num;
		}
	}

	fail_disk_id=_failed_disk_id;

	if (fail_disk_num > 0){
		if (fail_disk_num == 1){

			if(_rebuild_method==0){

				printf("CASE 1: Traditional Recover Method By Downloading Full File.\n");

				ConventionalRecover(fail_disk_id);
			}else if(_rebuild_method==1){
				printf("CASE 2: Dimakis Recover Method Without Node Encoding Capability.\n");

				DimakisRecoverWithNoEncodingCapability(fail_disk_id);
			}else if(_rebuild_method==2){

				printf("CASE 3: Recover Method With Node Encoding Capability.\n");

				if(TestConnectionToStorageNode(fail_disk_id)){
					RecoverWithEncodingCapability(fail_disk_id); 
				}else{
					printf("Error:Some Storage Have No Encoding Deamon!\n");
					exit(-1);
				}

			}else if(_rebuild_method==3){
				printf("CASE 4: Recover Method Using Accress Aggregation Methods.\n");

				RecoverUsingAccessAggregation(fail_disk_id);

			}else if(_rebuild_method==4){
				printf("CASE 5: Recover Method Using SOR-Based Multi-Proxy Methods.\n");

				RecoverUsingMultiThreads_SOR(fail_disk_id);
			}else if(_rebuild_method==5){
				printf("CASE 6: Recover Method Using SOR-Based Multi-Threading Methods.\n");

				RecoverUsingMultiThreads_NOR(fail_disk_id);
			}else{
				printf("Error:Unknown Recovery Method!\n");
				exit(-1);
			}
		}
		else if ((fail_disk_num == 2) && (coding_type == 6)){
			ConventionalRecover(fail_disk_id);
		}
		//Add by zhuyunfeng on 2011-03-28 end.
		else if ((coding_type == 1000) &&
				(fail_disk_num <= NCFS_DATA->mbr_n - NCFS_DATA->mbr_k)){
			ConventionalRecover(fail_disk_id);
		}
		else{
			printf("Too many disks fail.\n");
		}
	}
	else{
		printf("No disk fails.\n");
	}

}

//Initialize Disk Layer
StorageLayer* initializeStorageLayer(){

	//Currently only use blockstorage
	return new BlockStorage;
}

CodingLayer* initializeCodingLayer(){

	switch(NCFS_DATA->disk_raid_type){
		case 0: return new Coding4Raid0;break;
		case 1: return new Coding4Raid1;break;
		case 4: return new Coding4Raid4;break;
		case 5: return new Coding4Raid5;break;
		case 6: return new Coding4Raid6();break;	
		case 13: return new Coding4RS();break;	
		case 130: return new Coding4SrcRS();break;
		case 100: return new Coding4Jbod;break;
		case 1000: return new Coding4Mbr();break;
	}

	//AbnormalError();
	return NULL;  

}

/*Initialize CacheLayer, StorageLayer and CodingLayer*/
void recovery_init(){

	cacheLayer = new NoCache();
	//cacheLayer = new CacheLayer();
	//storageLayer = new StorageLayer();
	storageLayer = initializeStorageLayer();

	//reportLayer = new ReportLayer();

	if (NCFS_DATA->no_gui == 0){
		diskusageLayer = new DiskusageReport(100);
		processReport = new ProcessReport();
	}

	fileSystemLayer->codingLayer = initializeCodingLayer();
}

/*
 * Recovery Utility:User uses this utility by the following command:
 *     ./recover (new device name) (failed node id) (recovery method)
 *     ./recover (new device name) (failed node id) (recovery method) (number of threads)
 *     ./recover (new device name) (failed node id) (recovery method) (total sor threads) (number sor thread)
 */
int main(int argc, char *args[]){

	int j;

	if((argc!=4)&&(argc!=5)&&(argc!=6)){
		printf("Usage: ./recover (new device name) (failed node id) (recovery method)\n");
		abort();
	}

	struct ncfs_state *ncfs_data;

	ncfs_data = (struct ncfs_state *) calloc(sizeof(struct ncfs_state), 1);
	if (ncfs_data == NULL) {
		perror("main calloc");
		abort();
	}

	//Initialize the fileSystemLayer and configLayer
	fileSystemLayer = new FileSystemLayer();
	configLayer = new ConfigLayer();

	//initialize those variables for ncfs_data
	ncfs_data->no_cache = 0;
	ncfs_data->no_gui = 0;
	ncfs_data->run_experiment = 0;

	ncfs_data->process_state = 0;
	ncfs_data->encoding_time = 0;
	ncfs_data->decoding_time = 0;
	ncfs_data->diskread_time = 0;
	ncfs_data->diskwrite_time = 0;

	ncfs_data->space_list_num = 0;
	ncfs_data->space_list_head = NULL;

	//read the configure file
	fileSystemLayer->readSystemConfig(ncfs_data);

	for (j=0; j < ncfs_data->disk_total_num; j++){
		ncfs_data->free_offset[j] = (ncfs_data->free_offset[j]) / (ncfs_data->disk_block_size);
		ncfs_data->free_size[j] = (ncfs_data->free_size[j]) / (ncfs_data->disk_block_size);
		ncfs_data->disk_size[j] = (ncfs_data->disk_size[j]) / (ncfs_data->disk_block_size);
	}


	//set the default segment size
	ncfs_data->segment_size = 1;

	fileSystemLayer->get_disk_status(ncfs_data);

	fileSystemLayer->get_operation_mode(ncfs_data);

	for (j=0; j<ncfs_data->disk_total_num; j++){
		printf("***main: j=%d, dev=%s, free_offset=%d, free_size=%d\n",
				j,ncfs_data->dev_name[j], ncfs_data->free_offset[j],ncfs_data->free_size[j]);
	}

	//initialize gobal objects
	NCFS_DATA = ncfs_data;

	recovery_init();

	memset(_newdevice,0,PATH_MAX);
	strncpy(_newdevice,args[1],strlen(args[1]));
	_failed_disk_id=atoi(args[2]);
	_rebuild_method=atoi(args[3]);

	//recover the failed node using Stripe-Oriented-Recovery Multi-Threading Method
	if(_rebuild_method==5){

		_total_sor_threads=atoi(args[4]);

	}

	//recover the failed node using Stripe-Oriented-Recovery Multi-Proxy Method
	if(_rebuild_method==4){

		_total_sor_threads=atoi(args[4]);
		_number_sor_thread=atoi(args[5]);

	}

	//set status of the failed disk to be 1 
	fileSystemLayer->set_device_status(_failed_disk_id,1);

	if(_rebuild_method==4){

		printf("Recover: -%s -%d -%d -%d -%d\n",_newdevice,_failed_disk_id,_rebuild_method,
				_total_sor_threads,_number_sor_thread);
	}else if(_rebuild_method==5){

		printf("Recover: -%s -%d -%d -%d\n",_newdevice,_failed_disk_id,_rebuild_method,
				_total_sor_threads);
	}else{
		printf("Recover: -%s -%d -%d\n",_newdevice,_failed_disk_id,_rebuild_method);
	}

	RecoveryTool::recover();

	return 1;
}
