/*
 * scsicmds.h
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
 * N.B. What was formerly known as "SMART" are now called "informational
 * exceptions" in recent t10.org drafts (i.e. recent SCSI).
 *
 */


#ifndef SCSICMDS_H_
#define SCSICMDS_H_

#ifndef SCSICMDS_H_CVSID
#define SCSICMDS_H_CVSID "$Id: scsicmds.h,v 1.29 2003/06/01 12:36:12 dpgilbert Exp $\n"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

/* #define SCSI_DEBUG 1 */ /* Comment out to disable command debugging */

/* Following conditional defines bypass inclusion of scsi/scsi.h and
 * scsi/scsi_ioctl.h . Issue will be resolved later ... */
#ifndef TEST_UNIT_READY
#define TEST_UNIT_READY 0x0
#endif
#ifndef LOG_SENSE
#define LOG_SENSE 0x4d
#endif
#ifndef MODE_SENSE
#define MODE_SENSE 0x1a
#endif
#ifndef MODE_SENSE_10
#define MODE_SENSE_10 0x5a
#endif
#ifndef MODE_SELECT
#define MODE_SELECT 0x15
#endif
#ifndef MODE_SELECT_10
#define MODE_SELECT_10 0x55
#endif
#ifndef INQUIRY
#define INQUIRY 0x12
#endif
#ifndef REQUEST_SENSE
#define REQUEST_SENSE  0x03
#endif
#ifndef RECEIVE_DIAGNOSTIC
#define RECEIVE_DIAGNOSTIC  0x1c
#endif
#ifndef SEND_DIAGNOSTIC
#define SEND_DIAGNOSTIC  0x1d
#endif

typedef unsigned char UINT8;
typedef char INT8;
typedef unsigned int UINT32;
typedef int INT32;

#define DXFER_NONE        0
#define DXFER_FROM_DEVICE 1
#define DXFER_TO_DEVICE   2

struct scsi_cmnd_io
{
    UINT8 * cmnd;
    size_t  cmnd_len;
    int dxfer_dir;     /* DXFER_NONE, DXFER_FROM_DEVICE, or DXFER_TO_DEVICE */
    UINT8 * dxferp;
    size_t dxfer_len;
    UINT8 * sensep;     /* ptr to sense buffer when CHECK CONDITION status */
    size_t max_sense_len;
    unsigned timeout;   /* in seconds, 0-> default timeout (60 seconds?) */
    size_t resp_sense_len;  /* output: sense buffer length */
    UINT8 scsi_status;  /* output: 0->ok, 2->CHECK CONDITION, etc ... */
    int resid;          /* Number of bytes requested to be transferred less */
                        /*  actual number transferred (0 if not supported) */
};

struct scsi_sense_disect {
    UINT8 error_code;
    UINT8 sense_key;
    UINT8 asc; 
    UINT8 ascq;
};

/* Useful data from Informational Exception Control mode page (0x1c) */
#define SCSI_IECMP_RAW_LEN 64
struct scsi_iec_mode_page {
    UINT8 requestedCurrent;
    UINT8 gotCurrent;
    UINT8 requestedChangeable;
    UINT8 gotChangeable;
    UINT8 modese_10;
    UINT8 raw_curr[SCSI_IECMP_RAW_LEN];
    UINT8 raw_chg[SCSI_IECMP_RAW_LEN];
};

/* Carrier for Error counter log pages (e.g. read, write, verify ...) */
struct scsiErrorCounter {
    UINT8 gotPC[7];
    UINT8 gotExtraPC;
    unsigned long long counter[8];
};

/* Carrier for Non-medium error log page */
struct scsiNonMediumError {
    UINT8 gotPC0;
    UINT8 gotExtraPC;
    unsigned long long counterPC0;
};

/* SCSI Peripheral types (of interest) */
#define SCSI_PT_DIRECT_ACCESS           0x0
#define SCSI_PT_SEQUENTIAL_ACCESS       0x1
#define SCSI_PT_CDROM                   0x5
#define SCSI_PT_MEDIUM_CHANGER          0x8
#define SCSI_PT_ENCLOSURE               0xd

