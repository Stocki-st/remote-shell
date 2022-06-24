##############################
# SHELL                      #
# Author: Stefan Stockinger  #
#############################
all: shell

shellserver: shell.c shell.h sock.c 
	gcc -Wall -o shellserver shell.c sock.c -lpthread -lcap -DTCP_SHELL_SERVER

shell: shell.c
	gcc -Wall -o shell shell.c -lpthread -lcap

clean:
	rm -f shell

format:
	astyle *.c *.h


default: shell
