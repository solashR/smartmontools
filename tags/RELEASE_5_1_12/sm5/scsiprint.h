/*
 * scsiprint.h
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


/* scsismart version number */
#ifndef SCSI_PRINT_H_
#define SCSI_PRINT_H_

#ifndef SCSIPRINT_H_CVSID
#define SCSIPRINT_H_CVSID "$Id: scsiprint.h,v 1.13 2003/05/01 08:49:04 dpgilbert Exp $\n"
#endif

// MACROS to control printing behavior (from ataprint.h)
#define QUIETON(control)  {if (control->quietmode) control->veryquietmode=0;}
#define QUIETOFF(control) {if (control->quietmode && !control->veryquietmode) control->veryquietmode=1;}

int scsiPrintMain(const char * dev_name, int fd);

#endif