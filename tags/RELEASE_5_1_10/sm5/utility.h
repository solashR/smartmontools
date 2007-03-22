/*
 * utility.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#ifndef __UTILITY_H_
#define __UTILITY_H_

#ifndef UTILITY_H_CVSID
#define UTILITY_H_CVSID "$Id: utility.h,v 1.8 2003/04/18 22:09:14 pjwilliams Exp $\n"
#endif

#include <time.h>
#include <regex.h>

// Utility function prints current date and time and timezone into a
// character buffer of length>=64.  All the fuss is needed to get the
// right timezone info (sigh).
void dateandtimezone(char *buffer);
// Same, but for time defined by epoch tval
void dateandtimezoneepoch(char *buffer, time_t tval);



// utility function for printing out CVS strings
#define CVSMAXLEN 1024
void printone(char *block, const char *cvsid);

// like printf() except that we can control it better. Note --
// although the prototype is given here, in utility.h, the function
// itself is defined differently in smartctl and smartd.  So the
// function definition is in smartd.c and in smartctl.c.
void pout(char *fmt, ...)  
     __attribute__ ((format (printf, 1, 2)));

// replacement for perror() with redirected output.
void syserror(const char *message);

// Prints a warning message for a failed regular expression compilation from
// regcomp().
void printregexwarning(int errcode, regex_t *compiled);

// A wrapper for regcomp().  Returns zero for success, non-zero otherwise.
int compileregex(regex_t *compiled, const char *pattern, int cflags);

// Function for processing -r option in smartctl and smartd
int split_report_arg(char *s, int *i);

#endif