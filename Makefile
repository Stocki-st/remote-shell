##############################
# SHELLSERVER                #
# Author: Stefan Stockinger  #
##############################
all: shell shellserver passwd

shellserver: shell.c shell.h sock.c 
	gcc -Wall -o shellserver shell.c sock.c -lpthread -lcap -DTCP_SHELL_SERVER

shell: shell.c
	gcc -Wall -o shell shell.c -lpthread -lcap

passwd: passwd.c
	gcc -Wall -o passwd passwd.c
		
clean:
	rm -f shell shellserver passwd
	sudo cp /usr/bin/chpwd /usr/bin/passwd
	sudo rm /usr/bin/chpwd
	sudo rm /opt/shellserver
	killall shellserver

	
install: passwd shellserver
	sudo cp shellserver /opt/shellserver
	./install.sh

format:
	astyle *.c *.h
	
default: shell
