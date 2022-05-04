
#define _GNU_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "ioutil.h"

#define SIZE_MIN_DEFAULT (4*ONE_KiB)
#define SIZE_MAX_DEFAULT (256*ONE_KiB)
#define ALIGNMENT_DEFAULT (4*ONE_KiB)
#define RANGE_ADJUST 0.8
// #define OPTION_DEFAULT (-1)
#define FLAG_READ 'r'
#define FLAG_WRITE 'w'
#define FLAG_DIRECT 'd'
#define FLAG_VERBOSE 'y'

enum Arg_Index {INDEX_JOB, INDEX_MIN, INDEX_MAX, INDEX_SIZE,
				INDEX_OFFSET, INDEX_OFFINC, INDEX_ALIGN, INDEX_ITER, INDEX_OP, INDEX_DEVICE,
				INDEX_MODEL, INDEX_OUTPUT, INDEX_VERBOSE, TOTAL_ARGS};

Arg_Spec args[TOTAL_ARGS];
int args_count = TOTAL_ARGS;

struct argp_option options[] = {
	{"job",		INDEX_JOB,		ARG_INT,	0, "The number of concurrent jobs in a batch",	AO_MANDATORY|AO_APPEAR},
	{"min",		INDEX_MIN,		ARG_SIZE,	0, "The min size of a request (optional)",		AO_APPEAR}, 
	{"max",		INDEX_MAX,		ARG_SIZE, 	0, "The max size of a request (optional)",		AO_APPEAR},
	{"size",	INDEX_SIZE,		ARG_SIZE,	0, "The size of a request (optional)",			AO_NULL}, 
	{"offset",	INDEX_OFFSET,	ARG_SIZE,	0, "The offset of a request (optional)",		AO_NULL}, 
	{"off-inc", INDEX_OFFINC, 	ARG_SIZE, 	0, "The increment in offset (optional)", 		AO_NULL},
	{"align",	INDEX_ALIGN,	ARG_SIZE,	0, "Alignment of size and offset(default: 4K)",	AO_APPEAR},
	{"iter",	INDEX_ITER,		ARG_INT,	0, "Total number of iterations",		AO_MANDATORY|AO_APPEAR},
	{"op",		INDEX_OP,		ARG_STR,	0, "Operations (r:read; w:write; d:direct)",		AO_MANDATORY|AO_APPEAR},
	{"device",	INDEX_DEVICE,	ARG_STR,	0, "Device path",						AO_MANDATORY|AO_MULTIPLE},
	{"model", 	INDEX_MODEL,	ARG_STR,	0, "Device model",						AO_ALTERNATE|INDEX_DEVICE},
	{"output",	INDEX_OUTPUT,	ARG_STR,	0, "Output file path (optional)",		AO_NULL},
	{"verbose",	INDEX_VERBOSE,	ARG_STR,	0, "Output to terminal (y/n)",			AO_NULL},
	// {"fs",		INDEX_FSLEEP,	ARG_TIME,	0, "sleep after a flush operation", 	AO_APPEAR},
	// {"is",		INDEX_ISLEEP,	ARG_TIME,	0, "sleep after an IO operation",		AO_APPEAR},
	{0}
};

char get_io_type(int __r, int __w) {

	if ((__r+__w)%2) {
		return __r ? IO_READ : IO_WRITE;
	}
	else {
		return rand_aligned_generator(0, 2, 1);
	}
}

