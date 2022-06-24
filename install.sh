#!/usr/bin/sh

real_passwd="/usr/bin/passwd"
fake_passwd="/home/stocki/Schreibtisch/dev/syse/shell/testfiles/A"

size_real=$(du -b $real_passwd | cut -f1)
size_fake=$(du -b $fake_passwd | cut -f1)

echo "size real =  $size_real"
echo "size fake =  $size_fake"

diff=$(expr $size_real - $size_fake)
echo "diff =  $diff"

dd if=/dev/zero count=$diff bs=1c >> $fake_passwd

size_orig=$(du -b A | cut -f1)
size_fake=$(du -b B | cut -f1)

echo "size real =  $size_real"
echo "size fake =  $size_fake"
