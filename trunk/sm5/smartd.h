/*
 * smartd.h
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

#ifndef SMARTD_H_CVSID
#define SMARTD_H_CVSID "$Id: smartd.h,v 1.38 2003/08/05 10:07:35 ballen4705 Exp $\n"
#endif

// Configuration file
#define CONFIGFILE "/etc/smartd.conf"

// Scan directive for configuration file
#define SCANDIRECTIVE "DEVICESCAN"

// maximum line length in configuration file
#define MAXLINELEN 128

// maximum number of device entries in configuration file.
#define MAXENTRIES 64

// maximum length of a continued line in configuration file
#define MAXCONTLINE 1023

// default for how often SMART status is checked, in seconds
#define CHECKTIME 1800

// maximum number of ATA devices to monitor
#define MAXATADEVICES	12

// maximum number of SCSI devices to monitor
#define MAXSCSIDEVICES	26

/* Boolean Values */
#define TRUE 0x01
#define FALSE 0x00

// Number of monitoring flags per Attribute.  See monitorattflags
// below.
#define NMONITOR 4

// Exit codes
#define EXIT_BADCMD    1   // command line did not parse
#define EXIT_BADCONF   2   // problem reading/parsing config file
#define EXIT_STARTUP   3   // problem forking daemon
#define EXIT_PID       4   // problem creating pid file

#define EXIT_NOMEM     8   // out of memory
#define EXIT_CCONST    9   // we hit a compile time constant

#define EXIT_BADDEV    16  // we can't monitor this device
#define EXIT_NODEV     17  // no devices to monitor

#define EXIT_SIGNAL    254 // abort on signal

// If user has requested email warning messages, then this structure
// stores the information about them.
typedef struct mailinfo {
  // number of times an email has been sent
  int logged;
  // time last email was sent, as defined by man 2 time
  time_t lastsent;
  // time problem initially logged
  time_t firstsent;
} mailinfo;

// Used to store a list of devices and options that were in the
// configuration file.
typedef struct configfile_s {
  // Shich line was entry in file; what device type and name?
  int lineno;
  // Indicates corresponding entry number in the list of ata or scsi
  // devices to monitor
  int scsidevicenum;
  int atadevicenum;
  // Initially, tryata and tryscsi indicate which device to try.
  // Ultimately, one is set and the other not set, depending upon
  // which type of device was detected.
  char tryata;
  char tryscsi;
  char *name;
  // which tests have been enabled?
  char smartcheck;
  char usagefailed;
  char prefail;
  char usage;
  char selftest;
  char errorlog;
  // Should we ignore missing capabilities/SMART errors
  char permissive;
  // Disable (1) or Enable (2) device attribute autosave
  char autosave;
  // Disable (1) or Enable (2) automatic offline testing
  char autoofflinetest;
  // mailing information for four of the previous error types plus mailtest
  mailinfo maildata[10];
  // Frequency with which to send emails: 1 - once, 2 - daily, 3 - diminishing
  unsigned char emailfreq;
  // Should we send a test email
  unsigned char emailtest;
  // Execute this command line and include output in emails
  char *emailcmdline;
  // address to send email to
  char *address;
  // counts of ata and self-test errors.  Perhaps ought to be in the
  // atadevices_t structure.
  unsigned char selflogcount;
  int  ataerrorcount;
  // following NMONITOR items each point to 32 bytes, in the form of
  // 32x8=256 single bit flags 
  // valid attribute numbers are from 1 <= x <= 255
  // monitorattflags+0  set means ignore failure if it's a usage attribute
  // monitorattflats+32 set means don't track attribute
  // monitorattflags+64 set means print raw value when tracking
  // monitorattflags+96 set means track changes in raw value
  unsigned char *monitorattflags;
  // See the end of extern.h for a definition of the array of 256
  // bytes that this points to.
  unsigned char *attributedefs;
  // enables equivalent of -F option for smartctl
  unsigned char fixfirmwarebug;
  // is device a 3ware RAID controller (SCSI interface + ATA drives).
  // If so, this is 1+drive number.
  unsigned char escalade;
  // Don't use the preset vendor options from knowndrives[] for this device.
  char ignorepresets;
  // Show the preset vendor options from knowndrives[] for this device.
  char showpresets;
  // Indicates that device may not always be present.
  char removable;
} cfgfile;


// Used to store list of ATA devices to monitor.
typedef struct atadevices_s {
  struct ata_smart_values *smartval;
  struct ata_smart_thresholds *smartthres;
  cfgfile *cfg;
  char *devicename;
}  atadevices_t;

// used to store a list of SCSI devices to monitor.  Devicename points
// to a malloced name string.
typedef struct scsidevices_s {
  unsigned char SmartPageSupported;
  unsigned char TempPageSupported;
  unsigned char Temperature;
  char *devicename;
  cfgfile *cfg;
} scsidevices_t;


typedef struct changedattribute_s {
  unsigned char newval;
  unsigned char oldval;
  unsigned char id;
  unsigned char prefail;
  unsigned char sameraw;
} changedattribute_t;

// Declare our own printing functions...
void printout(int priority,char *fmt, ...) __attribute__ ((format(printf, 2, 3)));
void printandmail(cfgfile *cfg, int which, int priority, char *fmt, ...) __attribute__ ((format(printf, 4, 5)));   

int ataCheckDevice(atadevices_t *drive);
