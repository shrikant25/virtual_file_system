#include "vdconstants.h"
#include <stdlib.h>
#include <stdio.h>
#include "vddriver.h"
#include "vdsyslib.h"
#include "vdrun_disk.h"
#include "vdwrite_to_buffer.h"
#include <string.h>

enum task {task_insert_file = 1, task_delete_file = 2, task_fetch_file = 3};

void run_disk(DISKINFO DSKINF, FR_FLGBLK_LST *FFLST, unsigned long int *flags, int fd){

	char usrflnm;
	char task[100];
	int task_file;
	size_t task_file_len;
	size_t total_data_read;
	unsigned int data_to_beread = 0;
	char *buffer = malloc(sizeof(char) * VDKB); 
	int i;
	char ch;
	int last_task_end_index;
	FILE_ACTION_VARS FAV;
	
	FAV.DSKINF = DSKINF;
	FAV.FFLST = FFLST;
	FAV.flags = flags;
	FAV.disk_fd = fd;

	task_file = open("task_file", O_RDONLY, 00777);
	task_file_len = lseek(task_file, 0, SEEK_END);
	lseek(task_file, 0, SEEK_SET);

	while(total_data_read < task_file_len){

		memset(buffer, 0, VDKB);
		data_to_beread = (task_file_len - total_data_read) > VDKB ? VDKB : (task_file_len - total_data_read);
		read(task_file, buffer, data_to_beread);

		for(i = 0; i<data_to_beread; i++){
			
			if(buffer[i] != '\n')
				task[i] = buffer[i];
			else{

				last_task_end_index = i+1;
				task[i] = '\0';
				FAV.level_size = NULL;
				FAV.tree_depth = -1;
				FAV.dskblk_ofmtd = -1;
				FAV.loc_ofmtd_in_blk = -1;
				FAV.usrfl_fd = -1;
				FAV.usrflnm = usrflnm;
				FAV.usrflsz = -1;
				FAV.filebegloc = -1;

				perform_task(task_delete_file, FAV);

			}
				
		}
	}

	free(buffer);

}

int perform_task(int task, FILE_ACTION_VARS FAV){

	FAV.usrfl_fd = open(FAV.usrflnm, O_RDONLY, 00777);
		if (FAV.usrfl_fd == -1) return -1;

	unsigned int level_size[5]; 
	FAV.level_size = level_size;
	
	FAV.usrflsz = lseek(FAV.usrfl_fd, 0, SEEK_END);
	lseek(FAV.usrfl_fd, 0, SEEK_SET);

	get_tree_info(&FAV);

	switch (task){
		case task_insert_file:
								search(&FAV, 1);
								if(FAV.dskblk_ofmtd != -1 && FAV.loc_ofmtd_in_blk != -1){
									insert_file(&FAV);
									close(FAV.usrfl_fd);
									return 0;
								}
								return -1;
								break;
		
		case task_delete_file:
								search(&FAV, 0);
								if(FAV.dskblk_ofmtd != -1 && FAV.loc_ofmtd_in_blk != -1){
									delete(&FAV);
									close(FAV.usrfl_fd);
									return 0;
								}
								return -2;

		case task_fetch_file:
								search(&FAV, 0);
								if(FAV.dskblk_ofmtd != -1 && FAV.loc_ofmtd_in_blk != -1){
									fetch(&FAV);
									close(FAV.usrfl_fd);
									return 0;
								}
								return -3;
	}
	return -4;
}


void readflags(int fd, unsigned long int *flags, DISKINFO DSKINF){
	
	int j = 0;
	int i = 0;
	int dataread = 0;
	unsigned long *ptr;
	char *buffer = malloc(DSKINF.blkcnt);
	int total_data = DSKINF.flags_arrsz * VDQUAD;

	i = 1;
	while(total_data > 0){
	
		dataread = vdread(fd, buffer, i, DSKINF.blksz);
		i++;
		total_data -= dataread;
		ptr = (unsigned long *)buffer;

		while(dataread > 0){
			flags[j++] = *ptr++;
			dataread -= VDQUAD;
		}

	}
	
	free(buffer);

}


int main(int argc, char *argv[]){
	
	if(argc != 2){
		write(1, "Invalid arguments\n", 18);
		exit(EXIT_FAILURE);
	}
	
	unsigned int fd = 0; 
	unsigned long int *flags;
	int task = 0;
	DISKINFO DSKINF;
	FR_FLGBLK_LST FFLST;
	FFLST.frblkcnt = 0;
	FFLST.head = NULL;
	
	fd = open(argv[1], O_RDWR, 00777);
	if(fd == -1) return -1;

	read(fd, &DSKINF, sizeof(DSKINF));
	printf("blksz %ld\n", DSKINF.blksz);
	printf("blcknt %ld\n", DSKINF.blkcnt);
	printf("flags arrasz %d\n", DSKINF.flags_arrsz);
	printf("flagsblkcnt %d\n", DSKINF.flgblkcnt);
	printf("ttlmtdta_blks %d\n", DSKINF.ttlmtdta_blks);
	printf(" DSKINF.dsk_blk_for_mtdata %d\n",  DSKINF.dsk_blk_for_mtdata);
	printf(" DSKINF.mtdta_blk_ofst %d\n",  DSKINF.mtdta_blk_ofst);
	printf("flag blocks count : %d\n", DSKINF.flgblkcnt);
	printf("total metadata blocks  %u\n", DSKINF.ttlmtdta_blks);
	printf("dsk blocks required for metadata blocks %d\n", DSKINF.ttlmtdta_blks);
	
	flags = malloc(DSKINF.flags_arrsz * VDQUAD);
	readflags(fd, flags, DSKINF);
	
	build(DSKINF, flags, &FFLST);		
	
	printf("total blcks %d\n", FFLST.frblkcnt);
	printf("largest available block is at %d\n", FFLST.head->loc);
	printf("largest available block is at %ld\n", ((FFLST.head->loc)*DSKINF.blksz)+1);
	printf("total empty  blocks %d\n", FFLST.head->cnt);
	printf("total empty  bytes %ld\n", (FFLST.head->cnt)*DSKINF.blksz);
	run_disk(DSKINF, &FFLST, flags, fd);

	free(flags);
	close(fd);
	
	return 0;
}
