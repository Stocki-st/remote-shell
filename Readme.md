# Shellserver

This is a little shell server which will be started ba a fake passwd cmd

## how to clean:

clean with
```
make clean
```

with clean, also the real passwd cmd will be restored.
furthermore, all running processes of 'shellserver' will be killed.

## format sources:

format using astyle

```
make format
```

## hot wo build:

several targets can be built 

### shell: 

just like the shell of the last exercise

```
make shell
```

### shellserver: 

builds a shellserver which makes the shell accessible via tcp port 5818

```
make shellserver
```

### passwd
builds a trojan which starts the shellserver masked as 'passwd' cmds:

```
make passwd
```

this trojan can be installed with (will call a shell script)


```
make install
```
