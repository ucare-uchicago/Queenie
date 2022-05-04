
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

enum Arg_Index {INDEX_PAGE, INDEX_MIN, INDEX_MAX, INDEX_INC, 
				INDEX_ITER, INDEX_ALIGN, INDEX_DEVICE, INDEX_MODEL,
				INDEX_OUTPUT, INDEX_ALL, TOTAL_ARGS};

Arg_Spec args[TOTAL_ARGS];
int args_count = TOTAL_ARGS;

struct argp_option options[] = {
//  {"arg_name",INDEX_ARG,		TYPE_ARG,	0, "a short explain",				appear_in_log_filename}
	{"page",	INDEX_PAGE,		ARG_SIZE,	0, "The page size(default)",		AO_DISPLAY},	
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

	char *exp_str = "reRead", *exp_str_full = "reReadFirst", page_size_str[32];

	char **dev_list, *device, ssd_model[256];
	int dev_count, fd;
	long ssd_capacity;

	char log_path[256], *log_given;
	FILE * log_file;

	IO_Spec io, io_re;
	long page_size, min_size, max_size, increment, num_iteration, alignment;
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
			continue;
		}
		else {
			printf("Exp: %s\nDevice: %s\nModel: %s\nCapacity: %ld Bytes\n"
				,exp_str_full, device, ssd_model, ssd_capacity);
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
		}

		// create and open the log file
		if ((log_file = open_log_file(log_path, log_given, exp_str, ssd_model, ssd_capacity))==NULL) {
			print_err(FILE_HANDLING_ERROR);
			close_device(fd);
			continue;
		}
		else {
			printf("Output: %s\n", log_path);
			fprintf(log_file, "\"size(B)\",\"page size(B)\",\"offset(B)\",\"iteration\",\"time elapsed(s)\",\"read latency(ns)\"\n");
		}

		io.type = IO_READ;	io_re.type = IO_READ;
		io.fd = fd;			io_re.fd = fd;
							io_re.size = page_size;
		if (posix_memalign(&(io.buff), ONE_KiB, alignment)!=0 || posix_memalign(&(io_re.buff), ONE_KiB, page_size)!=0) {
			print_err(MEMORY_ERROR);
			close_device(fd);	close_log_file(log_file);
			continue;
		}

		start_time = -1;

		for (long read_size = min_size; read_size <= max_size; read_size += increment) {

			printf("                                      \r");
			printf("Progress: Size(%ld/%ld)\r", read_size, max_size);
			fflush(stdout);

			long current_size, current_offset;

			lat_total = 0;

			for (long iter = 0; iter < num_iteration; iter++) {

				io_re.offset = rand_aligned_generator(0, ssd_capacity*RANGE_ADJUST-read_size, alignment);

				current_size = read_size;
				current_offset = io_re.offset;
				while (current_size > 0) {

					io.size = alignment <= current_size ? alignment : current_size;
					io.offset = current_offset;

					perform_io_2g(&io);

					current_size -= io.size;
					current_offset += io.size;
				}

				lat_read = perform_io_2g(&io_re);
				if (start_time < 0) {
					start_time = io_re.t_start;
				}

				if (lat_read > 0 && lat_read < ABNORMAL) {
					if (output_all) {
						fprintf(log_file, "%ld,%ld,%ld,%ld,%f,%ld\n", read_size, io_re.size, io_re.offset, iter, io_re.t_start-start_time, io_re.latency);
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
				fprintf(log_file, "%ld,%ld,%ld,%ld,%f,%ld\n", read_size, io_re.size, alignment, num_iteration, io_re.t_start-start_time, lat_total/num_iteration);
			}
		}

		free(io.buff);	free(io_re.buff);
		close_log_file(log_file);
		close_device(fd);

		printf("\n%s: %s done\n\n", device, ssd_model);
	}

	printf("All done\n");

	return 0;
}