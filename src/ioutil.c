
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include "ioutil.h"
#include <errno.h>

typedef struct trans_unit {
	char symbol;
	double units;
} Trans_Unit;

long size_flatten(const char * __sstr);
int time_flatten(struct timespec * __time, const char * __tstr);
long int_flatten(const char * __istr);
int option_check(int __index, int __check);
int option_get_parameter(int __index);
int get_nvme_path(char* __model);
int get_sata_path(char* __name);

// used by print_err
const char * err_messages[] = {
	"An error has occurred\n",
	"\e[1;31mErr_1\e[0m: pread or pwrite failed\n",
	"\e[1;31mErr_2\e[0m: The number (size of amount) specified is illegal\n",
	"\e[1;31mErr_3\e[0m: Unexpected NULL pointer passed to function\n",
	"\e[1;31mErr_4\e[0m: Memory failure, could be malloc, posix_memalign, etc.\n",
	"\e[1;31mErr_5\e[0m: pthread failure\n",
	"\e[1;31mErr_6\e[0m: Device failure\n",
	"\e[1;31mErr_7\e[0m: Error in handling files\n",
	"\e[1;31mErr_8\e[0m: Illegal size format\n",
	"\e[1;31mErr_9\e[0m: Missing cmd arguments\n",
	"\e[1;31mErr_10\e[0m: Illegal bool type\n",
	"\e[1;31mErr_11\e[0m: Illegal time format\n",
	"\e[1;31mErr_12\e[0m: Illegal integer format\n",
	"\e[1;31mErr_13\e[0m: Error in sleep function\n",
	"\e[1;31mErr_14\e[0m: The arg option specified is not supported\n",
	"\e[1;31mErr_15\e[0m: The specified model is not found\n",
	"\e[1;31mErr_16\e[0m: Page size entry undefined in config\n",
	"\e[1;31mErr_17\e[0m: Base offset entry undefined in config\n",
	"\e[1;31mErr_18\e[0m: Total data written exceeds address space\n"
};
const char * undefined_err_message = "\e[1;31mErr_0\e[0m: Undefined error code\n";
//end

// used by io_container
int continue_exe, start_exe, *start_exe_queue;
sem_t all_threads_ready, all_threads_finished, main_thread_ready;

pthread_t * threads;

int io_count;
// end

#ifndef CMD_ARGS_OFF

extern Arg_Spec args[];
extern int args_count;
extern struct argp_option options[];

void single_parse(int __key, char * __arg) {
	
	const char * type__ = options[__key].arg;
	
	args[__key].v_str = __arg;
	
	if (strcmp(type__, ARG_INT) == 0) {
		str_upper(__arg);
		if ((args[__key].v_int = int_flatten(__arg))<=FAILED) {
			args[__key].status = args[__key].v_int;
		}
		else {
			args[__key].status = SUCCESS;
		}
	}
	else if (strcmp(type__, ARG_SIZE) == 0) {
		str_upper(__arg);
		if ((args[__key].v_size = size_flatten(__arg))<=FAILED) {
			args[__key].status = args[__key].v_size;
		}
		else {
			args[__key].status = SUCCESS;
		}
	}
	else if (strcmp(type__, ARG_BOOL) == 0){
		str_upper(__arg);
		if (strcmp(__arg, "TRUE")==0||strcmp(__arg, "T")==0||strcmp(__arg, "1")==0) {
			args[__key].v_bool = TRUE;
			args[__key].status = SUCCESS;
		}
		else if (strcmp(__arg, "FALSE")==0||strcmp(__arg, "F")==0||strcmp(__arg, "0")==0) {
			args[__key].v_bool = FALSE;
			args[__key].status = SUCCESS;
		}
		else {
			args[__key].status = ILLEGAL_BOOL_FORMAT;
		}
	}
	else if (strcmp(type__, ARG_STR) == 0){
		args[__key].status = SUCCESS;
	}
	else if (strcmp(type__, ARG_FLOAT) == 0) {
		args[__key].v_float = strtod(__arg, NULL);
		args[__key].status = SUCCESS;
	}
	else if (strcmp(type__, ARG_TIME) == 0) {
		args[__key].status = time_flatten(&(args[__key].v_time), __arg);
	}
	else {
		args[__key].status = UNSUPPORTED_ARG_OPTION;
	}
}

void multiple_parse(int __key, char * __arg) {
	
	const char * type__ = options[__key].arg;
	
	if (args[__key].status != OPTION_DEFAULT && args[__key].status != SUCCESS) {
		return;
	}
	
	if (strcmp(type__, ARG_STR) == 0) {
		
		char ** str_list;
		int degree;
		
		degree = args[__key].d_multi+1;
		if ((args[__key].v_multi = realloc(args[__key].v_multi, degree*sizeof(char *)))==NULL) {
			args[__key].status = MEMORY_ERROR;
			return ;
		}
		
		((char **)args[__key].v_multi)[degree-1] = __arg;
		args[__key].d_multi = degree;
		args[__key].status = SUCCESS;
	}
	else {
		args[__key].status = UNSUPPORTED_ARG_OPTION;
	}
}

