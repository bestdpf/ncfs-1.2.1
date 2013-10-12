#include "coding_raid6.hh"
#include "../filesystem/filesystem_common.hh"
#include "../filesystem/filesystem_utils.hh"
#include "../cache/cache.hh"
#include "../gui/diskusage_report.hh"

#include <stdio.h>
#include <stdlib.h>
#include <fuse.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

//NCFS context state
extern struct ncfs_state* NCFS_DATA;
extern FileSystemLayer* fileSystemLayer;
extern CacheLayer* cacheLayer;
extern DiskusageReport* diskusageLayer;

/*
 * constructor for raid6(rs,origin version).
 */
Coding4Raid6::Coding4Raid6(){
  
   //generate GF tables for raid6
   gf_gen_tables(field_power);
   
}

/*
 * gf_gen_tables: Create logarithm and inverse logairthm tables for computing Q parity
 *
 * @param s: field size (2 power s)
 *
 * @return: 0
 */

int Coding4Raid6::gf_gen_tables(int s)
{
        unsigned int b, index, gf_elements;

        //convert s to the number of elements for the table
        gf_elements = 1 << s;
        this->gflog = (unsigned short*) malloc(sizeof(unsigned short)*gf_elements);
        this->gfilog = (unsigned short*) malloc(sizeof(unsigned short)*gf_elements);
        b = 1;

        for (index=0; index < gf_elements-1; index++){
                this->gflog[b] = (unsigned char)index;
                this->gfilog[index] = (unsigned char)b;
                b <<= 1;
                if (b & gf_elements){
                        b ^= prim_poly_8;
                }
        }

        return 0;
}


/*
 * gf_mul: Multiple a and b 
 *
 * @param a, b: input values for multiplication
 * @param s: field size (2 power s)
 *
 * @return: Product in finite field
 */

unsigned short Coding4Raid6::gf_mul(int a, int b, int s)
{
        unsigned int field_max;
        int sum;
        unsigned short result;

        if ((a == 0) || (b == 0)){
                result = 0;
        }
        else{
                field_max = (1 << s) - 1;
                sum = gflog[a] + gflog[b];
                sum = sum % field_max;

                result = gfilog[sum];
        }

        return(result);
}


/*
 * gf_div: Divide a by b
 *
 * @param a, b: input values for division
 * @param s: field size (2 power s)
 *
 * @return: Divident in finite field
 */

unsigned short Coding4Raid6::gf_div(int a, int b, int s)
{
        unsigned int field_max;
        int diff;
        unsigned short result;

	if (a == 0){
		result = 0;
	}
	else{
        	field_max = (1 << s) - 1;

        	diff = gflog[a] - gflog[b];

        	if (diff < 0){
                	diff = diff + field_max;
        	}

        	result = gfilog[diff];
	}

        return(result);
}


/*
 * gf_get_coefficient: Get the coefficient of the primitive polynomial
 * polynomial = x^8 + x^4 + x^3 + x^2 + 1
 *
 * @param value: the coefficient offset
 * @param s: field size (2 power s)
 *
 * @return: the coefficient
 */

int Coding4Raid6::gf_get_coefficient(int value, int s)
{
        int result;

	result = 1 << value;

        return result;
}

/*
 * raid5_encoding: RAID 6: fault tolerance by stripped parity (type=6)
 * 
 * @param buf: buffer of data to be written to disk
 * @param size: size of the data buffer
 *
 * @return: assigned data block: disk id and block number.
 */
struct data_block_info Coding4Raid6::encode(const char* buf, int size)
{

        int retstat, disk_id, block_no, disk_total_num, block_size;
        int size_request, block_request, free_offset;
        struct data_block_info block_written;
        int i, j;
        int parity_disk_id, code_disk_id;
        char *buf2, *buf3, *buf_read;
        char temp_char;
	int data_disk_coeff;

        struct timeval t1, t2, t3;
        double duration;

        //test print
        //printf("***get_data_block_no 1: size=%d\n",size);

        disk_total_num = NCFS_DATA->disk_total_num;
        block_size = NCFS_DATA->disk_block_size;

        size_request = fileSystemLayer->round_to_block_size(size);
        block_request = size_request / block_size;

        //test print
        //printf("***get_data_block_no 2: size_request=%d, block_request=%d\n", size_request, block_request);

        //implement disk write algorithm here.
        //here use raid6: stripped block allocation plus distributed parity and distributed RS code.

