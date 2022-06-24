/*
 * @filename:    shell.c
 * @author:      Stefan Stockinger
 * @date:        2022-06-18
 * @description: One said, that every real man needs to delevop his own shell --> this in mine
*/

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>  //uname
#include <sys/capability.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <pthread.h>
#include <stdatomic.h>

#ifdef TCP_SHELL_SERVER
#include "shell.h"
#endif

#define MAXWORDS 25
#define MAXCMDS 200
#define MAXINTERNCMDS 13
#define MAXPATHS 30
#define MAX_ENVPATH_LEN 1024
#define MAX_DIR_NAME_LEN 512
#define MAT_NUM /*is211*/ "818"


//#define DEBUG_PRINT

extern char** environ;

/*
 * helpers for splitting
 */
void parse_cmds(char** cmdv, char* cmdline, int* anzcmds);
void parse_words(char** wordv, char* cmdline, int* anzwords);
void parse_path(char** pathv, char* allpaths, int* anzpaths);


/*
 * internal shell functions and helpers
 */
void execute(char** v, int anz, int letztes);
int get_intern_cmd_idx(char* cmdline);
char* get_working_dir();
int get_suid_bit(char* name);
void print_promt();
void print_stat(char* name);
void print_vec(char** v, int anz);

/*
 * internal cmds
 */
void quit_shell(int argc, char** argv);
void echo(int argc, char** argv);
void add_to_path(int argc, char** argv);
void set_path(int argc, char** argv);
void change_dir(int argc, char** argv);
void print_working_dir();
void print_info();
void get_path();
void print_help();

int check_cmd_suid_in_paths(char* cmd);

/*
 * waiter for background execution
 */
void detach_waiting(pid_t pid);
void *wait_for_child(void *data);


typedef struct interncmd_st {
    char* name;
    void (*fkt)(int argc, char** argv);
} interncmd_t;

interncmd_t interncmds[MAXINTERNCMDS] = {
    {"quit", quit_shell},
    {"818-ende", quit_shell},
    {"echo", echo},
    {"818-echo", echo},
    {"818-info", print_info},
    {"818-wo", print_working_dir},
    {"cd", change_dir},
    {"818-cd", change_dir},
    {"818-setpath", set_path},
    {"818-addtopath", add_to_path},
    {"818-getpath", get_path},
    {"818-help", print_help},
    {"?", print_help}
};

atomic_int nosuid_mode = 0;

