# include <stdio.h>
# include <stdlib.h>
# include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char** argv){
  
	if(argc!=4){
	  printf("Usage: ./readperformance (device name) (total read size) (block size)\n");
	  abort();
	}
	
	int fd;
	fd = open(argv[1],O_RDWR);
	
	if(fd<0){
	  printf("File open failed!\n");
	  abort();	  
	}
	
	long long total_size =atoi(argv[2]);
	long int block_size=atoi(argv[3]);
	
	char* buf;
	buf = (char*)malloc(sizeof(char)*block_size);
	memset(buf, 0, block_size);
	
	struct timeval starttime, endtime;
	double duration;
	  
	gettimeofday(&starttime,NULL);
		    
	for(long int i=0;i<total_size/block_size;i++){
	  
	  pread(fd,buf,block_size,i*block_size);
	  
	}
		    
	close(fd);
	
	gettimeofday(&endtime,NULL);

	duration = endtime.tv_sec - starttime.tv_sec + (endtime.tv_usec-starttime.tv_usec)/1000000.0;
	
	printf("Block Size = %ld, Elapsed time = %fs\n",block_size,duration);
		    
	return 0;
}
