/*
 * os_win32.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-6 Christian Franke <smartmontools-support@lists.sourceforge.net>
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
 */

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "extern.h"
extern smartmonctrl * con; // con->permissive,reportataioctl
#include "scsicmds.h"
#include "utility.h"
extern int64_t bytes; // malloc() byte count

#include <errno.h>
#ifdef _DEBUG
#include <assert.h>
#else
#define assert(x) /**/
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h> // offsetof()
#include <io.h> // access()

#define ARGUSED(x) ((void)(x))

// Macro to check constants at compile time using a dummy typedef
#define ASSERT_CONST(c, n) \
  typedef char assert_const_##c[((c) == (n)) ? 1 : -1]
#define ASSERT_SIZEOF(t, n) \
  typedef char assert_sizeof_##t[(sizeof(t) == (n)) ? 1 : -1]


// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id: os_win32.cpp,v 1.42 2006/09/27 21:42:03 chrfranke Exp $"
ATACMDS_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;


#ifndef HAVE_GET_OS_VERSION_STR
#error define of HAVE_GET_OS_VERSION_STR missing in config.h
#endif

// Return build host and OS version as static string
const char * get_os_version_str()
{
	static char vstr[sizeof(SMARTMONTOOLS_BUILD_HOST)-3-1+sizeof("-2003r2-sp2.1")+13];
	char * const vptr = vstr+sizeof(SMARTMONTOOLS_BUILD_HOST)-3-1;
	const int vlen = sizeof(vstr)-(sizeof(SMARTMONTOOLS_BUILD_HOST)-3);

	OSVERSIONINFOEXA vi;
	const char * w;

	// remove "-pc" to avoid long lines
	assert(!strncmp(SMARTMONTOOLS_BUILD_HOST+5, "pc-", 3));
	strcpy(vstr, "i686-"); strcpy(vstr+5, SMARTMONTOOLS_BUILD_HOST+5+3);
	assert(vptr == vstr+strlen(vstr) && vptr+vlen+1 == vstr+sizeof(vstr));

	memset(&vi, 0, sizeof(vi));
	vi.dwOSVersionInfoSize = sizeof(vi);
	if (!GetVersionExA((OSVERSIONINFOA *)&vi)) {
		memset(&vi, 0, sizeof(vi));
		vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
		if (!GetVersionExA((OSVERSIONINFOA *)&vi))
			return vstr;
	}

	if (vi.dwPlatformId > 0xff || vi.dwMajorVersion > 0xff || vi.dwMinorVersion > 0xff)
		return vstr;

	switch (vi.dwPlatformId << 16 | vi.dwMajorVersion << 8 | vi.dwMinorVersion) {
	  case VER_PLATFORM_WIN32_WINDOWS<<16|0x0400| 0:
		w = (vi.szCSDVersion[1] == 'B' ||
		     vi.szCSDVersion[1] == 'C'     ? "95-osr2" : "95");    break;
	  case VER_PLATFORM_WIN32_WINDOWS<<16|0x0400|10:
		w = (vi.szCSDVersion[1] == 'A'     ? "98se"    : "98");    break;
	  case VER_PLATFORM_WIN32_WINDOWS<<16|0x0400|90: w = "me";     break;
	//case VER_PLATFORM_WIN32_NT     <<16|0x0300|51: w = "nt3.51"; break;
	  case VER_PLATFORM_WIN32_NT     <<16|0x0400| 0: w = "nt4";    break;
	  case VER_PLATFORM_WIN32_NT     <<16|0x0500| 0: w = "2000";   break;
	  case VER_PLATFORM_WIN32_NT     <<16|0x0500| 1:
		w = (!GetSystemMetrics(87/*SM_MEDIACENTER*/) ?   "xp"
		                                             :   "xp-mc"); break;
	  case VER_PLATFORM_WIN32_NT     <<16|0x0500| 2:
		w = (!GetSystemMetrics(89/*SM_SERVERR2*/) ?      "2003"
		                                             :   "2003r2"); break;
	  case VER_PLATFORM_WIN32_NT     <<16|0x0600| 0: w = "vista";  break;
	  default: w = 0; break;
	}

	if (!w)
		snprintf(vptr, vlen, "-%s%lu.%lu",
			(vi.dwPlatformId==VER_PLATFORM_WIN32_NT ? "nt" : "9x"),
			vi.dwMajorVersion, vi.dwMinorVersion);
	else if (vi.wServicePackMinor)
		snprintf(vptr, vlen, "-%s-sp%u.%u", w, vi.wServicePackMajor, vi.wServicePackMinor);
	else if (vi.wServicePackMajor)
		snprintf(vptr, vlen, "-%s-sp%u", w, vi.wServicePackMajor);
	else
		snprintf(vptr, vlen, "-%s", w);
	return vstr;
}


static int ata_open(int drive, const char * options, int port);
static void ata_close(int fd);
static int ata_scan(unsigned long * drives, int * rdriveno, unsigned long * rdrives);
static const char * ata_get_def_options(void);

static int aspi_open(unsigned adapter, unsigned id);
static void aspi_close(int fd);
static int aspi_scan(unsigned long * drives);


static int is_permissive()
{
	if (!con->permissive) {
		pout("To continue, add one or more '-T permissive' options.\n");
		return 0;
	}
	con->permissive--;
	return 1;
}

static const char * skipdev(const char * s)
{
	return (!strncmp(s, "/dev/", 5) ? s + 5 : s);
}


// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type (const char * dev_name)
{
	dev_name = skipdev(dev_name);
	if (!strncmp(dev_name, "hd", 2))
		return CONTROLLER_ATA;
	if (!strncmp(dev_name, "scsi", 4))
		return CONTROLLER_SCSI;
	return CONTROLLER_UNKNOWN;
}


// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist), the
// others each contain null-terminated character strings.
int make_device_names (char*** devlist, const char* type)
{
	unsigned long drives[3];
	int rdriveno[2];
	unsigned long rdrives[2];
	int i, j, n, nmax, sz;
	const char * path;

	drives[0] = drives[1] = drives[2] = 0;
	rdriveno[0] = rdriveno[1] = -1;
	rdrives[0] = rdrives[1] = 0;
	
	if (!strcmp(type, "ATA")) {
		// bit i set => drive i present
		n = ata_scan(drives, rdriveno, rdrives);
		path = "/dev/hda";
		nmax = 10;
	}
	else if (!strcmp(type, "SCSI")) {
		// bit i set => drive with ID (i & 0x7) on adapter (i >> 3) present
		n = aspi_scan(drives);
		path = "/dev/scsi00";
		nmax = 10*8;
	}
	else
		return -1;

	if (n <= 0)
		return 0;

	// Alloc devlist
	sz = n * sizeof(char **);
	*devlist = (char **)malloc(sz); bytes += sz;

	// Add devices
	for (i = j = 0; i < n; ) {
		while (j < nmax && !(drives[j >> 5] & (1L << (j & 0x1f))))
			j++;
		assert(j < nmax);

		if (j == rdriveno[0] || j == rdriveno[1]) {
			// Add physical drives behind this logical drive
			int ci = (j == rdriveno[0] ? 0 : 1);
			for (int pi = 0; pi < 32 && i < n; pi++) {
				if (!(rdrives[ci] & (1L << pi)))
					continue;
				char rpath[20];
				sprintf(rpath, "/dev/hd%c,%u", 'a'+j, pi);
				sz = strlen(rpath)+1;
				char * s = (char *)malloc(sz); bytes += sz;
				strcpy(s, rpath);
				(*devlist)[i++] = s;
			}
		}
		else {
			sz = strlen(path)+1;
			char * s = (char *)malloc(sz); bytes += sz;
			strcpy(s, path);

			if (nmax <= 10) {
				assert(j <= 9);
				s[sz-2] += j; // /dev/hd[a-j]
			}
			else {
				assert((j >> 3) <= 9);
				s[sz-3] += (j >> 3);  // /dev/scsi[0-9].....
				s[sz-2] += (j & 0x7); //          .....[0-7]
			}
			(*devlist)[i++] = s;
		}
		j++;
	}

	return n;
}


// Like open().  Return positive integer handle, only used by
// functions below.  type="ATA" or "SCSI".  If you need to store extra
// information about your devices, create a private internal array
// within this file (see os_freebsd.cpp for an example).
int deviceopen(const char * pathname, char *type)
{
	int len;
	pathname = skipdev(pathname);
	len = strlen(pathname);

	if (!strcmp(type, "ATA")) {
		// hd[a-j](:[saic]+)? => ATA 0-9 with options
		char drive[1+1] = "", options[5+1] = ""; int n1 = -1, n2 = -1;
		if (   sscanf(pathname, "hd%1[a-j]%n:%5[saicp]%n", drive, &n1, options, &n2) >= 1
			&& ((n1 == len && !options[0]) || n2 == len)                                 ) {
			return ata_open(drive[0] - 'a', options, -1);
		}
		// hd[a-j],N => Physical drive 0-9, RAID port N
		drive[0] = 0; options[0] = 0; n1 = -1; n2 = -1;
		unsigned port = ~0;
		if (   sscanf(pathname, "hd%1[a-j],%u%n:%5[saicp]%n", drive, &port, &n1, options, &n2) >= 2
		    && port < 32 && ((n1 == len && !options[0]) || n2 == len)                              ) {
			return ata_open(drive[0] - 'a', options, port);
		}
	}

	else if (!strcmp(type, "SCSI")) {
		// scsi[0-9][0-f] => SCSI Adapter 0-9, ID 0-15, LUN 0
		unsigned adapter = ~0, id = ~0; int n = -1;
		if (sscanf(pathname,"scsi%1u%1x%n", &adapter, &id, &n) == 2 && n == len) {
			return aspi_open(adapter, id);
		}
	}

	errno = EINVAL;
	return -1;
}


