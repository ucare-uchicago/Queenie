
## Erasing an SSD with the secure\_erase command

SSDs can be fully erased via the secure\_erase command. We provide a convenient wrapper program `secure_erase.sh` to issue the erase command for SATA, SCSI, and NVMe SSD drives. 

prerequisites: `hdparm`; `sg3_utils`; `nvme-cli` ; root privilege is likely needed as well

Usage: `bash secure_erase.sh [device]` 

Example: `bash secure_erase.sh /dev/sdb` 

After successfully erasing an SSD drive, one can use `hexdump` to check the content of the drive (e.g., all 0s).

***WARNING: issuing secure\_erase to an SSD wipes out all data; please proceed with caution***

## Prepare an empty SSD drive

All read-only experiments in Queenie require the target SSD drive to be fully erased and then fully, sequentially written. One can use `fio` to issue a fully sequential write workload. An example config file is provided `seqw_nvme0.fio`, and one can use the command `fio --name=seqw seqw_nvme0.fio` 

***WARNING: erasing and fully writing an SSD drive decrease its lifetime; please plan your probing experiments wisely***