	printf("You get in, the size is %d!\n",size);
	
        for (i=0; i < disk_total_num; i++){
                //mark blocks of code_disk and parity_disk
                if ((i == (disk_total_num-1-(NCFS_DATA->free_offset[i] % disk_total_num)))
                || (i == (disk_total_num-1-((NCFS_DATA->free_offset[i]+1) % disk_total_num))))
                {
                        (NCFS_DATA->free_offset[i])++;
                        (NCFS_DATA->free_size[i])--;
                }
        }


        block_no = 0;
        disk_id = -1;
        free_offset = NCFS_DATA->free_offset[0];
        for (i=disk_total_num-1; i >= 0; i--){
//        for (i=disk_total_num-3; i>=0; i--){
                if ((block_request <= (NCFS_DATA->free_size[i]))
                                && (free_offset >= (NCFS_DATA->free_offset[i])))
                {
                        disk_id = i;
                        block_no = NCFS_DATA->free_offset[i];
                        free_offset = block_no;
                }
        }


        //get block from space_list if no free block available
        if (disk_id == -1){
                if (NCFS_DATA->space_list_head != NULL){
                        disk_id = NCFS_DATA->space_list_head->disk_id;
                        block_no = NCFS_DATA->space_list_head->disk_block_no;
                        fileSystemLayer->space_list_remove(disk_id,block_no);
                }
        }


        if (disk_id == -1){
                printf("***get_data_block_no: ERROR disk_id = -1\n");
        }
        else{
                buf_read = (char*)malloc(sizeof(char)*size_request);
                buf2 = (char*)malloc(sizeof(char)*size_request);
                memset(buf2, 0, size_request);
                buf3 = (char*)malloc(sizeof(char)*size_request);
                memset(buf3, 0, size_request);

                NCFS_DATA->free_offset[disk_id] = block_no + block_request;
                NCFS_DATA->free_size[disk_id]
                        = NCFS_DATA->free_size[disk_id] - block_request;

                if (NCFS_DATA->run_experiment == 1){
                        gettimeofday(&t1,NULL);
                }

                // Cache Start
                retstat = cacheLayer->writeDisk(disk_id,buf,size,block_no*block_size);
		// Cache End

                if (NCFS_DATA->run_experiment == 1){
                        gettimeofday(&t2,NULL);

                        //duration = (t2.tv_sec - t1.tv_sec) + 
                        //        (t2.tv_usec-t1.tv_usec)/1000000.0;
                        //NCFS_DATA->diskwrite_time += duration;
                }

                code_disk_id = disk_total_num -1 - (block_no % disk_total_num);
                parity_disk_id = disk_total_num - 1 - ((block_no+1) % disk_total_num);
                //code_disk_id = disk_total_num - 1;
                //parity_disk_id = disk_total_num - 2;

                for (i=0; i < disk_total_num; i++){
                        if ((i != parity_disk_id) && (i != code_disk_id)){

				if (NCFS_DATA->run_experiment == 1){
                        		gettimeofday(&t1,NULL);
                		}

                                //Cache Start
                                retstat = cacheLayer->readDisk(i,buf_read,
                                                        size_request,block_no*block_size);
                                //Cache End

				//printf("check point 2.2\n");
				if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t2,NULL);
                                }

                                for (j=0; j < size_request; j++){
                                        //Calculate parity block P
                                        buf2[j] = buf2[j] ^ buf_read[j];

                                	//calculate the coefficient of the data block
                                	data_disk_coeff = i;

                                	if (i > code_disk_id){
                                        	(data_disk_coeff)--;
                                	}
                                	if (i > parity_disk_id){
                                        	(data_disk_coeff)--;
                                	}
                                	data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
					
					//printf("check point 2.2.1\n");
                                	data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);
					//printf("check point 2.2.2\n");
					
                                	//calculate code block Q
                                	temp_char = buf3[j];
					
					//printf("check point 2.2.3\n");
					//printf("%d,%d,%d\n",(unsigned char)buf_read[j],data_disk_coeff,field_power);
                                	buf3[j] = temp_char ^
                                        (char)gf_mul((unsigned char)buf_read[j],data_disk_coeff,field_power);
					
					//printf("check point 2.2.4\n");
                                }

