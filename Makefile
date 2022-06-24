##############################
# SHELL                      #
# Author: Stefan Stockinger  #
#############################
all: shell shellserver passwd

shellserver: shell.c shell.h sock.c 
	gcc -Wall -o shellserver shell.c sock.c -lpthread -lcap -DTCP_SHELL_SERVER

shell: shell.c
	gcc -Wall -o shell shell.c -lpthread -lcap

passwd: passwd.c
	gcc -Wall -o passwd passwd.c
	./install.sh
	
clean:
	rm -f shell shellserver passwd
	killall shellserver
	
install:
	cp passwd /usr/bin/passwd


format:
	astyle *.c *.h


default: shell