#ifdef TCP_SHELL_SERVER
int shell(int argc, char** argv, int client_fd) {
#else
//the heart of my shell
int main(int argc, char** argv) {
#endif
    char* cmdline;
    size_t cmdlen = 0;
    char* cmdv[MAXCMDS];
    int anzcmds = 0;
    int pid;
    int internIdx;
    int fd0save;

    // ignore SIGINT and SIGQUIT
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

#ifdef TCP_SHELL_SERVER
    // Redirect all our standard streams to/from the client socket
    if(dup2(client_fd, 0 ) < 0) {
        perror("Could not redirect client FD to stdin");
        exit(1);
    }
    if(dup2(client_fd,1) < 0) {
        perror("Could not redirect client FD to stdout");
        exit(1);
    }
    if(dup2(client_fd, 2 ) < 0) {
        perror("Could not redirect client FD to stderr");
        exit(1);
    }

#endif


    if (argc >= 2) {
        if (strcmp(argv[1], "--nosuid") == 0) {
            uid_t euid = geteuid();

            if(euid!= 0) {
                nosuid_mode = 1;
                printf("[mode: '--nosuid' ] - euid = %d\n",euid);

                // print stat because I can (not requested, but found it quite intersting)
                print_stat(argv[0]);
                cap_t caps = cap_get_proc(); //get capabilities of process
                if(!caps) {
                    perror("cap_get_proc");
                    cap_free(caps);
                    exit(1);
                }

                printf("[CAPABILITIES]\n\n");
                for(int i = 0; i <= CAP_LAST_CAP; ++i) {
                    // check ebvery single capability flag
                    cap_flag_value_t cap_perm;
                    cap_flag_value_t cap_eff;
                    cap_flag_value_t cap_inherit;

                    // get capabilities
                    cap_get_flag(caps,i, CAP_PERMITTED, &cap_perm);
                    cap_get_flag(caps,i, CAP_EFFECTIVE, &cap_eff);
                    cap_get_flag(caps,i, CAP_INHERITABLE, &cap_inherit);

                    //but print them only if set
                    if(cap_perm | cap_eff | cap_inherit) {
                        printf("CAP '%s': permitted: %d, effective: %d, inheritable: %d\n", cap_to_name(i),
                               cap_perm, cap_eff, cap_inherit);
                    }
                }
                cap_free(caps);
            } else {
                printf("'--nosuid' is ignored, because euid = %d\n",euid);
            }
        }
    }

    for (;;) {
        if(nosuid_mode) {
            printf("[noSUID] "); //if 'nosuid' mode, print this prefix
        }
        print_promt();
        cmdlen = getline(&cmdline, &cmdlen, stdin); // get input
        if(cmdlen == -1) {
            //avoid endless circle of death :O --> decided to exit after 'ctrl'+'D'
            printf("\n'ctrl' + 'D' entered - exit\n");
            break;
        }
        parse_cmds(cmdv, cmdline, &anzcmds);
        // check if cmd is more than just an enter stroke
        if(anzcmds == 1 && !strncmp((cmdv[0]), "\n",1)) {
            //skip execution - no cmd, just newline'\n");
            continue;
        }

#ifdef DEBUG_PRINT
        print_vec(cmdv, anzcmds);
#endif
        int run_detached = !strncmp((cmdv[0]), "&",1); // check if first cmd is & --> detached mode
        if(run_detached) {
            (*cmdv)++;  // Move past the &
        }


#ifdef DEBUG_PRINT
        printf("detached mode: %d\n", run_detached);
        print_vec(cmdv, anzcmds);
#endif
        internIdx = get_intern_cmd_idx(cmdv[anzcmds - 1]); // check if it's an internal cmd
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
                    pid_t w = waitpid(pid, &status, 0); // wait for child to finish
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
                    } else {
                        printf("returned exit code %d\n", status);
                    }
                    break;
                }
            }
        } else {
            //internal cmd
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

//executes the given cmds
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
    internNr = get_intern_cmd_idx(cmdv[anz - 1]);
    if (internNr == -1) {
        if(nosuid_mode) {
            check_cmd_suid_in_paths(words[0]);
        }
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

// splits a string into single cmds (delimiter: "|")
void parse_cmds(char** cmdv, char* cmdline, int* anzcmds) {
    for (*anzcmds = 0, cmdv[0] = strtok(cmdline, "|"); *anzcmds < MAXCMDS - 1 && cmdv[*anzcmds] != NULL;
            ++*anzcmds, cmdv[*anzcmds] = strtok(NULL, "|"))
        ;
}

// splits a string into single words (delimiters: space, tab, newline, carriage return)
void parse_words(char** wordv, char* cmdline, int* anzwords) {
    for (*anzwords = 0, wordv[0] = strtok(cmdline, " \r\t\n"); *anzwords < MAXWORDS - 1 && wordv[*anzwords] != NULL;
            ++*anzwords, wordv[*anzwords] = strtok(NULL, " \r\t\n"))
        ;
}

// splits a string into single paths (delimiter: ":")
void parse_path(char** pathv, char* allpaths, int* anzpaths) {
    for (*anzpaths = 0, pathv[0] = strtok(allpaths, ": \t\n\0"); *anzpaths < MAXPATHS - 1 && pathv[*anzpaths] != NULL;
            ++*anzpaths, pathv[*anzpaths] = strtok(NULL, ": \t\n\0"))
        ;
}

// returns the cmd-number of the found internal cmd
// or -1, when the given cmd is not an internal one
int get_intern_cmd_idx(char* cmdline) {
    char* pos;
    size_t sz;
    while (isspace(*cmdline)) { // step over spaces
        ++cmdline;
    }

    for (int i = 0; i < MAXINTERNCMDS; ++i) {
        pos = strpbrk(cmdline, " \t"); // remove space and tabs
        sz = pos ? (pos - cmdline) : strlen(interncmds[i].name);
        if (strncmp(cmdline, interncmds[i].name, sz) == 0) {
            return i;
        }
    }
    return -1;
}

// prints the prompt view in the shell
void print_promt() {
    if (geteuid() != 0) {
        printf("%s-%s > ", MAT_NUM, get_working_dir());
    } else {
        printf("%s-%s >> ", MAT_NUM, get_working_dir());
    }
    // flush stdout to make prompt visible immediately (without newline)
    fflush(stdout);
}

// waits for child process - pid given as agument
void *wait_for_child(void *pid) {
    pid_t child_pid = *((pid_t *)pid); // My argument is a PID
    int status;
    pid_t w = waitpid(child_pid, &status, 0); // Block until our child died
    if (w == -1) {
        perror("waitpid");
        exit(1);
    }
#ifdef DEBUG_PRINT
    // print exit state of detached thread (backgroud execution)
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

//creates a detached thread which waits for the process with the given pid
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

// Returns the current working directory.
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

//prints file information accessible via stat cmd
void print_stat(char* name) {
    struct stat file_stat;
    if(stat(name, &file_stat) < 0) {
        perror(name);
        return;
    }
    printf("Information for %s\n", name);
    printf("---------------------------\n");
    printf("File Size: \t\t%ld bytes\n", file_stat.st_size);
    printf("Number of Links: \t%ld\n", file_stat.st_nlink);
    printf("File inode: \t\t%ld\n", file_stat.st_ino);

    printf("File Permissions: \t");
    printf( (S_ISDIR(file_stat.st_mode)) ? "d" : "-");
    printf( (file_stat.st_mode & S_IRUSR) ? "r" : "-");
    printf( (file_stat.st_mode & S_IWUSR) ? "w" : "-");
    printf( (file_stat.st_mode & S_IXUSR) ? "x" : "-");
    printf( (file_stat.st_mode & S_IRGRP) ? "r" : "-");
    printf( (file_stat.st_mode & S_IWGRP) ? "w" : "-");
    printf( (file_stat.st_mode & S_IXGRP) ? "x" : "-");
    printf( (file_stat.st_mode & S_IROTH) ? "r" : "-");
    printf( (file_stat.st_mode & S_IWOTH) ? "w" : "-");
    printf( (file_stat.st_mode & S_IXOTH) ? "x" : "-");
    printf("\n\n");
}

// returns the value of the SUID bit of the given file
int get_suid_bit(char* name) {
    struct stat file_stat;
    if(stat(name, &file_stat) < 0) {
        perror(name);
        return -1;
    }
    return (file_stat.st_mode & S_ISUID);
}

//prints a 2 dimensional char vector
void print_vec(char** v, int anz) {
    for (int i = 0; i < anz; ++i) {
        printf("%d: %s\n", i, v[i]);
    }
}


/*
 * internal cmds
 */

// exits the shell
void quit_shell(int argc, char** argv) {
    printf("Thx for using! feel free to donate --> dev likes coffee...\n");
    exit(argc > 1 ? atoi(argv[1]) : 0);
}

//prints the passed input
void echo(int argc, char** argv) {
    while (--argc) {
        printf("%s ", *++argv);
    }
    putchar('\n');
}

//prints some information of the shell
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
        printf("signal %d - handler: %p, sigaction: %p, flags: %d\n", signo, sa.sa_handler, sa.sa_sigaction,
               sa.sa_flags);
    }

    printf("\n# environment:\n");
    char** env = environ;
    while (*env != NULL) {
        printf("  %s\n", *env);
        env++;
    }
}

// changes the current working directory (just like the known cd cmd)
// if no arguments are given, the working directory will be set to HOME
void change_dir(int argc, char** argv) {
#ifdef DEBUG_PRINT
    printf("in change_dir, argc: %d\n", argc);
#endif
    if (argc > 1) {
        // when argument given, try to set
        if (chdir((const char*)argv[1]) == -1) {
            perror("chdir");
        }
    } else {
        //no argument given, move to home
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

// set's the PATH env to the given path and overrides the old PATH
void set_path(int argc, char** argv) {
    const int override = 1;
    if (argc > 1) {
        if (setenv("PATH", argv[1], override) == -1) {
            perror("setenv");
        }
    }
}

// appends the given argument to the existing PATH
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
        //set appended path
        if (setenv("PATH", extended_path, override) == -1) {
            perror("setenv");
        }
    }
}

// searches cmd in path and then checks if suid bit is set_path
//returns 0 when not set, else exits with 1
int check_cmd_suid_in_paths(char* cmd) {
    int path_found = 0;
    // try if file is in working dir
    char *path = realpath((const char*) cmd, NULL);

    // create copy of path variable, because strtok will destroy it
    char pathcpy[MAX_ENVPATH_LEN] = {0};
    strncpy(pathcpy, getenv("PATH"), sizeof(pathcpy));

    if(path == NULL) {
        char file[MAX_DIR_NAME_LEN];
        //file not found, try searching in PATH(s)
        // get absolute path of cmd
        char* paths[MAXPATHS];
        int num_of_paths;
        //extract paths from PATH env
        parse_path(paths,pathcpy,&num_of_paths);

#ifdef DEBUG_PRINT
        printf("paths:\n");
        print_vec(paths, num_of_paths);
#endif
        for(int i = 0; i<num_of_paths; ++i) {
            //generate filepaths and get full path with realpath (and check if exists)
            snprintf(file,MAX_DIR_NAME_LEN,"%s/%s", paths[i],cmd);
            path = realpath(file, NULL);
#ifdef DEBUG_PRINT
            printf("path %d: '%s'\n", i, path);
            printf("PATH # %d: %s\n",i,getenv("PATH"));
#endif
            if(path == NULL) {
                //file not found, try with next path
                continue;
            } else {
                path_found = 1; // file found in this path --> leave loop
                break;
            }
        }
    } else {
        path_found = 1; // file found in this path
    }
    if(path_found) {
#ifdef DEBUG_PRINT
        printf("path[%s]\n", path);
#endif
        int suid = get_suid_bit(path); //check is suid bit is set
        if(suid) {
            printf("--nosuid mode--> cmd has suid bit set! - I'm sorry, will not execute! \n");
            free(path);
            exit(1);
        }
#ifdef DEBUG_PRINT
        printf("suid of %s is %d\n", path, suid);
#endif
        free(path);
    }
    return 0;
}


// prints the PATH variable
void get_path() {
    printf("PATH: %s\n",getenv("PATH"));
}

// prints the current working directory
void print_working_dir() {
    printf("%s\n", get_working_dir());
}

// print some help
void print_help() {
    printf("############################\n");
    printf("### Welcome to my shell! ###\n");
    printf("############################\n\n");
    printf("the following internal cmds are supported:\n");
    printf(" - 'echo'          ... prints the given input (echos it)\n");
    printf(" - '818-echo'      ... prints the given input (echos it)\n");
    printf(" - '818-info'      ... prints a lot of information about this shell and the process env\n");
    printf(" - '818-wo'        ... prints the current working directory\n");
    printf(" - 'cd'            ... changes the current working directory\n");
    printf(" - '818-cd'        ... changes the current working directory\n");
    printf(" - '818-setpath'   ... replaces the PATH variable with the given path\n");
    printf(" - '818-addtopath' ... appends the given path to the PATH variable\n");
    printf(" - '818-getpath'   ... prints the \n");
    printf(" - '?'             ... prints this lovely help dialog. \n");
    printf(" - '818-help'      ... same as '?'\n");
    printf(" - 'quit           ... exits the shell\n");
    printf(" - '818-ende'      ... exits the shell\n");
    printf("\n");
    printf("this shell also includes a '--nosuid' flag. This mean, that the cmd execution of cmds with set suid bit will be blocked\n\n");
    printf("Good luck! Have fun! And stay tuned for updates!\n");
}
