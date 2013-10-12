/*
 * Repair Task Executer
 *
 * This Repair Deamon Is A Repair Task Executer 
 *
 */
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
#include <sys/time.h>
#include <arpa/inet.h>

#include "../filesystem/filesystem_common.hh"
#include "../filesystem/filesystem_utils.hh"

#include "../gui/report.hh"
#include "../gui/diskusage_report.hh"
#include "../gui/process_report.hh"

#include "../cache/cache.hh"
#include "../cache/no_cache.hh"

#include "../storage/storage.hh"
#include "../storage/blockstorage.hh"

#include "../config/config.hh"

#include "../coding/coding.hh"
#include "../coding/coding_rs.hh"
#include "../coding/coding_jbod.hh"
#include "../coding/coding_mbr.hh"
#include "../coding/coding_raid0.hh"
#include "../coding/coding_raid1.hh"
#include "../coding/coding_raid4.hh"
#include "../coding/coding_raid5.hh"
#include "../coding/coding_raid6.hh"
#include "../coding/coding_src_rs.hh"

struct ncfs_state* NCFS_DATA;
FileSystemLayer* fileSystemLayer;
ConfigLayer* configLayer;
CacheLayerBase* cacheLayer;
StorageLayer* storageLayer;
ReportLayer* reportLayer;
DiskusageReport* diskusageLayer;
ProcessReport *processReport;

#define PORT 54321
#define SPATH_MAX 40

int _failed_node_id;
int _rebuild_method;
int _repair_tasks_num;
int _repair_task_seq;
char _new_device_name[SPATH_MAX];


/*
 * Repair Task Parameters for Executer
 */
struct repair_task_params{

	//total number of repair tasks
	int repair_tasks_num;

	//seq of repair tasks
	int repair_task_seq;

	//id of the failed node
	int failed_node_id;

	//repair method
	int repair_method;

	//name of the device to 
	//replace the failed node
	char new_device_name[SPATH_MAX];
};


/*
 * recover failed storage node using multithreads
 * recover method id :4
 */
int RecoverUsingMultiThreads_NOR(int fail_disk_id){

	int retstat = fileSystemLayer->codingLayer->recover_using_multi_threads_nor(fail_disk_id,
			_new_device_name,_repair_tasks_num);

	return retstat;
}

void init_repair_setting(){

	NCFS_DATA->process_state = 1;

	NCFS_DATA->dev_name[_failed_node_id] = (char*)realloc(NCFS_DATA->dev_name[_failed_node_id],strlen(_new_device_name));
	memset(NCFS_DATA->dev_name[_failed_node_id],0,strlen(_new_device_name)+1);
	strncpy(NCFS_DATA->dev_name[_failed_node_id],_new_device_name,strlen(_new_device_name));
	cacheLayer->setDiskName(_failed_node_id,_new_device_name);
	storageLayer->DiskRenew(_failed_node_id);

}

/*
 * recover failed storage node using multithreads
 * recover method id :5
 */
int RecoverUsingMultiThreads_SOR(int fail_disk_id){

	int retstat = fileSystemLayer->codingLayer->recover_using_multi_threads_sor(fail_disk_id, _new_device_name
			, _repair_tasks_num, _repair_task_seq);

	return retstat;
}

/*
 * Recover Interface for Repair Task
 */
void recover_interface(){

	int failed_node_id = 0;

	failed_node_id=_failed_node_id;

	int fail_disk_num=1;

	if (fail_disk_num > 0){
		if (fail_disk_num == 1){

			if(_rebuild_method==0){

				printf("CASE 1: Traditional Recover Method By Downloading Full File Size Blocks.\n");

			}else if(_rebuild_method==1){
				printf("CASE 2: Hybrid Parity Recover Method Without Node Encoding Capability.\n");

			}else if(_rebuild_method==2){

				printf("CASE 3: Recover Method With Node Encoding Capability.\n");

			}else if(_rebuild_method==3){
				printf("CASE 4: Recover Method Using Accress Aggregation Methods.\n");

			}else if(_rebuild_method==4){
				printf("CASE 5: Recover Method Using SOR-Based Multi-Proxy Methods.\n");

				RecoverUsingMultiThreads_SOR(failed_node_id);
			}else if(_rebuild_method==5){
				printf("CASE 6: Recover Method Using SOR-Based Multi-Threading Methods.\n");

				RecoverUsingMultiThreads_NOR(failed_node_id);
			}else{
				printf("Error:Unknown Recovery Method!\n");
				exit(-1);
			}
		}
		else{
			printf("Too many disks fail.\n");
		}
	}
	else{
		printf("No Node Fails.\n");
	}

}


