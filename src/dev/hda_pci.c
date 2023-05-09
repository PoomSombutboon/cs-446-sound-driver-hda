/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the
 * United States National  Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2017, Peter Dinda
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Theerut Amornkasemwong <teeamorn@u.northwesttern.edu>
 *          Poom Sombutboon <poomsombutboon2022@u.northwestern.edu>
 *          Andre Tsai <andretsai2024@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "hda_pci_internal.h"
#include <dev/hda_pci.h>
#include <dev/pci.h>
#include <nautilus/idt.h>
#include <nautilus/irq.h>
#include <nautilus/list.h>
#include <nautilus/mm.h>
#include <nautilus/nautilus.h>
#include <nautilus/sounddev.h>
#include <nautilus/spinlock.h>

// ========== TEMPORARY FUNCTION ==========
// TODO: REMOVE AFTER

static int handler(excp_entry_t *e, excp_vec_t v, void *priv_data) { return 0; }

int hda_get_avaiable_modes(void *state, struct nk_sound_dev_params params[]) {
  return 0;
}

struct nk_sound_dev_stream *
hda_open_stream(void *state, nk_sound_dev_stream_t stream_type,
                struct nk_sound_dev_params *params) {
  struct nk_sound_dev_stream *ret;
  return ret;
}

int hda_close_stream(void *state, struct nk_sound_dev_stream *stream) {
  return 0;
}

int hda_write_to_stream(void *state, struct nk_sound_dev_stream *stream,
                        uint8_t *src, uint64_t len,
                        void (*callback)(nk_sound_dev_status_t status,
                                         void *context),
                        void *context) {
  return 0;
}
int hda_read_from_stream(void *state, struct nk_sound_dev_stream *stream,
                         uint8_t *dst, uint64_t len,
                         void (*callback)(nk_sound_dev_status_t status,
                                          void *context),
                         void *context) {
  return 0;
}

int hda_get_stream_params(void *state, struct nk_sound_dev_stream *stream,
                          struct nk_sound_dev_params *p) {
  return 0;
}

// ========== GLOBAL FIELDS ==========

// for protection of global state in the driver
static spinlock_t global_lock;

// list of all HDA devices
static struct list_head dev_list;

// sounddev interface
static struct nk_sound_dev_int ops = {
    .get_avaiable_modes = hda_get_avaiable_modes,
    .open_stream = hda_open_stream,
    .close_stream = hda_close_stream,
    .write_to_stream = hda_write_to_stream,
    .read_from_stream = hda_read_from_stream,
    .get_stream_params = hda_get_stream_params,
};

// ========== METHODS ==========