// Like close().  Acts only on handles returned by above function.
// (Never called in smartctl!)
int deviceclose(int fd)
{
	if ((fd & 0xff00) != 0x0100) {
		ata_close(fd);
	}
	else {
		aspi_close(fd);
	}
	return 0;
}


// print examples for smartctl
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n"
         "  smartctl -a /dev/hda                       (Prints all SMART information)\n\n"
#ifdef HAVE_GETOPT_LONG
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
#else
         "  smartctl -s on -o on -S on /dev/hda         (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
#endif
         "  smartctl -a /dev/scsi21\n"
         "             (Prints all information for SCSI disk on ASPI adapter 2, ID 1)\n"
         "  smartctl -A /dev/hdb,3\n"
         "                (Prints Attributes for physical drive 3 on 3ware 9000 RAID)\n"
         "\n"
         "  ATA SMART access methods and ordering may be specified by modifiers\n"
         "  following the device name: /dev/hdX:[saic], where\n"
         "  's': SMART_* IOCTLs,         'a': IOCTL_ATA_PASS_THROUGH,\n"
         "  'i': IOCTL_IDE_PASS_THROUGH, 'c': ATA via IOCTL_SCSI_PASS_THROUGH.\n"
         "  The default on this system is /dev/hdX:%s\n", ata_get_def_options()
  );
}


/////////////////////////////////////////////////////////////////////////////
// ATA Interface
/////////////////////////////////////////////////////////////////////////////

// SMART_* IOCTLs, also known as DFP_* (Disk Fault Protection)

// Deklarations from:
// http://cvs.sourceforge.net/viewcvs.py/mingw/w32api/include/ddk/ntdddisk.h?rev=1.3

#define FILE_READ_ACCESS       0x0001
#define FILE_WRITE_ACCESS      0x0002
#define METHOD_BUFFERED             0
#define CTL_CODE(DeviceType, Function, Method, Access) (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define FILE_DEVICE_DISK	   7
#define IOCTL_DISK_BASE        FILE_DEVICE_DISK

#define SMART_GET_VERSION \
  CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)

#define SMART_SEND_DRIVE_COMMAND \
  CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define SMART_RCV_DRIVE_DATA \
  CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(SMART_GET_VERSION       , 0x074080);
ASSERT_CONST(SMART_SEND_DRIVE_COMMAND, 0x07c084);
ASSERT_CONST(SMART_RCV_DRIVE_DATA    , 0x07c088);

#define SMART_CYL_LOW  0x4F
#define SMART_CYL_HI   0xC2


#pragma pack(1)

typedef struct _GETVERSIONOUTPARAMS {
	UCHAR  bVersion;
	UCHAR  bRevision;
	UCHAR  bReserved;
	UCHAR  bIDEDeviceMap;
	ULONG  fCapabilities;
	ULONG  dwReserved[4];
} GETVERSIONOUTPARAMS, *PGETVERSIONOUTPARAMS, *LPGETVERSIONOUTPARAMS;

ASSERT_SIZEOF(GETVERSIONOUTPARAMS, 24);


#define SMART_VENDOR_3WARE      0x13C1  // identifies 3ware specific parameters

typedef struct _GETVERSIONINPARAMS_EX {
	BYTE    bVersion;
	BYTE    bRevision;
	BYTE    bReserved;
	BYTE    bIDEDeviceMap;
	DWORD   fCapabilities;
	DWORD   dwDeviceMapEx;  // 3ware specific: RAID drive bit map
	WORD    wIdentifier;    // Vendor specific identifier
	WORD    wControllerId;  // 3ware specific: Controller ID (0,1,...)
	ULONG   dwReserved[2];
} GETVERSIONINPARAMS_EX, *PGETVERSIONINPARAMS_EX, *LPGETVERSIONINPARAMS_EX;

ASSERT_SIZEOF(GETVERSIONINPARAMS_EX, sizeof(GETVERSIONOUTPARAMS));


typedef struct _IDEREGS {
	UCHAR  bFeaturesReg;
	UCHAR  bSectorCountReg;
	UCHAR  bSectorNumberReg;
	UCHAR  bCylLowReg;
	UCHAR  bCylHighReg;
	UCHAR  bDriveHeadReg;
	UCHAR  bCommandReg;
	UCHAR  bReserved;
} IDEREGS, *PIDEREGS, *LPIDEREGS;

typedef struct _SENDCMDINPARAMS {
	ULONG  cBufferSize;
	IDEREGS  irDriveRegs;
	UCHAR  bDriveNumber;
	UCHAR  bReserved[3];
	ULONG  dwReserved[4];
	UCHAR  bBuffer[1];
} SENDCMDINPARAMS, *PSENDCMDINPARAMS, *LPSENDCMDINPARAMS;

ASSERT_SIZEOF(SENDCMDINPARAMS, 32+1);

typedef struct _SENDCMDINPARAMS_EX {
	DWORD   cBufferSize;
	IDEREGS irDriveRegs;
	BYTE    bDriveNumber;
	BYTE    bPortNumber;   // 3ware specific: port number
	WORD    wIdentifier;   // Vendor specific identifier
	DWORD   dwReserved[4];
	BYTE    bBuffer[1];
} SENDCMDINPARAMS_EX, *PSENDCMDINPARAMS_EX, *LPSENDCMDINPARAMS_EX;

ASSERT_SIZEOF(SENDCMDINPARAMS_EX, sizeof(SENDCMDINPARAMS));


/* DRIVERSTATUS.bDriverError constants (just for info, not used)
#define SMART_NO_ERROR                    0
#define SMART_IDE_ERROR                   1
#define SMART_INVALID_FLAG                2
#define SMART_INVALID_COMMAND             3
#define SMART_INVALID_BUFFER              4
#define SMART_INVALID_DRIVE               5
#define SMART_INVALID_IOCTL               6
#define SMART_ERROR_NO_MEM                7
#define SMART_INVALID_REGISTER            8
#define SMART_NOT_SUPPORTED               9
#define SMART_NO_IDE_DEVICE               10
*/

typedef struct _DRIVERSTATUS {
	UCHAR  bDriverError;
	UCHAR  bIDEError;
	UCHAR  bReserved[2];
	ULONG  dwReserved[2];
} DRIVERSTATUS, *PDRIVERSTATUS, *LPDRIVERSTATUS;

typedef struct _SENDCMDOUTPARAMS {
	ULONG  cBufferSize;
	DRIVERSTATUS  DriverStatus;
	UCHAR  bBuffer[1];
} SENDCMDOUTPARAMS, *PSENDCMDOUTPARAMS, *LPSENDCMDOUTPARAMS;

ASSERT_SIZEOF(SENDCMDOUTPARAMS, 16+1);

#pragma pack()


/////////////////////////////////////////////////////////////////////////////

static void print_ide_regs(const IDEREGS * r, int out)
{
	pout("%s=0x%02x,%s=0x%02x, SC=0x%02x, NS=0x%02x, CL=0x%02x, CH=0x%02x, SEL=0x%02x\n",
	(out?"STS":"CMD"), r->bCommandReg, (out?"ERR":" FR"), r->bFeaturesReg,
	r->bSectorCountReg, r->bSectorNumberReg, r->bCylLowReg, r->bCylHighReg, r->bDriveHeadReg);
}

static void print_ide_regs_io(const IDEREGS * ri, const IDEREGS * ro)
{
	pout("    Input : "); print_ide_regs(ri, 0);
	if (ro) {
		pout("    Output: "); print_ide_regs(ro, 1);
	}
}

/////////////////////////////////////////////////////////////////////////////

// call SMART_GET_VERSION, return device map or -1 on error

static int smart_get_version(HANDLE hdevice, unsigned long * portmap = 0)
{
	GETVERSIONOUTPARAMS vers;
	const GETVERSIONINPARAMS_EX & vers_ex = (const GETVERSIONINPARAMS_EX &)vers;
	DWORD num_out;

	memset(&vers, 0, sizeof(vers));
	if (!DeviceIoControl(hdevice, SMART_GET_VERSION,
		NULL, 0, &vers, sizeof(vers), &num_out, NULL)) {
		if (con->reportataioctl)
			pout("  SMART_GET_VERSION failed, Error=%ld\n", GetLastError());
		errno = ENOSYS;
		return -1;
	}
	assert(num_out == sizeof(GETVERSIONOUTPARAMS));

	if (portmap) {
		// Return bitmask of valid RAID ports
		if (vers_ex.wIdentifier != SMART_VENDOR_3WARE) {
			pout("  SMART_GET_VERSION returns unknown Identifier = %04x\n"
				 "  This is no 3ware 9000 controller or driver has no SMART support.\n", vers_ex.wIdentifier);
			errno = ENOENT;
			return -1;
		}
		*portmap = vers_ex.dwDeviceMapEx;
	}

	if (con->reportataioctl > 1) {
		pout("  SMART_GET_VERSION suceeded, bytes returned: %lu\n"
		     "    Vers = %d.%d, Caps = 0x%lx, DeviceMap = 0x%02x\n",
			num_out, vers.bVersion, vers.bRevision,
			vers.fCapabilities, vers.bIDEDeviceMap);
		if (vers_ex.wIdentifier == SMART_VENDOR_3WARE)
			pout("    Identifier = %04x(3WARE), ControllerId=%u, DeviceMapEx = 0x%08lx\n",
			vers_ex.wIdentifier, vers_ex.wControllerId, vers_ex.dwDeviceMapEx);
	}

	// TODO: Check vers.fCapabilities here?
	return vers.bIDEDeviceMap;
}


