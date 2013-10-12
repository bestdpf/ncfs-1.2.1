#ifndef __CODING_RAID6_HH__
#define __CODING_RAID6_HH__

#include "coding.hh"


class Coding4Raid6 : public CodingLayer {
 
 private:
	//GF operations
	unsigned short *gflog, *gfilog;
	static const unsigned int prim_poly_8 = 285;   //0x11d
	//primitive polynomial = x^8 + x^4 + x^3 + x^2 + 1
	static const int field_power = 8;
	//field size = 2^8

	int gf_gen_tables(int s);
	unsigned short gf_mul(int a, int b, int s);
	unsigned short gf_div(int a, int b, int s);
	int gf_get_coefficient(int value, int s);
	
  public:
        Coding4Raid6();
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