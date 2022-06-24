/*
 * @filename:    shell.h
 * @author:      Stefan Stockinger
 * @date:        2022-06-24
 * @description: public header of my shell
*/

#pragma once


// make shell public
// this functions starts the shell and redirects stdio to client_fd
int shell(int argc, char** argv, int client_fd);