int alternate_parse(int __key, char * __arg) {

	const char * type__ = options[__key].arg;

	if (strcmp(type__, ARG_STR) == 0) {

		if (get_nvme_path(__arg) <= FAILED) {
			if (get_sata_path(__arg) <= FAILED) {
				args[__key].status = UNMATCHED_MODEL;
				return UNMATCHED_MODEL;
			}
		}

		args[__key].status = SUCCESS;
		return SUCCESS;
	}
	else {
		args[__key].status = UNSUPPORTED_ARG_OPTION;
		return UNSUPPORTED_ARG_OPTION;
	}
}

static int parse_opt (int __key, char * __arg, struct argp_state *state) {
	
	if (__key >= 0 && __key <= args_count-1) {

		if (option_check(__key, AO_ALTERNATE)) {
			if (alternate_parse(__key, __arg) == SUCCESS) {
				__key = option_get_parameter(__key);
			}
			else {
				return 0;
			}
		}
		
		if (option_check(__key, AO_MULTIPLE)) {
			multiple_parse(__key, __arg);
		}
		else {
			single_parse(__key, __arg);
		}
	}
	
	return 0;
}

int args_parser(int argc, char **argv, struct argp_option __opts[]) {
	
	struct argp arg_p = {__opts, parse_opt};
	
	for (int i=0; i<args_count; i++) {
		args[i].status = OPTION_DEFAULT;
		args[i].v_str = NULL;
		args[i].d_multi = 0;
		args[i].v_multi = NULL;
	}
	
	argp_parse(&arg_p, argc, argv, 0, 0, 0);

	for (int i=0; i<args_count; i++) {
		if (args[i].status == OPTION_DEFAULT) {
			if (option_check(i, AO_MANDATORY)) {
				return OPTION_DEFAULT;
			}
		}
		else if (args[i].status <= FAILED) {
			return args[i].status;
		}
	}

	return SUCCESS;
}
/*
void arg_destroy() {
	
	for (int i=0; i<args_count; i++) {
		for (int j=0; j<args[i].d_multi; j++) {
			free()
		}
	}
}*/

int strrmv(char * __str, char __ch) {
	
	int len = strlen(__str), count = 0;
	
	for (int i=0; i<len; i++) {
		if (__str[i] == __ch) {
			for (int j=i; j<len; j++) {
				__str[j] = __str[j+1];
			}
			len--;
			i--;
			count++;
		}
	}
	
	return count;
}

FILE * open_log_file(char * __path, const char * __log, const char * __pre, const char * __dev, long __cap) {
	
	if (__log == NULL) {
		
		char file_prefix[256], server_str[16];
		char * server_path = "server.config";
		FILE * server_file;
		int id;
		
		if ((server_file=fopen(server_path, "r"))!=NULL) {

			if (fgets(server_str, 16, server_file)!=NULL) {
				server_str[strlen(server_str)-1] = '\0';
			}
			else {
				strcpy(server_str, "");
			}

			fclose(server_file);
		}
		else {
			strcpy(server_str, "");
		}

		if (strlen(server_str)==0) {
			strcpy(file_prefix, __pre);
		}
		else {
			sprintf(file_prefix, "%s_%s", __pre, server_str);
		}
		
		if (__cap/(long)ONE_TB == 0) {
			sprintf(file_prefix, "%s_[%s_%ldGB]", file_prefix, __dev, __cap/(long)ONE_GB);
		}
		else {
			sprintf(file_prefix, "%s_[%s_%.2fTB]", file_prefix, __dev, __cap/(double)ONE_TB);
		}
		
		for (int i=0; i<args_count; i++) {
			if ((option_check(i, AO_APPEAR) && args[i].status == SUCCESS)||option_check(i, AO_DISPLAY)) {
				if (strcmp(options[i].arg, ARG_BOOL)==0) {
					sprintf(file_prefix, "%s_[%s_%d]", file_prefix, options[i].name, args[i].v_bool);
				}
				else {
					sprintf(file_prefix, "%s_[%s_%s]", file_prefix, options[i].name, args[i].v_str);
				}	
			}
		}
		
		id = 0;
		strrmv(file_prefix, '/');
		sprintf(__path, "%s_[exp_%d].csv", file_prefix, id);
		while (access(__path, F_OK) != -1) {
			id++;
			sprintf(__path, "%s_[exp_%d].csv", file_prefix, id);
		}
		
		return fopen(__path, "w+");
	}
	else {
		strcpy(__path, __log);
		return fopen(__log, "w+");
	}
}

int close_log_file(FILE * __file) {
	return fclose(__file);
}

#endif

