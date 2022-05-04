
#ifndef IOUTIL_H_
#define IOUTIL_H_

#include <argp.h>
#include <stdio.h>
#include <time.h>

#define TRUE 1
#define FALSE 0

#define SUCCESS 0
#define FAILED (-1)
#define PREAD_PWRITE_ERROR (-2)
#define ILLEGAL_AMOUNT (-3)
#define NULL_POINTER (-4)
#define MEMORY_ERROR (-5)
#define PTHREAD_ERROR (-6)
#define DEVICE_ERROR (-7)
#define FILE_HANDLING_ERROR (-8)
#define ILLEGAL_SIZE_FORMAT (-9)
#define OPTION_DEFAULT (-10)
#define ILLEGAL_BOOL_FORMAT (-11)
#define ILLEGAL_TIME_FORMAT (-12)
#define ILLEGAL_INT_FORMAT (-13)
#define SLEEP_ERROR (-14)
#define UNSUPPORTED_ARG_OPTION (-15)
#define UNMATCHED_MODEL (-16)
#define PAGE_SIZE_DEFAULT (-17)
#define BASE_OFFSET_DEFAULT (-18)
#define DATA_OVERFLOW (-19)
#define TYPES_ERROR 19

#define DEVICE_NVME 1
#define DEVICE_SATA 2
#define DEVICE_UNKNOWN (-1)

#define BYTE_CHAR 'B'
#define KB_CHAR 'K'
#define MB_CHAR 'M'
#define GB_CHAR 'G'
#define TB_CHAR 'T'

#define ONE_BYTE 1
#define ONE_KiB 1024
#define ONE_MiB 1048576
#define ONE_GiB 1073741824
#define ONE_TiB 1099511627776L

#define ONE_KB 1e3
#define ONE_MB 1e6
#define ONE_GB 1e9
#define ONE_TB 1e12L

#define SIZE_2G 2147483647

#define HOUR_CHAR 'H'
#define MIN_CHAR 'M'
#define SEC_CHAR 's'
#define MS_CHAR 'm'
#define US_CHAR 'u'
#define NS_CHAR 'n'

#define ONE_HOUR ((long)3600e9)
#define ONE_MIN ((long)60e9)
#define ONE_SEC 1e9
#define ONE_MILLI 1e6
#define ONE_MICRO 1e3
#define ONE_NANO 1

#define ONE_CHAR 'O'
#define THOUSAND_CHAR 'K'
#define MILLION_CHAR 'M'
#define BILLION_CHAR 'B'

#define ONE_ONE 1
#define ONE_THOUSAND 1e3
#define ONE_MILLION 1e6
#define ONE_BILLION 1e9

#define IO_WRITE 1
#define IO_READ 0

#define ARG_INT "int"
#define ARG_SIZE "size"
#define ARG_BOOL "bool"
#define ARG_STR "string"
#define ARG_FLOAT "float"
#define ARG_TIME "time"

#define AO_NULL 0x0
#define AO_MANDATORY 0x100
#define AO_APPEAR 0x200			// conditionally display in log file name
#define AO_DISPLAY 0x1000		// display in log file name
#define AO_MULTIPLE 0x400		// only support multiple ARG_STR
#define AO_ALTERNATE 0x800		// only support multiple ARG_STR
#define AO_MASK_PARAMETER 0xff

#define CONFIG_BASE_OFFSET "baseOffset.config"

typedef struct io_spec {
	/* input */
	int id;
	char type;
	int fd;
	long offset;
	long size;
	void * buff;
	
	/* output */
	int status;
	long latency;
	double t_start;
} IO_Spec;

typedef struct arg_spec {
	char status;
	
	long v_int;
	long v_size;
	char v_bool;
	char * v_str;
	double v_float;
	struct timespec v_time;
	
	int d_multi;
	void * v_multi;
} Arg_Spec;

long perform_io(IO_Spec * __io);
long perform_io_2g(IO_Spec * __io);
int io_container_create(IO_Spec * __io_list, int __count);
void io_container_run(void);
void io_container_destroy(void);
int precise_io_container_create(IO_Spec * __io_list, int __count);
void precise_io_container_run(const struct timespec *__ts);
void precise_io_container_destroy(void);

//long nvme_flush(const char * __dev, int __enable, const struct timespec * __time);
long device_flush(const char * __dev, int __enable, const struct timespec * __time);

int open_device(const char * __dev, int __oflag, char * __name, long * __cap);
void close_device(int __fd);

int get_page_size(const char* __model, char * __size_str, int __index);
long get_total_data_written(long __min_size, long __max_size, long __inc_size, long __iter);
long get_base_offset(const char *__dev);
int save_base_offset(const char *__dev, long __offset);

FILE * open_log_file(char * __path, const char * __log, const char * __pre, const char * __dev, long __cap);
int close_log_file(FILE * __file);

long rand_aligned_generator(long __low, long __high, long __ali);

void print_err(int __code);

int args_parser(int argc, char **argv, struct argp_option __opts[]);

double get_current_time(void);
double get_min_t_start(const IO_Spec * __list, int __count);
int my_sleep(const struct timespec * __time);
char * str_upper(char * __str);
int contains_char(const char * __str, char __ch);

int latency_check(const IO_Spec * __list, int __count, long __low, long __high);
void latency_sort(IO_Spec * __list, int __count);

void timestamp_init(struct timespec *__ts);
int timestamp_check(const struct timespec *__ts);
long timestamp_remaining(const struct timespec *__ts);
void precise_nsleep(const struct timespec *__ts);

#endif // IOUTIL_H_