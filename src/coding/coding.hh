#ifndef __CODING_HH__
#define __CODING_HH__

#include "../filesystem/filesystem_common.hh"

#define AbnormalError() printf("%s %s %d\n", __FILE__, __func__,__LINE__);

class CodingLayer{

  public:
        //CodingStorageLayer();
        virtual struct data_block_info encode(const char* buf, int size)=0;
        virtual int decode(int disk_id, char* buf, long long size, long long offset)=0;  
	
	virtual int recover_conventional(int failed_disk_id,char* newdevice)=0;
	
        //interface for recovering failed disk
	//Assume that the device name of the failed disk has been modified
	//what you should do is just read the data in degraded mode and write
	//into the new device
	virtual int recover(int failed_disk_id,char* newdevice)=0;
	
	//recover the failed node with node encoding capability, node_socketid_array is for socket connection
	virtual int recover_enable_node_encode(int failed_disk_id, char* newdevice, int* node_socketid_array)=0;
	
	//recover the failed node using nor_multithreads
	virtual int recover_using_multi_threads_nor(int failed_disk_id, char* newdevice, int total_sor_threads)=0;
	
	//recover the failed node using sor_multithreads
	virtual int recover_using_multi_threads_sor(int failed_disk_id, char* newdevice
									, int total_sor_threads, int number_sor_thread)=0;
	
	//recover the failed node with access aggregation
	virtual int recover_using_access_aggregation(int failed_disk_id, char* newdevice)=0;
	
};

#endif
