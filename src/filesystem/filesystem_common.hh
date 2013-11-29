#ifndef __FILESYSTEM_COMMON_H_
#define __FILESYSTEM_COMMON_H_

// The FUSE API has been changed a number of times.  So, our code
// needs to define the version of the API that we assume.  As of this
// writing, the most current API version is 26
#define FUSE_USE_VERSION 26

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
//#define _XOPEN_SOURCE 500

//#define NCFS_DATA ((struct ncfs_state *) fuse_get_context()->private_data)

#define MAGIC_NUMBER 123
// maintain ncfs state in here
//#include <limits.h>
//#include <stdio.h>

#define MAX_VECTOR_LEN 50
typedef unsigned long long ticks;

struct ncfs_state {
    char *rootdir;
    char *mountdir;

    int disk_total_num;
    int data_disk_num;
    int disk_block_size;
    int disk_raid_type;
    int space_list_num;
    int operation_mode; //0 for normal; 1 for degraded; 2 for incapable
    int *disk_size;     //disk maximum size in number of blocks
    int *free_offset;   //in block number
    int *free_size;     //in number of blocks
    int *disk_status;
    char **dev_name;

    //Add by zhuyunfeng on 2011-07-04 begin.
    //Store the IP information for each storage node
    char **ip_addr;
    
    //Add by zhuyunfeng on 2011-07-04 end.
    
    //Add by zhuyunfeng on 2011-04-06 begin.
    
    // (m*w) * (k*w). m is fail tolerance and k is data disk number
    int *generator_matrix;
    
    int w_cacuhy_rs;
    
    //Add by zhuyunfeng on 2011-04-06 end.
    
    struct space_list *space_list_head;
    int no_cache; //1 for no cache
    int no_gui; //1 for no gui
    int run_experiment;  //1 for running experiment

    int process_state;  //0 for normal, 1 for recovery
    unsigned long long encoding_ticks;
    unsigned long long decoding_ticks;
    unsigned long long diskread_ticks;
    unsigned long long diskwrite_ticks;
    double encoding_time;   //in usec
    double decoding_time;   //in usec
    double diskread_time;   //in usec
    double diskwrite_time;   //in usec

	int segment_size;

    int mbr_n;
    int mbr_k;
    int mbr_m;
    int mbr_c;
    int mbr_segment_size;

    int msr_n;
    int msr_k;
    int msr_m;
    int msr_c;
    int msr_segment_size;
    //now we use different variables for mbr and msr. we may merge them later.

    int usebigblock;
};

struct space_info {
        int disk_id;
        int disk_block_no;      // in blocks
};

struct space_list {
        int disk_id;
        int disk_block_no;      // in blocks
        struct space_list *next;
};

struct data_block_info{
        int disk_id;
        int block_no;
};

struct raid_metadata{
        int disk_id;
        int free_offset;   //in bytes
        int free_size;   //in bytes
};

/*
 * encoding parameters for storage node to encode blocks on it
 */
struct encoding_params{
  
  int segment_number; //segment number for encoding
  int page_cache_size;
  int block_size; //block size
  int blocks_segment_perdisk; //number of blocks in a segment  per node
  int field_len; // field len
  int encoding_vector_len; //len of encoding vector
  int encoding_vector[MAX_VECTOR_LEN]; //encoding vector
};

struct recovery_info{
        int failed_disk_id;
        int surviving_disk_id;
};

struct recovery_another_info{
        int failed_disk_id;
        int part_number;
	int segment_num_per_part;
	int total_recover_size;
};

struct circular_buffer {
	int stripe_num_buffer;//number of stripe in circular buffer
	int stripe_recovered; // recovered stripe to be written into the new coming node
	int* buffer_fill_status;//fill value for each segment in circular buffer
	int* write_position;//write position for each recovery thread
	char* circular_buffer_area;//circular buffer area
};

#endif