long perform_io(IO_Spec * __io) {
	
	struct timespec t_start, t_end;
	long res__, size__, off__;
	
	size__ = __io->size;
	off__ = __io->offset;

	clock_gettime(CLOCK_REALTIME, &t_start);
	while (size__ > 0) {
		res__ = __io->type==IO_WRITE?pwrite(__io->fd, __io->buff, size__, off__):
								pread(__io->fd, __io->buff, size__, off__);
		if (res__ <= 0) {
			break;
		}
		else {
			size__ -= res__;
			off__ += res__;
		}
	}
	clock_gettime(CLOCK_REALTIME, &t_end);
	
	__io->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
	if (size__ == 0) {
		__io->status = SUCCESS;
		// __io->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
		__io->t_start = (double)t_start.tv_sec + ((double)t_start.tv_nsec / (double)ONE_SEC);
		return __io->latency;
	}
	else {
		__io->status = PREAD_PWRITE_ERROR;
		// __io->latency = PREAD_PWRITE_ERROR;
		return PREAD_PWRITE_ERROR;
	}
}

long perform_io_2g(IO_Spec * __io) {
	
	struct timespec t_start, t_end;
	long res__;
	
	if (__io->size > SIZE_2G || __io->size < 0) {
		__io->latency = ILLEGAL_AMOUNT;
		return ILLEGAL_AMOUNT;
	}
	
	clock_gettime(CLOCK_REALTIME, &t_start);
	res__ = __io->type==IO_WRITE?pwrite(__io->fd, __io->buff, __io->size, __io->offset)
						:pread(__io->fd, __io->buff, __io->size, __io->offset);
	clock_gettime(CLOCK_REALTIME, &t_end);
	
	__io->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
	if (res__ == __io->size) {
		__io->status = SUCCESS;
		// __io->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
		__io->t_start = (double)t_start.tv_sec + ((double)t_start.tv_nsec / (double)ONE_SEC);
		return __io->latency;
	}
	else {
		__io->status = PREAD_PWRITE_ERROR;
		// __io->latency = PREAD_PWRITE_ERROR;
		return PREAD_PWRITE_ERROR;
	}
}

void * perform_concurrent_io_2g(void * __arg) {
	
	struct timespec t_start, t_end;
	IO_Spec * io__ = (IO_Spec *)__arg;
	long res__;
	
	sem_wait(&main_thread_ready);
	while (continue_exe) {
		
		sem_post(&all_threads_ready);
		while (!start_exe);
		
		if (io__->size >= 0 && io__->size <= SIZE_2G) {
			clock_gettime(CLOCK_REALTIME, &t_start);
			res__ = io__->type==IO_WRITE?pwrite(io__->fd, io__->buff, io__->size, io__->offset)
								:pread(io__->fd, io__->buff, io__->size, io__->offset);
			clock_gettime(CLOCK_REALTIME, &t_end);
			
			io__->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
			if (res__ == io__->size) {
				io__->status = SUCCESS;
				// io__->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
				io__->t_start = (double)t_start.tv_sec + ((double)t_start.tv_nsec / (double)ONE_SEC);
			}
			else {
				io__->status = PREAD_PWRITE_ERROR;
				// io__->latency = PREAD_PWRITE_ERROR;
			}
		}
		else {
			io__->status = ILLEGAL_AMOUNT;
			// io__->latency = ILLEGAL_AMOUNT;
		}
		
		sem_post(&all_threads_finished);
		sem_wait(&main_thread_ready);
	}
}

void * perform_precise_concurrent_io_2g(void * __arg) {
	
	struct timespec t_start, t_end;
	IO_Spec * io__ = (IO_Spec *)__arg;
	long res__;
	
	sem_wait(&main_thread_ready);
	while (continue_exe) {
		
		sem_post(&all_threads_ready);
		while (!(start_exe_queue[io__->id]));
		
		if (io__->size >= 0 && io__->size <= SIZE_2G) {
			clock_gettime(CLOCK_REALTIME, &t_start);
			res__ = io__->type==IO_WRITE?pwrite(io__->fd, io__->buff, io__->size, io__->offset)
								:pread(io__->fd, io__->buff, io__->size, io__->offset);
			clock_gettime(CLOCK_REALTIME, &t_end);
			
			io__->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
			if (res__ == io__->size) {
				io__->status = SUCCESS;
				// io__->latency = (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
				io__->t_start = (double)t_start.tv_sec + ((double)t_start.tv_nsec / (double)ONE_SEC);
			}
			else {
				io__->status = PREAD_PWRITE_ERROR;
				// io__->latency = PREAD_PWRITE_ERROR;
			}
		}
		else {
			io__->status = ILLEGAL_AMOUNT;
			// io__->latency = ILLEGAL_AMOUNT;
		}
		
		sem_post(&all_threads_finished);
		sem_wait(&main_thread_ready);
	}
}