// call SMART_* ioctl

static int smart_ioctl(HANDLE hdevice, int drive, IDEREGS * regs, char * data, unsigned datasize, int port)
{
	SENDCMDINPARAMS inpar;
	SENDCMDINPARAMS_EX & inpar_ex = (SENDCMDINPARAMS_EX &)inpar;

	unsigned char outbuf[sizeof(SENDCMDOUTPARAMS)-1 + 512];
	const SENDCMDOUTPARAMS * outpar;
	DWORD code, num_out;
	unsigned int size_out;
	const char * name;

	memset(&inpar, 0, sizeof(inpar));
	inpar.irDriveRegs = *regs;
	// drive is set to 0-3 on Win9x only
	inpar.irDriveRegs.bDriveHeadReg = 0xA0 | ((drive & 1) << 4);
	inpar.bDriveNumber = drive;

	if (port >= 0) {
		// Set RAID port
		inpar_ex.wIdentifier = SMART_VENDOR_3WARE;
		inpar_ex.bPortNumber = port;
	}

	assert(datasize == 0 || datasize == 512);
	if (datasize) {
		code = SMART_RCV_DRIVE_DATA; name = "SMART_RCV_DRIVE_DATA";
		inpar.cBufferSize = size_out = 512;
	}
	else {
		code = SMART_SEND_DRIVE_COMMAND; name = "SMART_SEND_DRIVE_COMMAND";
		if (regs->bFeaturesReg == ATA_SMART_STATUS) {
			size_out = sizeof(IDEREGS); // ioctl returns new IDEREGS as data
			// Note: cBufferSize must be 0 on Win9x
			inpar.cBufferSize = size_out;
		}
		else
			size_out = 0;
	}

	memset(&outbuf, 0, sizeof(outbuf));

	if (!DeviceIoControl(hdevice, code, &inpar, sizeof(SENDCMDINPARAMS)-1,
		outbuf, sizeof(SENDCMDOUTPARAMS)-1 + size_out, &num_out, NULL)) {
		// CAUTION: DO NOT change "regs" Parameter in this case, see ata_command_interface()
		long err = GetLastError();
		if (con->reportataioctl && (err != ERROR_INVALID_PARAMETER || con->reportataioctl > 1)) {
			pout("  %s failed, Error=%ld\n", name, err);
			print_ide_regs_io(regs, NULL);
		}
		errno = (   err == ERROR_INVALID_FUNCTION /*9x*/
		         || err == ERROR_INVALID_PARAMETER/*NT/2K/XP*/ ? ENOSYS : EIO);
		return -1;
	}
	// NOTE: On Win9x, inpar.irDriveRegs now contains the returned regs

	outpar = (const SENDCMDOUTPARAMS *)outbuf;

	if (outpar->DriverStatus.bDriverError) {
		if (con->reportataioctl) {
			pout("  %s failed, DriverError=0x%02x, IDEError=0x%02x\n", name,
				outpar->DriverStatus.bDriverError, outpar->DriverStatus.bIDEError);
			print_ide_regs_io(regs, NULL);
		}
		errno = (!outpar->DriverStatus.bIDEError ? ENOSYS : EIO);
		return -1;
	}

	if (con->reportataioctl > 1) {
		pout("  %s suceeded, bytes returned: %lu (buffer %lu)\n", name,
			num_out, outpar->cBufferSize);
		print_ide_regs_io(regs, (regs->bFeaturesReg == ATA_SMART_STATUS ?
			(const IDEREGS *)(outpar->bBuffer) : NULL));
	}

	if (datasize)
		memcpy(data, outpar->bBuffer, 512);
	else if (regs->bFeaturesReg == ATA_SMART_STATUS) {
		if (nonempty(const_cast<unsigned char *>(outpar->bBuffer), sizeof(IDEREGS)))
			*regs = *(const IDEREGS *)(outpar->bBuffer);
		else {  // Workaround for driver not returning regs
			if (con->reportataioctl)
				pout("  WARNING: driver does not return ATA registers in output buffer!\n");
			*regs = inpar.irDriveRegs;
		}
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////

// IDE PASS THROUGH (2000, XP, undocumented)
//
// Based on WinATA.cpp, 2002 c't/Matthias Withopf
// ftp://ftp.heise.de/pub/ct/listings/0207-218.zip

#define FILE_DEVICE_CONTROLLER  4
#define IOCTL_SCSI_BASE         FILE_DEVICE_CONTROLLER

#define IOCTL_IDE_PASS_THROUGH \
  CTL_CODE(IOCTL_SCSI_BASE, 0x040A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(IOCTL_IDE_PASS_THROUGH, 0x04d028);

#pragma pack(1)

typedef struct {
	IDEREGS IdeReg;
	ULONG   DataBufferSize;
	UCHAR   DataBuffer[1];
} ATA_PASS_THROUGH;

ASSERT_SIZEOF(ATA_PASS_THROUGH, 12+1);

#pragma pack()


/////////////////////////////////////////////////////////////////////////////

static int ide_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, unsigned datasize)
{ 
	unsigned int size = sizeof(ATA_PASS_THROUGH)-1 + datasize;
	ATA_PASS_THROUGH * buf = (ATA_PASS_THROUGH *)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
	DWORD num_out;
	const unsigned char magic = 0xcf;

	if (!buf) {
		errno = ENOMEM;
		return -1;
	}

	buf->IdeReg = *regs;
	buf->DataBufferSize = datasize;
	if (datasize)
		buf->DataBuffer[0] = magic;

	if (!DeviceIoControl(hdevice, IOCTL_IDE_PASS_THROUGH,
		buf, size, buf, size, &num_out, NULL)) {
		long err = GetLastError();
		if (con->reportataioctl) {
			pout("  IOCTL_IDE_PASS_THROUGH failed, Error=%ld\n", err);
			print_ide_regs_io(regs, NULL);
		}
		VirtualFree(buf, 0, MEM_RELEASE);
		errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
		return -1;
	}

	// Check ATA status
	if (buf->IdeReg.bCommandReg/*Status*/ & 0x01) {
		if (con->reportataioctl) {
			pout("  IOCTL_IDE_PASS_THROUGH command failed:\n");
			print_ide_regs_io(regs, &buf->IdeReg);
		}
		VirtualFree(buf, 0, MEM_RELEASE);
		errno = EIO;
		return -1;
	}

	// Check and copy data
	if (datasize) {
		if (   num_out != size
		    || (buf->DataBuffer[0] == magic && !nonempty(buf->DataBuffer+1, datasize-1))) {
			if (con->reportataioctl) {
				pout("  IOCTL_IDE_PASS_THROUGH output data missing (%lu, %lu)\n",
					num_out, buf->DataBufferSize);
				print_ide_regs_io(regs, &buf->IdeReg);
			}
			VirtualFree(buf, 0, MEM_RELEASE);
			errno = EIO;
			return -1;
		}
		memcpy(data, buf->DataBuffer, datasize);
	}

	if (con->reportataioctl > 1) {
		pout("  IOCTL_IDE_PASS_THROUGH suceeded, bytes returned: %lu (buffer %lu)\n",
			num_out, buf->DataBufferSize);
		print_ide_regs_io(regs, &buf->IdeReg);
	}
	*regs = buf->IdeReg;

	// Caution: VirtualFree() fails if parameter "dwSize" is nonzero
	VirtualFree(buf, 0, MEM_RELEASE);
	return 0;
}


/////////////////////////////////////////////////////////////////////////////

// ATA PASS THROUGH (Win2003, XP SP2)

#define IOCTL_ATA_PASS_THROUGH \
	CTL_CODE(IOCTL_SCSI_BASE, 0x040B, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(IOCTL_ATA_PASS_THROUGH, 0x04d02c);

typedef struct _ATA_PASS_THROUGH_EX {
	USHORT  Length;
	USHORT  AtaFlags;
	UCHAR  PathId;
	UCHAR  TargetId;
	UCHAR  Lun;
	UCHAR  ReservedAsUchar;
	ULONG  DataTransferLength;
	ULONG  TimeOutValue;
	ULONG  ReservedAsUlong;
	ULONG/*_PTR*/ DataBufferOffset;
	UCHAR  PreviousTaskFile[8];
	UCHAR  CurrentTaskFile[8];
} ATA_PASS_THROUGH_EX, *PATA_PASS_THROUGH_EX;

ASSERT_SIZEOF(ATA_PASS_THROUGH_EX, 40);

#define ATA_FLAGS_DRDY_REQUIRED 0x01
#define ATA_FLAGS_DATA_IN       0x02
#define ATA_FLAGS_DATA_OUT      0x04
#define ATA_FLAGS_48BIT_COMMAND 0x08


/////////////////////////////////////////////////////////////////////////////

static int ata_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, int datasize)
{ 
	typedef struct {
		ATA_PASS_THROUGH_EX apt;
		ULONG Filler;
		UCHAR ucDataBuf[512];
	} ATA_PASS_THROUGH_EX_WITH_BUFFERS;

	ATA_PASS_THROUGH_EX_WITH_BUFFERS ab;
	IDEREGS * ctfregs;
	unsigned int size;
	DWORD num_out;
	const unsigned char magic = 0xcf;

	memset(&ab, 0, sizeof(ab));
	ab.apt.Length = sizeof(ATA_PASS_THROUGH_EX);
	//ab.apt.PathId = 0;
	//ab.apt.TargetId = 0;
	//ab.apt.Lun = 0;
	ab.apt.TimeOutValue = 10;
	size = offsetof(ATA_PASS_THROUGH_EX_WITH_BUFFERS, ucDataBuf);
	ab.apt.DataBufferOffset = size;
 
	if (datasize) {
		if (!(0 <= datasize && datasize <= (int)sizeof(ab.ucDataBuf))) {
			errno = EINVAL;
			return -1;
		}
		ab.apt.AtaFlags = ATA_FLAGS_DATA_IN;
		ab.apt.DataTransferLength = datasize;
		size += datasize;
		ab.ucDataBuf[0] = magic;
	}
	else {
		//ab.apt.AtaFlags = 0;
		//ab.apt.DataTransferLength = 0;
	}

	assert(sizeof(ab.apt.CurrentTaskFile) == sizeof(IDEREGS));
	ctfregs = (IDEREGS *)ab.apt.CurrentTaskFile;
	*ctfregs = *regs;

	if (!DeviceIoControl(hdevice, IOCTL_ATA_PASS_THROUGH,
		&ab, size, &ab, size, &num_out, NULL)) {
		long err = GetLastError();
		if (con->reportataioctl) {
			pout("  IOCTL_ATA_PASS_THROUGH_EX failed, Error=%ld\n", err);
			print_ide_regs_io(regs, NULL);
		}
		errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
		return -1;
	}

	// Check and copy data
	if (datasize) {
		if (   num_out != size
		    || (ab.ucDataBuf[0] == magic && !nonempty(ab.ucDataBuf+1, datasize-1))) {
			if (con->reportataioctl) {
				pout("  IOCTL_ATA_PASS_THROUGH_EX output data missing (%lu)\n", num_out);
				print_ide_regs_io(regs, ctfregs);
			}
			errno = EIO;
			return -1;
		}
		memcpy(data, ab.ucDataBuf, datasize);
	}

	if (con->reportataioctl > 1) {
		pout("  IOCTL_ATA_PASS_THROUGH_EX suceeded, bytes returned: %lu\n", num_out);
		print_ide_regs_io(regs, ctfregs);
	}
	*regs = *ctfregs;

	return 0;
}


/////////////////////////////////////////////////////////////////////////////

// ATA PASS THROUGH via SCSI PASS THROUGH (WinNT4 only)

// Declarations from:
// http://cvs.sourceforge.net/viewcvs.py/mingw/w32api/include/ddk/ntddscsi.h?rev=1.2

#define IOCTL_SCSI_PASS_THROUGH \
	CTL_CODE(IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(IOCTL_SCSI_PASS_THROUGH, 0x04d004);

#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define SCSI_IOCTL_DATA_UNSPECIFIED  2
// undocumented SCSI opcode to for ATA passthrough
#define SCSIOP_ATA_PASSTHROUGH    0xCC

typedef struct _SCSI_PASS_THROUGH {
	USHORT  Length;
	UCHAR  ScsiStatus;
	UCHAR  PathId;
	UCHAR  TargetId;
	UCHAR  Lun;
	UCHAR  CdbLength;
	UCHAR  SenseInfoLength;
	UCHAR  DataIn;
	ULONG  DataTransferLength;
	ULONG  TimeOutValue;
	ULONG/*_PTR*/ DataBufferOffset;
	ULONG  SenseInfoOffset;
	UCHAR  Cdb[16];
} SCSI_PASS_THROUGH, *PSCSI_PASS_THROUGH;

ASSERT_SIZEOF(SCSI_PASS_THROUGH, 44);


/////////////////////////////////////////////////////////////////////////////

static int ata_via_scsi_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, unsigned datasize)
{
	typedef struct {
		SCSI_PASS_THROUGH spt;
		ULONG Filler;
		UCHAR ucSenseBuf[32];
		UCHAR ucDataBuf[512];
	} SCSI_PASS_THROUGH_WITH_BUFFERS;

	SCSI_PASS_THROUGH_WITH_BUFFERS sb;
	IDEREGS * cdbregs;
	unsigned int size;
	DWORD num_out;
	const unsigned char magic = 0xcf;

	memset(&sb, 0, sizeof(sb));
	sb.spt.Length = sizeof(SCSI_PASS_THROUGH);
	//sb.spt.PathId = 0;
	sb.spt.TargetId = 1;
	//sb.spt.Lun = 0;
	sb.spt.CdbLength = 10; sb.spt.SenseInfoLength = 24;
	sb.spt.TimeOutValue = 10;
	sb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
	size = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
	sb.spt.DataBufferOffset = size;
 
	if (datasize) {
		if (datasize > sizeof(sb.ucDataBuf)) {
			errno = EINVAL;
			return -1;
		}
		sb.spt.DataIn = SCSI_IOCTL_DATA_IN;
		sb.spt.DataTransferLength = datasize;
		size += datasize;
		sb.ucDataBuf[0] = magic;
	}
	else {
		sb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
		//sb.spt.DataTransferLength = 0;
	}

	// Use pseudo SCSI command followed by registers
	sb.spt.Cdb[0] = SCSIOP_ATA_PASSTHROUGH;
	cdbregs = (IDEREGS *)(sb.spt.Cdb+2);
	*cdbregs = *regs;

	if (!DeviceIoControl(hdevice, IOCTL_SCSI_PASS_THROUGH,
		&sb, size, &sb, size, &num_out, NULL)) {
		long err = GetLastError();
		if (con->reportataioctl)
			pout("  ATA via IOCTL_SCSI_PASS_THROUGH failed, Error=%ld\n", err);
		errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
		return -1;
	}

	// Cannot check ATA status, because command does not return IDEREGS

	// Check and copy data
	if (datasize) {
		if (   num_out != size
		    || (sb.ucDataBuf[0] == magic && !nonempty(sb.ucDataBuf+1, datasize-1))) {
			if (con->reportataioctl) {
				pout("  ATA via IOCTL_SCSI_PASS_THROUGH output data missing (%lu)\n", num_out);
				print_ide_regs_io(regs, NULL);
			}
			errno = EIO;
			return -1;
		}
		memcpy(data, sb.ucDataBuf, datasize);
	}

	if (con->reportataioctl > 1) {
		pout("  ATA via IOCTL_SCSI_PASS_THROUGH suceeded, bytes returned: %lu\n", num_out);
		print_ide_regs_io(regs, NULL);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////

// Call GetDevicePowerState() if available (Win98/ME/2000/XP/2003)
// returns: 1=active, 0=standby, -1=error
// (This would also work for SCSI drives)

static int get_device_power_state(HANDLE hdevice)
{
	static HINSTANCE h_kernel_dll = 0;
#ifdef __CYGWIN__
	static DWORD kernel_dll_pid = 0;
#endif
	static BOOL (WINAPI * GetDevicePowerState_p)(HANDLE, BOOL *) = 0;

	BOOL state = TRUE;

	if (!GetDevicePowerState_p
#ifdef __CYGWIN__
	    || kernel_dll_pid != GetCurrentProcessId() // detect fork()
#endif
	   ) {
		if (h_kernel_dll == INVALID_HANDLE_VALUE) {
			errno = ENOSYS;
			return -1;
		}
		if (!(h_kernel_dll = LoadLibraryA("KERNEL32.DLL"))) {
			pout("Cannot load KERNEL32.DLL, Error=%ld\n", GetLastError());
			h_kernel_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
			errno = ENOSYS;
			return -1;
		}
		if (!(GetDevicePowerState_p = (BOOL (WINAPI *)(HANDLE, BOOL *))
		                              GetProcAddress(h_kernel_dll, "GetDevicePowerState"))) {
			if (con->reportataioctl)
				pout("  GetDevicePowerState() not found, Error=%ld\n", GetLastError());
			FreeLibrary(h_kernel_dll);
			h_kernel_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
			errno = ENOSYS;
			return -1;
		}
#ifdef __CYGWIN__
		kernel_dll_pid = GetCurrentProcessId();
#endif
	}

	if (!GetDevicePowerState_p(hdevice, &state)) {
		long err = GetLastError();
		if (con->reportataioctl)
			pout("  GetDevicePowerState() failed, Error=%ld\n", err);
		errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
		// TODO: This may not work as expected on transient errors,
		// because smartd interprets -1 as SLEEP mode regardless of errno.
		return -1;
	}

	if (con->reportataioctl > 1)
		pout("  GetDevicePowerState() succeeded, state=%d\n", state);
	return state;
}


/////////////////////////////////////////////////////////////////////////////

// TODO: Put in a struct indexed by fd (or better a C++ object of course ;-)
static HANDLE h_ata_ioctl = 0;
static const char * ata_def_options;
static char * ata_cur_options;
static int ata_driveno; // Drive number
static char ata_smartver_state[10]; // SMART_GET_VERSION: 0=unknown, 1=OK, 2=failed

// Print SMARTVSD error message, return errno

static int smartvsd_error()
{
	char path[MAX_PATH];
	unsigned len;
	if (!(5 <= (len = GetSystemDirectoryA(path, MAX_PATH)) && len < MAX_PATH/2))
		return ENOENT;
	// SMARTVSD.VXD present?
	strcpy(path+len, "\\IOSUBSYS\\SMARTVSD.VXD");
	if (!access(path, 0)) {
		// Yes, standard IDE driver used?
		HANDLE h;
		if (   (h = CreateFileA("\\\\.\\ESDI_506",
		             GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		             NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE
		    && GetLastError() == ERROR_FILE_NOT_FOUND                             ) {
			pout("Standard IDE driver ESDI_506.PDR not used, or no IDE/ATA drives present.\n");
			return ENOENT;
		}
		else {
			if (h != INVALID_HANDLE_VALUE) // should not happen
				CloseHandle(h);
			pout("SMART driver SMARTVSD.VXD is installed, but not loaded.\n");
			return ENOSYS;
		}
	}
	else {
		strcpy(path+len, "\\SMARTVSD.VXD");
		if (!access(path, 0)) {
			// Some Windows versions install SMARTVSD.VXD in SYSTEM directory
			// (http://support.microsoft.com/kb/265854/en-us).
			path[len] = 0;
			pout("SMART driver is not properly installed,\n"
				 " move SMARTVSD.VXD from \"%s\" to \"%s\\IOSUBSYS\"\n"
				 " and reboot Windows.\n", path, path);
		}
		else {
			// Some Windows versions do not provide SMARTVSD.VXD
			// (http://support.microsoft.com/kb/199886/en-us).
			path[len] = 0;
			pout("SMARTVSD.VXD is missing in folder \"%s\\IOSUBSYS\".\n", path);
		}
		return ENOSYS;
	}
}


// Get default ATA device options

static const char * ata_get_def_options()
{
	DWORD ver = GetVersion();
	if ((ver & 0x80000000) || (ver & 0xff) < 4) // Win9x/ME
		return "s"; // SMART_* only
	else if ((ver & 0xff) == 4) // WinNT4
		return "sc"; // SMART_*, SCSI_PASS_THROUGH
	else // WinXP, 2003, Vista
		return "psai"; // GetDevicePowerState(), SMART_*, ATA_, IDE_PASS_THROUGH
}


// Open ATA device

static int ata_open(int drive, const char * options, int port)
{
	int win9x;
	char devpath[30];
	int devmap;

	// TODO: This version does not allow to open more than 1 ATA devices
	if (h_ata_ioctl) {
		errno = ENFILE;
		return -1;
	}

	win9x = ((GetVersion() & 0x80000000) != 0);

	if (!(0 <= drive && drive <= (win9x ? 7 : 9))) {
		errno = ENOENT;
		return -1;
	}

	// path depends on Windows Version
	if (win9x)
		// Use patched "smartvse.vxd" for drives 4-7, see INSTALL file for details
		strcpy(devpath, (drive <= 3 ? "\\\\.\\SMARTVSD" : "\\\\.\\SMARTVSE"));
	else
		snprintf(devpath, sizeof(devpath)-1, "\\\\.\\PhysicalDrive%d", drive);

	// Open device
	if ((h_ata_ioctl = CreateFileA(devpath,
		GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
		long err = GetLastError();	
		pout("Cannot open device %s, Error=%ld\n", devpath, err);
		if (err == ERROR_FILE_NOT_FOUND)
			errno = (win9x && drive <= 3 ? smartvsd_error() : ENOENT);
		else if (err == ERROR_ACCESS_DENIED) {
			if (!win9x)
				pout("Administrator rights are necessary to access physical drives.\n");
			errno = EACCES;
		}
		else
			errno = EIO;
		h_ata_ioctl = 0;
		return -1;
	}

	if (con->reportataioctl > 1)
		pout("%s: successfully opened\n", devpath);

	// Save options
	if (!*options) {
		// Set default options according to Windows version
		if (!ata_def_options)
			ata_def_options = ata_get_def_options();
		options = (port < 0 ? ata_def_options : "s"); // RAID: SMART_* only
	}
	ata_cur_options = strdup(options);

	// NT4/2000/XP: SMART_GET_VERSION may spin up disk, so delay until first real SMART_* call
	ata_driveno = drive;
	if (!win9x && port < 0)
		return 0;

	// Win9X/ME: Get drive map
	// RAID: Get port map
	unsigned long portmap = 0;
	devmap = smart_get_version(h_ata_ioctl, (port >= 0 ? &portmap : 0));
	if (devmap < 0) {
		if (!is_permissive()) {
			ata_close(0);
			errno = ENOSYS;
			return -1;
		}
		devmap = 0x0f;
	}
	ata_smartver_state[drive] = 1;

	if (port >= 0) {
		// RAID: Check port existence
		if (!(portmap & (1L << port))) {
			pout("%s: Port %d is empty or does not exist\n", devpath, port);
			if (!is_permissive()) {
				ata_close(0);
				errno = ENOENT;
				return -1;
			}
		}
		// Encode port into pseudo fd
		return (0x0200 | port);
	}

	// Win9x/ME: Check device presence & type
	if (((devmap >> (drive & 0x3)) & 0x11) != 0x01) {
		unsigned char atapi = (devmap >> (drive & 0x3)) & 0x10;
		pout("%s: Drive %d %s (IDEDeviceMap=0x%02x).\n", devpath,
		     drive, (atapi?"is an ATAPI device":"does not exist"), devmap);
		// Win9x drive existence check may not work as expected
		// The atapi.sys driver incorrectly fills in the bIDEDeviceMap with 0x01
		// (The related KB Article Q196120 is no longer available)
		if (!is_permissive()) {
			ata_close(0);
			errno = (atapi ? ENOSYS : ENOENT);
			return -1;
		}
	}
	// Use drive number as fd for ioctl
	return (drive & 0x3);
}


static void ata_close(int fd)
{
	ARGUSED(fd);
	CloseHandle(h_ata_ioctl);
	h_ata_ioctl = 0;
	if (ata_cur_options) {
		free(ata_cur_options);
		ata_cur_options = 0;
	}
}


// Scan for ATA drives, fill bitmask of drives present, return #drives

static int ata_scan(unsigned long * drives, int * rdriveno, unsigned long * rdrives)
{
	int win9x = ((GetVersion() & 0x80000000) != 0);
	int cnt = 0, i;

	for (i = 0; i <= 9; i++) {
		char devpath[30];
		GETVERSIONOUTPARAMS vers;
		const GETVERSIONINPARAMS_EX & vers_ex = (const GETVERSIONINPARAMS_EX &)vers;
		DWORD num_out;
		HANDLE h;
		if (win9x)
			strcpy(devpath, "\\\\.\\SMARTVSD");
		else
			snprintf(devpath, sizeof(devpath)-1, "\\\\.\\PhysicalDrive%d", i);

		// Open device
		if ((h = CreateFileA(devpath,
			GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
			if (con->reportataioctl > 1)
				pout(" %s: Open failed, Error=%ld\n", devpath, GetLastError());
			if (win9x)
				break; // SMARTVSD.VXD missing or no ATA devices
			continue; // Disk not found or access denied (break;?)
		}

		// Get drive map
		memset(&vers, 0, sizeof(vers));
		if (!DeviceIoControl(h, SMART_GET_VERSION,
			NULL, 0, &vers, sizeof(vers), &num_out, NULL)) {
			if (con->reportataioctl)
				pout(" %s: SMART_GET_VERSION failed, Error=%ld\n", devpath, GetLastError());
			CloseHandle(h);
			if (win9x)
				break; // Should not happen
			continue; // Non ATA disk or no SMART ioctl support (possibly SCSI disk)
		}
		CloseHandle(h);

		if (con->reportataioctl) {
			pout(" %s: SMART_GET_VERSION (%ld bytes):\n"
			     "  Vers = %d.%d, Caps = 0x%lx, DeviceMap = 0x%02x\n",
				devpath, num_out, vers.bVersion, vers.bRevision,
				vers.fCapabilities, vers.bIDEDeviceMap);
			if (vers_ex.wIdentifier == SMART_VENDOR_3WARE)
				pout("  Identifier = %04x(3WARE), ControllerId=%u, DeviceMapEx = 0x%08lx\n",
					vers_ex.wIdentifier, vers_ex.wControllerId, vers_ex.dwDeviceMapEx);
		}

		if (win9x) {
			// Check ATA device presence, remove ATAPI devices
			drives[0] = (vers.bIDEDeviceMap & 0xf) & ~((vers.bIDEDeviceMap >> 4) & 0xf);
			cnt = (drives[0]&1) + ((drives[0]>>1)&1) + ((drives[0]>>2)&1) + ((drives[0]>>3)&1);
			break;
		}

		if (vers_ex.wIdentifier == SMART_VENDOR_3WARE) {
			// Skip if more than 2 controllers or logical drive from this controller already seen
			if (vers_ex.wControllerId >= 2 || rdriveno[vers_ex.wControllerId] >= 0)
				continue;
			assert(rdrives[vers_ex.wControllerId] == 0);
			// Count physical drives
			int pcnt = 0;
			for (int pi = 0; pi < 32; pi++) {
				if (vers_ex.dwDeviceMapEx & (1L << pi))
					pcnt++;
			}
			if (!pcnt)
				continue; // Should not happen
			rdrives[vers_ex.wControllerId] = vers_ex.dwDeviceMapEx;
			rdriveno[vers_ex.wControllerId] = i;
			cnt += pcnt-1;
		}

		// ATA drive exists and driver supports SMART ioctl
		drives[0] |= (1L << i);
		cnt++;
	}

	return cnt;
}


/////////////////////////////////////////////////////////////////////////////

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char * data)
{
	IDEREGS regs;
	int datasize;
	const char * valid_options;
	int i;

	int port = -1;
	if ((fd & ~0x1f) == 0x0200) {
		// RAID Port encoded into pseudo fd
		port = fd & 0x1f;
		fd = 0;
	}

	if (!(0 <= fd && fd <= 3)) {
		errno = EBADF;
		return -1;
	}

	// CMD,CYL default to SMART, changed by P?IDENTIFY and CHECK_POWER_MODE
	memset(&regs, 0, sizeof(regs));
	regs.bCommandReg = ATA_SMART_CMD;
	regs.bCylHighReg = SMART_CYL_HI; regs.bCylLowReg = SMART_CYL_LOW;
	datasize = 0;

	// Try all IOCTLS by default: SMART_*, ATA_, IDE_, SCSI_PASS_THROUGH
	valid_options = "saic";

	switch (command) {
	  case WRITE_LOG:
		// TODO. Requires DATA OUT support
		errno = ENOSYS;
		return -1;
	  case CHECK_POWER_MODE:
		// Not a SMART command, needs IDE register return
		regs.bCommandReg = ATA_CHECK_POWER_MODE;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		valid_options = "pai"; // Try GetDevicePowerState() first, ATA/IDE_PASS_THROUGH may spin up disk
		// Note: returns SectorCountReg in data[0]
		break;
	  case READ_VALUES:
		regs.bFeaturesReg = ATA_SMART_READ_VALUES;
		regs.bSectorNumberReg = regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case READ_THRESHOLDS:
		regs.bFeaturesReg = ATA_SMART_READ_THRESHOLDS;
		regs.bSectorNumberReg = regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case READ_LOG:
		regs.bFeaturesReg = ATA_SMART_READ_LOG_SECTOR;
		regs.bSectorNumberReg = select;
		regs.bSectorCountReg = 1;
		// Note: SMART_RCV_DRIVE_DATA supports this only on Win9x/ME
		datasize = 512;
		break;
	  case IDENTIFY:
		// Note: WinNT4/2000/XP return identify data cached during boot
		// (true for SMART_RCV_DRIVE_DATA and IOCTL_IDE_PASS_THROUGH)
		regs.bCommandReg = ATA_IDENTIFY_DEVICE;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case PIDENTIFY:
		regs.bCommandReg = ATA_IDENTIFY_PACKET_DEVICE;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case ENABLE:
		regs.bFeaturesReg = ATA_SMART_ENABLE;
		regs.bSectorNumberReg = 1;
		break;
	  case DISABLE:
		regs.bFeaturesReg = ATA_SMART_DISABLE;
		regs.bSectorNumberReg = 1;
		break;
	  case STATUS_CHECK:
		valid_options = "sai"; // Needs IDE register return
	  case STATUS:
		regs.bFeaturesReg = ATA_SMART_STATUS;
		break;
	  case AUTO_OFFLINE:
		regs.bFeaturesReg = ATA_SMART_AUTO_OFFLINE;
		regs.bSectorCountReg = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
		break;
	  case AUTOSAVE:
		regs.bFeaturesReg = ATA_SMART_AUTOSAVE;
		regs.bSectorCountReg = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
		break;
	  case IMMEDIATE_OFFLINE:
		regs.bFeaturesReg = ATA_SMART_IMMEDIATE_OFFLINE;
		regs.bSectorNumberReg = select;
		// Note: SMART_SEND_DRIVE_COMMAND supports ABORT_SELF_TEST only on Win9x/ME
		break;
	  default:
		pout("Unrecognized command %d in win32_ata_command_interface()\n"
		 "Please contact " PACKAGE_BUGREPORT "\n", command);
		errno = ENOSYS;
		return -1;
	}

	// Try all valid ioctls in the order specified in dev_ioctls;
	for (i = 0; ; i++) {
		char opt = ata_cur_options[i];
		int rc;

		if (!opt) {
			// No IOCTL found
			errno = ENOSYS;
			return -1;
		}
		if (!strchr(valid_options, opt))
			// Invalid for this command
			continue;

		errno = 0;
		assert(datasize == 0 || datasize == 512);
		switch (opt) {
		  default: assert(0);
		  case 's':
			// call SMART_GET_VERSION once for each drive
			assert(0 <= ata_driveno && ata_driveno < sizeof(ata_smartver_state));
			if (ata_smartver_state[ata_driveno] > 1) {
				rc = -1; errno = ENOSYS;
				break;
			}
			if (!ata_smartver_state[ata_driveno]) {
				assert(port == -1);
				if (smart_get_version(h_ata_ioctl) < 0) {
					if (!con->permissive) {
						pout("ATA/SATA driver is possibly a SCSI class driver not supporting SMART.\n");
						pout("If this is a SCSI disk, try \"scsi<adapter><id>\".\n");
						ata_smartver_state[ata_driveno] = 2;
						rc = -1; errno = ENOSYS;
						break;
					}
					con->permissive--;
				}
				ata_smartver_state[ata_driveno] = 1;
			}
			rc = smart_ioctl(h_ata_ioctl, fd, &regs, data, datasize, port);
			break;
		  case 'a':
			rc = ata_pass_through_ioctl(h_ata_ioctl, &regs, data, datasize);
			break;
		  case 'i':
			rc = ide_pass_through_ioctl(h_ata_ioctl, &regs, data, datasize);
			break;
		  case 'c':
			rc = ata_via_scsi_pass_through_ioctl(h_ata_ioctl, &regs, data, datasize);
			break;
		  case 'p':
			assert(command == CHECK_POWER_MODE && datasize == 0);
			rc = get_device_power_state(h_ata_ioctl);
			if (rc >= 0) { // Simulate ATA command result
				regs.bSectorCountReg = (rc ? 0xff/*ACTIVE/IDLE*/ : 0x00/*STANDBY*/);
				rc = 0;
			}
			break;
		}

		if (!rc)
			// Working ioctl found
			break;

		if (errno != ENOSYS)
			// Abort on I/O error
			return -1;

		// CAUTION: *_ioctl() MUST NOT change "regs" Parameter in the ENOSYS case
	}

	switch (command) {
	  case CHECK_POWER_MODE:
		// Return power mode from SectorCountReg in data[0]
		data[0] = regs.bSectorCountReg;
		return 0;

	  case STATUS_CHECK:
		// Cyl low and Cyl high unchanged means "Good SMART status"
		if (regs.bCylHighReg == SMART_CYL_HI && regs.bCylLowReg == SMART_CYL_LOW)
		  return 0;

		// These values mean "Bad SMART status"
		if (regs.bCylHighReg == 0x2c && regs.bCylLowReg == 0xf4)
		  return 1;

		// We haven't gotten output that makes sense; print out some debugging info
		syserror("Error SMART Status command failed");
		pout("Please get assistance from %s\n", PACKAGE_HOMEPAGE);
		print_ide_regs(&regs, 1);
		errno = EIO;
		return -1;

	  default:
		return 0;
	}
	/*NOTREACHED*/
}


#ifndef HAVE_ATA_IDENTIFY_IS_CACHED
#error define of HAVE_ATA_IDENTIFY_IS_CACHED missing in config.h
#endif

// Return true if OS caches the ATA identify sector
int ata_identify_is_cached(int fd)
{
	// Not RAID and WinNT4/2000/XP => true, RAID or Win9x/ME => false
	return (!(fd & 0xff00) && (GetVersion() & 0x80000000) == 0);
}


// Print not implemeted warning once
static void pr_not_impl(const char * what, int * warned)
{
	if (*warned)
		return;
	pout(
		"#######################################################################\n"
		"%s\n"
		"NOT IMPLEMENTED under Win32.\n"
		"Please contact " PACKAGE_BUGREPORT " if\n"
		"you want to help in porting smartmontools to Win32.\n"
		"#######################################################################\n"
		"\n", what
	);
	*warned = 1;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, int escalade_type, smart_command_set command, int select, char *data)
{
	static int warned = 0;
	ARGUSED(fd); ARGUSED(escalade_type); ARGUSED(command); ARGUSED(select); ARGUSED(data);
	if (!warned) {
		pout("Option '-d 3ware,%d' does not work on Windows.\n"
		     "Controller port can be specified in the device name: '/dev/hd%c,%d'.\n\n",
			disknum, 'a'+ata_driveno, disknum);
		warned = 1;
	}
	errno = ENOSYS;
	return -1;
}

// Interface to ATA devices behind Marvell chip-set based controllers.  See os_linux.c
int marvell_command_interface(int fd, smart_command_set command, int select, char * data)
{
	static int warned = 0;
	ARGUSED(fd); ARGUSED(command); ARGUSED(select); ARGUSED(data);
	pr_not_impl("Marvell chip-set command routine marvell_command_interface()", &warned);
	errno = ENOSYS;
	return -1;
}

// Interface to ATA devices behind HighPoint Raid controllers.  See os_linux.c
int highpoint_command_interface(int fd, smart_command_set command, int select, char * data)
{
	static int warned = 0;
	ARGUSED(fd); ARGUSED(command); ARGUSED(select); ARGUSED(data);
	pr_not_impl("HighPoint raid controller command routine highpoint_command_interface()", &warned);
	errno = ENOSYS;
	return -1;
}


/////////////////////////////////////////////////////////////////////////////
// ASPI Interface
/////////////////////////////////////////////////////////////////////////////

#pragma pack(1)

#define ASPI_SENSE_SIZE 18

// ASPI SCSI Request block header

typedef struct {
	unsigned char cmd;             // 00: Command code
	unsigned char status;          // 01: ASPI status
	unsigned char adapter;         // 02: Host adapter number
	unsigned char flags;           // 03: Request flags
	unsigned char reserved[4];     // 04: 0
} ASPI_SRB_HEAD;

// SRB for host adapter inquiry

typedef struct {
	ASPI_SRB_HEAD h;               // 00: Header
	unsigned char adapters;        // 08: Number of adapters
	unsigned char target_id;       // 09: Target ID ?
	char manager_id[16];           // 10: SCSI manager ID
	char adapter_id[16];           // 26: Host adapter ID
	unsigned char parameters[16];  // 42: Host adapter unique parmameters
} ASPI_SRB_INQUIRY;

// SRB for get device type

typedef struct {
	ASPI_SRB_HEAD h;               // 00: Header
	unsigned char target_id;       // 08: Target ID
	unsigned char lun;             // 09: LUN
	unsigned char devtype;         // 10: Device type
	unsigned char reserved;        // 11: Reserved
} ASPI_SRB_DEVTYPE;

// SRB for SCSI I/O

typedef struct {
	ASPI_SRB_HEAD h;               // 00: Header
	unsigned char target_id;       // 08: Target ID
	unsigned char lun;             // 09: LUN
	unsigned char reserved[2];     // 10: Reserved
	unsigned long data_size;       // 12: Data alloc. lenght
	void * data_addr;              // 16: Data buffer pointer
	unsigned char sense_size;      // 20: Sense alloc. length
	unsigned char cdb_size;        // 21: CDB length
	unsigned char host_status;     // 22: Host status
	unsigned char target_status;   // 23: Target status
	void * event_handle;           // 24: Event handle
	unsigned char workspace[20];   // 28: ASPI workspace
	unsigned char cdb[16+ASPI_SENSE_SIZE];
} ASPI_SRB_IO;

// Macro to retrieve start of sense information
#define ASPI_SRB_SENSE(srb,cdbsz) ((srb)->cdb + 16)

// SRB union

typedef union {
	ASPI_SRB_HEAD h;       // Common header
	ASPI_SRB_INQUIRY q;    // Inquiry
	ASPI_SRB_DEVTYPE t;    // Device type
	ASPI_SRB_IO i;         // I/O
} ASPI_SRB;

#pragma pack()

// ASPI commands
#define ASPI_CMD_ADAPTER_INQUIRE        0x00
#define ASPI_CMD_GET_DEVICE_TYPE        0x01
#define ASPI_CMD_EXECUTE_IO             0x02
#define ASPI_CMD_ABORT_IO               0x03

// Request flags
#define ASPI_REQFLAG_DIR_TO_HOST        0x08
#define ASPI_REQFLAG_DIR_TO_TARGET      0x10
#define ASPI_REQFLAG_DIR_NO_XFER        0x18
#define ASPI_REQFLAG_EVENT_NOTIFY       0x40

// ASPI status
#define ASPI_STATUS_IN_PROGRESS         0x00
#define ASPI_STATUS_NO_ERROR            0x01
#define ASPI_STATUS_ABORTED             0x02
#define ASPI_STATUS_ABORT_ERR           0x03
#define ASPI_STATUS_ERROR               0x04
#define ASPI_STATUS_INVALID_COMMAND     0x80
#define ASPI_STATUS_INVALID_ADAPTER     0x81
#define ASPI_STATUS_INVALID_TARGET      0x82
#define ASPI_STATUS_NO_ADAPTERS         0xE8

// Adapter (host) status
#define ASPI_HSTATUS_NO_ERROR           0x00
#define ASPI_HSTATUS_SELECTION_TIMEOUT  0x11
#define ASPI_HSTATUS_DATA_OVERRUN       0x12
#define ASPI_HSTATUS_BUS_FREE           0x13
#define ASPI_HSTATUS_BUS_PHASE_ERROR    0x14
#define ASPI_HSTATUS_BAD_SGLIST         0x1A

// Target status
#define ASPI_TSTATUS_NO_ERROR           0x00
#define ASPI_TSTATUS_CHECK_CONDITION    0x02
#define ASPI_TSTATUS_BUSY               0x08
#define ASPI_TSTATUS_RESERV_CONFLICT    0x18


static HINSTANCE h_aspi_dll; // DLL handle
static UINT (* aspi_entry)(ASPI_SRB * srb); // ASPI entrypoint
static unsigned num_aspi_adapters;

#ifdef __CYGWIN__
// h_aspi_dll+aspi_entry is not inherited by Cygwin's fork()
static DWORD aspi_dll_pid; // PID of DLL owner to detect fork()
#define aspi_entry_valid() (aspi_entry && (aspi_dll_pid == GetCurrentProcessId()))
#else
#define aspi_entry_valid() (!!aspi_entry)
#endif


static int aspi_call(ASPI_SRB * srb)
{
	int i;
	aspi_entry(srb);
	i = 0;
	while (((volatile ASPI_SRB *)srb)->h.status == ASPI_STATUS_IN_PROGRESS) {
		if (++i > 100/*10sek*/) {
			pout("ASPI Adapter %u: Timed out\n", srb->h.adapter);
			aspi_entry = 0;
			h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
			errno = EIO;
			return -1;
		}
		if (con->reportscsiioctl > 1)
			pout("ASPI Adapter %u: Waiting (%d) ...\n", srb->h.adapter, i);
		Sleep(100);
	}
	return 0;
}


// Get ASPI entrypoint from wnaspi32.dll

static FARPROC aspi_get_address(const char * name, int verbose)
{
	FARPROC addr;
	assert(h_aspi_dll && h_aspi_dll != INVALID_HANDLE_VALUE);

	if (!(addr = GetProcAddress(h_aspi_dll, name))) {
		if (verbose)
			pout("Missing %s() in WNASPI32.DLL\n", name);
		aspi_entry = 0;
		FreeLibrary(h_aspi_dll);
		h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
		errno = ENOSYS;
		return 0;
	}
	return addr;
}


static int aspi_open_dll(int verbose)
{
	UINT (*aspi_info)(void);
	UINT info, rc;

	assert(!aspi_entry_valid());

	// Check structure layout
	assert(sizeof(ASPI_SRB_HEAD) == 8);
	assert(sizeof(ASPI_SRB_INQUIRY) == 58);
	assert(sizeof(ASPI_SRB_DEVTYPE) == 12);
	assert(sizeof(ASPI_SRB_IO) == 64+ASPI_SENSE_SIZE);
	assert(offsetof(ASPI_SRB,h.cmd) == 0);
	assert(offsetof(ASPI_SRB,h.flags) == 3);
	assert(offsetof(ASPI_SRB_IO,lun) == 9);
	assert(offsetof(ASPI_SRB_IO,data_addr) == 16);
	assert(offsetof(ASPI_SRB_IO,workspace) == 28);
	assert(offsetof(ASPI_SRB_IO,cdb) == 48);

	if (h_aspi_dll == INVALID_HANDLE_VALUE) {
		// do not retry
		errno = ENOENT;
		return -1;
	}

	// Load ASPI DLL
	if (!(h_aspi_dll = LoadLibraryA("WNASPI32.DLL"))) {
		if (verbose)
			pout("Cannot load WNASPI32.DLL, Error=%ld\n", GetLastError());
		h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
		errno = ENOENT;
		return -1;
	}
	if (con->reportscsiioctl > 1) {
		// Print full path of WNASPI32.DLL
		char path[MAX_PATH];
		if (!GetModuleFileName(h_aspi_dll, path, sizeof(path)))
			strcpy(path, "*unknown*");
		pout("Using ASPI interface \"%s\"\n", path);
	}

	// Get ASPI entrypoints
	if (!(aspi_info = (UINT (*)(void))aspi_get_address("GetASPI32SupportInfo", verbose)))
		return -1;
	if (!(aspi_entry = (UINT (*)(ASPI_SRB *))aspi_get_address("SendASPI32Command", verbose)))
		return -1;

	// Init ASPI manager and get number of adapters
	info = (aspi_info)();
	if (con->reportscsiioctl > 1)
		pout("GetASPI32SupportInfo() returns 0x%04x\n", info);
	rc = (info >> 8) & 0xff;
	if (rc == ASPI_STATUS_NO_ADAPTERS) {
		num_aspi_adapters = 0;
	}
	else if (rc == ASPI_STATUS_NO_ERROR) {
		num_aspi_adapters = info & 0xff;
	}
	else {
		if (verbose)
			pout("Got strange 0x%04x from GetASPI32SupportInfo()\n", info);
		aspi_entry = 0;
		FreeLibrary(h_aspi_dll);
		h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
		errno = ENOENT;
		return -1;
	}

	if (con->reportscsiioctl)
		pout("%u ASPI Adapter%s detected\n",num_aspi_adapters, (num_aspi_adapters!=1?"s":""));

#ifdef __CYGWIN__
	// save PID to detect fork() in aspi_entry_valid()
	aspi_dll_pid = GetCurrentProcessId();
#endif
	assert(aspi_entry_valid());
	return 0;
}


static int aspi_io_call(ASPI_SRB * srb, unsigned timeout)
{
	HANDLE event;
	// Create event
	if (!(event = CreateEventA(NULL, FALSE, FALSE, NULL))) {
		pout("CreateEvent(): Error=%ld\n", GetLastError()); return -EIO;
	}
	srb->i.event_handle = event;
	srb->h.flags |= ASPI_REQFLAG_EVENT_NOTIFY;
	// Start ASPI request
	aspi_entry(srb);
	if (((volatile ASPI_SRB *)srb)->h.status == ASPI_STATUS_IN_PROGRESS) {
		// Wait for event
		DWORD rc = WaitForSingleObject(event, timeout*1000L);
		if (rc != WAIT_OBJECT_0) {
			if (rc == WAIT_TIMEOUT) {
				pout("ASPI Adapter %u, ID %u: Timed out after %u seconds\n",
					srb->h.adapter, srb->i.target_id, timeout);
			}
			else {
				pout("WaitForSingleObject(%lx) = 0x%lx,%ld, Error=%ld\n",
					(unsigned long)event, rc, rc, GetLastError());
			}
			// TODO: ASPI_ABORT_IO command
			aspi_entry = 0;
			h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
			return -EIO;
		}
	}
	CloseHandle(event);
	return 0;
}


static int aspi_open(unsigned adapter, unsigned id)
{
	ASPI_SRB srb;
	if (!(adapter <= 9 && id < 16)) {
		errno = ENOENT;
		return -1;
	}

	if (!aspi_entry_valid()) {
		if (aspi_open_dll(1/*verbose*/))
			return -1;
	}

	// Adapter OK?
	if (adapter >= num_aspi_adapters) {
		pout("ASPI Adapter %u does not exist (%u Adapter%s detected).\n",
			adapter, num_aspi_adapters, (num_aspi_adapters!=1?"s":""));
		if (!is_permissive()) {
			errno = ENOENT;
			return -1;
		}
	}

	// Device present ?
	memset(&srb, 0, sizeof(srb));
	srb.h.cmd = ASPI_CMD_GET_DEVICE_TYPE;
	srb.h.adapter = adapter; srb.i.target_id = id;
	if (aspi_call(&srb)) {
		errno = EIO;
		return -1;
	}
	if (srb.h.status != ASPI_STATUS_NO_ERROR) {
		pout("ASPI Adapter %u, ID %u: No such device (Status=0x%02x)\n", adapter, id, srb.h.status);
		if (!is_permissive()) {
			errno = (srb.h.status == ASPI_STATUS_INVALID_TARGET ? ENOENT : EIO);
			return -1;
		}
	}
	else if (con->reportscsiioctl)
		pout("ASPI Adapter %u, ID %u: Device Type=0x%02x\n", adapter, id, srb.t.devtype);

	return (0x0100 | ((adapter & 0xf)<<4) | (id & 0xf));
}


static void aspi_close(int fd)
{
	// No FreeLibrary(h_aspi_dll) to prevent problems with ASPI threads
	ARGUSED(fd);
}


// Scan for SCSI drives, fill bitmask [adapter:0-9][id:0-7] of drives present,
// return #drives

static int aspi_scan(unsigned long * drives)
{
	int cnt = 0;
	unsigned ad;

	if (!aspi_entry_valid()) {
		if (aspi_open_dll(con->reportscsiioctl/*default is quiet*/))
			return 0;
	}

	for (ad = 0; ad < num_aspi_adapters; ad++) {
		ASPI_SRB srb; unsigned id;

		if (ad > 9) {
			if (con->reportscsiioctl)
				pout(" ASPI Adapter %u: Ignored\n", ad);
			continue;
		}

		// Get adapter name
		memset(&srb, 0, sizeof(srb));
		srb.h.cmd = ASPI_CMD_ADAPTER_INQUIRE;
		srb.h.adapter = ad;
		if (aspi_call(&srb))
			return 0;

		if (srb.h.status != ASPI_STATUS_NO_ERROR) {
			if (con->reportscsiioctl)
				pout(" ASPI Adapter %u: Status=0x%02x\n", ad, srb.h.status);
			continue;
		}

		if (con->reportscsiioctl) {
			int i;
			for (i = 1; i < 16 && srb.q.adapter_id[i]; i++)
				if (!(' ' <= srb.q.adapter_id[i] && srb.q.adapter_id[i] <= '~'))
					srb.q.adapter_id[i] = '?';
			pout(" ASPI Adapter %u (\"%.16s\"):\n", ad, srb.q.adapter_id);
		}

		bool ignore = !strnicmp(srb.q.adapter_id, "3ware", 5);

		for (id = 0; id <= 7; id++) {
			// Get device type
			memset(&srb, 0, sizeof(srb));
			srb.h.cmd = ASPI_CMD_GET_DEVICE_TYPE;
			srb.h.adapter = ad; srb.i.target_id = id;
			if (aspi_call(&srb))
				return 0;
			if (srb.h.status != ASPI_STATUS_NO_ERROR) {
				if (con->reportscsiioctl > 1)
					pout("  ID %u: No such device (Status=0x%02x)\n", id, srb.h.status);
				continue;
			}

			if (!ignore && srb.t.devtype == 0x00/*HDD*/) {
				if (con->reportscsiioctl)
					pout("  ID %u: Device Type=0x%02x\n", id, srb.t.devtype);
				drives[ad >> 2] |= (1L << (((ad & 0x3) << 3) + id));
				cnt++;
			}
			else if (con->reportscsiioctl)
				pout("  ID %u: Device Type=0x%02x (ignored)\n", id, srb.t.devtype);
		}
	}
	return cnt;
}


/////////////////////////////////////////////////////////////////////////////

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
	ASPI_SRB srb;

	if (!aspi_entry_valid())
		return -EBADF;
	if (!((fd & ~0xff) == 0x100))
		return -EBADF;

	if (!(iop->cmnd_len == 6 || iop->cmnd_len == 10 || iop->cmnd_len == 12 || iop->cmnd_len == 16)) {
		pout("do_scsi_cmnd_io: bad CDB length\n");
		return -EINVAL;
	}

	if (report > 0) {
		// From os_linux.c
		int k, j;
		const unsigned char * ucp = iop->cmnd;
		const char * np;
		char buff[256];
		const int sz = (int)sizeof(buff);

		np = scsi_get_opcode_name(ucp[0]);
		j = snprintf(buff, sz, " [%s: ", np ? np : "<unknown opcode>");
		for (k = 0; k < (int)iop->cmnd_len; ++k)
			j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "%02x ", ucp[k]);
		if ((report > 1) && 
			(DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
			int trunc = (iop->dxfer_len > 256) ? 1 : 0;

			j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n  Outgoing "
						  "data, len=%d%s:\n", (int)iop->dxfer_len,
						  (trunc ? " [only first 256 bytes shown]" : ""));
			dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
		}
		else
			j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
		pout(buff);
	}

	memset(&srb, 0, sizeof(srb));
	srb.h.cmd = ASPI_CMD_EXECUTE_IO;
	srb.h.adapter = ((fd >> 4) & 0xf);
	srb.i.target_id = (fd & 0xf);
	//srb.i.lun = 0;
	srb.i.sense_size = ASPI_SENSE_SIZE;
	srb.i.cdb_size = iop->cmnd_len;
	memcpy(srb.i.cdb, iop->cmnd, iop->cmnd_len);

	switch (iop->dxfer_dir) {
		case DXFER_NONE:
			srb.h.flags = ASPI_REQFLAG_DIR_NO_XFER;
			break;
		case DXFER_FROM_DEVICE:
			srb.h.flags = ASPI_REQFLAG_DIR_TO_HOST;
			srb.i.data_size = iop->dxfer_len;
			srb.i.data_addr = iop->dxferp;
			break;
		case DXFER_TO_DEVICE:
			srb.h.flags = ASPI_REQFLAG_DIR_TO_TARGET;
			srb.i.data_size = iop->dxfer_len;
			srb.i.data_addr = iop->dxferp;
			break;
		default:
			pout("do_scsi_cmnd_io: bad dxfer_dir\n");
			return -EINVAL;
	}

	iop->resp_sense_len = 0;
	iop->scsi_status = 0;
	iop->resid = 0;

	if (aspi_io_call(&srb, (iop->timeout ? iop->timeout : 60))) {
		// Timeout
		return -EIO;
	}

	if (srb.h.status != ASPI_STATUS_NO_ERROR) {
		if (   srb.h.status        == ASPI_STATUS_ERROR
		    && srb.i.host_status   == ASPI_HSTATUS_NO_ERROR
		    && srb.i.target_status == ASPI_TSTATUS_CHECK_CONDITION) {
			// Sense valid
			const unsigned char * sense = ASPI_SRB_SENSE(&srb.i, iop->cmnd_len);
			int len = (ASPI_SENSE_SIZE < iop->max_sense_len ? ASPI_SENSE_SIZE : iop->max_sense_len);
			iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
			if (len > 0 && iop->sensep) {
				memcpy(iop->sensep, sense, len);
				iop->resp_sense_len = len;
				if (report > 1) {
					pout("  >>> Sense buffer, len=%d:\n", (int)len);
					dStrHex(iop->sensep, len , 1);
				}
			}
			if (report) {
				pout("  sense_key=%x asc=%x ascq=%x\n",
				 sense[2] & 0xf, sense[12], sense[13]);
			}
			return 0;
		}
		else {
			if (report)
				pout("  ASPI call failed, (0x%02x,0x%02x,0x%02x)\n", srb.h.status, srb.i.host_status, srb.i.target_status);
			return -EIO;
		}
	}

	if (report > 0)
		pout("  OK\n");

	if (iop->dxfer_dir == DXFER_FROM_DEVICE && report > 1) {
		 int trunc = (iop->dxfer_len > 256) ? 1 : 0;
		 pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
			  (trunc ? " [only first 256 bytes shown]" : ""));
				dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
	}

	return 0;
}