// search for all HDA devices on the PCI bus
static int discover_devices(struct pci_info *pci) {
  struct list_head *curbus, *curdev;

  INIT_LIST_HEAD(&dev_list);

  if (!pci) {
    ERROR("No PCI info\n");
    return -1;
  }

  DEBUG("Finding Intel High Definition Audio (HDA) devices\n");

  list_for_each(curbus, &(pci->bus_list)) {
    struct pci_bus *bus = list_entry(curbus, struct pci_bus, bus_node);

    DEBUG("Searching PCI bus %u for HDA devices\n", bus->num);

    list_for_each(curdev, &(bus->dev_list)) {
      struct pci_dev *pdev = list_entry(curdev, struct pci_dev, dev_node);
      struct pci_cfg_space *cfg = &pdev->cfg;

      DEBUG("Device %u is a %x:%x\n", pdev->num, cfg->vendor_id,
            cfg->device_id);

      if (cfg->vendor_id == INTEL_VENDOR_ID &&
          cfg->device_id == HDA_DEVICE_ID) {
        DEBUG("Compatible HDA Device Found\n");

        struct hda_pci_dev *hdev;
        hdev = malloc(sizeof(struct hda_pci_dev));

        if (!hdev) {
          ERROR("Cannot allocate device\n");
          return -1;
        }

        memset(hdev, 0, sizeof(*hdev));
        // hdev->pci_intr = cfg->dev_cfg.intr_pin;
        // hdev->intr_vec = map_pci_irq_to_vec(bus, pdev);

        spinlock_init(&hdev->lock);

        // pci configuration space consists of up to six 32-bit base address
        // registers for each device
        // however, we only expect one bar to exist: it will be memory-mapped
        // and will be at bar 0
        hdev->method = NONE;
        for (int i = 0; i < 6; i++) {
          uint32_t bar = pci_cfg_readl(bus->num, pdev->num, 0, 0x10 + i * 4);

          DEBUG("bar %d: 0x%0x\n", i, bar);

          if (i >= 1 && bar != 0) {
            DEBUG("Not expecting this to be a non-empty bar...\n");
          }

          // if pci bars are memory-mapped (bit 0 is 0x0), make sure they
          // are only 32 bits wide
          // if type (bits 1 and 2) is 0x0, base register is 32 bits wide
          // if type is 0x2, bar is 64 bits wide
          if (!(bar & 0x1)) {
            uint8_t mem_bar_type = (bar & 0x6) >> 1;
            if (mem_bar_type != 0) {
              ERROR("Cannot handle memory bar type 0x%x\n", mem_bar_type);
              return -1;
            }
          }

          // to determine amonut of address space needed by device
          // 1. write a value of all 1's to the register then read it back
          // 2. mask the information bits
          // 3. perform a bitwise NOT and then increment result by 1

          // 1. write a value of all 1's to the register then read it back
          pci_cfg_writel(bus->num, pdev->num, 0, 0x10 + i * 4, 0xffffffff);
          uint32_t size = pci_cfg_readl(bus->num, pdev->num, 0, 0x10 + i * 4);

          // 2. mask the information bits
          if (bar & 0x1) {
            // i/o
            size &= 0xfffffffc;
          } else {
            // memory
            size &= 0xfffffff0;
          }

          // 3. perform a bitwise NOT and then increment result by 1
          size = ~size;
          size++;

          // put back the original bar value
          pci_cfg_writel(bus->num, pdev->num, 0, 0x10 + i * 4, bar);

          // skip if size of bar is 0 (non-existent)
          if (!size) {
            continue;
          }

          // make sure no other bar exists
          if (size > 0 && i >= 1) {
            ERROR("unexpected hda pci bar with size > 0!\n");
            return -1;
          }

          if (bar & 0x1) {
            hdev->ioport_start = bar & 0xffffffc0;
            hdev->ioport_end = hdev->ioport_start + size;
            hdev->method = IO;
          } else {
            hdev->mem_start = bar & 0xffffffc0;
            hdev->mem_end = hdev->mem_start + size;
            hdev->method = MEMORY;
          }
        }

        if (hdev->method == NONE) {
          ERROR("Device has no register access method\n");
          panic("Device has no register access method\n");
          return -1;
        }

        hdev->pci_dev = pdev;

        INFO("Found HDA device: bus=%u dev=%u func=%u: pci_intr=%u "
             "intr_vec=%u ioport_start=%p ioport_end=%p mem_start=%p "
             "mem_end=%p access_method=%s\n",
             bus->num, pdev->num, 0, hdev->pci_intr, hdev->intr_vec,
             hdev->ioport_start, hdev->ioport_end, hdev->mem_start,
             hdev->mem_end,
             hdev->method == IO       ? "IO"
             : hdev->method == MEMORY ? "MEMORY"
                                      : "NONE");

        list_add(&hdev->hda_node, &dev_list);
      }
    }
  }

  return 0;
}

