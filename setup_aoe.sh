#!/bin/bash
CMDLN_ARGS="$@" # Command line arguments for this script
export CMDLN_ARGS

# Run this script as root if not already.
chk_root () {

  if [ ! $( id -u ) -eq 0 ]; then
	echo "Please run as root"
	exit 0
#    echo "Please enter root's password."
#    exec su -c "${0} ${CMDLN_ARGS}" # Call this prog as root
#    exit ${?}  # sice we're 'execing' above, we wont reach this exit
               # unless something goes wrong.
  fi

}

chk_root

[ "$#" -lt 2 ] && echo "Usage: bash setup (number of disks) (size of one disk MB)" && exit 0
fusermount -u mountdir
rm -r rootdir
rm -r mountdir
mkdir rootdir
mkdir mountdir
rm raid_setting
echo "disk_total_num=$1" 	> raid_setting
disk1=$(($1 - 1))
echo "data_disk_num=$disk1"          >> raid_setting
echo "disk_block_size=4096"	>>raid_setting
echo "disk_raid_type=5"		>>raid_setting
echo "space_list_num=0"		>>raid_setting
echo "no_cache=0"		>>raid_setting
echo "no_gui=0"			>>raid_setting
echo "run_experiment=0"		>>raid_setting
rm raid_health
touch raid_health
for((i=0;i<$1;i=i+1))
do
blockdev=$(($i + 1))
echo "$i.dev_name=/dev/etherd/e21.$blockdev"	>>raid_setting
echo "$i.free_offset=0"					>>raid_setting
size=$((1048576 * $2))
echo "$i.free_size=$size"				>>raid_setting
echo "0"								>>raid_health
done
rm raid_metadata
touch raid_metadata
for((i=1;i<$1+1;i=i+1))
do
        dd if=/dev/zero of=/dev/etherd/e21.$i bs=1M count=$2
done

