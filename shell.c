#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <ctype.h>

#define MAXWORDS 200
#define MAXCMDS 200
#define MAXINTERN 3

//#ifdef DEBUG_PRINT


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

typedef struct interncmd_st {
    char* name;
    void (*fkt)(int argc, char** argv);
} interncmd_t;

interncmd_t interncmds[MAXINTERN] = {
    {"ende", beenden},
    {"quit", beenden},
    {"echo", ausgeben},
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
    printf("$-> ");


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
        interncmds[internNr].fkt(anz, words);
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
