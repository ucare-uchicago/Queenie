
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "ioutil.h"

#define FLUSH_LEVEL_ONE 'p'
#define FLUSH_LEVEL_TWO 's'
#define FLUSH_LEVEL_THREE 'i'

#define USE_MODEL 'm'
#define USE_PATH 'p'

enum Arg_Index {INDEX_MIN, INDEX_MAX, INDEX_INC,
				INDEX_ITER, INDEX_FLUSH, INDEX_DEVICE, INDEX_MODEL,
				INDEX_OUTPUT, INDEX_FSLEEP, INDEX_ISLEEP, TOTAL_ARGS};

Arg_Spec args[TOTAL_ARGS];
int args_count = TOTAL_ARGS;

struct argp_option options[] = {
//  {"arg_name",INDEX_ARG,		TYPE_ARG,	0, "a short explain",				appear_in_log_filename}
	{"min",		INDEX_MIN,		ARG_SIZE,	0, "The min size of a write",		AO_MANDATORY|AO_APPEAR}, 
	{"max",		INDEX_MAX,		ARG_SIZE, 	0, "The max size of a write",		AO_MANDATORY|AO_APPEAR},
	{"inc",		INDEX_INC,		ARG_SIZE,	0, "The increment of write size",	AO_MANDATORY|AO_APPEAR}, 
	{"iter",	INDEX_ITER,		ARG_INT,	0, "Total number of iterations",	AO_MANDATORY|AO_APPEAR},
	{"flush",	INDEX_FLUSH,	ARG_STR,	0, "Toggle flush operation (n:off; p,s,i:on)", AO_APPEAR},
	{"device",	INDEX_DEVICE,	ARG_STR,	0, "Device path",					AO_MANDATORY|AO_MULTIPLE},
	{"model", 	INDEX_MODEL,	ARG_STR,	0, "Device model",					AO_ALTERNATE|INDEX_DEVICE},
	{"output",	INDEX_OUTPUT,	ARG_STR,	0, "Output file path (optional)",	AO_NULL},
	{"fs",		INDEX_FSLEEP,	ARG_TIME,	0, "sleep after a flush operation", AO_APPEAR},
	{"is",		INDEX_ISLEEP,	ARG_TIME,	0, "sleep after an IO operation",	AO_APPEAR},
	{0}
};

int main(int argc, char **argv) {
	
	char *exp_str = "SeqW", *exp_str_full = "sequentialWrite";

	char ** dev_list;
	int dev_count;
	char * device;
	char ssd_model[256];
	int fd;
	long ssd_capacity;
	
	char log_path[256];
	char * log;
	FILE * log_file;
	
	IO_Spec io;
	long base_offset = 0;
	long min_size = 0;
	long max_size = 0;
	long increment = 0;
	long num_iteration = 0;
	long new_offset = 0;
	int level_one_flush, level_two_flush, level_three_flush, indicator;
	struct timespec * f_sleep, * i_sleep;
	
	long offset, lat_flush, lat_write;
	
	double start_time;
	
	// parse the cmd line args
	if ((fd=args_parser(argc, argv, options)) != SUCCESS) {
		print_err(fd);
		exit(1);
	}
	
	// copy all the args
	min_size=args[INDEX_MIN].v_size;	max_size=args[INDEX_MAX].v_size;	 num_iteration=args[INDEX_ITER].v_int;
	increment=args[INDEX_INC].v_size; 	dev_list=(char**)(args[INDEX_DEVICE].v_multi);
	log=args[INDEX_OUTPUT].v_str;		dev_count=args[INDEX_DEVICE].d_multi;
	level_one_flush = contains_char(args[INDEX_FLUSH].v_str, FLUSH_LEVEL_ONE);
	level_two_flush = contains_char(args[INDEX_FLUSH].v_str, FLUSH_LEVEL_TWO);
	level_three_flush = contains_char(args[INDEX_FLUSH].v_str, FLUSH_LEVEL_THREE);
	f_sleep = args[INDEX_FSLEEP].status==SUCCESS?(&args[INDEX_FSLEEP].v_time):(NULL);
	i_sleep = args[INDEX_ISLEEP].status==SUCCESS?(&args[INDEX_ISLEEP].v_time):(NULL);
	
	for (int i = 0; i < dev_count; i++) {
	
		device = dev_list[i];
	
		// open the device
		if ((fd = open_device(device, O_DIRECT | O_WRONLY, ssd_model, &ssd_capacity )) <= FAILED) {
			print_err(DEVICE_ERROR);
			continue;
		}
		else {
			printf("Exp: %s\nDevice: %s\nModel: %s\nCapacity: %ld Bytes\n"
				, exp_str_full, device, ssd_model, ssd_capacity);
		}
		
		base_offset = get_base_offset(ssd_model);
		indicator = base_offset >= 0 ? USE_MODEL : USE_PATH;
		base_offset = base_offset >= 0 ? base_offset : get_base_offset(device);
		if (base_offset < 0) {
			print_err(base_offset);	close_device(fd);
			continue;
		}

		new_offset = base_offset + get_total_data_written(min_size, max_size, increment, num_iteration);
		if (new_offset >= ssd_capacity) {
			print_err(DATA_OVERFLOW); close_device(fd);
			continue;
		}
		else {
			if (save_base_offset(indicator==USE_MODEL?ssd_model:device, new_offset)!=SUCCESS) {
				print_err(FILE_HANDLING_ERROR); close_device(fd);
				continue;
			}
		}

		// create and open the log file
		if ((log_file = open_log_file(log_path, log, exp_str, ssd_model, ssd_capacity))==NULL) {
			print_err(FILE_HANDLING_ERROR);	close_device(fd);
			continue;
		}
		else {
			printf("Output: %s\n", log_path);
			fprintf(log_file, "\"size(B)\",\"offset(B)\",\"iteration\",\"flush latency(ns)\",\"time elapsed(s)\",\"write latency(ns)\"\n");
		}
		
		io.type = IO_WRITE;
		io.fd = fd;
		if (posix_memalign(&(io.buff), ONE_KiB, max_size)!=0) {
			print_err(MEMORY_ERROR);	close_device(fd); close_log_file(log_file);
			continue;
		}
		offset = base_offset;
		
		start_time = -1;
		lat_flush = 0;
		device_flush(device, 0, NULL);
		lat_flush += device_flush(NULL, level_one_flush, f_sleep);
		
		for (long size = min_size; size <= max_size && offset+size < ssd_capacity; size+=increment) {
			
			printf("                                      \r");
			printf("Progress: Size(%ld/%ld)\r", size, max_size);
			fflush(stdout);
			
			io.size = size;
			
			lat_flush += device_flush(NULL, level_two_flush, f_sleep);
			
			for (long iter = 0; iter < num_iteration && offset+size < ssd_capacity; iter++) {
				
				io.offset = offset;
				
				lat_flush += device_flush(NULL, level_three_flush, f_sleep);
				
				lat_write = perform_io_2g(&io);
				if (start_time < 0) {
					start_time = io.t_start;
				}
				
				fprintf(log_file, "%ld,%ld,%ld,%ld,%f,%ld\n", io.size, io.offset, iter, lat_flush, io.t_start-start_time, io.latency);
				
				offset += io.size;
				lat_flush = 0;
				
				my_sleep(i_sleep);
			}
		}
		
		free(io.buff);
		close_log_file(log_file);
		close_device(fd);
		
		printf("\n%s done\n\n", device);
	}
	
	printf("All done\n");
	
	return 0;
}