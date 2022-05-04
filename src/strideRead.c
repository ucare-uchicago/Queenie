
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "ioutil.h"

#define DEFAULT_ALIGNMENT (256*ONE_KiB)
#define RANGE_ADJUST 0.8
#define OUTPUT_ALL_ITER 'y'
#define ABNORMAL 1300000000

enum Arg_Index {INDEX_PAGE, INDEX_JOB, INDEX_MIN, INDEX_MAX, 
				INDEX_ITER, INDEX_ALIGN, INDEX_DEVICE, INDEX_MODEL,
				INDEX_OUTPUT, INDEX_ALL, TOTAL_ARGS};

Arg_Spec args[TOTAL_ARGS];
int args_count = TOTAL_ARGS;

struct argp_option options[] = {
//  {"arg_name",INDEX_ARG,		TYPE_ARG,	0, "a short explain",		ARG_OPTIONS}
	{"page",	INDEX_PAGE,		ARG_SIZE,	0, "The page size (default)",			AO_DISPLAY}, 
	{"job",		INDEX_JOB,		ARG_INT,	0, "The number of jobs",	AO_MANDATORY|AO_APPEAR}, 
	{"min",		INDEX_MIN,		ARG_INT,	0, "The min stride",		AO_MANDATORY|AO_APPEAR}, 
	{"max",		INDEX_MAX,		ARG_INT, 	0, "The max stride",		AO_MANDATORY|AO_APPEAR},
	{"iter",	INDEX_ITER,		ARG_INT,	0, "Total number of iterations",	AO_MANDATORY|AO_APPEAR},
	{"align",	INDEX_ALIGN,	ARG_SIZE,	0, "Alignment of random offset(default: page)",	AO_APPEAR},
	{"device",	INDEX_DEVICE,	ARG_STR,	0, "Device path",			AO_MANDATORY|AO_MULTIPLE},
	{"model", 	INDEX_MODEL,	ARG_STR,	0, "Device model",			AO_ALTERNATE|INDEX_DEVICE},
	{"output",	INDEX_OUTPUT,	ARG_STR,	0, "Output file path (optional)",	AO_NULL},
	{"all",		INDEX_ALL,		ARG_STR,	0, "Output all iterations: y/n (default: n)", 	AO_APPEAR},
	{0}
};

