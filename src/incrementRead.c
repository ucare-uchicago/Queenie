
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

enum Arg_Index {INDEX_MIN, INDEX_MAX, INDEX_INC, INDEX_ITER,
				INDEX_ALIGN, INDEX_DEVICE, INDEX_MODEL,
				INDEX_OUTPUT, INDEX_ALL, TOTAL_ARGS};

Arg_Spec args[TOTAL_ARGS];
int args_count = TOTAL_ARGS;

struct argp_option options[] = {
//  {"arg_name",INDEX_ARG,		TYPE_ARG,	0, "a short explain",				appear_in_log_filename}
	{"min",		INDEX_MIN,		ARG_SIZE,	0, "The min size of a read",		AO_MANDATORY|AO_APPEAR}, 
	{"max",		INDEX_MAX,		ARG_SIZE, 	0, "The max size of a read",		AO_MANDATORY|AO_APPEAR},
	{"inc",		INDEX_INC,		ARG_SIZE,	0, "The increment of read size",	AO_MANDATORY|AO_APPEAR}, 
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
	long min_size = 0;
	long max_size = 0;
	long increment = 1;
	long num_iteration = 0;
	long alignment;
	int output_all = FALSE;

	long lat_read, lat_total;

	double start_time;

	// parse the cmd line args
	if ((fd=args_parser(argc, argv, options)) != SUCCESS) {
		print_err(fd);
		exit(1);
	}

	min_size=args[INDEX_MIN].v_size;		max_size=args[INDEX_MAX].v_size;		increment=args[INDEX_INC].v_size;
	num_iteration=args[INDEX_ITER].v_int;	dev_list=(char**)(args[INDEX_DEVICE].v_multi);	dev_count=args[INDEX_DEVICE].d_multi;
	log_given=args[INDEX_OUTPUT].v_str;
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
			printf("Exp: incrementRead\nDevice: %s\nModel: %s\nCapacity: %ld Bytes\n"
				, device, ssd_model, ssd_capacity);
		}

		// create and open the log file
		if ((log_file = open_log_file(log_path, log_given, "incRead", ssd_model, ssd_capacity))==NULL) {
			print_err(FILE_HANDLING_ERROR);
			exit(1);
		}
		else {
			printf("Output: %s\n", log_path);
			fprintf(log_file, "\"size(B)\",\"offset(B)\",\"iteration\",\"time elapsed(s)\",\"read latency(ns)\"\n");
		}

		io.type = IO_READ;
		io.fd = fd;
		if (posix_memalign(&(io.buff), ONE_KiB, max_size)!=0) {
			print_err(MEMORY_ERROR);
			exit(1);
		}

		start_time = -1;

		for (long read_size = min_size; read_size <= max_size; read_size += increment) {

			printf("                                      \r");
			printf("Progress: Size(%ld/%ld)\r", read_size, max_size);
			fflush(stdout);

			io.size = read_size;
			lat_total = 0;

			for (long iter = 0; iter < num_iteration; iter++) {

				io.offset = rand_aligned_generator(0, ssd_capacity*RANGE_ADJUST-io.size, alignment);

				lat_read = perform_io_2g(&io);
				if (start_time < 0) {
					start_time = io.t_start;
				}

				if (lat_read > 0 && lat_read < ABNORMAL) {
					if (output_all) {
						fprintf(log_file, "%ld,%ld,%ld,%f,%ld\n", io.size, io.offset, iter, io.t_start-start_time, io.latency);
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
				fprintf(log_file, "%ld,%ld,%ld,%f,%ld\n", io.size, alignment, num_iteration, io.t_start-start_time, lat_total/num_iteration);
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