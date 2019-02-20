/****************************************************************************\
 *
 * File:
 * 	logger.h
 *
 * Description:
 * 	Function declarations and constants for logger.c
 *
 * Author:
 * 	David Stockhouse
 *
 * Revision 0.1
 * 	Last edited 2/20/2019
 *
\***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// To use localtime_r
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE
#endif

#define LOG_FILENAME_LENGTH 256

typedef struct {
	int fd;
	int bin;
	char filename[LOG_FILENAME_LENGTH];
	int filenameLength;
	time_t timestamp;
} LOG;

int generateFilename(char *buf, int bufSize, time_t *time, 
		const char *dir, const char *pre, const char *ext);

int initLog(const char *dir, const char *pre, int bin);

