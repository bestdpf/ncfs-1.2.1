/*
 * Repair Task Scheduler
 *
 * The Function Of Repair Task Scheduler
 * Is As Following:
 *
 *      1. Connnet With Repair Task Executer On 
 *  Each Repair Node
 *      2. Allocate The Repair Task To Availiable
 *  Repair Task Executer
 *      3. Determine Process Of The Whole Repair
 *  Work
 *      4. Calculte Related Global Technical Information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define PORT 54321
#define SPATH_MAX 40
#define MAX_CONNECTION_NUM 6

#define SCHEDULE_QUEUE_LEN 16

int _failed_node_id;
int _rebuild_method;
int _total_repair_tasks;
int _total_task_executers;
char _newdevice[SPATH_MAX];

//store socket id which are used for connecting
int _socketid[MAX_CONNECTION_NUM];

int _high_socketid;

//store the ip information for each repair task executer
char **_executer_ip_addr;

int * _executer_tasks_num;

//store the tasks to be executed
int *task_schedule_queue;

//store the status of the repair task
int *task_status_queue;

//fd descriptor list for select()
fd_set repair_executer_fds;

int next_task_to_schedule;

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


void InitializeRepairExecuters(){
  
  char** ipAddrTemp=(char**)calloc(MAX_CONNECTION_NUM, sizeof(char *));
  
  ipAddrTemp[0] = (char*)calloc(SPATH_MAX,1);
  strcpy(ipAddrTemp[0],"192.168.0.17");
  
  ipAddrTemp[1] = (char*)calloc(SPATH_MAX,1);
  strcpy(ipAddrTemp[1],"192.168.0.117");
  
  ipAddrTemp[2] = (char*)calloc(SPATH_MAX,1);
  strcpy(ipAddrTemp[2],"192.168.0.118");
  
  _executer_ip_addr= (char**)calloc(_total_task_executers, sizeof(char *));
  
  _executer_tasks_num=(int *)calloc(_total_task_executers, sizeof(int));
  
  for(int i=0;i<_total_task_executers;i++){
   	_executer_ip_addr[i] = (char*)calloc(SPATH_MAX,1);	
	strncpy(_executer_ip_addr[i],ipAddrTemp[i],SPATH_MAX); 
	
	_executer_tasks_num[i]=0;
  }
  
  
}

/*
 * If Scheduler Can Connnet To All Repair Task Executers Successfully, 
 * Then Return 1, Else Return 0 
 */
