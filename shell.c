#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>  //uname
#include <signal.h>

#define MAXWORDS 200
#define MAXCMDS 200
#define MAXINTERN 6

#define MAX_DIR_NAME_LEN 1024
#define MAT_NUM /*is211*/"818"
#define DEBUG_PRINT

extern char **environ;

void parse_cmds(char** cmdv, char* cmdline, int *anzcmds);
void parse_words(char** wordv, char* cmdline, int *anzwords);

void print_vec(char **v, int anz);
void execute (char **v, int anz, int letztes);

int getInternIdx(char* cmdline);

void beenden(int argc, char** argv) {
    printf("Thx for using - donate here!\n");
    exit (argc>1 ? atoi(argv[1]) : 0);
}
void ausgeben(int argc, char** argv) {
    while(--argc) {
        printf("%s ",*++argv);
    }
    putchar('\n');
} 

char* get_working_dir();
void print_promt();
void print_working_dir();

void print_info(){
    printf("Process information:\n");
    
    //https://www.man7.org/linux/man-pages/man3/getumask.3.html
    //get umask by setting new one and set it back
    mode_t mask = umask( 0 );
    umask(mask);

    printf("pid=%d, ppid=%d, uid=%d, euid=%d, gid=%d, egid=%d, umask=%d\n\n", getpid(), getppid(), geteuid(), getgid(), getegid(), mask);
    
    printf("working dir: %s\n\n", get_working_dir());

    printf("# Signalhandler:\n");
      for(int signo=1; signo<=31; signo++) {
        struct sigaction sa;
        sigaction(signo, NULL, &sa);
        printf("signal %d - handler: %p, sigaction: %p, mask: %d, flags: %d\n",signo, sa.sa_handler, sa.sa_sigaction, sa.sa_mask, sa.sa_flags);
  }

    
    printf("\n# environment:\n");
    char **env = environ;
    while(*env != NULL) {
        printf("  %s\n", *env);
        env++;
    }


}



void change_dir(int argc, char** argv){
#ifdef DEBUG_PRINT
    printf("in change_dir, argc: %d\n", argc);
#endif
    if(argc > 1 ){
        if(chdir((const char *) argv[1])==-1){
            perror("chdir");
        }
    }else{
        const char* home_dir = getenv("HOME");
#ifdef DEBUG_PRINT
        printf("home_dir: %s\n", home_dir);
#endif       
        if(home_dir == NULL){
            perror("getenv(HOME)");
            return;
        }
        if(chdir(home_dir)==-1){
            perror("chdir");
        }
    }
}

typedef struct interncmd_st {
    char* name;
    void (*fkt)(int argc, char** argv);
} interncmd_t;

interncmd_t interncmds[MAXINTERN] = {
    {"818-ende", beenden},
    {"quit", beenden},
    {"echo", ausgeben},
    {"818-info", print_info},
    {"818-wo", print_working_dir},
    {"cd", change_dir}
};


int getInternIdx(char* cmdline) {
    char* pos;
    size_t sz;
    while(isspace(*cmdline)) {
        ++cmdline;
    }

    for(int i=0; i<MAXINTERN; ++i) {
        pos=strpbrk(cmdline," \t");
        sz = pos ? (pos-cmdline) : strlen(interncmds[i].name);
        if(strncmp(cmdline,interncmds[i].name,sz)==0) {
            return i;
        }
    }
    return -1;
}


int main() {
    char  *cmdline;
    size_t cmdlen = 0;
    char *cmdv[MAXCMDS];
    int anzcmds = 0;
    int pid;
    int internIdx;
    int fd0save;

    for(;;) {
        print_promt();

        cmdlen = getline(&cmdline, &cmdlen,stdin);
        parse_cmds(cmdv, cmdline, &anzcmds);
        
#ifdef DEBUG_PRINT
        print_vec(cmdv, anzcmds);
#endif
                internIdx = getInternIdx(cmdv[anzcmds-1]);

        if(internIdx == -1) {
            switch(pid=fork()) {
            case -1:
                perror("fork");
                break;
            case 0:
                execute(cmdv,anzcmds,0);
                break;
            default:
                waitpid(pid,NULL, 0); ///TODO: auswerten, warum beendet
                break;
            }
        } else {
            fd0save=dup(0);
            execute(cmdv,anzcmds,1);
            fclose(stdin);
            dup2(fd0save, 0);
            close(fd0save);
            stdin = fdopen(0, "r");
        }
    }
    free(cmdline);
    return 0;
}

void execute(char **cmdv, int anz, int letztes) {
    char* words[MAXWORDS];
    int anzworte;
    int pd[2];
    int pid;
    int internNr;

    if(anz>1) { //dann gibts eine pipe

        if(pipe(pd) == -1) {
            perror("Pipe");
            exit(2);
        }

        switch(pid = fork()) {
        case -1:
            perror("fork");
            break;
        case 0:
            fclose(stdout);
            dup2(pd[1],1); // überschreibt stdout FD mit der schreibseite der pipe
            stdout = fdopen(1,"w");  //wegen interer cmds auch die stdio clib
            close(pd[0]);  // schlieẞen was wir nicht brauchen
            close(pd[1]);
            execute(cmdv, anz-1, 0);
            break;
        default:
            fclose(stdin);
            dup2(pd[0],0); // überschreibt stdout FD mit der leseseite der pipe
            stdin = fdopen(0,"r"); //wegen interer cmds auch die stdio clib
            close(pd[0]);  // schließen was wir nicht brauchen
            close(pd[1]);  // ohne schließen kein EOF
            break;
        }
    }
    parse_words(words,cmdv[anz-1],&anzworte);
#ifdef DEBUG_PRINT
    print_vec(words,anzworte);
#endif
    internNr = getInternIdx(cmdv[anz-1]);
    if(internNr==-1) {
        execvp(words[0], words);
        perror("exec");
        exit(3);
    } else {
        interncmds[internNr].fkt(anzworte, words);
        if(!letztes){
            exit(0);            
        }
    }
}

void parse_cmds(char** cmdv, char* cmdline, int *anzcmds) {

    for(*anzcmds = 0, cmdv[0] = strtok(cmdline,"|");
            *anzcmds<MAXCMDS-1 && cmdv[*anzcmds] != NULL;
            ++*anzcmds, cmdv[*anzcmds] = strtok(NULL,"|"))
        ;
}

void parse_words(char** wordv, char* cmdline, int *anzwords) {

    for(*anzwords = 0, wordv[0] = strtok(cmdline," \t\n");
            *anzwords<MAXWORDS-1 && wordv[*anzwords] != NULL;
            ++*anzwords, wordv[*anzwords] = strtok(NULL," \t\n"))
        ;
}


void print_vec(char **v, int anz) {

    for (int i = 0; i<anz; ++i) {
        printf("%d: %s\n",i, v[i]);
    }
}

char* get_working_dir(){
    char dir_name[MAX_DIR_NAME_LEN];
    if(getcwd(dir_name,sizeof(dir_name)) == NULL){
        perror("getcwd");
        exit(1);
    }
#ifdef DEBUG_PRINT    
    printf("current working dir: %s\n", dir_name);
#endif
    return strdup(dir_name);
}

void print_promt(){
    if(geteuid() != 0){
        printf("%s-%s > ", MAT_NUM, get_working_dir());
    } else{
        printf("%s-%s >> ", MAT_NUM, get_working_dir());
    }
}

void print_working_dir(){
    printf("%s\n",get_working_dir());
}
