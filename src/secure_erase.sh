#!/bin/bash
# secure_erase

dev_path=$1
sata_prefix='/dev/sd'
nvme_prefix='/dev/nvme'

BLUE='\033[1;34m'
RED='\033[0;31m'
NC='\033[0m'

echo 'Secure erase script by Nanqinqin Li'
echo 'This script automatically detects the type(SATA/SAS/NVMe) of the given device and applies appropriate cmds to issue an secure_erase'
echo ''

if [ ${#dev_path} == 0 ]; then
	echo -e "${RED}Error${NC}: please specify a device"
	echo 'Usage: bash secure_erase.sh [device]'
	exit 1
fi

echo -e "Device to be erase: ${BLUE}${dev_path}${NC}"
echo 'Before erase, please check that you are running this script under root. For SATA drives, please make sure that the drive is not frozen'
echo 'Please also ensure that the following packages have been installed: hdparm; sg3_utils; nvme-cli'
echo ''
read -p "Do you wish to proceed? [Y/n]" yn
case $yn in
	[Yy]* ) echo 'secure_erase process started';;
	* ) echo 'secure_erase process aborted'; exit 1;;
esac

if [ ${dev_path:0:7} == $sata_prefix ]; then
	
	id="$('lsscsi' | grep $dev_path | grep -o -P '(?<=\[).*(?=\])')"
	ls_output="$(ls /sys/class/scsi_disk/$id/device/ | grep sas)"
	
	if [ ${#ls_output} == 0 ]; then
		
		echo -e "${BLUE}${dev_path}${NC} is a ${BLUE}SATA${NC} device, invoking ${BLUE}hdparm${NC} command to issue secure_erase"
		hdparm --user-master u --security-set-pass p ${dev_path}
		enhance="$('hdparm' -I ${dev_path} | grep 'enhanced erase' | grep 'not')"
		
		if [ ${#enhance} == 0 ]; then
			echo -e "${BLUE}${dev_path}${NC}: enhanced erase supported"
			hdparm --user-master u --security-erase-enhanced p ${dev_path}
		else
			echo -e "${BLUE}${dev_path}${NC}: enhanced erase not supported"
			hdparm --user-master u --security-erase p ${dev_path}
		fi
	else
		echo -e "${BLUE}${dev_path}${NC} is a ${BLUE}SAS${NC} device, invoking ${BLUE}sg_format${NC} command to issue secure_erase"
		sg_format --format ${dev_path}
	fi
elif [ ${dev_path:0:9} == $nvme_prefix ]; then
	
	echo -e "${BLUE}${dev_path}${NC} is a ${BLUE}NVMe${NC} device, invoking ${BLUE}nvme format${NC} command to issue secure_erase"
	support="$('nvme' id-ctrl -H ${dev_path} | grep 'Format NVM Supported')"

	if [ ${#support} == 0 ]; then
		echo -e "${BLUE}${dev_path}${NC}: Secure Erase not Supported, aborting..."
		exit 1
	else
		echo -e "${BLUE}${dev_path}${NC}: Secure Erase Supported"
		nvme format ${dev_path} --ses=1
	fi
else
	echo -e "${RED}Error${NC}: Unknown device type"
	exit 1
fi

echo ''
echo "secure_erase finished. Please run 'sudo hexdump ${dev_path}' to check device status"
echo 'If errors prompted, please check the problems and try again'

