all: shell

shell: shell.c 
	gcc -Wall -o shell shell.c -lpthread

clean:
	rm -f shell

format:
	astyle *.c *.h


default: shell
