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

[ "$#" -lt 3 ] && echo "Usage: bash setup (number of storage nodes) (size of one storage node MB) (etherd num)" && exit 0

#Umount fuse filesystem
fusermount -u mountdir

#Reset rootdir and mountdir
rm -r rootdir
rm -r mountdir
mkdir rootdir
mkdir mountdir

# Initialize all storage nodes
for((i=1;i<$1+1;i=i+1))
do
        dd if=/dev/zero of=/dev/etherd/e$3.$i bs=1M count=$2
done

# Remove all testfiles
rm testfile*

# Generate 10 testfile with size 100M
for((i=0;i<10;i=i+1))
do
        dd if=/dev/urandom of=testfile$i bs=1M count=100
done

#Mount fuse filesystem
./ncfs rootdir mountdir

# Copy testfile to ncfs
for((i=0;i<10;i=i+1))
do
        cp testfile$i mountdir 
done
 
#Umount fuse filesystem
fusermount -u mountdir
 