int io_container_create(IO_Spec * __io_list, int __count) {
	
	int i;
	
	if (__io_list == NULL) {
		return NULL_POINTER;
	}
	
	io_count = __count;
	
	if (io_count > 0) {
		
		/*for (i=0; i<io_count; i++) {
			if (__io_list[i] == NULL) {
				return NULL_POINTER;
			}
		}*/
		
		if ((threads = (pthread_t *)malloc(io_count * sizeof(pthread_t)))==NULL) {
			return MEMORY_ERROR;
		}
	}
	else {
		return ILLEGAL_AMOUNT;
	}
	
	continue_exe = TRUE;
	start_exe = FALSE;
	sem_init(&all_threads_ready, 0, 0);
	sem_init(&all_threads_finished, 0, 0);
	sem_init(&main_thread_ready, 0, 0);
	
	for (i=0; i<io_count; i++) {
		if (pthread_create(&threads[i], NULL, &perform_concurrent_io_2g, (void *)&(__io_list[i]))!=0) {
			return PTHREAD_ERROR;
		}
	}
	
	return SUCCESS;
}

int precise_io_container_create(IO_Spec * __io_list, int __count) {

	int i;
	
	if (__io_list == NULL) {
		return NULL_POINTER;
	}
	
	io_count = __count;
	
	if (io_count > 0) {
		
		if ((threads = (pthread_t *)malloc(io_count * sizeof(pthread_t)))==NULL
			|| (start_exe_queue = (int *)malloc(io_count * sizeof(int)))==NULL) {
			return MEMORY_ERROR;
		}
	}
	else {
		return ILLEGAL_AMOUNT;
	}
	
	continue_exe = TRUE;
	for (i=0; i<io_count; i++) {
		start_exe_queue[i] = FALSE;
		__io_list[i].id = i;
	}
	sem_init(&all_threads_ready, 0, 0);
	sem_init(&all_threads_finished, 0, 0);
	sem_init(&main_thread_ready, 0, 0);
	
	for (i=0; i<io_count; i++) {
		if (pthread_create(&threads[i], NULL, &perform_precise_concurrent_io_2g, (void *)&(__io_list[i]))!=0) {
			return PTHREAD_ERROR;
		}
	}
	
	return SUCCESS;
}

void io_container_run(void) {
	
	int i;
	
	start_exe = FALSE;
	
	for (i=0; i<io_count; i++) {
		sem_post(&main_thread_ready);
	}
	
	for (i=0; i<io_count; i++) {
		sem_wait(&all_threads_ready);
	}
	
	start_exe = TRUE;
	
	for (i=0; i<io_count; i++) {
		sem_wait(&all_threads_finished);
	}
}

void precise_io_container_run(const struct timespec *__ts) {

	int i;
	
	for (i=0; i<io_count; i++) {
		start_exe_queue[i] = FALSE;
	}
	
	for (i=0; i<io_count; i++) {
		sem_post(&main_thread_ready);
	}
	
	for (i=0; i<io_count; i++) {
		sem_wait(&all_threads_ready);
	}
	
	for (i=0; i<io_count; i++) {
		start_exe_queue[i] = TRUE;
		if (__ts != NULL) {
			// nanosleep(__ts, NULL);
			precise_nsleep(__ts);
		}
	}
	
	for (i=0; i<io_count; i++) {
		sem_wait(&all_threads_finished);
	}
}

void io_container_destroy(void) {
	
	int i;
	
	continue_exe = FALSE;
	
	for (i=0; i<io_count; i++) {
		sem_post(&main_thread_ready);
	}
	
	for (i=0; i<io_count; i++) {
		pthread_join(threads[i], NULL);
	}
	
	sem_destroy(&all_threads_ready);
	sem_destroy(&all_threads_finished);
	sem_destroy(&main_thread_ready);
}

void precise_io_container_destroy(void) {

	int i;
	
	continue_exe = FALSE;
	
	for (i=0; i<io_count; i++) {
		sem_post(&main_thread_ready);
	}
	
	for (i=0; i<io_count; i++) {
		pthread_join(threads[i], NULL);
	}
	
	sem_destroy(&all_threads_ready);
	sem_destroy(&all_threads_finished);
	sem_destroy(&main_thread_ready);

	free(start_exe_queue);
}

int get_device_type(const char* __dev) {

	const char * pre__[] = {"/dev/nvme", "/dev/sd"};
	int dev_type__[] = {DEVICE_NVME, DEVICE_SATA};
	int count__ = 2;

	for (int i=0; i<count__; i++) {
		if (strncmp(__dev, pre__[i], strlen(pre__[i])) == 0) {
			return dev_type__[i];
		}
	}

	return DEVICE_UNKNOWN;
}

