# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define MAX_EXPERIMENTS 50

int main(int argc, char** argv){
  

  
	if(argc<=1){
		printf("Please input the file name!\n");
		exit(0);
	}
	FILE * fp;
	char *filename=argv[1];
	char *info=(char*)malloc(100*sizeof(char));
	int num_of_recovery=0;
	float data_recovery[MAX_EXPERIMENTS];
	float sum_recovery=0, average_recovery=0;
	
	float max_recovery=0;
	float min_recovery=10000.0;

	
	if((fp=fopen(filename,"r"))==0){
	  
		printf("File Not Found\n");
		exit(0);
	}

	while(fscanf(fp,"%s",info)!=EOF){
	  
		if(strcmp(info,"Elapsed")==0){
		  
			if(num_of_recovery==100){
				printf("Please reset the number of Max Experiments\n");
				exit(0);
			}
			
			for(int i=0;i<2;i++){
				fscanf(fp,"%s",info);
			}
			
			fscanf(fp,"%fs",&data_recovery[num_of_recovery]);
			
			
			//Add by zhuyunfeng on 2011-09-05 begin.
			
			//A bigger recovery time
			if(data_recovery[num_of_recovery]>max_recovery){
			  
			  max_recovery=data_recovery[num_of_recovery];
			}
			
			//A smaller recovery time
			if(data_recovery[num_of_recovery]<min_recovery){
			  
			  min_recovery=data_recovery[num_of_recovery];
			}
			
			//Add by zhuyunfeng on 2011-09-05 end.
			
			num_of_recovery++;
		}
	}	

	fclose(fp);
	
	for(int i=0;i<num_of_recovery;i++){
		sum_recovery+=data_recovery[i];
	}
	average_recovery=sum_recovery/num_of_recovery;
	
	printf("\nThere are %d experiments, and the average recovery time is %fs\n", num_of_recovery, average_recovery);
	printf("The maximum recovery time is %fs\n", max_recovery);
	printf("The minimum recovery time is %fs\n", min_recovery);

	return 0;
	
        /*
	if(argc<=1){
		printf("Please input the file name!\n");
		exit(0);
	}
	FILE * fp;
	char *filename=argv[1];
	char *info=(char*)malloc(100*sizeof(char));
	int num_of_recovery=0;
	float data_recovery[MAX_EXPERIMENTS];
	float sum_recovery=0, average_recovery=0;
	
	float max_recovery=0;
	float min_recovery=10000.0;

	int num_of_read=0;
	float data_read[MAX_EXPERIMENTS];
	float sum_read=0, average_read=0;
	
	int num_of_encoding=0;
	float data_encoding[MAX_EXPERIMENTS];
	float sum_encoding=0, average_encoding=0;
	
	int num_of_write=0;
	float data_write[MAX_EXPERIMENTS];
	float sum_write=0, average_write=0;
	
	if((fp=fopen(filename,"r"))==0){
	  
		printf("File Not Found\n");
		exit(0);
	}

	while(fscanf(fp,"%s",info)!=EOF){
	  
		if(strcmp(info,"Elapsed")==0){
		  
			if(num_of_recovery==100){
				printf("Please reset the number of Max Experiments\n");
				exit(0);
			}
			
			for(int i=0;i<2;i++){
				fscanf(fp,"%s",info);
			}
			
			fscanf(fp,"%fs",&data_recovery[num_of_recovery]);
			
			
			//Add by zhuyunfeng on 2011-09-05 begin.
			
			//A bigger recovery time
			if(data_recovery[num_of_recovery]>max_recovery){
			  
			  max_recovery=data_recovery[num_of_recovery];
			}
			
			//A smaller recovery time
			if(data_recovery[num_of_recovery]<min_recovery){
			  
			  min_recovery=data_recovery[num_of_recovery];
			}
			
			//Add by zhuyunfeng on 2011-09-05 end.
			
			num_of_recovery++;
		}else if(strcmp(info,"Read")==0){
		  
			if(num_of_read==100){
				printf("Please reset the number of Max Experiments\n");
				exit(0);
			}
			
			for(int i=0;i<2;i++){
				fscanf(fp,"%s",info);
			}
			
			fscanf(fp,"%fs",&data_read[num_of_read]);
			num_of_read++;		  
		  
		  
		}else if(strcmp(info,"Encoding")==0){
		  
			if(num_of_encoding==100){
				printf("Please reset the number of Max Experiments\n");
				exit(0);
			}
			
			for(int i=0;i<2;i++){
				fscanf(fp,"%s",info);
			}
			
			fscanf(fp,"%fs",&data_encoding[num_of_encoding]);
			num_of_encoding++;		  
		  
		  
		}else if(strcmp(info,"Write")==0){
		  
			if(num_of_write==100){
				printf("Please reset the number of Max Experiments\n");
				exit(0);
			}
			
			for(int i=0;i<2;i++){
				fscanf(fp,"%s",info);
			}
			
			fscanf(fp,"%fs",&data_write[num_of_write]);
			num_of_write++;		  
		  
		  
		  
		}
	}	

	fclose(fp);
	
	for(int i=0;i<num_of_recovery;i++){
		sum_recovery+=data_recovery[i];
	}
	average_recovery=sum_recovery/num_of_recovery;

	for(int i=0;i<num_of_read;i++){
		sum_read+=data_read[i];
	}
	average_read=sum_read/num_of_read;
	
	for(int i=0;i<num_of_encoding;i++){
		sum_encoding+=data_encoding[i];
	}
	average_encoding=sum_encoding/num_of_encoding;
	
	for(int i=0;i<num_of_write;i++){
		sum_write+=data_write[i];
	}
	average_write=sum_write/num_of_write;
	
	printf("\nThere are %d experiments, and the average recovery time is %fs\n", num_of_recovery, average_recovery);
	printf("The maximum recovery time is %fs\n", max_recovery);
	printf("The minimum recovery time is %fs\n", min_recovery);
	printf("There are %d experiments, and the average read time is %fs\n", num_of_read, average_read);
	printf("There are %d experiments, and the average encoding time is %fs\n", num_of_encoding, average_encoding);
	printf("There are %d experiments, and the average write time is %fs\n", num_of_write, average_write);
	return 0;
	
	
	*/
}
