/*
 * scsiprint.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003 Douglas Gilbert <dougg@torque.net>
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


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "extern.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"

#define GBUF_SIZE 65535

const char* scsiprint_c_cvsid="$Id: scsiprint.c,v 1.44 2003/05/04 13:11:21 dpgilbert Exp $"
EXTERN_H_CVSID SCSICMDS_H_CVSID SCSIPRINT_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// control block which points to external global control variables
extern smartmonctrl *con;

UINT8 gBuf[GBUF_SIZE];
#define LOG_RESP_LEN 252

/* Log pages supported */
static int gSmartPage = 0;
static int gTempPage = 0;
static int gSelfTestPage = 0;
static int gStartStopPage = 0;
static int gTapeAlertsPage = 0;


static void scsiGetSupportedLogPages(int device)
{
    int i, err;

    if ((err = scsiLogSense(device, SUPPORTED_LOG_PAGES, gBuf, 
                            LOG_RESP_LEN))) {
        if (con->reportscsiioctl > 0)
            pout("Log Sense for supported pages failed [%s]\n", 
                 scsiErrString(err)); 
        return;
    } 

    for (i = 4; i < gBuf[3] + LOGPAGEHDRSIZE; i++) {
        switch (gBuf[i])
        {
            case TEMPERATURE_PAGE:
                gTempPage = 1;
                break;
            case STARTSTOP_CYCLE_COUNTER_PAGE:
                gStartStopPage = 1;
                break;
            case SELFTEST_RESULTS_PAGE:
                gSelfTestPage = 1;
                break;
            case IE_LOG_PAGE:
                gSmartPage = 1;
                break;
            case TAPE_ALERTS_PAGE:
                gTapeAlertsPage = 1;
                break;
            default:
                break;
        }
    }
}

void scsiGetSmartData(int device)
{
    UINT8 asc;
    UINT8 ascq;
    UINT8 currenttemp = 0;
    const char * cp;
    int err;

    QUIETON(con);
    if ((err = scsiCheckIE(device, gSmartPage, gTempPage,
                           &asc, &ascq, &currenttemp))) {
        /* error message already announced */
        QUIETOFF(con);
        return;
    }
    QUIETOFF(con);
    cp = scsiGetIEString(asc, ascq);
    if (cp) {
        QUIETON(con);
        pout("SMART Sense: %s [asc=%x,ascq=%x]\n", cp, asc, ascq); 
        QUIETOFF(con);
    } else
        pout("SMART Sense: Ok!\n");

    if (currenttemp && !gTempPage) {
        if (255 != currenttemp)
            pout("Current Drive Temperature:     %d C\n", currenttemp);
        else
            pout("Current Drive Temperature:     <not available>\n");
    }
}


// Returns number of logged errors or zero if none or -1 if fetching
// TapeAlerts fails
static char *severities = "CWI";

