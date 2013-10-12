#ifndef __CODING_RAID1_HH__
#define __CODING_RAID1_HH__

#include "coding.hh"

class Coding4Raid1 : public CodingLayer {

  public:
        //CodingStorage4Raid1();
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