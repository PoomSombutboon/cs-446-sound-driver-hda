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
#include <nautilus/list.h>
#include <nautilus/mm.h>
#include <nautilus/nautilus.h>
#include <nautilus/spinlock.h>

// for protection of global state in the driver
static spinlock_t global_lock;

// list of all HDA devices
static struct list_head dev_list;

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

// called by init.c to initialize the Intel HDA driver
int hda_pci_init(struct naut_info *naut) {
  INFO("init\n");

  spinlock_init(&global_lock);

  if (discover_devices(naut->sys.pci)) {
    ERROR("Discovery failed\n");
    return -1;
  }

  // TODO: return bringup_devices();
  return 0;
}

// deinitialize the Intel HDA driver
int hda_pci_deinit() {
  INFO("deinit\n");
  return 0;
}