/* ANSI SCSI-3 Log Pages retrieved by LOG SENSE. */
#define SUPPORTED_LOG_PAGES                     0x00
#define BUFFER_OVERRUN_PAGE                     0x01
#define WRITE_ERROR_COUNTER_PAGE                0x02
#define READ_ERROR_COUNTER_PAGE                 0x03
#define READ_REVERSE_ERROR_COUNTER_PAGE         0x04
#define VERIFY_ERROR_COUNTER_PAGE               0x05
#define NON_MEDIUM_ERROR_PAGE                   0x06
#define LAST_N_ERROR_PAGE                       0x07
#define FORMAT_STATUS_PAGE                      0x08
#define TEMPERATURE_PAGE                        0x0d
#define STARTSTOP_CYCLE_COUNTER_PAGE            0x0e
#define APPLICATION_CLIENT_PAGE                 0x0f
#define SELFTEST_RESULTS_PAGE                   0x10
#define IE_LOG_PAGE                             0x2f

/* See the SSC-2 document at www.t10.org . Earler note: From IBM 
Documentation, see http://www.storage.ibm.com/techsup/hddtech/prodspecs.htm */
#define TAPE_ALERTS_PAGE                         0x2e

/* ANSI SCSI-3 Mode Pages */
#define VENDOR_UNIQUE_PARAMETERS                 0x00
#define READ_WRITE_ERROR_RECOVERY_PARAMETERS     0x01
#define DISCONNECT_RECONNECT_PARAMETERS          0x02
#define FORMAT_DEVICE_PARAMETERS                 0x03
#define RIGID_DISK_DRIVE_GEOMETRY_PARAMETERS     0x04
#define FLEXIBLE_DISK_PARAMETERS                 0x05
#define VERIFY_ERROR_RECOVERY_PARAMETERS         0x07
#define CACHING_PARAMETERS                       0x08
#define PERIPHERAL_DEVICE                        0x09
#define XOR_CONTROL_MODE_PARAMETERS              0x10
#define CONTROL_MODE_PAGE_PARAMETERS             0x0a
#define MEDIUM_TYPES_SUPPORTED                   0x0b
#define NOTCH_PARAMETERS                         0x0c
#define POWER_CONDITION_PARAMETERS               0x0d
#define CD_DEVICE_PARAMETERS                     0x0d
#define CD_AUDIO_CONTROL_PAGE                    0x0e
#define DATA_COMPRESSION_PARAMETERS              0x0f
#define MEDIUM_PARTITION_MODE_PARAMTERES_1       0x11
#define MEDIUM_PARTITION_MODE_PARAMTERES_2       0x12
#define MEDIUM_PARTITION_MODE_PARAMTERES_3       0x13
#define MEDIUM_PARTITION_MODE_PARAMTERES_4       0x14
#define ENCLOSURE_SERVICES_MANAGEMENT            0x14
#define LUN_CONTROL                              0x18
#define PORT_CONTROL                             0x19
#define POWER_CONTROL                            0x1a
#define LUN_MAPPING_PAGE                         0x1b
#define INFORMATIONAL_EXCEPTIONS_CONTROL         0x1c
#define FAULT_FAILURE_REPORTING_PAGE             0x1c
#define ELEMENT_ADDRESS_ASSIGNMENT               0x1d
#define TIMEOUT_AND_PROTECT_PARAMETERS           0x1d
#define TRANSPORT_GEOMETRY_PARAMETERS            0x1e
#define DEVICE_CAPABILITIES                      0x1f
#define CD_CAPABILITIES_AND_MECHANISM_STATUS     0x2a

#define ALL_PARAMETERS                           0x3f

/* defines for useful SCSI Status codes */
#define SCSI_STATUS_CHECK_CONDITION     0x2

/* defines for useful Sense Key codes */
#define SCSI_SK_NOT_READY               0x2
#define SCSI_SK_ILLEGAL_REQUEST         0x5
#define SCSI_SK_UNIT_ATTENTION          0x6

/* defines for useful Additional Sense Codes (ASCs) */
#define SCSI_ASC_UNKNOWN_OPCODE         0x20
#define SCSI_ASC_UNKNOWN_FIELD          0x24
#define SCSI_ASC_UNKNOWN_PARAM          0x26
#define SCSI_ASC_WARNING                0xb
#define SCSI_ASC_IMPENDING_FAILURE      0x5d