/* Repair Task Executer
 * @param s
 *        socket id
 */
void process_th(int s) {

	struct repair_task_params *repairTaskParam;

	//allocate space for encoding_params
	repairTaskParam = (struct repair_task_params *) calloc(sizeof(struct repair_task_params), 1);

	int clisd = s;

	int first_execute=1;


	while (1) {

		//receive repair task scheduler repair request
		int len = recv(clisd, repairTaskParam, sizeof(struct repair_task_params), 0);

		if (len == 0) {
			printf("Connection closed\n");
			close(clisd);
			return;
		} else if (len < 0) {
			perror("ERROR: error in recv\n");
			close(clisd);
			return;
		}

		//get the parameters for repair task
		_failed_node_id=repairTaskParam->failed_node_id;
		_rebuild_method=repairTaskParam->repair_method;
		_repair_tasks_num=repairTaskParam->repair_tasks_num;
		_repair_task_seq=repairTaskParam->repair_task_seq;

		memset(_new_device_name,0,SPATH_MAX);
		strncpy(_new_device_name,repairTaskParam->new_device_name,strlen(repairTaskParam->new_device_name));

		int* repair_task_finish = (int*)malloc(sizeof(int));
		memset(repair_task_finish, 0, sizeof(int));

		//set status of the failed node to be 1 
		fileSystemLayer->set_device_status(_failed_node_id,1);

		if(_rebuild_method==4){

			printf("Recover: -%s -%d -%d -%d -%d\n",_new_device_name,_failed_node_id,_rebuild_method,
					_repair_tasks_num,_repair_task_seq);

		}else if(_rebuild_method==5){

			printf("Recover: -%s -%d -%d -%d\n",_new_device_name,_failed_node_id,_rebuild_method,
					_repair_tasks_num);

		}else{
			printf("Recover: -%s -%d -%d\n",_new_device_name,_failed_node_id,_rebuild_method);

		}

		//The first time to execute the repair task,
		//you need to initialize some environment 
		//parameters
		if(first_execute){

			init_repair_setting();

			first_execute=0;
		}

		recover_interface();

		repair_task_finish[0]=_repair_task_seq;

		//send repair result
		if ((len = send(clisd, repair_task_finish, sizeof(int), 0)) >= 0) {

			printf("Success: Task Executer Have Finished Repair Task %d\n", repair_task_finish[0]);

		}else{		
			perror("Error: Task Executer Can't Send Information To Scheduler\n");
			break;
		}

	}

	close(clisd);
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
				   //default: return -1;break;
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

int main(int argc, char *argv[]){

	int j;


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

	struct sockaddr_in executer_addr, scheduler_addr;
	int executer_sd, scheduler_sd;
	int scheduler_addrlen;
	int one = 1;

	/* create socket for repair task executer */	
	if ((executer_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("ERROR: cannot create socket\n");
		exit(-1);
	}

	/* set socket option */
	if (setsockopt(executer_sd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one))< 0) {
		perror("ERROR: cannot set socket option\n");
		exit(-1);
	}

	/* prepare the address structure */	
	executer_addr.sin_family = AF_INET;
	executer_addr.sin_port = htons(PORT);
	executer_addr.sin_addr.s_addr = htonl(INADDR_ANY); 

	/* bind the socket to network structure */
	if (bind(executer_sd, (struct sockaddr *)&executer_addr, sizeof(executer_addr)) < 0) {
		perror("Can't bind the socket\n");
		exit(-1);
	}

	/* listen for any pending request */
	if (listen(executer_sd, 100) < 0) {
		perror("Can't listen\n");
		exit(-1);
	}

	printf("Repair Task Executer Is Ready To Work.....\n");

	/* get the size of the client address sturcture */
	scheduler_addrlen = sizeof(scheduler_addr);

	while (1) {

		/* wait for connection from Repair Task Scheduler, scheduler_sd is created for scheduler specially*/
		scheduler_sd = accept(executer_sd, (struct sockaddr *)&scheduler_addr, (socklen_t*) &scheduler_addrlen);

		printf("Repair Task Executer Has Connected With Repair Task Scheduler.\n");

		process_th(scheduler_sd);

	}

	/* Control never goes here */
	return 0;
}
