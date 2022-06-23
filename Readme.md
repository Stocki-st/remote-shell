# Shell

Every real man needs to write his own shell at least once in his lifetime. This one is mine (first steps...)

## how to build:

clean with
```
make clean
```

format using astyle
```
make format
```

build 
```
make all
```


## how to run:

There are 2 modes available:

1. "normal" --> no argument passen

```
./shell
```

2. '--nosuid' --> started with this option, the execution of cmds with SUID bit will be blocked. 

```
./shell --nosuid
```

## General information:
############################
### Welcome to my shell! ###
############################

the following internal cmds are supported:
 - 'echo'          ... prints the given input (echos it)
 - '818-echo'      ... prints the given input (echos it)
 - '818-info'      ... prints a lot of information about this shell and the process env
 - '818-wo'        ... prints the current working directory
 - 'cd'            ... changes the current working directory
 - '818-cd'        ... changes the current working directory
 - '818-setpath'   ... replaces the PATH variable with the given path
 - '818-addtopath' ... appends the given path to the PATH variable
 - '818-getpath'   ... prints the 
 - '?'             ... prints this lovely help dialog. 
 - '818-help'      ... same as '?'
 - 'quit           ... exits the shell
 - '818-ende'      ... exits the shell
