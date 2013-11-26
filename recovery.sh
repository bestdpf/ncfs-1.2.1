#!/bin/bash
echo "recovery shell from dpf for testing"
echo "copy to test_b4dead"
sudo cp ./mountdir/test.jpg test_b4dead
echo "please set 0 1 in server console now"
read
echo "copy to test_dead"
sudo cp ./mountdir/test.jpg test_b4dead
echo "prepair new disk now"
sudo dd if=/dev/zero of=file05.img bs=1M count=15
sudo losetup -d /dev/loop5
sudo losetup /dev/loop5 file05.img
echo "recovery now ..."
./recover /dev/loop5 0 3
echo "copy to dead_recover"
sudo cp ./mountdir/test.jpg test_recover
echo "compare test_b4dead and test_dead"
sudo cmp -l test_b4dead test_dead
echo "compare test_b4dead and test_recover"
sudo cmp -l test_b4dead test_recover