int TestConnectionToRepairTaskExecuter(){
  
        int retstat=1;
	
	//socket address for deamon on storage node
	struct sockaddr_in servaddr;
	
	_high_socketid=-1;
	
	//Find Avaliable Task Executers
	for(int i=0;i<_total_task_executers;i++){

	    if ((_socketid[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		    retstat=0;
		    break;
	    }else{
	      
		    //find max _socketid 
		    if(_socketid[i]>_high_socketid){
		      
		      _high_socketid=_socketid[i];
		    }

		    printf("Repair Executer %d Ip Address is %s.\n",i,_executer_ip_addr[i]);
		    
		    printf("Create Connection Socket for Repair Executer %d Successfully and Socket Id is %d.\n",
							i,_socketid[i]);
							
	    }

	    memset(&servaddr, 0, sizeof(servaddr));
	    servaddr.sin_family = AF_INET;
	    
	    //Ip Address Information for Repair Executers
	    servaddr.sin_addr.s_addr = inet_addr(_executer_ip_addr[i]);
	    servaddr.sin_port = htons(PORT);

	    /* associate the opened socket with the destination's address */
	    if (connect(_socketid[i], (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		    
		    printf("Error: Connection to The %dth Executer Fails.\n",i);
		    retstat=0;
		    break;
	    }	      
	  
	}
	
	
	return retstat;
}

/*
 * Judge whether finish the whole repair work
 * @return 
 *        1: Yes, you have finished the whole work
 *        0: No, there are still some work to do
 */
int finish_repair(){
   
   for(int i=0;i<SCHEDULE_QUEUE_LEN;i++){
    
      if(task_status_queue[i]!=-1){
		return 0;
      }
     
   }
   
   return 1;
}

/*
 * Find available repair task
 * @return 
 *        Repair Task Seq
 */
int find_available_task(){
  
    int retstat=-1;
    
   for(int i=0;i<SCHEDULE_QUEUE_LEN;i++){
    
      //New Task Waitting for Executing
      if(task_status_queue[i]==0){
	
	  retstat=task_schedule_queue[i];
	  return retstat;
      }
     
   }
   
   return -1;
  
}

/*
 * Find available repair task
 * @param start
 * @return 
 *        Repair Task Seq Position
 */
int find_available_task_position(int start){
  
    int retstat=-1;
    
    retstat=start+1;
    
    for(int i=0;i<SCHEDULE_QUEUE_LEN;i++){
    
      retstat=retstat%SCHEDULE_QUEUE_LEN;
      
      //New Task Waitting for Executing
      if(task_status_queue[retstat]==0){
	
	  return retstat;
      }
     
     retstat=retstat+1;
   }
   
   return -1;
  
}
/*
 * At first, allocate a repair task to each task executer
 */
void InitializeRepairTaskAllocation(){
  
	int tmp_task_seq, len;
	
	for(int i=0;i<_total_task_executers;i++){
		    
		tmp_task_seq=find_available_task();
		  
		if(tmp_task_seq!=-1){
		    
		    repair_task_params* tmp_repair_task_params = (struct repair_task_params *) calloc(
									  sizeof(struct repair_task_params), 1);						  
		    tmp_repair_task_params->failed_node_id = _failed_node_id;
		    strncpy(tmp_repair_task_params->new_device_name,_newdevice,strlen(_newdevice));
		    tmp_repair_task_params->repair_method = _rebuild_method;
		    tmp_repair_task_params->repair_tasks_num = _total_repair_tasks;
		    tmp_repair_task_params->repair_task_seq = tmp_task_seq;
		      
		    //Send Repair Task Parameter
		    if ((len = send(_socketid[i], tmp_repair_task_params, sizeof(struct repair_task_params), 0)) >= 0) {
		      
			//the i-th task is now under processing
			task_status_queue[i]=1;
			
		    }else{		
			perror("Error: Fail to Connect to Repair Task Executer.\n");
			exit(-1);
		    }
		
		}//if
		  
	}//for
  
}

/*
 * Find available repair task position
 * @param task_id
 *        Repair Task Id
 * @return 
 *        Repair Task Position
 */
int find_task_position(int task_id){
    
   for(int i=0;i<SCHEDULE_QUEUE_LEN;i++){
    
      //New Task Waitting for Executing
      if(task_schedule_queue[i]==task_id){
	
	  return i;
      }
     
   }
   
   return -1;
  
}

/*
 * Go on to allocate repair task to each task executer
 */
void AllocateRepairTask(){
  
	int tmp_task_seq_pos, len;
	
	for(int i=0;i<_total_task_executers;i++){
		    
		//socketid[i] has finish its repair task
		if(FD_ISSET(_socketid[i],&repair_executer_fds)){
		  
		  //Add by zhuyunfeng on Sep 15, 2011 begin.
		  _executer_tasks_num[i]++;
		  //Add by zhuyunfeng on Sep 15, 2011 end.
		  
		  //Find the task id
  		  int* repair_task_id = (int*)malloc(sizeof(int));
		  memset(repair_task_id, 0, sizeof(int));
		  len = recv(_socketid[i], repair_task_id,sizeof(int), 0);
		  
		  if (len == 0) {
			printf("Connection Closed.\n");
			close(_socketid[i]);
			exit(1);
		  } else if (len < 0) {
			perror("Receive Failed.\n");
			close(_socketid[i]);
			exit(-1);
		  }
		  
		  //Replace with next_task_to_schedule and Add the next_task_to_schedule
		  int repaired_task_position=find_task_position(repair_task_id[0]);
		  
		  if(next_task_to_schedule<_total_repair_tasks){
		    task_schedule_queue[repaired_task_position]=next_task_to_schedule;
		    task_status_queue[repaired_task_position]=0;
		    next_task_to_schedule++;
		  }else{
		     task_status_queue[repaired_task_position]=-1;
		  }
		  
		  //Find available task to be scheduled
		  tmp_task_seq_pos=find_available_task_position(repaired_task_position);
		    
		  if(tmp_task_seq_pos!=-1){
		      
		      repair_task_params* tmp_repair_task_params = (struct repair_task_params *) calloc(
									    sizeof(struct repair_task_params), 1);						  
		      tmp_repair_task_params->failed_node_id = _failed_node_id;
		      strncpy(tmp_repair_task_params->new_device_name,_newdevice,strlen(_newdevice));
		      tmp_repair_task_params->repair_method = _rebuild_method;
		      tmp_repair_task_params->repair_tasks_num = _total_repair_tasks;
		      tmp_repair_task_params->repair_task_seq = task_schedule_queue[tmp_task_seq_pos];
			
		      //Send Repair Task Parameter
		      if ((len = send(_socketid[i], tmp_repair_task_params, sizeof(struct repair_task_params), 0)) >= 0) {
			
			  //the i-th task is now under processing
			  task_status_queue[tmp_task_seq_pos]=1;
			  
		      }else{		
			  perror("Error: Fail to Connect to Repair Task Executer.\n");
			  exit(-1);
		      }
		  
		  }//if
		  
		}//if
		  
	}//for
}

/*
 * Scheduler Utility:
 *     ./scheduler (new device name) (failed node id) (recovery method) (total repair tasks) (total executers)
 */
int main(int argc, char *args[]) {
	
	if(argc!=6){
	  printf("Usage: ./scheduler (new device name) (failed node id) (recovery method) (total repair tasks) (total executers)\n");
	  abort();
	}
	
	struct timeval starttime, endtime;
	double duration;
	double data_size;
	
	gettimeofday(&starttime,NULL);
		
	//get the name for the new device
	memset(_newdevice,0,SPATH_MAX);
	strncpy(_newdevice,args[1],strlen(args[1]));
	
	//get the failed node id and recovery method
	_failed_node_id=atoi(args[2]);
	_rebuild_method=atoi(args[3]);
		
	//get number of repair tasks and number of task executers
	_total_repair_tasks=atoi(args[4]);
	_total_task_executers=atoi(args[5]);
	
	InitializeRepairExecuters();
	
	//Check Whether Scheduler Can Connect To Each Executer Successfully
	if(TestConnectionToRepairTaskExecuter()){
    
	    struct timeval tv;
	    
	    tv.tv_sec=60;
	    tv.tv_usec=0;
	    
	    //Initialize socket id set
	    FD_ZERO(&repair_executer_fds);
	    
	    for(int i=0;i<_total_task_executers;i++){
		FD_SET(_socketid[i],&repair_executer_fds);
	    }
	    
	    //Initialize Task Queue and Task Status
	    task_schedule_queue = (int *)malloc(sizeof(int)*SCHEDULE_QUEUE_LEN);
	    
	    for(int i=0;i<SCHEDULE_QUEUE_LEN;i++){
	      task_schedule_queue[i]=i;
	    }

	    //next task that should be scheduled
	    next_task_to_schedule=SCHEDULE_QUEUE_LEN;
	    
	    task_status_queue = (int *)malloc(sizeof(int)*SCHEDULE_QUEUE_LEN);

	    for(int i=0;i<SCHEDULE_QUEUE_LEN;i++){
	      task_status_queue[i]=0;
	    }
	    
	    InitializeRepairTaskAllocation();
	    
	    //Not Finish Yet
	    while(!finish_repair()){
	      
		  //Re Initialize the socket id set
		  FD_ZERO(&repair_executer_fds);
		  
		  for(int i=0;i<_total_task_executers;i++){
		      FD_SET(_socketid[i],&repair_executer_fds);
		  }
		 
		tv.tv_sec=60;
		tv.tv_usec=0;
	    
		 int finish_tasks = select(_high_socketid+1, &repair_executer_fds, NULL, NULL, &tv);
		 
		 if(finish_tasks < 0) {
			  perror("Select Failure!");
			  exit(EXIT_FAILURE);
		 }else if(finish_tasks == 0) {
			  printf("Time Out");
		 }else{
			  AllocateRepairTask();
		 }	    
	      
	    } 
      
	}
	
	gettimeofday(&endtime,NULL);

	fprintf(stderr,"\n\n\nRecovery on disk %d, new device %s Done.\n\n\n",_failed_node_id,_newdevice);

        duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
	
	data_size  = 1024;
	int disk_block_size=524288;
        printf("Elapsed time = %fs\n",duration);
        printf("Throughput = %f MB/s\n",(float)(data_size / duration));
	printf("Data size = %f MB\n",(float)data_size);

		//Add By Zhuyunfeng on Aug 23,2011 begin. 
	//record the experiment results
	FILE *result_record_file;
	    
	//record experiment results
	if ((result_record_file = fopen("ExperimentResult","a")) != NULL){
		
		fprintf(result_record_file,"Elapsed Time = %fs\n", duration);
		fprintf(result_record_file,"Repair Throughput = %f MB/s\n", (float)(data_size / duration));
		fprintf(result_record_file,"Storage Node Size = %f MB\n", (float)data_size);
		fprintf(result_record_file,"Block Size = %d B\n", disk_block_size);
		
		for(int i=0;i<_total_task_executers;i++){ 
		      fprintf(result_record_file,"Task Executer %d finished %d taskes\n", i,_executer_tasks_num[i]);
		}
  
		fclose(result_record_file);
	}
	//Add By Zhuyunfeng on Aug 23,2011 end. 
	
	return 0;
}

