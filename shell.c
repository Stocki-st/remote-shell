#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>  //uname
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <pthread.h>

#define MAXWORDS 200
#define MAXCMDS 200
#define MAXINTERN 12

#define MAX_DIR_NAME_LEN 1024
#define MAT_NUM /*is211*/ "818"
//#define DEBUG_PRINT

extern char** environ;

void parse_cmds(char** cmdv, char* cmdline, int* anzcmds);
void parse_words(char** wordv, char* cmdline, int* anzwords);

void print_vec(char** v, int anz);
void execute(char** v, int anz, int letztes);

int getInternIdx(char* cmdline);

void beenden(int argc, char** argv);
void ausgeben(int argc, char** argv);
void add_to_path(int argc, char** argv);
void set_path(int argc, char** argv);
void change_dir(int argc, char** argv);
char* get_working_dir();
void print_promt();
void print_working_dir();
void print_info();

void get_path() {
}

void print_help() {
}

typedef struct interncmd_st {
    char* name;
    void (*fkt)(int argc, char** argv);
} interncmd_t;

interncmd_t interncmds[MAXINTERN] = {
    {"818-ende", beenden},
    {"quit", beenden},
    {"echo", ausgeben}, 
    {"818-echo", ausgeben},
    {"818-info", print_info},
    {"818-wo", print_working_dir}, 
    {"cd", change_dir}, 
    {"818-cd", change_dir}, 
    {"818-setpath", set_path}, 
    {"818-addtopath", add_to_path},
    {"818-getpath", get_path}, 
    {"818-help", print_help}, 
};


