# Probing data from Kelpie

This directory contains data of some probing experiments from Kelpie. *Disclaimer: the data can only be used for pure academic purposes.*

To obtain the data, please fill out this [Google form](https://forms.gle/2G2H2ah39RQugR2A8). A link will be sent to you for download after you submit the form.

The file names follow the format: **\[experiment\]\_\[drive label\]\_\[parameters\]**. The first field is the name of the probing experiment. The second is the anonymized label of the probed SSD drive. Start from the third field are the parameters specific to the experiment and the drive. 

Below explains the data format for each experiment in more details

### pushSec and pushPage

These two are results generated from the F1\_pushRead probing function for probing P1 page size and P3 chunk size. F1\_pushRead assumes a default sector size of 512 bytes
<!-- The names of the data files follow the format: **\[pushSec|pushPage\]\_\[drive\]** -->

- `size(B)` - The size of the read in bytes. 
- `offset(B)` - This refers to the alignment for the randomly generated offsets. The default is 256 KB, i.e., the offset for each read is generated randomly in multiples of 256 KBs.
- `additional offset(B)` This is the additional offset a described in the pseudo-code. 
- `iteration` The number of total reads issued (note: each read uses a random offset).
- `time elapsed(s)` Time elapsed between the start of the probing experiment and the completion of the current read batch. 
- `read latency(ns)` The average read latency over `iteration` number of reads. 

*What to look for: periodic latency spikes*

### rangeRead

This corresponds to results generated from the F2\_rangeRead probing function for P2 page type. The names of the data files contain parameters `from` and `to`, which indicate the range. 

- `size(B)` - The size of the read in bytes, which defaults to the page size. 
- `offset(B)` - The exact offset of the read. 
- `iteration` The number of total reads issued.
- `time elapsed(s)` Time elapsed between the start of the probing experiment and the completion of the current read batch. 
- `read latency(ns)` The average read latency over `iteration` number of reads. 

*What to look for: groupings of different latency levels*

### strideRead 

This is the experiment probing P4 stripe width and P5 channel/chip layout using function F3\_strideRead. The file names contain the parameter `job` referring to the number of concurrent reads. We call reads issued concurrently a batch. Reads in a batch has the same `stride`. The latency of each read in a batch is a separate entry in the data files. Reads in a batch are also sorted and grouped in the data files. 

- `size(B)` - The size of the read in bytes, which defaults to the page size.   
- `offset(B)` - This refers to the alignment for the randomly generated offsets. The default is the page size.
- `stride` - The stride `St` in the unit of pages, as described in the pseudo-code.
- `iteration` The number of total read batches issued. The total number of reads equals `iteration`\*`job`
- `time elapsed(s)` Time elapsed between the start of the probing experiment and the completion of the current read batch.
- `read latency(ns)` The average read latency over `iteration` number of batches. Each batch is sorted before averaging. 

*What to look for: multiple levels of periodic latency spikes*

### incRead

This experiment probes P6 read performance consistency with function F4\_incRead. 

- `size(B)` - The size of the read in bytes. 
- `offset(B)` - This refers to the alignment for the randomly generated offsets. The default is 256 KB, i.e., the offset for each read is generated randomly in multiples of 256 KBs.
- `iteration` The number of total reads issued (note: each read uses a random offset).
- `time elapsed(s)` Time elapsed between the start of the probing experiment and the completion of the current read batch. 
- `read latency(ns)` The average read latency over `iteration` number of reads.

*What to look for: larger reads are even faster?*

### reRead 

P7 read buffer capacity can be extracted from this experiment with F5\_reRead. 

- `size(B)` - The size of the first read in bytes. 
- `page size(B)` - The page size of the SSD, which is also the size of the follow-up read. 
- `offset(B)` - This refers to the alignment for the randomly generated offsets. The default is 256 KB, i.e., the offset for each read is generated randomly in multiples of 256 KBs.
- `iteration` The number of total reads issued (note: each read uses a random offset).
- `time elapsed(s)` Time elapsed between the start of the probing experiment and the completion of the current read batch. 
- `read latency(ns)` The average latency of the follow-up page read  over `iteration` number of reads.

*What to look for: some follow-up reads are cached*

### SeqW, conSeqW, and SeqW\_sleep

`SeqW` and `conSeqW` are results generated from function F6\_conSeqW for probing P8 write buffer capacity and P9 write parallelism. The main difference is that `SeqW` only issues one write at a time, while `conSeqW` issues concurrent writes in batches. The format of `conSeqW` files are very similar to those of `strideRead` except that the latencies of `conSeqW` are not sorted or averaged. 

`SeqW_sleep` uncovers P10 internal flush window using F7\_seqwSleep. `SeqW_sleep` is basically the same as `SeqW` except that a short period of sleep is injected between writes. The `is` parameter in the file names is the length of the injected sleep, where `s` as in seconds, `m` as in milliseconds, and `u` as in microseconds. 

- `size(B)` The size of the write in bytes.
- `offset(B)` The exact offset of the write. 
- `iteration` This is simply a label for each individual write. It does not mean the number of total writes. 
- `flush latency(ns)` If a flush command is issued before this write, its latency is recorded here. Otherwise 0.
- `time elapsed(s)` Time elapsed between the start of the probing experiment and the completion of the current write. 
- `write latency(ns)` The latency of the write. 

*What to look for: periodic latency spikes*