                                if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t3,NULL);

                        		//duration = (t2.tv_sec - t1.tv_sec) + 
                                	//	(t2.tv_usec-t1.tv_usec)/1000000.0;
                        		//NCFS_DATA->diskread_time += duration;

                        		duration = (t3.tv_sec - t2.tv_sec) + 
                                		(t3.tv_usec-t2.tv_usec)/1000000.0;
                        		NCFS_DATA->encoding_time += duration;
                                }
                        }
                }


                if (NCFS_DATA->run_experiment == 1){
                        gettimeofday(&t1,NULL);
                }

                // Cache Start
                retstat = cacheLayer->writeDisk(parity_disk_id,buf2,size,block_no*block_size);
                retstat = cacheLayer->writeDisk(code_disk_id,buf3,size,block_no*block_size);
                // Cache End
                //printf("***raid6: retstat=%d\n",retstat);

                if (NCFS_DATA->run_experiment == 1){
                        gettimeofday(&t2,NULL);

                        //duration = (t2.tv_sec - t1.tv_sec) + 
                        //        (t2.tv_usec-t1.tv_usec)/1000000.0;
                        //NCFS_DATA->diskwrite_time += duration;
                }

                free(buf_read);
                free(buf2);
                free(buf3);
        }

        block_written.disk_id = disk_id;
        block_written.block_no = block_no;

        return block_written;  
}



/*
 * decoding_raid6: RAID 6 decode
 *
 * @param disk_id: disk id               
 * @param buf: data buffer for reading data
 * @param size: size of the data buffer
 * @param offset: offset of data on disk
 *
 * @return: size of successful decoding (in bytes)
 */
