/*
 * atacmds.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
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

#ifndef _ATACMDS_H_
#define _ATACMDS_H_

#ifndef ATACMDS_H_CVSID
#define ATACMDS_H_CVSID "$Id: atacmds.h,v 1.48 2003/08/05 16:27:43 ballen4705 Exp $\n"
#endif

#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <sys/fcntl.h>
#include <sys/types.h>


typedef enum {
  // returns no data, just succeeds or fails
  ENABLE,
  DISABLE,
  AUTOSAVE,
  IMMEDIATE_OFFLINE,
  AUTO_OFFLINE,
  STATUS,       // just says if SMART is working or not
  STATUS_CHECK, // says if disk's SMART status is healthy, or failing
  // return 512 bytes of data:
  READ_VALUES,
  READ_THRESHOLDS,
  READ_LOG,
  IDENTIFY,
  PIDENTIFY
} smart_command_set;

/* These defines SHOULD BE in the kernel
   if not we define them */
							  
#ifndef WIN_SMART
#define WIN_SMART		0xb0	
#endif

#ifndef SMART_READ_VALUES
#define SMART_READ_VALUES	0xd0
#endif

#ifndef SMART_READ_THRESHOLDS	
#define SMART_READ_THRESHOLDS	0xd1
#endif

#ifndef SMART_AUTOSAVE 
#define SMART_AUTOSAVE		0xd2
#endif

#ifndef SMART_SAVE
#define SMART_SAVE		0xd3
#endif

#ifndef SMART_IMMEDIATE_OFFLINE	
#define SMART_IMMEDIATE_OFFLINE	0xd4
#endif

#ifndef SMART_READ_LOG_SECTOR
#define SMART_READ_LOG_SECTOR 0xd5
#endif

#ifndef SMART_WRITE_LOG_SECTOR
#define SMART_WRITE_LOG_SECTOR 0xd6
#endif

// The following is obsolete -- don't use it!
#ifndef SMART_WRITE_THRESHOLDS
#define SMART_WRITE_THRESHOLDS  0xd7
#endif

#ifndef SMART_ENABLE
#define SMART_ENABLE		0xd8
#endif

#ifndef SMART_DISABLE
#define SMART_DISABLE		0xd9
#endif

#ifndef SMART_STATUS
#define SMART_STATUS		0xda
#endif

// The following is also marked obsolete in ATA-5
#ifndef SMART_AUTO_OFFLINE
#define SMART_AUTO_OFFLINE	0xdb
#endif

#define OFFLINE_FULL_SCAN		0
#define SHORT_SELF_TEST			1
#define EXTEND_SELF_TEST		2
#define CONVEYANCE_SELF_TEST            3
#define SELECTIVE_SELF_TEST             4
#define ABORT_SELF_TEST                 127
#define SHORT_CAPTIVE_SELF_TEST		129
#define EXTEND_CAPTIVE_SELF_TEST	130
#define CONVEYANCE_CAPTIVE_SELF_TEST    131
#define SELECTIVE_CAPTIVE_SELF_TEST     132
#define CAPTIVE_MASK                    (0x01<<7)

#define NUMBER_ATA_SMART_ATTRIBUTES 	30

#define ATA_SMART_SEC_SIZE		512

#ifndef HDIO_DRIVE_CMD_HDR_SIZE
#define HDIO_DRIVE_CMD_HDR_SIZE 	4
#endif

#ifndef HDIO_DRIVE_TASK_HDR_SIZE
#define HDIO_DRIVE_TASK_HDR_SIZE	7
#endif

/* ata_smart_attribute is the vendor specific in SFF-8035 spec */ 
struct ata_smart_attribute {
  unsigned char id;
  // meaning of flag bits: see MACROS just below
  unsigned short flags; 
  unsigned char current;
  unsigned char worst;
  unsigned char raw[6];
  unsigned char reserv;
} __attribute__ ((packed));

// MACROS to interpret the flags bits in the previous structure.
// These have not been implemented using bitflags and a union, to make
// it portable across bit/little endian and different platforms.

// 0: Prefailure bit