static int scsiGetTapeAlertsData(int device, int peripheral_type)
{
    unsigned short pagelength;
    unsigned short parametercode;
    int i, err;
    char *s;
    const char *ts;
    int failures = 0;

    QUIETON(con);
    if ((err = scsiLogSense(device, TAPE_ALERTS_PAGE, gBuf, LOG_RESP_LEN))) {
        pout("scsiGetTapesAlertData Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return -1;
    }
    if (gBuf[0] != 0x2e) {
        pout("TapeAlerts Log Sense Failed\n");
        QUIETOFF(con);
        return -1;
    }
    pagelength = (unsigned short) gBuf[2] << 8 | gBuf[3];

    for (s=severities; *s; s++) {
	for (i = 4; i < pagelength; i += 5) {
	    parametercode = (unsigned short) gBuf[i] << 8 | gBuf[i+1];

	    if (gBuf[i + 4]) {
		ts = SCSI_PT_MEDIUM_CHANGER == peripheral_type ?
		    scsiTapeAlertsChangerDevice(parametercode) :
		    scsiTapeAlertsTapeDevice(parametercode);
		if (*ts == *s) {
		    if (!failures)
			pout("TapeAlert Errors (C=Critical, W=Warning, I=Informational):\n");
		    pout("[0x%02x] %s\n", parametercode, ts);
		    failures += 1; 
		}
	    }
	}
    }
    QUIETOFF(con);

    if (! failures)
        pout("TapeAlert: Ok!\n");

    return failures;
}

void scsiGetStartStopData(int device)
{
    UINT32 currentStartStop;
    UINT32 recommendedStartStop; 
    int err, len, k;
    char str[6];

    if ((err = scsiLogSense(device, STARTSTOP_CYCLE_COUNTER_PAGE, gBuf,
                            LOG_RESP_LEN))) {
        QUIETON(con);
        pout("scsiGetStartStopData Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return;
    }
    if (gBuf[0] != STARTSTOP_CYCLE_COUNTER_PAGE) {
        QUIETON(con);
        pout("StartStop Log Sense Failed, page mismatch\n");
        QUIETOFF(con);
        return;
    }
    len = ((gBuf[2] << 8) | gBuf[3]) + 4;
    if (len > 13) {
        for (k = 0; k < 2; ++k)
            str[k] = gBuf[12 + k];
        str[k] = '\0';
        pout("Manufactured in week %s of year ", str);
        for (k = 0; k < 4; ++k)
            str[k] = gBuf[8 + k];
        str[k] = '\0';
        pout("%s\n", str);
    }
    if (len > 39) {
        recommendedStartStop = (gBuf[28] << 24) | (gBuf[29] << 16) |
                               (gBuf[30] << 8) | gBuf[31];
        currentStartStop = (gBuf[36] << 24) | (gBuf[37] << 16) |
                           (gBuf[38] << 8) | gBuf[39];
        pout("Current start stop count:      %u times\n", currentStartStop);
        pout("Recommended start stop count:  %u times\n", 
             recommendedStartStop);
    }
} 

static void scsiPrintErrorCounterLog(int device)
{
    struct scsiErrorCounter errCounterArr[3];
    struct scsiErrorCounter * ecp;
    struct scsiNonMediumError nme;
    int found[3] = {0, 0, 0};
    const char * pageNames[3] = {"read:   ", "write:  ", "verify: "};
    int k;
    double processed_gb;

    if (0 == scsiLogSense(device, READ_ERROR_COUNTER_PAGE, gBuf, 
                          LOG_RESP_LEN)) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[0]);
        found[0] = 1;
    }
    if (0 == scsiLogSense(device, WRITE_ERROR_COUNTER_PAGE, gBuf, 
                          LOG_RESP_LEN)) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[1]);
        found[1] = 1;
    }
    if (0 == scsiLogSense(device, VERIFY_ERROR_COUNTER_PAGE, gBuf, 
                          LOG_RESP_LEN)) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[2]);
        ecp = &errCounterArr[2];
        for (k = 0; k < 7; ++k) {
            if (ecp->gotPC[k] && ecp->counter[k]) {
                found[2] = 1;
                break;
            }
        }
    }
    if (found[0] || found[1] || found[2]) {
        pout("\nError counter log:\n");
        pout("          Errors Corrected    Total      Total   "
             "Correction     Gigabytes    Total\n");
        pout("              delay:       [rereads/    errors   "
             "algorithm      processed    uncorrected\n");
        pout("            minor | major  rewrites]  corrected  "
             "invocations   [10^9 bytes]  errors\n");
        for (k = 0; k < 3; ++k) {
            if (! found[k])
                continue;
            ecp = &errCounterArr[k];
            pout("%s%8llu %8llu  %8llu  %8llu   %8llu", 
                 pageNames[k], ecp->counter[0], ecp->counter[1], 
                 ecp->counter[2], ecp->counter[3], ecp->counter[4]);
            processed_gb = ecp->counter[5] / 1000000000.0;
            pout("   %12.3f    %8llu\n", processed_gb, ecp->counter[6]);
        }
    }
    else 
        pout("\nNo Error counter log to report\n");
    if (0 == scsiLogSense(device, NON_MEDIUM_ERROR_PAGE, gBuf, 
                          LOG_RESP_LEN)) {
        scsiDecodeNonMediumErrPage(gBuf, &nme);
        if (nme.gotPC0)
            pout("\nNon-medium error count: %8llu\n", nme.counterPC0);
    }
}

const char * self_test_code[] = {
        "Default         ", 
        "Background short", 
        "Background long ", 
        "Reserved(3)     ",
        "Abort background", 
        "Foreground short", 
        "Foreground long ",
        "Reserved(7)     "
};

const char * self_test_result[] = {
        "Completed                ",
        "Interrupted ('-X' switch)",
        "Interrupted (bus reset ?)",
        "Unknown error, incomplete",
        "Completed, segment failed",
        "Failed in first segment  ",
        "Failed in second segment ",
        "Failed in segment -->    ",
        "Reserved(8)              ", 
        "Reserved(9)              ", 
        "Reserved(10)             ", 
        "Reserved(11)             ", 
        "Reserved(12)             ", 
        "Reserved(13)             ", 
        "Reserved(14)             ",
        "Self test in progress ..."
};