long nvme_flush(const char * __dev, int __enable, const struct timespec * __time) {
	
	static char flush_cmd[256];
	
	if (__dev != NULL) {
		const char * tmp_path__ = "flush.tmp";
		
		sprintf(flush_cmd, "nvme flush %s > %s", __dev, tmp_path__);
		
		return 0;
	}
	else if (__enable) {
		struct timespec t_start, t_end;
		
		clock_gettime(CLOCK_REALTIME, &t_start);
		system(flush_cmd);
		clock_gettime(CLOCK_REALTIME, &t_end);
		
		my_sleep(__time);
		
		return (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);
	}
	else {
		return 0;
	}
}

long device_flush(const char * __dev, int __enable, const struct timespec * __time) {

	static char flush_cmd[256];

	if (__dev != NULL) {

		const char * tmp_path__ = "flush.tmp";
		int type__;
		
		if ((type__=get_device_type(__dev)) == DEVICE_NVME) {

			sprintf(flush_cmd, "nvme flush %s > %s", __dev, tmp_path__);
		}
		else if (type__ == DEVICE_SATA) {

			sprintf(flush_cmd, "hdparm -F %s > %s", __dev, tmp_path__);
		}
		else {
			return DEVICE_ERROR;
		}
		
		return SUCCESS;
	}
	else if (__enable) {

		struct timespec t_start, t_end;
		
		clock_gettime(CLOCK_REALTIME, &t_start);
		system(flush_cmd);
		clock_gettime(CLOCK_REALTIME, &t_end);
		
		my_sleep(__time);
		
		return (t_end.tv_sec-t_start.tv_sec) * (long)ONE_SEC + (t_end.tv_nsec-t_start.tv_nsec);

	}
	else {
		return 0;
	}
}

int get_page_size(const char* __model, char * __size_str, int __index) {

	const char* config_path__ = "pageSize.config";
	char line__[256], * token;
	const char* delim__ = ",\n\r";
	long size__;

	FILE * page_file__;

	if ((page_file__=fopen(config_path__, "r"))==NULL) {
		// args[__index].status = PAGE_SIZE_DEFAULT;
		return PAGE_SIZE_DEFAULT;
	}

	while (fgets(line__, 256, page_file__) != NULL) {

		line__[strlen(line__)-1] = '\0';

		token = strtok(line__, delim__);
		if (strcmp(token, __model) == 0) {

			token = strtok(NULL, delim__);

			strcpy(__size_str, token);
			fclose(page_file__);

			size__ = size_flatten(token);

			if (size__ >= SUCCESS) {

				// args[__index].status = SUCCESS;
				args[__index].v_size = size__;
				args[__index].v_str = __size_str;

				return SUCCESS;
			}
			else {
				// args[__index].status = size__;
				return size__;
			}
		}
		else {
			continue;
		}
	}

	fclose(page_file__);
	// args[__index].status = PAGE_SIZE_DEFAULT;
	return PAGE_SIZE_DEFAULT;
}

int get_other_stat(const char* __dev, int __fd, char* __name, long* __cap) {

	// strcpy(__name, __dev);
	// copy device path as the model name, filtering '/'
	__name[0] = '\0';
	for (int i=0, j=0; __dev[i] != '\0'; i++) {
		if (__dev[i] != '/') {
			__name[j] = __dev[i];
			j++;
		}
		__name[j] = '\0';
	}

	if (ioctl(__fd, BLKGETSIZE64, __cap) == -1) {
		if ((*__cap = lseek(__fd, 0, SEEK_END)) == -1) {
			return DEVICE_ERROR;
		}
	}

	return SUCCESS;
}

int get_sata_stat(const char* __dev, int __fd, char* __name, long* __cap) {

	const char * cmd__ = "lsscsi > sataSpec.out", * spec_path__ = "sataSpec.out";
	char buff__[256], * token;
	char * str_list__[64];
	const char * delim__ = " \t\r\n\a";
	int len__;
	const int start_pos__ = 3;

	FILE * spec_file__;

	if (system(cmd__) == -1) {
		return FAILED;
	}

	if (ioctl(__fd, BLKGETSIZE64, __cap) == -1) {
		return DEVICE_ERROR;
	}

	if ((spec_file__=fopen(spec_path__, "r")) == NULL) {
		return FILE_HANDLING_ERROR;
	}

	while (fgets(buff__, 256, spec_file__) != NULL) {

		len__ = 0;
		token = strtok(buff__, delim__);
		while (token != NULL) {
			str_list__[len__] = token;
			len__++;
			token = strtok(NULL, delim__);
		}

		if (len__ > start_pos__) {

			if (strcmp(__dev, str_list__[len__-1]) == 0) {

				strcpy(__name, str_list__[start_pos__]);

				for (int i=start_pos__+1; i<len__-2; i++) {
					sprintf(__name, "%s %s", __name, str_list__[i]);
				}

				fclose(spec_file__);
				remove(spec_path__);
				return SUCCESS;
			}
		}
	}

	fclose(spec_file__);
	remove(spec_path__);
	return FAILED;
}

