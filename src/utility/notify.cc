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


int _total_task_executers;

//store socket id which are used for connecting
int _socketid[MAX_CONNECTION_NUM];

//store the ip information for each repair task executer
char **_executer_ip_addr;


/*
 * Repair Task Parameters for Executer
 */
/*
 * Trace Task Parameters for Executer
 */
struct trace_task_params{
  
    //id of the failed node
    int failed_node_id;
};


void InitializeRepairExecuters(){
  
  char** ipAddrTemp=(char**)calloc(MAX_CONNECTION_NUM, sizeof(char *));
  
  ipAddrTemp[0] = (char*)calloc(SPATH_MAX,1);
  strcpy(ipAddrTemp[0],"192.168.0.119");
  
  ipAddrTemp[1] = (char*)calloc(SPATH_MAX,1);
  strcpy(ipAddrTemp[1],"192.168.0.18");
  
  ipAddrTemp[2] = (char*)calloc(SPATH_MAX,1);
  strcpy(ipAddrTemp[2],"192.168.0.19");
  
  _executer_ip_addr= (char**)calloc(_total_task_executers, sizeof(char *));
 
  
  for(int i=0;i<_total_task_executers;i++){
   	_executer_ip_addr[i] = (char*)calloc(SPATH_MAX,1);	
	strncpy(_executer_ip_addr[i],ipAddrTemp[i],SPATH_MAX); 
	
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
	
	
	//Find Avaliable Task Executers
	for(int i=0;i<_total_task_executers;i++){

	    if ((_socketid[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		    retstat=0;
		    break;
	    }else{
	    
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
 * notify Utility:
 *     ./notify (failed node id) (total trace executer)
 */
int main(int argc, char *args[]) {
	
	int len;
	
	if(argc!=3){
	  printf("Usage: ./notify (failed node id) (total trace executer)\n");
	  abort();
	}
	
	//get the failed node id
	int failed_node_id=atoi(args[1]);
		
	//get number of task executers
	_total_task_executers=atoi(args[2]);
	
	InitializeRepairExecuters();
		
	//Check Whether Scheduler Can Connect To Each Executer Successfully
	if(TestConnectionToRepairTaskExecuter()){
	  
		    struct trace_task_params *traceTaskParams;
	
		    //allocate space for encoding_params
		    traceTaskParams = (struct trace_task_params *) calloc(sizeof(struct trace_task_params), 1);
		    
		    traceTaskParams->failed_node_id=failed_node_id;
	
	            //Send Repair Task Parameter
		    if ((len = send(_socketid[0], traceTaskParams, sizeof(struct trace_task_params), 0)) >= 0) {
			 printf("Notify the tracer id of the failed node.\n");
			
		    }else{		
			perror("Error: Fail to Connect to Repair Task Executer.\n");
			exit(-1);
		    }
		    
	}
	
	
	
	return 0;
}