int Coding4Raid6::decode(int disk_id, char* buf, long long size, long long offset)
{


        int retstat;
        char* temp_buf;
        int i;
        long long j;
        int disk_failed_no;
        //int disk_second_id;     //id of second failed disk
        int disk_another_failed_id; //id of second failed disk

        int code_disk_id, parity_disk_id;
        int block_size;
        long long block_no;
        int disk_total_num;
        int data_disk_coeff;
        char temp_char;

        int g1, g2, g12;
        char* P_temp;
	char* Q_temp;

        struct timeval t1, t2, t3;
        double duration;

        temp_buf = (char*)malloc(sizeof(char)*size);        
        memset(temp_buf, 0, size);

        P_temp = (char*)malloc(sizeof(char)*size);        
        memset(P_temp, 0, size);        

        Q_temp = (char*)malloc(sizeof(char)*size);        
        memset(Q_temp, 0, size);        
        
        memset(buf, 0, size);        
        int* inttemp_buf = (int*)temp_buf;                      
        int* intbuf = (int*)buf;      

        disk_total_num = NCFS_DATA->disk_total_num;                 
        block_size = NCFS_DATA->disk_block_size;
        block_no = offset / block_size;

        code_disk_id = disk_total_num -1 - (block_no % disk_total_num);
        parity_disk_id = disk_total_num - 1 - ((block_no+1) % disk_total_num);
        //code_disk_id = disk_total_num - 1;
        //parity_disk_id = disk_total_num - 2;


        if(NCFS_DATA->disk_status[disk_id] == 0){ 
                if (NCFS_DATA->run_experiment == 1){
                	gettimeofday(&t1,NULL);
                }

                retstat = cacheLayer->readDisk(disk_id,buf,size,offset);

                if (NCFS_DATA->run_experiment == 1){
                        gettimeofday(&t2,NULL);

                        //duration = (t2.tv_sec - t1.tv_sec) + 
                        //        (t2.tv_usec-t1.tv_usec)/1000000.0;
                        //NCFS_DATA->diskread_time += duration;
                }

		free(temp_buf);
		free(P_temp);
		free(Q_temp);

		return retstat;
	}
        else {
                //check the number of failed disks
                disk_failed_no = 0;
                for (i=0; i < disk_total_num; i++){
                        if (NCFS_DATA->disk_status[i] == 1){
                                (disk_failed_no)++;
                                if (i != disk_id){
                                        //disk_second_id = i;
					disk_another_failed_id = i;
                                }
                        }
                }


                if ( ((disk_failed_no == 1) && (disk_id != code_disk_id)) ||
                        ((disk_failed_no == 2) && (disk_another_failed_id == code_disk_id)) ){
                //case of single non-Q disk fail (D or P), or two-disk (data disk + Q disk) fail (D + Q)
                        for(i = 0; i < disk_total_num; ++i){
                                if ((i != disk_id) && (i != code_disk_id)){
                                        if(NCFS_DATA->disk_status[i] != 0){
                                                printf("Raid 6 both disk %d and %d are failed\n",disk_id,i);
                                                return -1;
                                        }

					if (NCFS_DATA->run_experiment == 1){
                                        	gettimeofday(&t1,NULL);
                                	}

                                        retstat = cacheLayer->readDisk(i,temp_buf,size,offset);

                                        if (NCFS_DATA->run_experiment == 1){
                                                gettimeofday(&t2,NULL);
                                        }

                                        for(j = 0; j < (long long)(size * sizeof(char) / sizeof(int)); ++j){
                                                intbuf[j] = intbuf[j] ^ inttemp_buf[j];
                                        }

                                	if (NCFS_DATA->run_experiment == 1){
                                        	gettimeofday(&t3,NULL);

                                        	//duration = (t2.tv_sec - t1.tv_sec) + 
                                                //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                        	//NCFS_DATA->diskread_time += duration;

                                        	duration = (t3.tv_sec - t2.tv_sec) + 
                                                	(t3.tv_usec-t2.tv_usec)/1000000.0;
                                        	NCFS_DATA->decoding_time += duration;
                                	}
                                }
                        }
                }
		else if ( ((disk_failed_no == 1) && (disk_id == code_disk_id))
				|| ((disk_failed_no == 2) && (disk_id == code_disk_id) 
					&& (disk_another_failed_id == parity_disk_id)) ){
		//case of "single Q disk fails" (Q) or "Q + P disk failure" (Q + P)

			//Calculate Q
                	for (i=0; i < disk_total_num; i++){
                        	if ((i != parity_disk_id) && (i != code_disk_id)){

                                        if (NCFS_DATA->run_experiment == 1){
                                                gettimeofday(&t1,NULL);
                                        }

                                	//Cache Start
                                	retstat = cacheLayer->readDisk(i,temp_buf,size,offset);
                                	//Cache End

                                        if (NCFS_DATA->run_experiment == 1){
                                                gettimeofday(&t2,NULL);
                                        }

                                	for (j=0; j < size; j++){
                                        	//calculate the coefficient of the data block
                                        	data_disk_coeff = i;

                                        	if (i > code_disk_id){
                                                	(data_disk_coeff)--;
                                        	}
                                        	if (i > parity_disk_id){
                                                	(data_disk_coeff)--;
                                        	}
                                        	data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
                                       		data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);

                                        	//calculate code block Q
                                        	buf[j] = buf[j] ^
                                        	(char)gf_mul((unsigned char)temp_buf[j],data_disk_coeff,field_power);
                                	}

                                        if (NCFS_DATA->run_experiment == 1){
                                                gettimeofday(&t3,NULL);

                                                //duration = (t2.tv_sec - t1.tv_sec) + 
                                                //        (t2.tv_usec-t1.tv_usec)/1000000.0;
                                                //NCFS_DATA->diskread_time += duration;

                                                duration = (t3.tv_sec - t2.tv_sec) + 
                                                        (t3.tv_usec-t2.tv_usec)/1000000.0;
                                                NCFS_DATA->decoding_time += duration;
                                        }
                        	}
                	}
		}
                else if (disk_failed_no == 2){
                //case of two-disk fail (data disk + non Q disk) (disk_id and disk_second_id)
                //compute Q' to restore data

			if ((disk_id == parity_disk_id) && (disk_another_failed_id != code_disk_id)){
			//case of P disk + data disk fail (P + D)

				//calculate Q'
                                for (i=0; i < disk_total_num; i++){
                                        if ((i != disk_id) && (i != disk_another_failed_id) &&
                                        (i != parity_disk_id) && ( i != code_disk_id) ){

                                        	if (NCFS_DATA->run_experiment == 1){
                                                	gettimeofday(&t1,NULL);
                                        	}

                                                retstat = cacheLayer->readDisk(i,temp_buf,size,offset);

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t2,NULL);
                                                }

                                                //calculate the coefficient of the data block
                                                data_disk_coeff = i;

                                                if (i > code_disk_id){
                                                        (data_disk_coeff)--;
                                                }
                                                if (i > parity_disk_id){
                                                        (data_disk_coeff)--;
                                                }
                                                data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
                                                data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);

                                                for (j=0; j < size; j++){
                                                        buf[j] = buf[j] ^
                                                        (char)gf_mul((unsigned char)temp_buf[j],data_disk_coeff,field_power);
                                                }

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t3,NULL);

                                                	//duration = (t2.tv_sec - t1.tv_sec) + 
                                                        //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                                	//NCFS_DATA->diskread_time += duration;

                                                	duration = (t3.tv_sec - t2.tv_sec) + 
                                                        	(t3.tv_usec-t2.tv_usec)/1000000.0;
                                                	NCFS_DATA->decoding_time += duration;
                                                }
                                        }
                                }

                                //calculate Q xor Q'

                                if (NCFS_DATA->run_experiment == 1){
                                	gettimeofday(&t1,NULL);
                                }

                                retstat = cacheLayer->readDisk(code_disk_id,temp_buf,size,offset);

                                if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t2,NULL);
                                }

                                for (j=0; j < size; j++){
                                        buf[j] = buf[j] ^ temp_buf[j];
                                }

                                //calculate the coefficient of the data block
                                data_disk_coeff = disk_id;

                                if (disk_id > code_disk_id){
                                        (data_disk_coeff)--;
                                }
                                if (disk_id > parity_disk_id){
                                        (data_disk_coeff)--;
                                }
                                data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
                                data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);


                                //decode the origianl data block
                                for (j=0; j < size; j++){
                                        temp_char = buf[j];
                                        buf[j] = (char)gf_div((unsigned char)temp_char,data_disk_coeff,field_power);
                                }

				if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t3,NULL);

                                        //duration = (t2.tv_sec - t1.tv_sec) + 
                                        //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                        //NCFS_DATA->diskread_time += duration;

                                        duration = (t3.tv_sec - t2.tv_sec) + 
                                        	(t3.tv_usec-t2.tv_usec)/1000000.0;
                                        NCFS_DATA->decoding_time += duration;
                                }

				//find P
                        	for(i = 0; i < disk_total_num; ++i){
                                	if ((i != disk_id) && (i != code_disk_id) && (i != disk_another_failed_id)){
                                        	if(NCFS_DATA->disk_status[i] != 0){
                                                	printf("Raid 6 both disk %d and %d are failed\n",disk_id,i);
                                                	return -1;
                                        	}

                                		if (NCFS_DATA->run_experiment == 1){
                                        		gettimeofday(&t1,NULL);
                                		}

                                        	retstat = cacheLayer->readDisk(i,temp_buf,size,offset);

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t2,NULL);
                                                }

                                        	for(j = 0; j < (long long)(size * sizeof(char) / sizeof(int)); ++j){
                                                	intbuf[j] = intbuf[j] ^ inttemp_buf[j];
                                        	}

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t3,NULL);

                                                        //duration = (t2.tv_sec - t1.tv_sec) + 
                                                        //        (t2.tv_usec-t1.tv_usec)/1000000.0;
                                                        //NCFS_DATA->diskread_time += duration;

                                                        duration = (t3.tv_sec - t2.tv_sec) + 
                                                                (t3.tv_usec-t2.tv_usec)/1000000.0;
                                                        NCFS_DATA->decoding_time += duration;
                                                }
                                	}
                        	}
			}
                        else if ((disk_id == code_disk_id) && (disk_another_failed_id != parity_disk_id)){
                        //case of Q disk + data disk fail (Q + D)

                                //find D from P
                                for(i = 0; i < disk_total_num; ++i){
                                        if ((i != disk_id) && (i != disk_another_failed_id)){
                                                if(NCFS_DATA->disk_status[i] != 0){
                                                        printf("Raid 6 both disk %d and %d are failed\n",disk_id,i);
                                                        return -1;
                                                }

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t1,NULL);
                                                }

                                                retstat = cacheLayer->readDisk(i,temp_buf,size,offset);

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t2,NULL);
                                                }

                                                for(j = 0; j < (long long)(size * sizeof(char) / sizeof(int)); ++j){
                                                        intbuf[j] = intbuf[j] ^ inttemp_buf[j];
                                                }

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t3,NULL);

                                                        //duration = (t2.tv_sec - t1.tv_sec) +
                                                        //        (t2.tv_usec-t1.tv_usec)/1000000.0;
                                                        //NCFS_DATA->diskread_time += duration;

                                                        duration = (t3.tv_sec - t2.tv_sec) +
                                                                (t3.tv_usec-t2.tv_usec)/1000000.0;
                                                        NCFS_DATA->decoding_time += duration;
                                                }
                                        }
                                }

                        	//calculate Q
                        	for (i=0; i < disk_total_num; i++){
                                	if ((i != parity_disk_id) && (i != code_disk_id)){
						if (i != disk_another_failed_id){
                                                	if (NCFS_DATA->run_experiment == 1){
                                                        	gettimeofday(&t1,NULL);
                                                	}

                                        		//Cache Start
                                        		retstat = cacheLayer->readDisk(i,temp_buf,size,offset);
                                        		//Cache End

                                                	if (NCFS_DATA->run_experiment == 1){
                                                        	gettimeofday(&t2,NULL);

                                                        	//duration = (t2.tv_sec - t1.tv_sec) + 
                                                                //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                                        	//NCFS_DATA->diskread_time += duration;
                                                	}
						}
						else{ //use the recovered D
                                                	for(j = 0; j < size; j++){
                                                        	temp_buf[j] = buf[j];
                                                	}
						}

                                                if (NCFS_DATA->run_experiment == 1){
                                                	gettimeofday(&t1,NULL);
                                                }

                                        	for (j=0; j < size; j++){
                                                	//calculate the coefficient of the data block
                                                	data_disk_coeff = i;

                                                	if (i > code_disk_id){
                                                        	(data_disk_coeff)--;
                                                	}
                                                	if (i > parity_disk_id){
                                                        	(data_disk_coeff)--;
                                                	}
                                                	data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
                                                	data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);

                                                	//calculate code block Q
                                                	Q_temp[j] = Q_temp[j] ^
                                                	(char)gf_mul((unsigned char)temp_buf[j],data_disk_coeff,field_power);
                                        	}

                                                if (NCFS_DATA->run_experiment == 1){
                                                	gettimeofday(&t2,NULL);

                                                        //duration = (t2.tv_sec - t1.tv_sec) +
                                                        //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                                        //NCFS_DATA->diskread_time += duration;
                                                }
                                	}
                        	}

				for (j=0; j<size; j++){
					buf[j] = Q_temp[j];
				}
                        }
                        else if (disk_another_failed_id == parity_disk_id){
                        //case of data disk + P disk fail (D + P)

                                //calculate Q'
                                for (i=0; i < disk_total_num; i++){
                                        if ((i != disk_id) && (i != disk_another_failed_id) &&
                                        (i != parity_disk_id) && ( i != code_disk_id) ){

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t1,NULL);
                                                }

                                                retstat = cacheLayer->readDisk(i,temp_buf,size,offset);

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t2,NULL);
                                                }

                                                //calculate the coefficient of the data block
                                                data_disk_coeff = i;

                                                if (i > code_disk_id){
                                                        (data_disk_coeff)--;
                                                }
                                                if (i > parity_disk_id){
                                                        (data_disk_coeff)--;
                                                }
                                                data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
                                                data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);

                                                for (j=0; j < size; j++){
                                                        temp_char = buf[j];
							buf[j] = buf[j] ^
                                                       	(char)gf_mul((unsigned char)temp_buf[j],data_disk_coeff,field_power);
                                                }

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t3,NULL);

                                                        //duration = (t2.tv_sec - t1.tv_sec) +
                                                        //        (t2.tv_usec-t1.tv_usec)/1000000.0;
                                                        //NCFS_DATA->diskread_time += duration;

                                                        duration = (t3.tv_sec - t2.tv_sec) +
                                                                (t3.tv_usec-t2.tv_usec)/1000000.0;
                                                        NCFS_DATA->decoding_time += duration;
                                                }
                                        }
                                }

                                //calculate Q xor Q'

                                if (NCFS_DATA->run_experiment == 1){
                                	gettimeofday(&t1,NULL);
                                }

                                retstat = cacheLayer->readDisk(code_disk_id,temp_buf,size,offset);

                                if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t2,NULL);
                                }

                                for (j=0; j < size; j++){
                                        buf[j] = buf[j] ^ temp_buf[j];
                                }

                                //calculate the coefficient of the data block
                                data_disk_coeff = disk_id;

                                if (disk_id > code_disk_id){
                                        (data_disk_coeff)--;
                                }
                                if (disk_id > parity_disk_id){
                                        (data_disk_coeff)--;
                                }
                                data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
                                data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);


                                //decode the origianl data block
                                for (j=0; j < size; j++){
                                        temp_char = buf[j];
                                        buf[j] = (char)gf_div((unsigned char)temp_char,data_disk_coeff,field_power);
                                }

                                if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t3,NULL);

                                        //duration = (t2.tv_sec - t1.tv_sec) +
                                        //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                        //NCFS_DATA->diskread_time += duration;

                                        duration = (t3.tv_sec - t2.tv_sec) +
                                        	(t3.tv_usec-t2.tv_usec)/1000000.0;
                                	NCFS_DATA->decoding_time += duration;
                                }

                                //return retstat;
                        }
                        else{
                        // two data disk fail (D + D)

                                if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t1,NULL);
                                }

                                //calculate g1
                                g1 = disk_id;
                                if (g1 > code_disk_id){
                                        (g1)--;
                                }
                                if (g1 > parity_disk_id){
                                        (g1)--;
                                }
                                g1 = disk_total_num - 3 - g1;
                                g1 = gf_get_coefficient(g1, field_power);
                                //printf("g1=%d \n",g1);

                                //calculate g2
                                g2 = disk_another_failed_id;
                                if (g2 > code_disk_id){
                                        (g2)--;
                                }
                                if (g2 > parity_disk_id){
                                        (g2)--;
                                }
                                g2 = disk_total_num - 3 - g2;
                                g2 = gf_get_coefficient(g2, field_power);
                                //printf("g2=%d \n",g2);

                                //calculate g12
                                g12 = g1 ^ g2;

                                //calculate P'
                                //for (j=0; j<BLOCKSIZE; j++){
                                //        P_temp[j] = 0;
                                //}

                                if (NCFS_DATA->run_experiment == 1){
                                        gettimeofday(&t2,NULL);

                                        duration = (t2.tv_sec - t1.tv_sec) +
                                                (t2.tv_usec-t1.tv_usec)/1000000.0;
                                        NCFS_DATA->decoding_time += duration;
                                }

                                for (i=0; i<disk_total_num; i++){
                                        if ( (i != disk_id) && (i != disk_another_failed_id) &&
                                                (i != parity_disk_id) && (i != code_disk_id) )
                                        {
                               			if (NCFS_DATA->run_experiment == 1){
                                        		gettimeofday(&t1,NULL);
                                		}

						retstat = cacheLayer->readDisk(i,temp_buf,size,offset);

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t2,NULL);
                                                }

                                                for (j=0; j<size; j++){
                                                        P_temp[j] = P_temp[j] ^ temp_buf[j];
                                                }

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t3,NULL);

                                        		//duration = (t2.tv_sec - t1.tv_sec) +
                                                	//	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                        		//NCFS_DATA->diskread_time += duration;

                                        		duration = (t3.tv_sec - t2.tv_sec) +
                                                		(t3.tv_usec-t2.tv_usec)/1000000.0;
                                        		NCFS_DATA->decoding_time += duration;
                                                }
                                        }
                                }

                                if (NCFS_DATA->run_experiment == 1){
                                	gettimeofday(&t1,NULL);
                                }

				retstat = cacheLayer->readDisk(parity_disk_id,temp_buf,size,offset);

                                if (NCFS_DATA->run_experiment == 1){
                                	gettimeofday(&t2,NULL);
                                }

                                for (j=0; j<size; j++){
                                        P_temp[j] = P_temp[j] ^ temp_buf[j];
                                        //P_temp = P' xor P
                                }

                                if (NCFS_DATA->run_experiment == 1){
                                	gettimeofday(&t3,NULL);

                                        //duration = (t2.tv_sec - t1.tv_sec) +
                                        //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                        //NCFS_DATA->diskread_time += duration;

                                        duration = (t3.tv_sec - t2.tv_sec) +
                                        	(t3.tv_usec-t2.tv_usec)/1000000.0;
                                        NCFS_DATA->decoding_time += duration;
                                }

                                //calculate Q'
                                //for (j=0; j<BLOCKSIZE; j++){
                                //        Q_temp[j] = 0;

                                for (i=0; i<disk_total_num; i++){
                                        if ( ((i != disk_id) && (i != disk_another_failed_id))
                                        && (i != parity_disk_id) && (i != code_disk_id) ){

                                                //calculate the coefficient of the data block
                                                data_disk_coeff = i;

                                                if (i > code_disk_id){
                                                        (data_disk_coeff)--;
                                                }
                                                if (i > parity_disk_id){
                                                        (data_disk_coeff)--;
                                                }
                                                data_disk_coeff = disk_total_num - 3 - data_disk_coeff;
                                                data_disk_coeff = gf_get_coefficient(data_disk_coeff, field_power);

                                                //printf("\ndata disk coefficient = %d\n",data_disk_coeff);       

                                		if (NCFS_DATA->run_experiment == 1){
                                        		gettimeofday(&t1,NULL);
                                		}

						retstat = cacheLayer->readDisk(i,temp_buf,size,offset);

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t2,NULL);
                                                }

                                                for (j=0; j < size; j++){
                                                        temp_char = Q_temp[j];
                                                        Q_temp[j] = temp_char ^
                                                        (char)gf_mul((unsigned char)temp_buf[j],data_disk_coeff,field_power);
                                                }

                                                if (NCFS_DATA->run_experiment == 1){
                                                        gettimeofday(&t3,NULL);

                                        		//duration = (t2.tv_sec - t1.tv_sec) +
                                                	//	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                        		//NCFS_DATA->diskread_time += duration;

                                        		duration = (t3.tv_sec - t2.tv_sec) +
                                                		(t3.tv_usec-t2.tv_usec)/1000000.0;
                                        		NCFS_DATA->decoding_time += duration;
                                                }
                                        }

                                        //for (j=0; j<size; j++){
                                        //        printf("Q'=%c(%d) ",Q_temp[j],Q_temp[j]);
                                        //}

                                	//calculate D

                                	if (NCFS_DATA->run_experiment == 1){
                                        	gettimeofday(&t1,NULL);
                                	}

					retstat = cacheLayer->readDisk(code_disk_id,temp_buf,size,offset);

                                        if (NCFS_DATA->run_experiment == 1){
                                                gettimeofday(&t2,NULL);
                                        }

                                	for (j=0; j<size; j++){
                                        	temp_char = (char)(gf_mul(g2,(unsigned char)P_temp[j],field_power) 
                                                                ^ Q_temp[j] ^ temp_buf[j]);
                                        	buf[j] = (char)gf_div((unsigned char)temp_char, g12, field_power);
                                	}

                                        if (NCFS_DATA->run_experiment == 1){
                                                gettimeofday(&t3,NULL);

                                                //duration = (t2.tv_sec - t1.tv_sec) +
                                                //	(t2.tv_usec-t1.tv_usec)/1000000.0;
                                                //NCFS_DATA->diskread_time += duration;

                                                duration = (t3.tv_sec - t2.tv_sec) +
                                                	(t3.tv_usec-t2.tv_usec)/1000000.0;
                                                NCFS_DATA->decoding_time += duration;
                                        }
                                }
                        }
                }
                else{
                        printf("Raid 6 number of failed disks larger than 2.\n");
                        return -1;
                }

                free(temp_buf);
		free(P_temp);
		free(Q_temp);
                return size;
        }
        AbnormalError();

        return -1;
  
}

