/*
 * usb-emu.c - USB driver for USBIP emulation
 *
 * Copyright (C) 2017  Flying Stone Technology
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * This file is a part of Chopstx, a thread library for embedded.
 *
 * Chopstx is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chopstx is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdint.h>
#include <stdlib.h>

#include "../chopstx/usb_lld.h"

int
usb_lld_ctrl_ack (struct usb_dev *dev)
{
  return 0;
}

int
usb_lld_ctrl_recv (struct usb_dev *dev, void *p, size_t len)
{
  return 0;
}

int
usb_lld_ctrl_send (struct usb_dev *dev, const void *buf, size_t buflen)
{
  return 0;
}

uint8_t
usb_lld_current_configuration (struct usb_dev *dev)
{
  return 0;
}

void
usb_lld_prepare_shutdown (void)
{
}

void
usb_lld_ctrl_error (struct usb_dev *dev)
{
}

void
usb_lld_reset (struct usb_dev *dev, uint8_t feature)
{
}

void
usb_lld_set_configuration (struct usb_dev *dev, uint8_t config)
{
}



void
usb_lld_shutdown (void)
{
}

void
usb_lld_setup_endpoint (int ep_num, int ep_type, int ep_kind,
			int ep_rx_addr, int ep_tx_addr,
			int ep_rx_memory_size)
{
}

void
usb_lld_stall (int ep_num)
{
}


void
usb_lld_stall_tx (int ep_num)
{
  usb_lld_stall (ep_num);
}

void
usb_lld_stall_rx (int ep_num)
{
  usb_lld_stall (ep_num);
}