int get_sata_path(char* __name) {

	const char * cmd__ = "lsscsi > sataSpec.out", * spec_path__ = "sataSpec.out";
	char buff__[256], * token, comp__[256];
	char * str_list__[64];
	const char * delim__ = " \t\r\n\a";
	int len__;
	const int start_pos__ = 3, path_len__ = 8;

	FILE * spec_file__;

	if (strlen(__name) < path_len__) {
		return MEMORY_ERROR;
	}

	if (system(cmd__) == -1) {
		return FAILED;
	}

	if ((spec_file__=fopen(spec_path__, "r")) == NULL) {
		return FILE_HANDLING_ERROR;
	}

	while (fgets(buff__, 256, spec_file__) != NULL) {

		len__ = 0;
		token = strtok(buff__, delim__);
		while (token != NULL) {
			str_list__[len__] = token;
			len__++;
			token = strtok(NULL, delim__);
		}

		if (len__ > start_pos__) {

			strcpy(comp__, str_list__[start_pos__]);

			for (int i=start_pos__+1; i<len__-2; i++) {
				sprintf(comp__, "%s %s", comp__, str_list__[i]);
			}

			if (strcmp(__name, comp__) == 0) {

				strcpy(__name, str_list__[len__-1]);

				fclose(spec_file__);
				remove(spec_path__);
				return SUCCESS;
			}
		}
	}

	fclose(spec_file__);
	remove(spec_path__);
	return FAILED;
}

int get_nvme_stat(const char* __dev, char* __name, long * __cap) {

	char * py_path__ = "getDevName.py", * tmp_path__ = "tmp.out";
	char cmd__[256];
	FILE * tmp_file__;

	if (access(py_path__, F_OK) != -1) {

		sprintf(cmd__, "python3 %s %s > %s", py_path__, __dev, tmp_path__);
		if (system(cmd__) == -1) {
			return FAILED;
		}
		
		if ((tmp_file__ = fopen(tmp_path__, "r")) != NULL) {
			
			if (fgets(__name, 256, tmp_file__) != NULL &&
				fgets(cmd__, 256, tmp_file__) != NULL) {
				
				__name[strlen(__name)-1] = '\0';
				*__cap = strtol(cmd__, NULL, 10);
				
				fclose(tmp_file__);
				remove(tmp_path__);
				
				if (*__cap <= 0) {
					return ILLEGAL_AMOUNT;
				}
			}
			else {
				fclose(tmp_file__);
				remove(tmp_path__);
				return FILE_HANDLING_ERROR;
			}
		}
		else {
			return FILE_HANDLING_ERROR;
		}
	}
	else {
		return FILE_HANDLING_ERROR;
	}
	
	return SUCCESS;
}

int get_nvme_path(char* __model) {

	char * py_path__ = "getNVMePath.py", * tmp_path__ = "path.out";
	char cmd__[256];
	FILE * tmp_file__;
	int path_len__ = 12;

	if (strlen(__model) < path_len__) {
		return MEMORY_ERROR;
	}

	if (access(py_path__, F_OK) != -1) {

		sprintf(cmd__, "python3 %s \'%s\' > %s", py_path__, __model, tmp_path__);
		if (system(cmd__) == -1) {
			return FAILED;
		}
		
		if ((tmp_file__ = fopen(tmp_path__, "r")) != NULL) {

			if (fgets(__model, 256, tmp_file__) != NULL) {
				
				__model[strlen(__model)-1] = '\0';
				
				fclose(tmp_file__);
				remove(tmp_path__);

				return SUCCESS;
			}
			else {
				fclose(tmp_file__);
				remove(tmp_path__);
				return FILE_HANDLING_ERROR;
			}
		}
		else {
			return FILE_HANDLING_ERROR;
		}
	}
	else {
		return FILE_HANDLING_ERROR;
	}
}

long get_total_data_written(long __min_size, long __max_size, long __inc_size, long __iter) {

	long total__;

	total__ = 0;
	while (__min_size <= __max_size) {

		total__ += __min_size * __iter;
		__min_size += __inc_size;
	}

	return total__;
}

long get_base_offset(const char *__dev) {

	const char *config_path__ = CONFIG_BASE_OFFSET, *delim__ = ",\n\r";
	char line__[256], *token;

	FILE *config_file__;

	if ((config_file__=fopen(config_path__, "r")) == NULL) {
		return BASE_OFFSET_DEFAULT;
	}

	while (fgets(line__, 256, config_file__) != NULL) {

		line__[strlen(line__)-1] = '\0';

		token = strtok(line__, delim__);
		if (strcmp(token, __dev) == 0) {

			token = strtok(NULL, delim__);
			fclose(config_file__);

			return strtol(token, NULL, 10);
		}
		else {
			continue;
		}
	}

	fclose(config_file__);
	return BASE_OFFSET_DEFAULT;
}

