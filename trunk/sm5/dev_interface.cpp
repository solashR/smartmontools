/*
 * dev_interface.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2008 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "dev_interface.h"
#include "dev_tunnelled.h"
#include "utility.h"

#include <stdexcept>

const char * dev_interface_cpp_cvsid = "$Id: dev_interface.cpp,v 1.1 2008/07/25 21:16:00 chrfranke Exp $"
  DEV_INTERFACE_H_CVSID;

/////////////////////////////////////////////////////////////////////////////
// smart_device

smart_device::smart_device(smart_interface * intf, const char * dev_name,
    const char * dev_type, const char * req_type)
: m_intf(intf), m_info(dev_name, dev_type, req_type),
  m_ata_ptr(0), m_scsi_ptr(0)
{
}

smart_device::smart_device(do_not_use_in_implementation_classes)
: m_intf(0), m_ata_ptr(0), m_scsi_ptr(0)
{
  throw std::logic_error("smart_device: wrong constructor called in implementation class");
}

smart_device::~smart_device() throw()
{
}

bool smart_device::set_err(int no, const char * msg, ...)
{
  if (!msg)
    return set_err(no);
  m_err.no = no;
  va_list ap; va_start(ap, msg);
  m_err.msg = vstrprintf(msg, ap);
  va_end(ap);
  return false;
}

bool smart_device::set_err(int no)
{
  smi()->set_err_var(&m_err, no);
  return false;
}

smart_device * smart_device::autodetect_open()
{
  open();
  return this;
}

bool smart_device::owns(const smart_device * /*dev*/) const
{
  return false;
}

void smart_device::release(const smart_device * /*dev*/)
{
}


/////////////////////////////////////////////////////////////////////////////
// ata_device

ata_cmd_in::ata_cmd_in()
: direction(no_data),
  buffer(0),
  size(0)
{
}

ata_cmd_out::ata_cmd_out()
{
}

bool ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!in.in_regs.is_48bit_cmd())
    return ata_pass_through_28bit(in, out);
  else
    return ata_pass_through_48bit(in, out);
}

bool ata_device::ata_pass_through(const ata_cmd_in & in)
{
  ata_cmd_out dummy;
  return ata_pass_through(in, dummy);
}

bool ata_device::ata_pass_through_48bit(const ata_cmd_in & /*in*/, ata_cmd_out & /*out*/)
{
  return set_err(ENOSYS, "48-bit ATA commands not supported");
}

bool ata_device::ata_identify_is_cached() const
{
  return false;
}


/////////////////////////////////////////////////////////////////////////////
// tunnelled_device_base

tunnelled_device_base::tunnelled_device_base(smart_device * tunnel_dev)
: smart_device(never_called),
  m_tunnel_base_dev(tunnel_dev)
{
}

tunnelled_device_base::~tunnelled_device_base() throw()
{
  delete m_tunnel_base_dev;
}

bool tunnelled_device_base::is_open() const
{
  return (m_tunnel_base_dev && m_tunnel_base_dev->is_open());
}

bool tunnelled_device_base::open()
{
  if (!m_tunnel_base_dev)
    return set_err(ENOSYS);
  if (!m_tunnel_base_dev->open())
    return set_err(m_tunnel_base_dev->get_err());
  return true;
}

bool tunnelled_device_base::close()
{
  if (!m_tunnel_base_dev)
    return true;
  if (!m_tunnel_base_dev->close())
    return set_err(m_tunnel_base_dev->get_err());
  return true;
}

bool tunnelled_device_base::owns(const smart_device * dev) const
{
  return (m_tunnel_base_dev && (m_tunnel_base_dev == dev));
}

void tunnelled_device_base::release(const smart_device * dev)
{
  if (m_tunnel_base_dev == dev)
    m_tunnel_base_dev = 0;
}


/////////////////////////////////////////////////////////////////////////////
// smart_interface

// Pointer to (usually singleton) interface object returned by ::smi()
smart_interface * smart_interface::s_instance;

const char * smart_interface::get_os_version_str()
{
  return SMARTMONTOOLS_BUILD_HOST;
}

const char * smart_interface::get_valid_dev_types_str()
{
  static char buf[80+1];
  if (buf[0])
    return buf;
  // default
  strcpy(buf, "ata, scsi, sat[,N]");
  // append custom
  const char * add = get_valid_custom_dev_types_str();
  if (!add || !*add)
    return buf;
  strcat(buf, ", ");
  strcat(buf, add);
  return buf;
}

const char * smart_interface::get_app_examples(const char * /*appname*/)
{
  return 0;
}

void smart_interface::set_err(int no, const char * msg, ...)
{
  if (!msg) {
    set_err(no); return;
  }
  m_err.no = no;
  va_list ap; va_start(ap, msg);
  m_err.msg = vstrprintf(msg, ap);
  va_end(ap);
}

void smart_interface::set_err(int no)
{
  set_err_var(&m_err, no);
}

void smart_interface::set_err_var(smart_device::error_info * err, int no)
{
  err->no = no;
  err->msg = get_msg_for_errno(no);
  if (err->msg.empty() && no != 0)
    err->msg = strprintf("Unknown error %d", no);
}

const char * smart_interface::get_msg_for_errno(int no)
{
  return strerror(no);
}


/////////////////////////////////////////////////////////////////////////////
// Default device factory

smart_device * smart_interface::get_smart_device(const char * name, const char * type)
{
  clear_err();
  if (!type || !*type) {
    smart_device * dev = autodetect_smart_device(name);
    if (!dev && !get_errno())
      set_err(EINVAL, "Unable to detect device type");
    return dev;
  }

  smart_device * dev = get_custom_smart_device(name, type);
  if (dev || get_errno())
    return dev;

  if (!strcmp(type, "ata"))
    dev = get_ata_device(name, type);
  else if (!strcmp(type, "scsi"))
    dev = get_scsi_device(name, type);
  else if (   (!strncmp(type, "sat", 3) && (!type[3] || type[3] == ','))
           || (!strncmp(type, "usb", 3)))
    dev = get_sat_device(name, type /*, 0*/);
  else {
    set_err(EINVAL, "Unknown device type '%s'", type);
    return 0;
  }
  if (!dev && !get_errno())
    set_err(EINVAL, "Not a device of type '%s'", type);
  return dev;
}

smart_device * smart_interface::get_custom_smart_device(const char * /*name*/, const char * /*type*/)
{
  return 0;
}

const char * smart_interface::get_valid_custom_dev_types_str()
{
  return 0;
}