/* Simplified error code (negative values as per errno) */
#define SIMPLE_NO_ERROR                 0
#define SIMPLE_ERR_NOT_READY            1
#define SIMPLE_ERR_BAD_OPCODE           2
#define SIMPLE_ERR_BAD_FIELD            3       /* in cbd */
#define SIMPLE_ERR_BAD_PARAM            4       /* in data */


/* defines for functioncode parameter in SENDDIAGNOSTIC function */

#define SCSI_DIAG_NO_SELF_TEST          0x00
#define SCSI_DIAG_DEF_SELF_TEST         0xff
#define SCSI_DIAG_BG_SHORT_SELF_TEST    0x01
#define SCSI_DIAG_BG_EXTENDED_SELF_TEST 0x02
#define SCSI_DIAG_FG_SHORT_SELF_TEST    0x05
#define SCSI_DIAG_FG_EXTENDED_SELF_TEST 0x06
#define SCSI_DIAG_ABORT_SELF_TEST       0x04



#define LOGPAGEHDRSIZE  4

void scsi_do_sense_disect(const struct scsi_cmnd_io * in,
                          struct scsi_sense_disect * out);

const char * scsiErrString(int scsiErr);

/* STANDARD SCSI Commands  */
int scsiTestUnitReady(int device);

int scsiStdInquiry(int device, UINT8 *pBuf, int bufLen);

int scsiInquiryVpd(int device, int vpd_page, UINT8 *pBuf, int bufLen);

int scsiLogSense(int device, int pagenum, UINT8 *pBuf, int bufLen,
                 int known_resp_len);

int scsiModeSense(int device, int pagenum, int pc, UINT8 *pBuf, int bufLen);

int scsiModeSelect(int device, int pagenum, int sp, UINT8 *pBuf, int bufLen);

int scsiModeSense10(int device, int pagenum, int pc, UINT8 *pBuf, int bufLen);

int scsiModeSelect10(int device, int pagenum, int sp, UINT8 *pBuf, int bufLen);

int scsiRequestSense(int device, struct scsi_sense_disect * sense_info);

int scsiSendDiagnostic(int device, int functioncode, UINT8 *pBuf, int bufLen);

int scsiReceiveSiagnostic(int device, int pcv, int pagenum, UINT8 *pBuf,
                      int bufLen);
/* SMART specific commands */

#define CHECK_SMART_BY_LGPG_2F  0x01
#define CHECK_SMART_BY_REQSENSE 0x00

int scsiCheckIE(int device, int method, int hasTempLogPage,
                UINT8 *asc, UINT8 *ascq, UINT8 *currenttemp);

int scsiFetchIECmpage(int device, struct scsi_iec_mode_page *iecp);
int scsi_IsExceptionControlEnabled(const struct scsi_iec_mode_page *iecp);
int scsi_IsWarningEnabled(const struct scsi_iec_mode_page *iecp);
int scsiSetExceptionControlAndWarning(int device, int enabled,
                            const struct scsi_iec_mode_page *iecp);
int scsiDecodeIEModePage(const struct scsi_iec_mode_page *iecp,
        UINT8 *byte_2p, UINT8 *mrie_p, unsigned int *interval_timer_p,
        unsigned int *report_count_p);
void scsiDecodeErrCounterPage(unsigned char * resp,
                              struct scsiErrorCounter *ecp);
void scsiDecodeNonMediumErrPage(unsigned char * resp,
                                struct scsiNonMediumError *nmep);
int scsiFetchExtendedSelfTestTime(int device, int * durationSec);

/* T10 Standard IE Additional Sense Code strings taken from t10.org */

const char* scsiGetIEString(UINT8 asc, UINT8 ascq);
int scsiGetTemp(int device, UINT8 *currenttemp, UINT8 *triptemp);


int scsiSmartIBMOfflineTest(int device);

int scsiSmartDefaultSelfTest(int device);
int scsiSmartShortSelfTest(int device);
int scsiSmartExtendSelfTest(int device);
int scsiSmartShortCapSelfTest(int device);
int scsiSmartExtendCapSelfTest(int device);
int scsiSmartSelfTestAbort(int device);

const char * scsiTapeAlertsTapeDevice(unsigned short code);
const char * scsiTapeAlertsChangerDevice(unsigned short code);
#endif