// From SFF 8035i Revision 2 page 19: Bit 0 (pre-failure/advisory bit)
// - If the value of this bit equals zero, an attribute value less
// than or equal to its corresponding attribute threshold indicates an
// advisory condition where the usage or age of the device has
// exceeded its intended design life period. If the value of this bit
// equals one, an attribute value less than or equal to its
// corresponding attribute threshold indicates a prefailure condition
// where imminent loss of data is being predicted.
#define ATTRIBUTE_FLAGS_PREFAILURE(x) (x & 0x01)

// 1: Online bit 

//  From SFF 8035i Revision 2 page 19: Bit 1 (on-line data collection
// bit) - If the value of this bit equals zero, then the attribute
// value is updated only during off-line data collection
// activities. If the value of this bit equals one, then the attribute
// value is updated during normal operation of the device or during
// both normal operation and off-line testing.
#define ATTRIBUTE_FLAGS_ONLINE(x) (x & 0x02)

// 2: Performance bit

// The following are (probably) IBM's or Quantum's definitions for the
// vendor-specific bits:
#define ATTRIBUTE_FLAGS_PERFORMANCE(x) (x & 0x04)

// 3: Errorrate bit
#define ATTRIBUTE_FLAGS_ERRORRATE(x) (x & 0x08)

// 4: Eventcount bit
#define ATTRIBUTE_FLAGS_EVENTCOUNT(x) (x & 0x10)

// 5: Selfpereserving bit
#define ATTRIBUTE_FLAGS_SELFPRESERVING(x) (x & 0x20)


// Last ten bits are reserved for future use


