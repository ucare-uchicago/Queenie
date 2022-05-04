
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "ioutil.h"

#define OUTPUT_ALL_ITER 'y'
#define ABNORMAL 1300000000

enum Arg_Index {INDEX_FROM, INDEX_TO, INDEX_RS, INDEX_INC,
				INDEX_ITER, INDEX_DEVICE, INDEX_MODEL, 
				INDEX_OUTPUT, INDEX_ALL, TOTAL_ARGS};

Arg_Spec args[TOTAL_ARGS];
int args_count = TOTAL_ARGS;

struct argp_option options[] = {
//  {"arg_name",INDEX_ARG,		TYPE_ARG,	0, "a short explain",				ARG_OPTIONS}
	{"from",	INDEX_FROM,		ARG_SIZE,	0, "The start point",				AO_MANDATORY|AO_APPEAR}, 
	{"to",		INDEX_TO,		ARG_SIZE, 	0, "The end point",					AO_MANDATORY|AO_APPEAR},
	{"rs",		INDEX_RS,		ARG_SIZE, 	0, "The size of a read (default:page)",			AO_DISPLAY},
	{"inc",		INDEX_INC,		ARG_SIZE,	0, "The increment of offset (default:page)",	AO_DISPLAY}, 
	{"iter",	INDEX_ITER,		ARG_INT,	0, "Total number of iterations",	AO_MANDATORY|AO_APPEAR},
	{"device",	INDEX_DEVICE,	ARG_STR,	0, "Device path",					AO_MANDATORY|AO_MULTIPLE},
	{"model", 	INDEX_MODEL,	ARG_STR,	0, "Device model",					AO_ALTERNATE|INDEX_DEVICE},
	{"output",	INDEX_OUTPUT,	ARG_STR,	0, "Output file path (optional)",	AO_NULL},
	{"all",		INDEX_ALL,		ARG_STR,	0, "Output all iterations: y/n (default: n)", 	AO_APPEAR},
	{0}
};

int main(int argc, char **argv) {

	char * exp_str = "rangeRead";

	char ** dev_list;
	int dev_count;
	char * device;
	char ssd_model[256];
	int fd;
	long ssd_capacity;

	char read_size_str[32], inc_str[32];

	char log_path[256];
	char * log_given;
	FILE * log_file;

	IO_Spec io;
	long start_offset, end_offset, read_size, increment, num_iteration;
	int output_all = FALSE;

	long lat_read, lat_total;

	double start_time;

	// parse the cmd line args
	if ((fd=args_parser(argc, argv, options)) != SUCCESS) {
		print_err(fd);
		exit(1);
	}

	start_offset=args[INDEX_FROM].v_size;	end_offset=args[INDEX_TO].v_size;
	num_iteration=args[INDEX_ITER].v_int;	dev_list=(char**)(args[INDEX_DEVICE].v_multi);
	dev_count=args[INDEX_DEVICE].d_multi;	log_given=args[INDEX_OUTPUT].v_str;
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
				, exp_str, device, ssd_model, ssd_capacity);
		}

		// get the page size for read_size and increment if undefined in cmd args
		read_size = args[INDEX_RS].status <= FAILED ? get_page_size(ssd_model, read_size_str, INDEX_RS) : args[INDEX_RS].status;
		increment = args[INDEX_INC].status <= FAILED ? get_page_size(ssd_model, inc_str, INDEX_INC) : args[INDEX_INC].status;
		if (read_size <= FAILED || increment <= FAILED) {
			print_err(read_size <= FAILED ? read_size : increment);
			close_device(fd);
			continue;
		}
		else {
			read_size = args[INDEX_RS].v_size;
			increment = args[INDEX_INC].v_size;
		}
		
		// create and open the log file
		if ((log_file = open_log_file(log_path, log_given, exp_str, ssd_model, ssd_capacity))==NULL) {
			print_err(FILE_HANDLING_ERROR);
			close_device(fd);
			continue;
		}
		else {
			printf("Output: %s\n", log_path);
			fprintf(log_file, "\"size(B)\",\"offset(B)\",\"iteration\",\"time elapsed(s)\",\"read latency(ns)\"\n");
		}
		
		io.type = IO_READ;
		io.fd = fd;
		io.size = read_size;
		if (posix_memalign(&(io.buff), ONE_KiB, read_size)!=0) {
			print_err(MEMORY_ERROR);
			close_device(fd);	close_log_file(log_file);
			continue;
		}

		start_time = -1;

		// for (long current_offset = start_offset; 
		// 	current_offset <= end_offset && current_offset+read_size <= ssd_capacity; 
		// 	current_offset+=increment) {
		for (long current_offset = end_offset;
			current_offset >= start_offset;
			current_offset -= increment){

			printf("                                      \r");
			printf("Progress: Offset(%ld/%ld)\r", current_offset, end_offset);
			fflush(stdout);

			io.offset = current_offset;
			lat_total = 0;

			for (long iter = 0; iter < num_iteration; iter++) {

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
				fprintf(log_file, "%ld,%ld,%ld,%f,%ld\n", io.size, io.offset, num_iteration, io.t_start-start_time, lat_total/num_iteration);
			}
		}
		
		free(io.buff);
		close_log_file(log_file);
		close_device(fd);

		printf("\n%s: %s done\n\n", device, ssd_model);
	}

	printf("All done\n");

	return 0;
}