int save_base_offset(const char *__dev, long __offset) {

	const char *config_path__ = CONFIG_BASE_OFFSET, *delim__ = ",\n\r";
	char tmp_path__[256], line__[256], to_split__[256], *token;

	FILE *tmp_file__, *config_file__;

	sprintf(tmp_path__, "%s.tmp", config_path__);
	if (rename(config_path__, tmp_path__) != 0) {
		return FILE_HANDLING_ERROR;
	}

	if ((tmp_file__=fopen(tmp_path__, "r")) == NULL || (config_file__=fopen(config_path__, "w+")) == NULL) {
		return FILE_HANDLING_ERROR;
	}

	while (fgets(line__, 256, tmp_file__) != NULL) {

		line__[strlen(line__)-1] = '\0';
		strcpy(to_split__, line__);

		token = strtok(to_split__, delim__);
		if (strcmp(token, __dev) == 0) {

			sprintf(line__, "%s,%ld", __dev, __offset);
		}

		fprintf(config_file__, "%s\n", line__);
	}

	fclose(tmp_file__);
	fclose(config_file__);
	if (remove(tmp_path__) != 0) {
		return FILE_HANDLING_ERROR;
	}
	else {
		return SUCCESS;
	}
}

int open_device(const char * __dev, int __oflag, char * __name, long * __cap) {
	
	int fd__;
	
	if (__dev == NULL) {
		return NULL_POINTER;
	}
	
	fd__ = open(__dev, __oflag);
	
	if (fd__ < 0) {
		return DEVICE_ERROR;
	}
	else {
		
		int type__, res__;

		if ((type__=get_device_type(__dev)) == DEVICE_NVME) {

			if ((res__=get_nvme_stat(__dev, __name, __cap)) == SUCCESS) {
				return fd__;
			}
			else {
				close(fd__);
				return res__;
			}
		}
		else if (type__ == DEVICE_SATA) {

			if ((res__=get_sata_stat(__dev, fd__, __name, __cap)) == SUCCESS) {
				return fd__;
			}
			else {
				close(fd__);
				return res__;
			}
		}
		else {

			if ((res__=get_other_stat(__dev, fd__, __name, __cap)) == SUCCESS) {
				return fd__;
			}
			else {
				close(fd__);
				return DEVICE_ERROR;
			}
		}
	}
}

void close_device(int __fd) {
	close(__fd);
}

double flatten_unsigned_combined(const char * __str, Trans_Unit * __ref, int __count) {
	
	const char * delim__ = "-";
	char * cpy_str__, * token__, unit__;
	double tot__;
	int which__;
	
	if (__str == NULL || strlen(__str) <= 0) {
		return 0.0;
	}
	else { 
		if ((cpy_str__ = (char *)malloc((strlen(__str)+1)*sizeof(char)))==NULL) {
			return MEMORY_ERROR;
		}
		else {
			strcpy(cpy_str__, __str);
			
			tot__ = 0.0;
			token__ = strtok(cpy_str__, delim__);
			while (token__ != NULL) {
				
				which__ = 0;
				unit__ = token__[strlen(token__)-1];
				for (int i = 0; i < __count; i++) {
					if (unit__ == __ref[i].symbol) {
						which__ = i;
						break;
					}
				}

				tot__ += strtod(token__, NULL) * __ref[which__].units;
				token__ = strtok(NULL, delim__);
			}

			return tot__;
		}
	}
}

long size_flatten(const char * __sstr) {
	
	Trans_Unit trans__[] = {{BYTE_CHAR,ONE_BYTE},{KB_CHAR,ONE_KiB},{MB_CHAR,ONE_MiB},{GB_CHAR,ONE_GiB},{TB_CHAR,ONE_TiB}};
	long size__;
	
	if ((size__=flatten_unsigned_combined(__sstr, trans__, 5))<=FAILED) {
		return ILLEGAL_SIZE_FORMAT;
	}
	else {
		return size__;
	}
}

int time_flatten(struct timespec * __time, const char * __tstr) {
	
	Trans_Unit trans__[] = {{NS_CHAR,ONE_NANO},{US_CHAR,ONE_MICRO},{MS_CHAR,ONE_MILLI},{SEC_CHAR,ONE_SEC},
							{MIN_CHAR, ONE_MIN}, {HOUR_CHAR, ONE_HOUR}};
	long time__;
	
	if ((time__=flatten_unsigned_combined(__tstr, trans__, 6))<=FAILED) {
		return ILLEGAL_TIME_FORMAT;
	}
	else {
		__time->tv_sec = time__ / ((long)ONE_SEC);
		__time->tv_nsec = time__ - (long)ONE_SEC * __time->tv_sec;
		return SUCCESS;
	}
}

long int_flatten(const char * __istr) {
	
	Trans_Unit trans__[] = {{ONE_CHAR,ONE_ONE},{THOUSAND_CHAR,ONE_THOUSAND},{MILLION_CHAR,ONE_MILLION},{BILLION_CHAR,ONE_BILLION}};
	long int__;
	
	if ((int__=flatten_unsigned_combined(__istr, trans__, 4))<=FAILED) {
		return ILLEGAL_INT_FORMAT;
	}
	else {
		return int__;
	}
}