// See Working Draft SCSI Primary Commands - 3 (SPC-3) pages 231-233
// T10/1416-D Rev 10
static int scsiPrintSelfTest(int device)
{
    int num, k, n, res, err, durationSec;
    int noheader = 1;
    UINT8 * ucp;
    unsigned long long ull=0;

    if ((err = scsiLogSense(device, SELFTEST_RESULTS_PAGE, gBuf, 
                            LOG_RESP_LEN))) {
        QUIETON(con);
        pout("scsiPrintSelfTest Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    if (gBuf[0] != SELFTEST_RESULTS_PAGE) {
        QUIETON(con);
        pout("Self-test Log Sense Failed, page mismatch\n");
        QUIETOFF(con);
        return 1;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3];
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        QUIETON(con);
        pout("Self-test Log Sense length is 0x%x not 0x190 bytes\n",num);
        QUIETOFF(con);
        return 1;
    }
    // loop through the twenty possible entries
    for (k = 0, ucp = gBuf + 4; k < 20; ++k, ucp += 20 ) {
        int i;

        // timestamp in power-on hours (or zero if test in progress)
        n = (ucp[6] << 8) | ucp[7];

        // The spec says "all 20 bytes will be zero if no test" but
        // DG has found otherwise.  So this is a heuristic.
        if ((0 == n) && (0 == ucp[4]))
            break;

        // only print header if needed
        if (noheader) {
            pout("\nSMART Self-test log\n");
            pout("Num  Test              Status                 segment  "
                   "LifeTime  LBA_first_err [SK ASC ASQ]\n");
            pout("     Description                              number   "
                   "(hours)\n");
            noheader=0;
        }

        // print parameter code (test number) & self-test code text
        pout("#%2d  %s", (ucp[0] << 8) | ucp[1], 
            self_test_code[(ucp[4] >> 5) & 0x7]);

        // self-test result
        res = ucp[4] & 0xf;
        pout("  %s", self_test_result[res]);

        // self-test number identifies test that failed and consists
        // of either the number of the segment that failed during
        // the test, or the number of the test that failed and the
        // number of the segment in which the test was run, using a
        // vendor-specific method of putting both numbers into a
        // single byte.
        if (ucp[5])
            pout(" %3d",  (int)ucp[5]);
        else
            pout("   -");

        // print time that the self-test was completed
        if (n==0 && res==0xf)
        // self-test in progress
            pout("   NOW");
        else   
            pout(" %5d", n);
          
        // construct 8-byte integer address of first failure
        for (i = 0; i < 8; i++) {
            ull <<= 8;
            ull |= ucp[i+8];
        }
        // print Address of First Failure, if sensible
        if ((0xffffffffffffffffULL != ull) && (res > 0) && ( res < 0xf))
            pout("  0x%16llx", ull);
        else
            pout("                   -");

        // if sense key nonzero, then print it, along with
        // additional sense code and additional sense code qualifier
        if (ucp[16] & 0xf)
            pout(" [0x%x 0x%x 0x%x]\n", ucp[16] & 0xf, ucp[17], ucp[18]);
        else
            pout(" [-   -    -]\n");
    }

    // if header never printed, then there was no output
    if (noheader)
        pout("No self-tests have been logged\n");
    else
        pout("\n");
    if ((0 == scsiFetchExtendedSelfTestTime(device, &durationSec)) &&
        (durationSec > 0))
        pout("Long (extended) Self Test duration: %d seconds "
             "[%.1f minutes]\n", durationSec, durationSec / 60.0);
    return 0;
}

static const char * peripheral_dt_arr[] = {
        "disk",
        "tape",
        "printer",
        "processor",
        "optical disk(4)",
        "CD/DVD",
        "scanner",
        "optical disk(7)",
        "medium changer",
        "communications",
        "graphics(10)",
        "graphics(11)",
        "storage array",
        "enclosure",
        "simplified disk",
        "optical card reader"
};
 