void *wait_for_child(void *data) {
    pid_t child_pid = *((pid_t *)data); // My argument is a PID
    int status;
    pid_t w = waitpid(child_pid, &status, 0); // Block until our child died
    if (w == -1) {
        perror("waitpid");
        exit(1);
    }
#ifdef DEBUG_PRINT
    if (WIFEXITED(status)) {
        printf("[detached] exited, status=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[detached] killed by signal %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        printf("[detached] stopped by signal %d\n", WSTOPSIG(status));
    } else  {
        printf("[detached] returned exit code %d\n", status);
    }

#endif
    return NULL;
}

void detach_waiting(pid_t pid) {
    pid_t *thread_data;
    pthread_t thread;
    thread_data = calloc(sizeof(pid_t), 1);
    *thread_data = pid;

    if(pthread_create(&thread, NULL, wait_for_child, (void *)thread_data)) {
        free(thread_data);
        return;
    }
    pthread_detach(thread);
}

void print_stat(char* name){
 struct stat fileStat;
    if(stat(name, &fileStat) < 0)    
        return 1;

    printf("Information for %s\n", name);
    printf("---------------------------\n");
    printf("File Size: \t\t%d bytes\n", fileStat.st_size);
    printf("Number of Links: \t%d\n", fileStat.st_nlink);
    printf("File inode: \t\t%d\n", fileStat.st_ino);

    printf("File Permissions: \t");
    printf( (S_ISDIR(fileStat.st_mode)) ? "d" : "-");
    printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
    printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
    printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
    printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
    printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
    printf("\n\n");   
}

int main(int argc, char** argv) {
    char* cmdline;
    size_t cmdlen = 0;
    char* cmdv[MAXCMDS];
    int anzcmds = 0;
    int pid;
    int internIdx;
    int fd0save;
    int nosuid = 0;
    
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);


    if (argc >= 2) {
          if (strcmp(argv[1], "--nosuid") == 0) {
              printf("[mode: '--nosuid' ]\n");
              nosuid = 1;
              print_stat(argv[0]);
              if(geteuid() != 0){
                  /*needed? i check die angabe ned ganz :D
                   * if (seteuid(0) < 0) {
                     perror("seteuid");
                     exit(1);
                   */
                  
           }
              }
         }
       

       
    for (;;) {
        if(nosuid){
            printf("[noSUID] ");
        } 
        print_promt();
             
        cmdlen = getline(&cmdline, &cmdlen, stdin);
        
        if(cmdlen == -1){
           //avoid endless circle of death :O
           printf("\n'ctrl' + 'D' entered - exit\n");
           break;
        }
        
        parse_cmds(cmdv, cmdline, &anzcmds);

        // check if cmd is more than just an enter stroke
        if(anzcmds == 1 && !strncmp((cmdv[0]), "\n",1)) {
#ifdef DEBUG_PRINT
            printf("skip execution - no cmd, just newline'\n");
#endif
            continue;
        }

#ifdef DEBUG_PRINT
        print_vec(cmdv, anzcmds);
#endif
        int run_detached = !strncmp((cmdv[0]), "&",1);
        if(run_detached) {
            (*cmdv)++;  // Move past the & argument
        }


#ifdef DEBUG_PRINT
        printf("detached mode: %d\n", run_detached);
        print_vec(cmdv, anzcmds);
#endif
        internIdx = getInternIdx(cmdv[anzcmds - 1]);
        if (internIdx == -1) {
            switch (pid = fork()) {
            case -1:
                perror("fork");
                break;
            case 0:
                //child
                if (!run_detached) {
                    // set default signal handling, when running in foreground
                    // else sig ignore is inherited from parent
                    signal(SIGINT, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                }
                execute(cmdv, anzcmds, 0);
                exit(0);
            default:
                //parent
                if (run_detached) {
                    detach_waiting(pid);
                    break;
                } else {
                    int status = 0;

#ifdef DEBUG_PRINT
                    printf("wait for child with pid %d\n", pid);
#endif
                    pid_t w = waitpid(pid, &status, 0);
                    if (w == -1) {
                        perror("waitpid");
                        exit(1);
                    }
                    if (WIFEXITED(status)) {
                        printf("exited, status=%d\n", WEXITSTATUS(status));
                    } else if (WIFSIGNALED(status)) {
                        printf("killed by signal %d\n", WTERMSIG(status));
                    } else if (WIFSTOPPED(status)) {
                        printf("stopped by signal %d\n", WSTOPSIG(status));
                    } else  {
                        printf("returned exit code %d\n", status);
                    }
                    break;
                }
            }
        } else {
            fd0save = dup(0);
            execute(cmdv, anzcmds, 1);
            fclose(stdin);
            dup2(fd0save, 0);
            close(fd0save);
            stdin = fdopen(0, "r");
        }
    }
    free(cmdline);
    return 0;
}

void execute(char** cmdv, int anz, int letztes) {
    char* words[MAXWORDS];
    int anzworte;
    int pd[2];
    int pid;
    int internNr;

    if (anz > 1) {  // dann gibts eine pipe

        if (pipe(pd) == -1) {
            perror("Pipe");
            exit(2);
        }

        switch (pid = fork()) {
        case -1:
            perror("fork");
            break;
        case 0:
            // child
            fclose(stdout);
            dup2(pd[1], 1);           // überschreibt stdout FD mit der schreibseite der pipe
            stdout = fdopen(1, "w");  // wegen interer cmds auch die stdio clib
            close(pd[0]);             // schlieẞen was wir nicht brauchen
            close(pd[1]);
            execute(cmdv, anz - 1, 0);
            break;
        default:
            //parent
            fclose(stdin);
            dup2(pd[0], 0);          // überschreibt stdout FD mit der leseseite der pipe
            stdin = fdopen(0, "r");  // wegen interer cmds auch die stdio clib
            close(pd[0]);            // schließen was wir nicht brauchen
            close(pd[1]);            // ohne schließen kein EOF
            break;
        }
    }
    parse_words(words, cmdv[anz - 1], &anzworte);
#ifdef DEBUG_PRINT
    print_vec(words, anzworte);
#endif
    internNr = getInternIdx(cmdv[anz - 1]);
    if (internNr == -1) {
        execvp(words[0], words);
        perror("exec");
        exit(3);
    } else {
        interncmds[internNr].fkt(anzworte, words);
        if (!letztes) {
            exit(0);
        }
    }
}

void parse_cmds(char** cmdv, char* cmdline, int* anzcmds) {
    for (*anzcmds = 0, cmdv[0] = strtok(cmdline, "|"); *anzcmds < MAXCMDS - 1 && cmdv[*anzcmds] != NULL;
            ++*anzcmds, cmdv[*anzcmds] = strtok(NULL, "|"))
        ;
}

void parse_words(char** wordv, char* cmdline, int* anzwords) {
    for (*anzwords = 0, wordv[0] = strtok(cmdline, " \t\n"); *anzwords < MAXWORDS - 1 && wordv[*anzwords] != NULL;
            ++*anzwords, wordv[*anzwords] = strtok(NULL, " \t\n"))
        ;
}

int getInternIdx(char* cmdline) {
    char* pos;
    size_t sz;
    while (isspace(*cmdline)) {
        ++cmdline;
    }

    for (int i = 0; i < MAXINTERN; ++i) {
        pos = strpbrk(cmdline, " \t");
        sz = pos ? (pos - cmdline) : strlen(interncmds[i].name);
        if (strncmp(cmdline, interncmds[i].name, sz) == 0) {
            return i;
        }
    }
    return -1;
}

char* get_working_dir() {
    char dir_name[MAX_DIR_NAME_LEN];
    if (getcwd(dir_name, sizeof(dir_name)) == NULL) {
        perror("getcwd");
        exit(1);
    }
#ifdef DEBUG_PRINT
    printf("current working dir: %s\n", dir_name);
#endif
    return strdup(dir_name);
}

void beenden(int argc, char** argv) {
    printf("Thx for using - donate here!\n");
    exit(argc > 1 ? atoi(argv[1]) : 0);
}

void ausgeben(int argc, char** argv) {
    while (--argc) {
        printf("%s ", *++argv);
    }
    putchar('\n');
}

void print_info() {
    printf("Process information:\n");

    // https://www.man7.org/linux/man-pages/man3/getumask.3.html
    // get umask by setting new one and set it back
    mode_t mask = umask(0);
    umask(mask);

    printf("pid=%d, ppid=%d, uid=%d, euid=%d, gid=%d, egid=%d, umask=%d\n\n", getpid(), getppid(), getuid(), geteuid(),
           getgid(), getegid(), mask);

    printf("working dir: %s\n\n", get_working_dir());

    printf("# Signalhandler:\n");
    for (int signo = 1; signo <= 31; signo++) {
        struct sigaction sa;
        sigaction(signo, NULL, &sa);
        printf("signal %d - handler: %p, sigaction: %p, mask: %p, flags: %d\n", signo, sa.sa_handler, sa.sa_sigaction,
               sa.sa_mask, sa.sa_flags);
    }

    printf("\n# environment:\n");
    char** env = environ;
    while (*env != NULL) {
        printf("  %s\n", *env);
        env++;
    }
}

void change_dir(int argc, char** argv) {
#ifdef DEBUG_PRINT
    printf("in change_dir, argc: %d\n", argc);
#endif
    if (argc > 1) {
        if (chdir((const char*)argv[1]) == -1) {
            perror("chdir");
        }
    } else {
        const char* home_dir = getenv("HOME");
#ifdef DEBUG_PRINT
        printf("home_dir: %s\n", home_dir);
#endif
        if (home_dir == NULL) {
            perror("getenv(HOME)");
            return;
        }
        if (chdir(home_dir) == -1) {
            perror("chdir");
        }
    }
}

void set_path(int argc, char** argv) {
    const int override = 1;
    if (argc > 1) {
        if (setenv("PATH", argv[1], override) == -1) {
            perror("setenv");
        }
    }
}

void add_to_path(int argc, char** argv) {
    if (argc > 1) {
        char* path = getenv("PATH");
        char* extension_sign = ":";

        int extended_path_len = strlen(path) + strlen(extension_sign) + strlen(argv[1]) + 1;  //+1 for terminator
        char extended_path[extended_path_len];
        // store the "new" path (including given argument)
        snprintf(extended_path, extended_path_len, "%s%s%s", path, extension_sign, argv[1]);

#ifdef DEBUG_PRINT
        printf("path_len=%d\n", extended_path_len);
        printf("path='%s'\n", path);
        printf("extended_path='%s'\n", extended_path);
#endif
        const int override = 1;

        if (setenv("PATH", extended_path, override) == -1) {
            perror("setenv");
        }
    }
}

void print_promt() {
    if (geteuid() != 0) {
        printf("%s-%s > ", MAT_NUM, get_working_dir());
    } else {
        printf("%s-%s >> ", MAT_NUM, get_working_dir());
    }
    fflush(stdout);
}

void print_working_dir() {
    printf("%s\n", get_working_dir());
}

void print_vec(char** v, int anz) {
    for (int i = 0; i < anz; ++i) {
        printf("%d: %s\n", i, v[i]);
    }
}