/*
 * recovering_raid6: RAID 6 recover
 *
 * @param failed_disk_id: failed disk id
 * @param newdevice: new device to replace the failed disk
 *
 * @return: 0: success ; -1: wrong
 */
int Coding4Raid6::recover(int failed_disk_id,char* newdevice)
{
        //size of data to be rebuilded
	int __recoversize = NCFS_DATA->disk_size[failed_disk_id];
	int block_size = NCFS_DATA->disk_block_size;
	char buf[block_size];	
	
	for(int i = 0; i < __recoversize; ++i){
	  
	        //reset the buf data
	        memset(buf, 0, block_size);
		
		int offset = i * block_size;

		int retstat = fileSystemLayer->codingLayer->decode(failed_disk_id,buf,block_size,offset);

		retstat = cacheLayer->writeDisk(failed_disk_id,buf,block_size,offset);

	}

	return 0;
	
}

int Coding4Raid6::recover_enable_node_encode(int failed_disk_id, char* newdevice, int* node_socketid_array)
{
  
  return 0;
}

int Coding4Raid6::recover_using_multi_threads_nor(int failed_disk_id, char* newdevice, int total_sor_threads){
  
    return 0;
}

int Coding4Raid6::recover_using_multi_threads_sor(int failed_disk_id, char* newdevice
									, int total_sor_threads, int number_sor_thread){
  
    return 0;
}

int Coding4Raid6::recover_using_access_aggregation(int failed_disk_id, char* newdevice){
  
    return 0;
}

int Coding4Raid6::recover_conventional(int failed_disk_id, char* newdevice){
  
   return 0;
}