/* Returns 0 on success */
static int scsiGetDriveInfo(int device, UINT8 * peripheral_type, int all)
{
    char manufacturer[9];
    char product[17];
    char revision[5];
    char timedatetz[64];
    struct scsi_iec_mode_page iec;
    int err, len;
    int is_tape = 0;
    int peri_dt = 0;
        
    memset(gBuf, 0, 36);
    if ((err = scsiStdInquiry(device, gBuf, 36))) {
        QUIETON(con);
        pout("Standard Inquiry failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    len = gBuf[4] + 5;
    peri_dt = gBuf[0] & 0x1f;
    if (peripheral_type)
        *peripheral_type = peri_dt;
    if (! all)
	return 0;

    if (len >= 36) {
        memset(manufacturer, 0, sizeof(manufacturer));
        strncpy(manufacturer, &gBuf[8], 8);
     
        memset(product, 0, sizeof(product));
        strncpy(product, &gBuf[16], 16);
            
        memset(revision, 0, sizeof(revision));
        strncpy(revision, &gBuf[32], 4);
        pout("Device: %s %s Version: %s\n", manufacturer, product, revision);
        if (0 == scsiInquiryVpd(device, 0x80, gBuf, 64)) {
            /* should use VPD page 0x83 and fall back to this page (0x80)
             * if 0x83 not supported. NAA requires a lot of decoding code */
            len = gBuf[3];
            gBuf[4 + len] = '\0';
            pout("Serial number: %s\n", &gBuf[4]);
        }
    } else {
        QUIETON(con);
        pout("Short INQUIRY response, skip product id\n");
        QUIETOFF(con);
    }
    // print SCSI peripheral device type
    if (peri_dt < (sizeof(peripheral_dt_arr) / sizeof(peripheral_dt_arr[0])))
        pout("Device type: %s\n", peripheral_dt_arr[peri_dt]);
    else
        pout("Device type: <%d>\n", peri_dt);

    // print current time and date and timezone
    dateandtimezone(timedatetz);
    pout("Local Time is: %s\n", timedatetz);

    if ((SCSI_PT_SEQUENTIAL_ACCESS == *peripheral_type) ||
        (SCSI_PT_MEDIUM_CHANGER == *peripheral_type))
        is_tape = 1;
    // See if unit accepts SCSI commmands from us
    if ((err = scsiTestUnitReady(device))) {
        if (1 == err) {
            QUIETON(con);
            pout("device is NOT READY (media absent, spun down, etc)\n");
            QUIETOFF(con);
        } else {
            QUIETON(con);
            pout("device Test Unit Ready  [%s]\n", scsiErrString(err));
            QUIETOFF(con);
        }
	return 0;
    }
   
    if ((err = scsiFetchIECmpage(device, &iec))) {
	if (!is_tape) {
            QUIETON(con);
	    pout("Device does not support SMART [%s]\n", 
		 scsiErrString(err));
            QUIETOFF(con);
        }
        return 0;
    }
    if (!is_tape)
        pout("Device supports SMART and is %s\n",
             (scsi_IsExceptionControlEnabled(&iec)) ? "Enabled" : "Disabled");
    pout("%s\n", (scsi_IsWarningEnabled(&iec)) ? 
                  "Temperature Warning Enabled" :
                  "Temperature Warning Disabled or Not Supported");
    return 0;
}

static int scsiSmartEnable(int device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec))) {
        QUIETON(con);
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    if ((err = scsiSetExceptionControlAndWarning(device, 1, &iec))) {
        QUIETON(con);
        pout("unable to enable Exception control and warning [%s]\n",
             scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    /* Need to refetch 'iec' since could be modified by previous call */
    if ((err = scsiFetchIECmpage(device, &iec))) {
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        return 1;
    }
    pout("Informational Exceptions (SMART) %s\n",
         scsi_IsExceptionControlEnabled(&iec) ? "enabled" : "disabled");
    pout("Temperature warning %s\n",
         scsi_IsWarningEnabled(&iec) ? "enabled" : "disabled");
    return 0;
}
        
static int scsiSmartDisable(int device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec))) {
        QUIETON(con);
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    if ((err = scsiSetExceptionControlAndWarning(device, 0, &iec))) {
        QUIETON(con);
        pout("unable to disable Exception control and warning [%s]\n",
             scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    /* Need to refetch 'iec' since could be modified by previous call */
    if ((err = scsiFetchIECmpage(device, &iec))) {
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        return 1;
    }
    pout("Informational Exceptions (SMART) %s\n",
         scsi_IsExceptionControlEnabled(&iec) ? "enabled" : "disabled");
    pout("Temperature warning %s\n",
         scsi_IsWarningEnabled(&iec) ? "enabled" : "disabled");
    return 0;
}

void scsiPrintTemp(int device)
{
    UINT8 temp = 0;
    UINT8 trip = 0;

    if (scsiGetTemp(device, &temp, &trip))
        return;
  
    if (temp) {
        if (255 != temp)
            pout("Current Drive Temperature:     %d C\n", temp);
        else
            pout("Current Drive Temperature:     <not available>\n");
    }
    if (trip)
        pout("Drive Trip Temperature:        %d C\n", trip);
}

// Compares failure type to policy in effect, and either exits or
// simply returns to the calling routine.
static void failuretest(int type, int returnvalue)
{
    // If this is an error in an "optional" SMART command
    if (type == OPTIONAL_CMD) {
        if (con->conservative) {
            pout("An optional SMART command has failed: exiting.\n"
                 "To continue, set the tolerance level to something other "
                 "than 'conservative'\n");
            exit(returnvalue);
        }
    }
    return;
}


/* Main entry point used by smartctl command. Return 0 for success */
int scsiPrintMain(const char *dev_name, int fd)
{
    int checkedSupportedLogPages = 0;
    UINT8 peripheral_type = 0;
    int returnval=0;
    int res;

    if (scsiGetDriveInfo(fd, &peripheral_type, con->driveinfo)) {
        pout("Smartctl: SCSI device INQUIRY Failed\n\n");
        failuretest(MANDATORY_CMD, returnval |= FAILID);
    }

    if (con->smartenable) {
        if (scsiSmartEnable(fd))
            failuretest(MANDATORY_CMD, returnval |= FAILSMART);
    }

    if (con->smartdisable) {
        if (scsiSmartDisable(fd))
            failuretest(MANDATORY_CMD,returnval |= FAILSMART);
    }

    if (con->checksmart) {
        scsiGetSupportedLogPages(fd);
        checkedSupportedLogPages = 1;
        if ((SCSI_PT_SEQUENTIAL_ACCESS == peripheral_type) ||
            (SCSI_PT_MEDIUM_CHANGER == peripheral_type)) { /* tape device */
            if (gTapeAlertsPage) {
		if (con->driveinfo)
		    pout("TapeAlert Supported\n");
                if (-1 == scsiGetTapeAlertsData(fd, peripheral_type))
                    failuretest(OPTIONAL_CMD, returnval |= FAILSMART);
	    }
	    else
		pout("TapeAlert Not Supported\n");
            if (gTempPage)
                scsiPrintTemp(fd);         
            if (gStartStopPage)
                scsiGetStartStopData(fd);
        } else { /* disk, cd/dvd, enclosure, etc */
            scsiGetSmartData(fd);
            if (gTempPage)
                scsiPrintTemp(fd);         
            if (gStartStopPage)
                scsiGetStartStopData(fd);
        }
    }   
    if (con->smarterrorlog)
        scsiPrintErrorCounterLog(fd);
    if (con->smartselftestlog) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(fd);
        res = 0;
        if (gSelfTestPage)
            res = scsiPrintSelfTest(fd);
        else {
            pout("Warning: device does not support Self Test Logging\n");
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    if (con->smartexeoffimmediate) {
        if (scsiSmartDefaultSelfTest(fd)) {
            pout( "Default Self Test Failed\n");
            return returnval;
        }
        pout("Default Self Test Successful\n");
    }
    if (con->smartshortcapselftest) {
        if (scsiSmartShortCapSelfTest(fd)) {
            pout("Short Foreground Self Test Failed\n");
            return returnval;
        }
        pout("Short Foreground Self Test Successful\n");
    }
    if (con->smartshortselftest ) { 
        if ( scsiSmartShortSelfTest(fd)) {
            pout("Short Background Self Test Failed\n");
            return returnval;
        }
        pout("Short Background Self Test has begun\n");
        pout("Use smartctl -X to abort test\n");
    }
    if (con->smartextendselftest) {
        if (scsiSmartExtendSelfTest(fd)) {
            pout("Extended Background Self Test Failed\n");
            return returnval;
        }
        pout("Extended Background Self Test has begun\n");
        pout("Use smartctl -X to abort test\n");        
    }
    if (con->smartextendcapselftest) {
        if (scsiSmartExtendCapSelfTest(fd)) {
            pout("Extended Foreground Self Test Failed\n");
            return returnval;
        }
        pout("Extended Foreground Self Test Successful\n");
    }
    if (con->smartselftestabort) {
        if (scsiSmartSelfTestAbort(fd)) {
            pout("Self Test Abort Failed\n");
            return returnval;
        }
        pout("Self Test returned without error\n");
    }           
    return returnval;
}