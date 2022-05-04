
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "ioutil.h"

#define DEFAULT_SECTOR_SIZE 512
#define DEFAULT_ALIGNMENT (256*ONE_KiB)
#define RANGE_ADJUST 0.8
#define OUTPUT_ALL_ITER 'y'
#define ABNORMAL 1300000000

enum Arg_Index {INDEX_SEC, INDEX_NUM, INDEX_MAX, INDEX_ITER,
				INDEX_ALIGN, INDEX_DEVICE, INDEX_MODEL,
				INDEX_OUTPUT, INDEX_ALL, TOTAL_ARGS};

Arg_Spec args[TOTAL_ARGS];
int args_count = TOTAL_ARGS;

struct argp_option options[] = {
//  {"arg_name",INDEX_ARG,		TYPE_ARG,	0, "a short explain",				appear_in_log_filename}
	{"sec",		INDEX_SEC,		ARG_SIZE,	0, "The size of a sector(default: 512B)",		AO_APPEAR},
	{"num",		INDEX_NUM,		ARG_INT,	0, "The number of sectors",			AO_MANDATORY|AO_APPEAR},
	{"max",		INDEX_MAX,		ARG_SIZE,	0, "The max range of pushing",		AO_MANDATORY|AO_APPEAR},
	{"iter",	INDEX_ITER,		ARG_INT,	0, "Total number of iterations",	AO_MANDATORY|AO_APPEAR},
	{"align",	INDEX_ALIGN,	ARG_SIZE,	0, "Alignment of random offset(default: 256K)",	AO_APPEAR},
	{"device",	INDEX_DEVICE,	ARG_STR,	0, "Device path",					AO_MANDATORY|AO_MULTIPLE},
	{"model", 	INDEX_MODEL,	ARG_STR,	0, "Device model",					AO_ALTERNATE|INDEX_DEVICE},
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

	char log_path[256];
	char * log_given;
	FILE * log_file;

	IO_Spec io;
	long sector_size = 512;
	long num_sector = 1;
	long max_range = 0;
	long num_iteration = 0;
	int output_all = FALSE;

	long alignment, lat_read, lat_total;

	double start_time;

	// parse the cmd line args
	if ((fd=args_parser(argc, argv, options)) != SUCCESS) {
		print_err(fd);
		exit(1);
	}

	num_sector=args[INDEX_NUM].v_int;		max_range=args[INDEX_MAX].v_size;	num_iteration=args[INDEX_ITER].v_int;
	dev_count=args[INDEX_DEVICE].d_multi;	dev_list=(char**)(args[INDEX_DEVICE].v_multi);	log_given=args[INDEX_OUTPUT].v_str;
	sector_size = args[INDEX_SEC].status == SUCCESS ? args[INDEX_SEC].v_size : DEFAULT_SECTOR_SIZE;
	alignment = args[INDEX_ALIGN].status == SUCCESS ? args[INDEX_ALIGN].v_size : DEFAULT_ALIGNMENT;
	output_all = contains_char(args[INDEX_ALL].v_str, OUTPUT_ALL_ITER);

	for (int i = 0; i < dev_count; i++) {

		device = dev_list[i];

		// open the device
		if ((fd = open_device(device, O_DIRECT | O_RDONLY, ssd_model, &ssd_capacity )) <= FAILED) {
			print_err(DEVICE_ERROR);
			exit(1);
		}
		else {
			printf("Exp: pushSectors\nDevice: %s\nModel: %s\nCapacity: %ld Bytes\n"
				, device, ssd_model, ssd_capacity);
		}

		// create and open the log file
		if ((log_file = open_log_file(log_path, log_given, "pushSec", ssd_model, ssd_capacity))==NULL) {
			print_err(FILE_HANDLING_ERROR);
			exit(1);
		}
		else {
			printf("Output: %s\n", log_path);
			fprintf(log_file, "\"size(B)\",\"offset(B)\",\"additional offset(B)\",\"iteration\",\"time elapsed(s)\",\"read latency(ns)\"\n");
		}

		io.type = IO_READ;
		io.fd = fd;
		io.size = sector_size*num_sector;
		if (posix_memalign(&(io.buff), ONE_KiB*4, io.size)!=0) {
			print_err(MEMORY_ERROR);
			exit(1);
		}

		start_time = -1;

		for (long additional_offset = 0; additional_offset <= max_range; additional_offset += sector_size) {

			printf("                                      \r");
			printf("Progress: Push(%ld/%ld)\r", additional_offset, max_range);
			fflush(stdout);

			lat_total = 0;

			for (long iter = 0; iter < num_iteration; iter++) {

				io.offset = rand_aligned_generator(0, ssd_capacity*RANGE_ADJUST-io.size, alignment) + additional_offset;
			
				lat_read = perform_io_2g(&io);
				if (start_time < 0) {
					start_time = io.t_start;
				}

				if (lat_read > 0 && lat_read < ABNORMAL) {
					if (output_all) {
						fprintf(log_file, "%ld,%ld,%ld,%ld,%f,%ld\n", io.size, io.offset, additional_offset, iter, io.t_start-start_time, io.latency);
					}
					else {
						lat_total += lat_read;
					}
				}
				else {
					iter--;
					continue;
				}
			}

			if (!output_all) {
				fprintf(log_file, "%ld,%ld,%ld,%ld,%f,%ld\n", io.size, alignment, additional_offset, num_iteration, io.t_start-start_time, lat_total/num_iteration);
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