#ifndef __CODING_MBR_HH__
#define __CODING_MBR_HH__

#include "coding.hh"

class Coding4Mbr : public CodingLayer {

  private:

    	int field_power;
	int mbr_find_block_id(int disk_id, int block_no, int mbr_segment_size);
	int mbr_find_dup_block_id(int disk_id, int block_no, int mbr_segment_size);
	int mbr_get_disk_id(int mbr_block_id, int mbr_segment_size);
	int mbr_get_block_no(int disk_id, int mbr_block_id, int mbr_segment_size);
	int mbr_get_dup_disk_id(int mbr_block_id, int mbr_segment_size);
	int mbr_get_dup_block_no(int disk_id, int mbr_block_id, int mbr_segment_size);
	
  public:
    
        Coding4Mbr();
        virtual struct data_block_info encode(const char* buf, int size);
        virtual int decode(int disk_id, char* buf, long long size, long long offset);	
	virtual int recover(int failed_disk_id,char* newdevice);
	virtual int recover_enable_node_encode(int failed_disk_id, char* newdevice, int* node_socketid_array);
	//recover the failed node using nor_multithreads
	virtual int recover_using_multi_threads_nor(int failed_disk_id, char* newdevice, int total_sor_threads);
	
	//recover the failed node using sor_multithreads
	virtual int recover_using_multi_threads_sor(int failed_disk_id, char* newdevice
									, int total_sor_threads, int number_sor_thread);
	virtual int recover_using_access_aggregation(int failed_disk_id, char* newdevice);
	
	virtual int recover_conventional(int failed_disk_id,char* newdevice);
};

#endif