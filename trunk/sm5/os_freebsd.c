#include "os_freebsd.h"

const char *os_XXXX_c_cvsid="$Id: os_freebsd.c,v 1.5 2003/10/08 09:00:41 ballen4705 Exp $" OS_XXXX_H_CVSID UTILITY_H_CVSID;

// Private table of open devices: guaranteed zero on startup since
// part of static data.
struct freebsd_dev_channel *devicetable[FREEBSD_MAXDEV];

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int device, smart_command_set command, int select, char *data) {
  struct freebsd_dev_channel* con = (struct freebsd_dev_channel*)device;
  int retval, copydata=0;
  struct ata_cmd iocmd;
  unsigned char buff[512];
  bzero(buff,512);

  bzero(&iocmd,sizeof(struct ata_cmd));
  bzero(buff,512);
  iocmd.cmd=ATAREQUEST;
  iocmd.channel=con->channel;
  iocmd.device=con->device;

  iocmd.u.request.u.ata.command=WIN_SMART;
  iocmd.u.request.timeout=600;
  switch (command){
  case READ_VALUES:
    iocmd.u.request.u.ata.feature=SMART_READ_VALUES;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case READ_THRESHOLDS:
    iocmd.u.request.u.ata.feature=SMART_READ_THRESHOLDS;
    iocmd.u.request.u.ata.count=1;
    iocmd.u.request.u.ata.lba=1|(0xc24f<<8);
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case READ_LOG:
    iocmd.u.request.u.ata.feature=SMART_READ_LOG_SECTOR;
    iocmd.u.request.u.ata.lba=select|(0xc24f<<8);
    iocmd.u.request.u.ata.count=1;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case IDENTIFY:
    iocmd.u.request.u.ata.command=WIN_IDENTIFY;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case PIDENTIFY:
    iocmd.u.request.u.ata.command=WIN_PIDENTIFY;
    iocmd.u.request.flags=ATA_CMD_READ;
    iocmd.u.request.data=buff;
    iocmd.u.request.count=512;
    copydata=1;
    break;
  case ENABLE:
    iocmd.u.request.u.ata.feature=SMART_ENABLE;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case DISABLE:
    iocmd.u.request.u.ata.feature=SMART_DISABLE;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case AUTO_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete
    iocmd.u.request.u.ata.feature=SMART_AUTO_OFFLINE;
    iocmd.u.request.u.ata.lba=select|(0xc24f<<8);
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case AUTOSAVE:
    iocmd.u.request.u.ata.feature=SMART_AUTOSAVE;
    iocmd.u.request.u.ata.count=0xf1;  // to enable autosave
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case IMMEDIATE_OFFLINE:
    iocmd.u.request.u.ata.feature=SMART_IMMEDIATE_OFFLINE;
    iocmd.u.request.u.ata.lba = select|(0xc24f<<8); // put test in sector
    iocmd.u.request.flags=ATA_CMD_CONTROL;
    break;
  case STATUS_CHECK: // same command, no HDIO in FreeBSD
  case STATUS:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    iocmd.u.request.u.ata.feature=SMART_STATUS;
    iocmd.u.request.u.ata.lba=0xc24f<<8;
    iocmd.u.request.flags=ATA_CMD_CONTROL|ATA_CMD_READ_REG;
    break;
  default:
    pout("Unrecognized command %d in linux_ata_command_interface()\n", command);
    exit(1);
    break;
  }
  
  if (command==STATUS_CHECK){
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    unsigned char low,high;
    
    if ((retval=ioctl(con->atacommand, IOCATA, &iocmd)))
      return -1;

    high = (iocmd.u.request.u.ata.lba >> 16) & 0xff;
    low = (iocmd.u.request.u.ata.lba >> 8) & 0xff;
    
    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (low==normal_lo && high==normal_hi)
      return 0;
    
    // These values mean "Bad SMART status"
    if (low==failed_lo && high==failed_hi)
      return 1;
    
    // We haven't gotten output that makes sense; print out some debugging info
    syserror("Error SMART Status command failed");
    pout("Please get assistance from %s\n",PROJECTHOME);
    pout("Register values returned from SMART Status command are:\n");
    pout("CMD=0x%02x\n",(int)iocmd.u.request.u.ata.command);
    pout("FR =0x%02x\n",(int)iocmd.u.request.u.ata.feature);
    pout("NS =0x%02x\n",(int)iocmd.u.request.u.ata.count);
    pout("SC =0x%02x\n",(int)((iocmd.u.request.u.ata.lba) & 0xff));
    pout("CL =0x%02x\n",(int)((iocmd.u.request.u.ata.lba>>8) & 0xff));
    pout("CH =0x%02x\n",(int)((iocmd.u.request.u.ata.lba>>16) & 0xff));
    pout("RETURN =0x%04x\n",(int)iocmd.u.request.error);
    return 0;   
  }

  if ((retval=ioctl(con->atacommand, IOCATA, &iocmd))) {
    perror("Failed command: ");
    return -1;
  }
  // 
  if (copydata)
    memcpy(data, buff, 512);
  
  return 0; 
}


// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report)
{
  // not currently supported
  return -ENOSYS;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data) {
  // not currently supported
  return -1;
}

// Guess device type (ata or scsi) based on device name (FreeBSD
// specific) SCSI device name in FreeBSD can be sd, sr, scd, st, nst,
// osst, nosst and sg.
static const char * fbsd_dev_prefix = "/dev/";
static const char * fbsd_dev_ata_disk_prefix = "ad";
static const char * fbsd_dev_scsi_disk_plus = "s";
static const char * fbsd_dev_scsi_tape1 = "ns";
static const char * fbsd_dev_scsi_tape2 = "os";
static const char * fbsd_dev_scsi_tape3 = "nos";

int parse_ata_chan_dev(const char * dev_name, struct freebsd_dev_channel *chan) {
  int len;
  int dev_prefix_len = strlen(fbsd_dev_prefix);
  
  // if dev_name null, or string length zero
  if (!dev_name || !(len = strlen(dev_name)))
    return GUESS_DEVTYPE_DONT_KNOW;
  
  // Remove the leading /dev/... if it's there
  if (!strncmp(fbsd_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len) 
      // if nothing else in the string, unrecognized
      return GUESS_DEVTYPE_DONT_KNOW;
    // else advance pointer to following characters
    dev_name += dev_prefix_len;
  }
  // form /dev/ad* or ad*
  if (!strncmp(fbsd_dev_ata_disk_prefix, dev_name,
	       strlen(fbsd_dev_ata_disk_prefix))) {
    int  devnum = *(dev_name += strlen(fbsd_dev_ata_disk_prefix)) - '0';
    if (chan != NULL) {
      chan->channel=devnum/2;	// 2 drives per channel
      chan->device=devnum%2;	// so dividend = channel, remainder=device
    }
    return GUESS_DEVTYPE_ATA;
  }
  
  // form /dev/s* or s*
  if (!strncmp(fbsd_dev_scsi_disk_plus, dev_name,
	       strlen(fbsd_dev_scsi_disk_plus)))
    return GUESS_DEVTYPE_SCSI;

  // we failed to recognize any of the forms
  return GUESS_DEVTYPE_DONT_KNOW;
}

int guess_device_type (const char* dev_name) {
  return parse_ata_chan_dev(dev_name,NULL);
}

// Like open().  Return positive integer handle, used by functions below only.  type="ATA" or "SCSI".
int deviceopen (const char* dev, char* mode) {
  struct freebsd_dev_channel *fdchan;
  int parse_ok, i;

  // Search table for a free entry
  for (i=0; i<FREEBSD_MAXDEV; i++)
    if (!freebsd_dev_channel[i])
      break;
  
  // If no free entry found, return error.  We have max allowed number
  // of "file descriptors" already allocated.
  if (i==FREEBSD_MAX)
    // Eduard, consider setting errno=EMFILE for this case...
    return -1;

  fdchan = malloc(sizeof(struct freebsd_dev_channel));
  if (fdchan == NULL)
    // Eduard, consider setting errno=ENOMEM for this case
    return -1;

  parse_ok = parse_ata_chan_dev (dev,fdchan);
  if (parse_ok != GUESS_DEVTYPE_ATA) {
    // Eduard, consider setting errno=ENOTTY for this case
    free(fdchan);
    return -1; // can't handle non ATA for now
  }

  if ((fdchan->atacommand = open("/dev/ata",O_RDWR))<0) {
    free (fdchan);
    return -1;
  }
  
  // return pointer to "file descriptor" table entry, properly offset.
  devicetable[i]=fdchan;
  return i+FREEBSD_FDOFFSET;
}

// Like close().  Acts on handles returned by above function.
int deviceclose (int fd) {
  struct freebsd_dev_channel *fdchan;
  int failed;

  // put valid "file descriptor" into range 0...FREEBSD_MAXDEV-1
  fd -= FREEBSD_FDOFFSET;
  
  // check for validity of "file descriptor".
  if (fd<0 || fd>=FREEBSD_MAXDEV || !(fdchan=devicetable[fd]))
    // FIXME: Eduard, consider setting errno=EINVAL or errno=ENODEV
    // before returning -1 here.
    return -1;
  
  // close device
  failed=close(fdchan->atacommand);
  
  // if close succeeded, then remove from device list
  // Eduard, should we also remove it from list if close() fails?  I'm
  // not sure. Here I only remove it from list if close() worked.
  if (!failed)
    devicetable[fd]=NULL;

  return failed;
}