int my_sleep(const struct timespec * __time) {
	
	if (__time == NULL) {
		return NULL_POINTER;
	}
	else {
		if ((__time->tv_sec == 0) && (__time->tv_nsec < ONE_MILLI)) {
			precise_nsleep(__time);
		}
		else {
			nanosleep(__time, NULL);
		}
		return TRUE;
		// if (nanosleep(__time, NULL)==0) {
		// 	return SUCCESS;
		// }
		// else {
		// 	return SLEEP_ERROR;
		// }
		// precise_nsleep(__time);
	}
}

/*
	incluing low bound, excluding high bound
 */
long rand_aligned_generator(long __low, long __high, long __ali) {
	
	static char inited__ = FALSE;
	
	if (!inited__) {
		srandom(time(NULL));
		inited__ = TRUE;
	}
	
	long max__ = (__high - __low) / __ali;
	
	return (random() % max__) * __ali + __low;
}

void print_err(int __code) {
	
	fprintf(stderr, "my_errno: %d; errno: %d\n", __code, errno);

	__code = 0 - __code - 1;
	
	if (__code >= 0 && __code <= (TYPES_ERROR-1)) {
		write(STDERR_FILENO, err_messages[__code], strlen(err_messages[__code]));
	}
	else {
		write(STDERR_FILENO, undefined_err_message, strlen(undefined_err_message));
	}
}

double get_current_time(void) {
	struct timespec t__;
	
	clock_gettime(CLOCK_REALTIME, &t__);
	
	return (double)t__.tv_sec + ((double)t__.tv_nsec / (double)ONE_SEC);
}

char to_upper(char __ch) {
	
	if (__ch >= 'a' && __ch <= 'z') {
		return __ch + 'A' - 'a';
	}
	else {
		return __ch;
	}
}

char * str_upper(char * __str) {
	
	char * p = __str;
	
	for ( ; *__str; ++__str) *__str = to_upper(*__str);
	
	return p;
}

int contains_char(const char * __str, char __ch) {
	if (__str == NULL) {
		return FALSE;
	}
	else {
		if (strchr(__str, __ch) != NULL) {
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
}

int option_check(int __index, int __check) {
	
	if ((options[__index].group & __check) == 0) {
		return FALSE;
	}
	else {
		return TRUE;
	}
}

int option_get_parameter(int __index) {

	return options[__index].group & AO_MASK_PARAMETER;
}

double get_min_t_start(const IO_Spec * __list, int __count) {

	double min__ = __list[0].t_start;
	for (int i=0; i<__count; i++) {
		if (min__>__list[i].t_start) {
			min__ = __list[i].t_start;
		}
	}

	return min__;
}

// including low, excluding high
int latency_check(const IO_Spec * __list, int __count, long __low, long __high) {

	for (int i=0; i<__count; i++) {
		if (__list[i].latency < __low || __list[i].latency >= __high) {
			return FALSE;
		}
	}

	return TRUE;
}

void latency_sort(IO_Spec * __list, int __count) {

	long tmp;

	for (int i=0; i<__count; i++) {
		for (int j=__count-1; j>i; j--) {
			if (__list[j].latency < __list[j-1].latency) {
				tmp = __list[j].latency;
				__list[j].latency = __list[j-1].latency;
				__list[j-1].latency = tmp;
			}
		}
	}
}

void timestamp_init(struct timespec *__ts) {

	struct timespec init_ts__;
	long nsec__;

	clock_gettime(CLOCK_REALTIME, &init_ts__);
	nsec__ = __ts->tv_nsec + init_ts__.tv_nsec;

	__ts->tv_nsec = nsec__ % ((long)ONE_SEC);
	__ts->tv_sec += (nsec__ / ((long)ONE_SEC)) + init_ts__.tv_sec;
}

int timestamp_check(const struct timespec *__ts) {

	struct timespec cur_ts__;

	clock_gettime(CLOCK_REALTIME, &cur_ts__);

	if (cur_ts__.tv_sec < __ts->tv_sec) {
		return TRUE;
	}
	else if (cur_ts__.tv_sec == __ts->tv_sec && cur_ts__.tv_nsec < __ts->tv_nsec) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

long timestamp_remaining(const struct timespec *__ts) {

	struct timespec cur_ts__;

	clock_gettime(CLOCK_REALTIME, &cur_ts__);

	return __ts->tv_sec-cur_ts__.tv_sec;
}

void precise_nsleep(const struct timespec *__ts) {

	struct timespec against_ts__;

	against_ts__.tv_sec = __ts->tv_sec;
	against_ts__.tv_nsec = __ts->tv_nsec;

	timestamp_init(&against_ts__);

	while (timestamp_check(&against_ts__) == TRUE);

	return;
}