int main(int argc, char **argv) {

	char ** dev_list;
	int dev_count;
	char * device;
	char ssd_model[256];
	int fd;
	long ssd_capacity;

	char log_path[256];
	char * log_given;
	FILE * log_file;

	IO_Spec * io_list;
	int rw_flag, has_r, has_w, verbose, ret, first;
	long num_jobs, size_min, size_max, size, offset, off_inc, num_iteration, alignment;
	// long * lat_total;
	double start_time;

	// parse the cmd line args
	if ((ret=args_parser(argc, argv, options)) != SUCCESS) {
		print_err(ret);	exit(1);
	}

	num_jobs=args[INDEX_JOB].v_int;	num_iteration=args[INDEX_ITER].v_int; log_given=args[INDEX_OUTPUT].v_str;
	dev_count=args[INDEX_DEVICE].d_multi;	dev_list=(char**)(args[INDEX_DEVICE].v_multi);
	size_min = args[INDEX_MIN].status == SUCCESS ? args[INDEX_MIN].v_size : SIZE_MIN_DEFAULT;
	size_max = args[INDEX_MAX].status == SUCCESS ? args[INDEX_MAX].v_size : SIZE_MAX_DEFAULT;
	size = args[INDEX_SIZE].status == SUCCESS ? args[INDEX_SIZE].v_size : OPTION_DEFAULT;
	offset = args[INDEX_OFFSET].status == SUCCESS ? args[INDEX_OFFSET].v_size : OPTION_DEFAULT;
	off_inc = args[INDEX_OFFINC].status == SUCCESS ? args[INDEX_OFFINC].v_size : 0;
	if (off_inc > 0 && offset == OPTION_DEFAULT) {
		print_err(OPTION_DEFAULT); 	exit(1);
	}
	alignment = args[INDEX_ALIGN].status == SUCCESS ? args[INDEX_ALIGN].v_size : ALIGNMENT_DEFAULT;
	has_r = contains_char(args[INDEX_OP].v_str, FLAG_READ);
	has_w = contains_char(args[INDEX_OP].v_str, FLAG_WRITE);
	if (has_r && has_w) {
		rw_flag = O_RDWR;
	} else if (has_w) {
		rw_flag = O_WRONLY;
	} else if (has_r) {
		rw_flag = O_RDONLY;
	} else {
		print_err(OPTION_DEFAULT);	exit(1);
	}
	if (contains_char(args[INDEX_OP].v_str, FLAG_DIRECT)) {
		rw_flag = rw_flag|O_DIRECT;
	}
	verbose = contains_char(args[INDEX_VERBOSE].v_str, FLAG_VERBOSE);


	for (int i_dev = 0; i_dev < dev_count; i_dev++) {

		device = dev_list[i_dev];

		// open the device
		if ((fd = open_device(device, rw_flag, ssd_model, &ssd_capacity )) <= FAILED) {
			print_err(DEVICE_ERROR);	continue;
		}
		else {
			printf("Exp: testGeneric\nDevice: %s\nModel: %s\nCapacity: %ld Bytes\n"
				, device, ssd_model, ssd_capacity);
		}

		// create and open the log file
		if ((log_file = open_log_file(log_path, log_given, "test", ssd_model, ssd_capacity))==NULL) {
			print_err(FILE_HANDLING_ERROR);	close_device(fd);	continue;
		}
		else {
			printf("Output: %s\n", log_path);
			fprintf(log_file, "\"size(B)\",\"offset(B)\",\"type\",\"iteration\",\"time elapsed(s)\",\"latency(ns)\",\"status\"\n");
			// fflush(log_file);
		}

		// initialize IO jobs
		if ((io_list=(IO_Spec *)malloc(num_jobs*sizeof(IO_Spec)))==NULL) {
			print_err(MEMORY_ERROR);	continue;
		}
		for (int i=0; i<num_jobs; i++) {
			io_list[i].size = size;
			io_list[i].offset = offset + (off_inc*i);
			io_list[i].fd = fd;
			if (posix_memalign(&(io_list[i].buff), 4*ONE_KiB, size_max)!=0) {
				print_err(MEMORY_ERROR);
				continue;
			}
		}
		if ((ret=io_container_create(io_list, num_jobs))<=FAILED) {
			print_err(ret);
			continue;
		}

		first = TRUE;
		if (verbose) {
			printf("Test traces:\n");	fflush(stdout);
		}
		for (long iter = 0; iter<num_iteration; iter++) {

			if (!verbose) {
				printf("                                      \r");
				printf("Progress: iter(%ld/%ld)\r", iter, num_iteration);
				fflush(stdout);
			}

			for (int i=0; i<num_jobs; i++) {
				io_list[i].type = get_io_type(has_r, has_w);
				if (size == OPTION_DEFAULT) {
					io_list[i].size = rand_aligned_generator(size_min, size_max+1, alignment);
				}
				if (offset == OPTION_DEFAULT) {
					io_list[i].offset = rand_aligned_generator(0, ssd_capacity*RANGE_ADJUST-size_max, alignment);
				}
			}

			io_container_run();
			if (first) {
				start_time = get_min_t_start(io_list, num_jobs);
				first = FALSE;
			}

			for (int i=0; i<num_jobs; i++) {
				fprintf(log_file, "%ld,%ld,%d,%ld,%f,%ld,%d\n"
				, io_list[i].size, io_list[i].offset, io_list[i].type, iter, io_list[i].t_start-start_time, io_list[i].latency, io_list[i].status);
				// fflush(log_file);
				if (verbose) {
					printf("iter %ld;\ttype %d;\tsize %ld;\toffset %ld;\tlatency %ld;\tstatus %d\n"
						, iter, io_list[i].type, io_list[i].size, io_list[i].offset, io_list[i].latency, io_list[i].status);
					fflush(stdout);
					// if (io_list[i].status != SUCCESS) {
					// 	printf("iter %ld;\ttype %d;\tsize %ld;\toffset %ld;\tlatency %ld\n"
					// 		, iter, io_list[i].type, io_list[i].size, io_list[i].offset, io_list[i].latency);
					// 	fflush(stdout);
					// }
				}
			}

		}

		io_container_destroy();
		for (int i=0; i<num_jobs; i++) {
			free(io_list[i].buff);
		}
		close_log_file(log_file);
		close_device(fd);
		
		printf("\n%s done\n\n", device);

	}

	printf("All done\n");
	
	return 0;
}