static int configure_msi_interrupts(struct hda_pci_dev *dev) {
  DEBUG("Configuring interrupts using MSI for device %u:%u.%u\n",
        dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);

  if (dev->pci_dev->msi.type == PCI_MSI_NONE) {
    ERROR("Device %u:%u.%u does not support MSI\n");
    return -1;
  }

  uint64_t num_vecs = dev->pci_dev->msi.num_vectors_needed;
  uint64_t base_vec;

  if (idt_find_and_reserve_range(num_vecs, 1, (ulong_t *)&base_vec)) {
    ERROR("Unabled to reserve %d interrupt table slots for device %u:%u.%u\n",
          num_vecs, dev->pci_dev->bus->num, dev->pci_dev->num,
          dev->pci_dev->fun);
    return -1;
  }

  if (pci_dev_enable_msi(dev->pci_dev, base_vec, num_vecs, 0)) {
    ERROR("Failed to enable MSI on device %u:%u.%u\n", dev->pci_dev->bus->num,
          dev->pci_dev->num, dev->pci_dev->fun);
    return -1;
  }

  for (uint64_t i = base_vec; i < base_vec + num_vecs; i++) {
    if (register_int_handler(i, handler, dev)) {
      ERROR("Failed to register interrupt %d on device %u:%u.%u\n", i,
            dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);
      return -1;
    }
    DEBUG("Registered interrupt %d for device %u:%u.%u\n", i,
          dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);
  }

  for (uint64_t i = base_vec; i < base_vec + num_vecs; i++) {
    if (pci_dev_unmask_msi(dev->pci_dev, i)) {
      ERROR("Failed to unmask interrupt %d on device %u:%u.%u\n", i,
            dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);
      return -1;
    }
    DEBUG("Unmasked interrupt %d for device %u:%u.%u\n", i,
          dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);
  }

  DEBUG("Enabled MSI interrupt for vectors[%d, %d)\n", base_vec,
        base_vec + num_vecs);

  return 0;
}

static int bringup_device(struct hda_pci_dev *dev) {
  DEBUG("Bringing up device %u:%u.%u. Starting Address is: %x\n",
        dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun,
        dev->mem_start);

  if (configure_msi_interrupts(dev)) {
    ERROR("Failed to configure MSI interrupts");
    return -1;
  }

  // make sure pci config space command register is acceptable
  uint16_t cmd = pci_dev_cfg_readw(dev->pci_dev, HDA_PCI_COMMAND_OFFSET);
  cmd &= ~0x0400; // turn off interrupt disable
  cmd |= 0x7;     // make sure bus master, memory, and io space are enabled
  DEBUG("Writing PCI command register to 0x%x\n", cmd);
  pci_dev_cfg_writew(dev->pci_dev, 0x4, HDA_PCI_COMMAND_OFFSET);

  uint16_t status = pci_dev_cfg_readw(dev->pci_dev, HDA_PCI_STATUS_OFFSET);
  DEBUG("Reading PCI status register as 0x%x\n", status);

  // TODO: Initialize device...

  return 0;
}

static int bringup_devices() {

  DEBUG("Bringing up HDA devices\n");

  // number of devices (used to assign names)
  int num_devs = 0;

  struct list_head *curdev, tx_node;
  list_for_each(curdev, &(dev_list)) {
    struct hda_pci_dev *dev = list_entry(curdev, struct hda_pci_dev, hda_node);

    char buf[80];
    snprintf(buf, 80, "hda%d", num_devs++);

    if (bringup_device(dev)) {
      ERROR("Failed to bring up HDA device %s (%u:%u.%u)\n", buf,
            dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);
      return -1;
    }

    dev->nk_dev = nk_sound_dev_register(buf, 0, &ops, dev);
    if (!dev->nk_dev) {
      ERROR("Failed to register HDA device %s (%u:%u.%u)\n", buf,
            dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);
      return -1;
    }

    DEBUG("Brought up and registered HDA device %s (%u:%u.%u)\n", buf,
          dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun);
  }

  return 0;
}

// called by init.c to initialize the Intel HDA driver
int hda_pci_init(struct naut_info *naut) {
  INFO("init\n");

  spinlock_init(&global_lock);

  if (discover_devices(naut->sys.pci)) {
    ERROR("Discovery failed\n");
    return -1;
  }

  return bringup_devices();
}

// deinitialize the Intel HDA driver
int hda_pci_deinit() {
  INFO("deinit\n");
  return 0;
}