// What follows is the ATA/ATAPI-4 Rev 13 data structure equivalent:
// Table 23 DEVICE SMART DATA STRUCTURE
#if 0
struct ata_smart_values {
  unsigned short int revnumber;
  struct ata_smart_attribute vendor_attributes [NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char offline_data_collection_status;
  unsigned char vendor_specific_363;  //IBM # segments for offline collection
  unsigned short int total_time_to_complete_off_line; // IBM different
  unsigned char vendor_specific_366; // IBM curent segment pointer
  unsigned char offline_data_collection_capability;
  unsigned short int smart_capability;
  unsigned char reserved_370_385;;
  unsigned char vendor_specific_386-510;  // IBM: self-test failure checkpoint
  unsigned char chksum;
} __attribute__ ((packed));
#endif


/* ata_smart_values is format of the read drive Attribute command */
/* see Table 34 of T13/1321D Rev 1 spec (Device SMART data structure) for *some* info */
struct ata_smart_values {
  unsigned short int revnumber;
  struct ata_smart_attribute vendor_attributes [NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char offline_data_collection_status;
  unsigned char self_test_exec_status;  //IBM # segments for offline collection
  unsigned short int total_time_to_complete_off_line; // IBM different
  unsigned char vendor_specific_366; // IBM curent segment pointer
  unsigned char offline_data_collection_capability;
  unsigned short int smart_capability;
  unsigned char errorlog_capability;
  unsigned char vendor_specific_371;  // IBM: self-test failure checkpoint
  unsigned char short_test_completion_time;
  unsigned char extend_test_completion_time;
  unsigned char conveyance_test_completion_time;
  unsigned char reserved_375_385[11];
  unsigned char vendor_specific_386_510[125];
  unsigned char chksum;
} __attribute__ ((packed));

/* Smart Threshold data structures */

/* Vendor attribute of SMART Threshold (compare to ata_smart_attribute above) */
struct ata_smart_threshold_entry {
  unsigned char id;
  unsigned char threshold;
  unsigned char reserved[10];
} __attribute__ ((packed));


/* Format of Read SMART THreshold Command */
/* Compare to ata_smart_values above */
struct ata_smart_thresholds {
  unsigned short int revnumber;
  struct ata_smart_threshold_entry thres_entries[NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char reserved[149];
  unsigned char chksum;
} __attribute__ ((packed));


// Table 42 of T13/1321D Rev 1 spec (Error Data Structure)
struct ata_smart_errorlog_error_struct {
  unsigned char reserved;
  unsigned char error_register;
  unsigned char sector_count;
  unsigned char sector_number;
  unsigned char cylinder_low;
  unsigned char cylinder_high;
  unsigned char drive_head;
  unsigned char status;
  unsigned char extended_error[19];
  unsigned char state;
  unsigned short timestamp;
} __attribute__ ((packed));

// Table 41 of T13/1321D Rev 1 spec (Command Data Structure)
struct ata_smart_errorlog_command_struct {
  unsigned char devicecontrolreg;
  unsigned char featuresreg;
  unsigned char sector_count;
  unsigned char sector_number;
  unsigned char cylinder_low;
  unsigned char cylinder_high;
  unsigned char drive_head;
  unsigned char commandreg;
  unsigned int timestamp;
} __attribute__ ((packed));

// Table 40 of T13/1321D Rev 1 spec (Error log data structure)
struct ata_smart_errorlog_struct {
  struct ata_smart_errorlog_command_struct commands[5];
  struct ata_smart_errorlog_error_struct error_struct;
}  __attribute__ ((packed));

// Table 39 of T13/1321D Rev 1 spec (SMART error log sector)
struct ata_smart_errorlog {
  unsigned char revnumber;
  unsigned char error_log_pointer;
  struct ata_smart_errorlog_struct errorlog_struct[5];
  unsigned short int ata_error_count;
  unsigned char reserved[57];
  unsigned char checksum;
} __attribute__ ((packed));

// Table 45 of T13/1321D Rev 1 spec (Self-test log descriptor entry)
struct ata_smart_selftestlog_struct {
  unsigned char selftestnumber; // Sector number register
  unsigned char selfteststatus;
  unsigned short int timestamp;
  unsigned char selftestfailurecheckpoint;
  unsigned int lbafirstfailure;
  unsigned char vendorspecific[15];
} __attribute__ ((packed));

// Table 44 of T13/1321D Rev 1 spec (Self-test log data structure)
struct ata_smart_selftestlog {
  unsigned short int revnumber;
  struct ata_smart_selftestlog_struct selftest_struct[21];
  unsigned char vendorspecific[2];
  unsigned char mostrecenttest;
  unsigned char reserved[2];
  unsigned char chksum;
} __attribute__ ((packed));



// SMART LOG DIRECTORY Table 52 of T13/1532D Vol 1 Rev 1a
struct ata_smart_log_entry {
  unsigned char numsectors;
  unsigned char reserved;
} __attribute__ ((packed));

struct ata_smart_log_directory {
  unsigned short int logversion;
  struct ata_smart_log_entry entry[255];
} __attribute__ ((packed));


// SMART SELECTIVE SELF-TEST LOG Table 61 of T13/1532D Volume 1
// Revision 3
struct test_span {
  unsigned long long start;
  unsigned long long end;
} __attribute__ ((packed));

struct ata_selective_self_test_log {
  unsigned short     logversion;
  struct test_span   span[5];
  unsigned char      reserved[337-82+1];
  unsigned char      vendor_specific1[491-338+1];
  unsigned long long currentlba;
  unsigned short     currentspan;
  unsigned short     flags;
  unsigned char      vendor_specific2[507-504+1];
  unsigned short     pendingtime;
  unsigned char      undefined;  // Error in specs!
  unsigned char      checksum;
} __attribute__ ((packed));

/* Read S.M.A.R.T information from drive */
int ataReadHDIdentity(int device, struct hd_driveid *buf);
int ataReadSmartValues(int device,struct ata_smart_values *);
int ataReadSmartThresholds(int device, struct ata_smart_thresholds *);
int ataReadErrorLog(int device, struct ata_smart_errorlog *);
int ataReadSelfTestLog(int device, struct ata_smart_selftestlog *);
int ataSmartStatus(int device);
int ataSetSmartThresholds(int device, struct ata_smart_thresholds *);
int ataReadLogDirectory(int device, struct ata_smart_log_directory *);	

/* Enable/Disable SMART on device */
int ataEnableSmart ( int device );
int ataDisableSmart (int device );
int ataEnableAutoSave(int device);
int ataDisableAutoSave(int device);

/* Automatic Offline Testing */
int ataEnableAutoOffline ( int device );
int ataDisableAutoOffline (int device );

/* S.M.A.R.T. test commands */
int ataSmartOfflineTest (int device);
int ataSmartExtendSelfTest (int device);
int ataSmartShortSelfTest (int device);
int ataSmartShortCapSelfTest (int device);
int ataSmartExtendCapSelfTest (int device);
int ataSmartSelfTestAbort (int device);

// Returns the latest compatibility of ATA/ATAPI Version the device
// supports. Returns -1 if Version command is not supported
int ataVersionInfo (const char **description, struct hd_driveid *drive, unsigned short *minor);

// If SMART supported, this is guaranteed to return 1 if SMART is enabled, else 0.
int ataDoesSmartWork(int device);

// returns 1 if SMART supported, 0 if not supported or can't tell
int ataSmartSupport ( struct hd_driveid *drive);

// Return values:
//  1: SMART enabled
//  0: SMART disabled
// -1: can't tell if SMART is enabled -- try issuing ataDoesSmartWork command to see
int ataIsSmartEnabled(struct hd_driveid *drive);

/* Check SMART for Threshold failure */
// onlyfailed=0 : are or were any age or prefailure attributes <= threshold
// onlyfailed=1:  are any prefailure attributes <= threshold now
int ataCheckSmart ( struct ata_smart_values *data, struct ata_smart_thresholds *thresholds, int onlyfailed);

int ataSmartStatus2(int device);

// int isOfflineTestTime ( struct ata_smart_values data)
//  returns S.M.A.R.T. Offline Test Time in seconds
int isOfflineTestTime ( struct ata_smart_values *data);

int isShortSelfTestTime ( struct ata_smart_values *data);

int isExtendedSelfTestTime ( struct ata_smart_values *data);

int isSmartErrorLogCapable ( struct ata_smart_values *data);

int isGeneralPurposeLoggingCapable(struct hd_driveid *identity);

int isSupportExecuteOfflineImmediate ( struct ata_smart_values *data);

int isSupportAutomaticTimer ( struct ata_smart_values *data);

int isSupportOfflineAbort ( struct ata_smart_values *data);

int isSupportOfflineSurfaceScan ( struct ata_smart_values *data);

int isSupportSelfTest (struct ata_smart_values *data);

int isSupportConveyanceSelfTest(struct ata_smart_values *data);

int isSupportSelectiveSelfTest(struct ata_smart_values *data);

int ataSmartTest(int device, int testtype);

int TestTime(struct ata_smart_values *data,int testtype);

// Prints the raw value (with appropriate formatting) into the
// character string out.
long long ataPrintSmartAttribRawValue(char *out, 
				      struct ata_smart_attribute *attribute,
				      unsigned char *defs);

// Prints Attribute Name for standard SMART attributes. Writes a
// 30 byte string with attribute name into output
void ataPrintSmartAttribName(char *output, unsigned char id, unsigned char val);

// This checks the n'th attribute in the attribute list, NOT the
// attribute with id==n.  If the attribute does not exist, or the
// attribute is > threshold, then returns zero.  If the attribute is
// <= threshold (failing) then we the attribute number if it is a
// prefail attribute.  Else we return minus the attribute number if it
// is a usage attribute.
int ataCheckAttribute(struct ata_smart_values *data,
		      struct ata_smart_thresholds *thresholds,
		      int n);

// External handler function, for when a checksum is not correct.  Can
// simply return if no action is desired, or can print error messages
// as needed, or exit.  Is passed a string with the name of the Data
// Structure with the incorrect checksum.
void checksumwarning(const char *string);

extern const char *vendorattributeargs[];

// function to parse pairs like "9,minutes" or "220,temp".  See end of
// extern.h for definition of defs[].  Returns 0 if pair recognized,
// else 1 if there is a problem.
int parse_attribute_def(char *pair, unsigned char *defs);

// Function to return a string containing a list of the arguments in
// vendorattributeargs[].  Returns NULL if the required memory can't
// be allocated.
char *create_vendor_attribute_arg_list(void);

#endif /* _ATACMDS_H_ */