int main(int argc, char **argv) {

	char ** dev_list;
	int dev_count;
	char * device;
	char ssd_model[256];
	int fd;
	long ssd_capacity;

	char page_size_str[32];

	char log_path[256];
	char * log_given;
	FILE * log_file;

	IO_Spec * io_list;
	long page_size, num_jobs, min_stride, max_stride, num_iteration, alignment, output_all;
	long * lat_total;
	double start_time;

	// parse the cmd line args
	if ((fd=args_parser(argc, argv, options)) != SUCCESS) {
		print_err(fd);
		exit(1);
	}

	min_stride=args[INDEX_MIN].v_int;		max_stride=args[INDEX_MAX].v_int;
	num_iteration=args[INDEX_ITER].v_int;	dev_count=args[INDEX_DEVICE].d_multi;	dev_list=(char**)(args[INDEX_DEVICE].v_multi);
	log_given=args[INDEX_OUTPUT].v_str;		num_jobs=args[INDEX_JOB].v_int;
	output_all = contains_char(args[INDEX_ALL].v_str, OUTPUT_ALL_ITER);

	for (int i_dev = 0; i_dev < dev_count; i_dev++) {

		device = dev_list[i_dev];

		// open the device
		if ((fd = open_device(device, O_DIRECT | O_RDONLY, ssd_model, &ssd_capacity )) <= FAILED) {
			print_err(DEVICE_ERROR);
			continue;
		}
		else {
			printf("Exp: pushSectors\nDevice: %s\nModel: %s\nCapacity: %ld Bytes\n"
				, device, ssd_model, ssd_capacity);
		}

		// get the page size if undefined in cmd args
		page_size = args[INDEX_PAGE].status <= FAILED ? get_page_size(ssd_model, page_size_str, INDEX_PAGE) : args[INDEX_PAGE].status;
		if (page_size <= FAILED) {
			print_err(page_size);
			close_device(fd);
			continue;
		}
		else {
			page_size = args[INDEX_PAGE].v_size;
			alignment = args[INDEX_ALIGN].status == SUCCESS ? args[INDEX_ALIGN].v_size : page_size;
		}

		// create and open the log file
		if ((log_file = open_log_file(log_path, log_given, "strideRead", ssd_model, ssd_capacity))==NULL) {
			print_err(FILE_HANDLING_ERROR);
			close_device(fd);
			continue;
		}
		else {
			printf("Output: %s\n", log_path);
			fprintf(log_file, "\"size(B)\",\"offset(B)\",\"stride\",\"iteration\",\"time elapsed(s)\",\"read latency(ns)\"\n");
		}

		// initialize IO jobs
		if ((io_list=(IO_Spec *)malloc(num_jobs*sizeof(IO_Spec)))==NULL || (lat_total=(long*)malloc(num_jobs*sizeof(long)))==NULL) {
			print_err(MEMORY_ERROR);
			close_device(fd);	close_log_file(log_file);
			continue;
		}
		for (int i=0; i<num_jobs; i++) {
			io_list[i].type = IO_READ;
			io_list[i].fd = fd;
			io_list[i].size = page_size;
			if (posix_memalign(&(io_list[i].buff), ONE_KiB, page_size)!=0) {
				print_err(MEMORY_ERROR);
				close_device(fd);	close_log_file(log_file);
				exit(1);
			}
		}
		if ((lat_total[0]=io_container_create(io_list, num_jobs))<=FAILED) {
			print_err(lat_total[0]);
			close_device(fd);	close_log_file(log_file);
			exit(1);
		}

		start_time = -1;

		for (long stride = min_stride; stride <= max_stride; stride++) {

			printf("                                      \r");
			printf("Progress: Stride(%ld/%ld)\r", stride, max_stride);
			fflush(stdout);

			for (int i=0; i<num_jobs; i++) {
				lat_total[i] = 0;
			}

			for (long iter = 0; iter < num_iteration; iter++) {

				io_list[0].offset = rand_aligned_generator(0, ssd_capacity*RANGE_ADJUST-page_size, alignment);
				for (int i = 1; i < num_jobs; i++) {
					io_list[i].offset = io_list[0].offset + i*stride*page_size;
				}

				io_container_run();

				if (start_time < 0) {
					start_time = get_min_t_start(io_list, num_jobs);
				}

				if (latency_check(io_list, num_jobs, 1, ABNORMAL)) {
					if (output_all) {
						for (int i=0; i<num_jobs; i++) {
							fprintf(log_file, "%ld,%ld,%ld,%ld,%f,%ld\n", io_list[i].size, io_list[i].offset, stride, iter, io_list[i].t_start-start_time, io_list[i].latency);
						}
					}
					else {
						latency_sort(io_list, num_jobs);
						for (int i=0; i<num_jobs; i++) {
							lat_total[i] += io_list[i].latency;
						}
					}
				}
				else {
					iter--;
					continue;
				}
			}

			if (!output_all) {
				for (int i=0; i<num_jobs; i++) {
					fprintf(log_file, "%ld,%ld,%ld,%ld,%f,%ld\n", page_size, alignment, stride, num_iteration, io_list[i].t_start-start_time, lat_total[i]/num_iteration);
				}
			}
		}

		io_container_destroy();
		for (int i=0; i<num_jobs; i++) {
			free(io_list[i].buff);
		}
		free(io_list);
		free(lat_total);
		close_log_file(log_file);
		close_device(fd);

		printf("\n%s done\n\n", device);
	}
	
	printf("All done\n");

	return 0;
}