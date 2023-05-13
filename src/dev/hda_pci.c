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

#include <dev/hda_pci.h>
#include <dev/pci.h>
#include <nautilus/cpu.h>
#include <nautilus/idt.h>
#include <nautilus/irq.h>
#include <nautilus/list.h>
#include <nautilus/mm.h>
#include <nautilus/nautilus.h>
#include <nautilus/sounddev.h>
#include <nautilus/spinlock.h>

#include "hda_pci_internal.h"
#include "nautilus/printk.h"

// ========== TEMPORARY FUNCTION ==========
// TODO: REMOVE AFTER

static int handler(excp_entry_t *e, excp_vec_t v, void *priv_data) {
  DEBUG("**** INSIDE HANDLER ****\n");
  return 0;
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

// ========== FORWARD DECLRATION FOR INTERFACE HELPERS ==========

static int check_valid_params(struct hda_pci_dev *dev,
                              struct nk_sound_dev_params *params);
static int reset_stream(struct hda_pci_dev *dev,
                        struct nk_sound_dev_stream *stream);
static int configure_stream(struct hda_pci_dev *dev,
                            struct nk_sound_dev_stream *stream,
                            struct nk_sound_dev_params *params);

static int initialize_bdl(struct hda_pci_dev *dev,
                          struct nk_sound_dev_stream *stream, bdl_t *bdl);

// ==================== TEMPORARY FUNCTION =========================
static int find_avaliable_slot(struct hda_pci_dev *dev,
                               struct nk_sound_dev_stream *stream) {
  // find first available stream number
  // stream numbers range from 1 to 15, inclusive
  // for (uint8_t i = 1; i <= HDA_MAX_NUM_OF_STREAMS; i++) {
  //   // stream exists, move onto next stream
  //   if (dev->streams[i]) {
  //     continue;
  //   }
  //   // create new stream state
  //   stream = malloc(sizeof(struct nk_sound_dev_stream));
  //   if (!stream) {
  //     ERROR("Cannot allocate stream\n");
  //     return -1;
  //   }
  //   DEBUG("Opened new stream %i\n");
  //   return i;
  // }
  // ERROR("No streams available\n");
  // return -1;
  stream = malloc(sizeof(struct nk_sound_dev_stream));
  if (!stream) {
    ERROR("Cannot allocate a stream\n");
  }
  return OUTPUT_STREAM_NUM;
}
// ================================================================

// ========== INTERFACE ==========
int hda_open_stream(void *state, struct nk_sound_dev_params *params) {
  DEBUG("Opening new stream\n");

  if (!state) {
    ERROR("The device state pointer is null\n");
    return -1;
  }

  if (!params) {
    ERROR("The device parameters pointer is null\n");
    return -1;
  }

  struct hda_pci_dev *dev = (struct hda_pci_dev *)state;

  // check if the given sound parameters are valid
  DEBUG("Check if the given parameters are valid\n");
  if (check_valid_params(dev, params)) {
    ERROR("Given parameters are not compatible with the device\n");
    return -1;
  }
  DEBUG("Valid parameters check completed\n");

  // TODO: Setting up stream id
  // ============== TEMP ==============
  DEBUG("Allocating a stream\n");
  // Is it ok to do forward declaration like this?
  struct nk_sound_dev_stream *stream;
  int slot = find_avaliable_slot(dev, stream);
  if (slot == -1) {
    ERROR("Cannot allocate a stream\n");
  }
  DEBUG("Finished allocating a stream\n");
  stream->stream_id = slot;
  // ==================================

  DEBUG("Resetting stream\n");
  if (reset_stream(dev, stream)) {
    ERROR("Cannot reset stream\n");
  }
  DEBUG("Finished resetting stream\n");

  DEBUG("Configuring stream parameters\n");
  if (configure_stream(dev, stream, params)) {
    ERROR("Cannot configure stream parameter\n");
  }
  DEBUG("Finished configuring stream parameters\n");

  DEBUG("Initiallizing BDL\n");
  bdl_t *bdl;
  if (initialize_bdl(dev, stream, bdl)) {
    ERROR("Cannot initialize BDL\n");
  }
  DEBUG("Finished initializing BDL\n");

  // TODO:
  // Setup interupt

  struct hda_stream_info *stream_info;
  stream_info = malloc(sizeof(struct hda_stream_info));
  stream_info->stream;
  stream_info->bdl;

  // TODO:
  // Put the stream info into avaliable slot
}

int hda_get_avaiable_modes(void *state, struct nk_sound_dev_params params[],
                           uint32_t params_size) {

  DEBUG("Getting available modes on HDA device\n");

  if (!state) {
    ERROR("The device state pointer is null\n");
    return -1;
  }

  if (!params) {
    ERROR("The pointer to a list of params is null\n");
    return -1;
  }

  struct hda_pci_dev *dev = (struct hda_pci_dev *)state;

  struct list_head *curmode;
  uint32_t i = 0;
  list_for_each(curmode, &(dev->available_modes_list)) {
    struct available_mode *mode =
        list_entry(curmode, struct available_mode, node);
    params[i++] = mode->params;

    if (i == params_size) {
      break;
    }
  }

  DEBUG("Found %d available modes on HDA device %d\n", i, dev->pci_dev->num);
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

static inline uint32_t hda_pci_read_regl(struct hda_pci_dev *dev,
                                         uint32_t offset) {
  uint32_t result;
  if (dev->method == MEMORY) {
    uint64_t addr = dev->mem_start + offset;
    __asm__ __volatile__("movl (%1), %0" : "=r"(result) : "r"(addr) : "memory");
  } else {
    result = inl(dev->ioport_start + offset);
  }
  DEBUG_REGS("readl 0x%08x returns 0x%08x\n", offset, result);
  return result;
}

static inline uint16_t hda_pci_read_regw(struct hda_pci_dev *dev,
                                         uint32_t offset) {
  uint16_t result;
  if (dev->method == MEMORY) {
    uint64_t addr = dev->mem_start + offset;
    __asm__ __volatile__("movw (%1), %0" : "=r"(result) : "r"(addr) : "memory");
  } else {
    result = inw(dev->ioport_start + offset);
  }
  DEBUG_REGS("readw 0x%08x returns 0x%04x\n", offset, result);
  return result;
}

static inline uint8_t hda_pci_read_regb(struct hda_pci_dev *dev,
                                        uint32_t offset) {
  uint8_t result;
  if (dev->method == MEMORY) {
    uint64_t addr = dev->mem_start + offset;
    __asm__ __volatile__("movb (%1), %0" : "=r"(result) : "r"(addr) : "memory");
  } else {
    result = inb(dev->ioport_start + offset);
  }
  DEBUG_REGS("readb 0x%08x returns 0x%02x\n", offset, result);
  return result;
}

static inline void hda_pci_write_regl(struct hda_pci_dev *dev, uint32_t offset,
                                      uint32_t data) {
  DEBUG_REGS("writel 0x%08x with 0x%08x\n", offset, data);
  if (dev->method == MEMORY) {
    uint64_t addr = dev->mem_start + offset;
    __asm__ __volatile__("movl %1, (%0)" : : "r"(addr), "r"(data) : "memory");
  } else {
    outl(data, dev->ioport_start + offset);
  }
}

static inline void hda_pci_write_regw(struct hda_pci_dev *dev, uint32_t offset,
                                      uint16_t data) {
  DEBUG_REGS("writew 0x%08x with 0x%04x\n", offset, data);
  if (dev->method == MEMORY) {
    uint64_t addr = dev->mem_start + offset;
    __asm__ __volatile__("movw %1, (%0)" : : "r"(addr), "r"(data) : "memory");
  } else {
    outw(data, dev->ioport_start + offset);
  }
}

static inline void hda_pci_write_regb(struct hda_pci_dev *dev, uint32_t offset,
                                      uint8_t data) {
  DEBUG_REGS("writeb 0x%08x with 0x%02x\n", offset, data);
  if (dev->method == MEMORY) {
    uint64_t addr = dev->mem_start + offset;
    __asm__ __volatile__("movb %1, (%0)" : : "r"(addr), "r"(data) : "memory");
  } else {
    outb(data, dev->ioport_start + offset);
  }
}

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
            return -1;
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
            hdev->mem_start = bar & 0xfffffff0;
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

static int reset(struct hda_pci_dev *d) {
  gctl_t gctl;

  // CRST bit (offset 0x8, bit 0) should already be 0, but set it again here
  // to be sure
  gctl.val = hda_pci_read_regl(d, GCTL);
  gctl.crst = 0;
  hda_pci_write_regl(d, GCTL, gctl.val);

  DEBUG("Initiate HDA reset, gctl = %08x\n", gctl.val);

  // write 1 to the CRST bit to take controller out of reset
  // CRST bit will remain as 0 until reset is complete
  udelay(1000);
  gctl.crst = 1;
  hda_pci_write_regl(d, GCTL, gctl.val);

  // wait for CRST bit to be updated to 1
  do {
    gctl.val = hda_pci_read_regl(d, GCTL);
  } while (gctl.crst != 1);
  gctl.val = hda_pci_read_regl(d, GCTL);

  DEBUG("Reset completed, gctl = %08x\n", gctl.val);
  return 0;
}

static int discover_codecs(struct hda_pci_dev *d) {
  statests_t statests;

  // wait at least 521us after CRST bit has been set to 1
  udelay(1000);

  statests.val = hda_pci_read_regw(d, STATESTS);

  DEBUG("Discovering codecs, statests = %x\n", statests.val);

  // get addresses of codecs that are present
  int found = 0;
  for (int i = 0; i < SDIMAX; i++) {
    if (SDIWAKE(statests, i)) {
      DEBUG("Codec %d exists\n", i);
      d->codecs[i] = 1;
      found = 1;
    }
  }

  if (!found) {
    ERROR("Failed to find any codecs");
    return -1;
  }

  DEBUG("Discovered codecs, statests = %x\n", statests.val);
  return 0;
}

// the steps are defined by the specification, revision 1.0a, section 4.4.1.3
// on "Initializing the CORB", page 64
static int setup_corb(struct hda_pci_dev *d) {
  corbctl_t cc;
  corbsize_t cs;

  // stop CORB by turning off DMA
  cc.val = hda_pci_read_regb(d, CORBCTL);
  cc.corbrun = 0;
  hda_pci_write_regb(d, CORBCTL, cc.val);
  DEBUG("CORB turned off\n");

  // determine and configure CORB size on HDA device
  cs.val = hda_pci_read_regb(d, CORBSIZE);
  if (CORBSIZECAP_HAS_256(cs)) {
    d->corb.size = 256;
    cs.corbsize = 2;
  } else if (CORBSIZECAP_HAS_16(cs)) {
    d->corb.size = 16;
    cs.corbsize = 1;
  } else if (CORBSIZECAP_HAS_2(cs)) {
    d->corb.size = 2;
    cs.corbsize = 0;
  } else {
    ERROR("Cannot determine CORB available sizes from CODEC\n");
    return -1;
  }
  hda_pci_write_regb(d, CORBSIZE, cs.val);
  DEBUG("CORB size set to %d\n", d->corb.size);

  // update CORBUBASE and CORBLBASE registers
  corbubase_t cu = (uint32_t)(((uint64_t)d->corb.buf) >> 32);
  corblbase_t cl = (uint32_t)(((uint64_t)d->corb.buf));
  hda_pci_write_regl(d, CORBUBASE, cu);
  hda_pci_write_regl(d, CORBLBASE, cl);
  DEBUG("CORB DMA address set to %x:%x (%p)\n", cu, cl, d->corb.buf);

  // reset CORB read pointer; to reset:
  // 1. write 1 to CORBRPRST bit and wait for completion
  // 2. write 0 to CORBRPRST bit and wait for completion
  corbrp_t rp;

  rp.val = hda_pci_read_regw(d, CORBRP);
  rp.corbrprst = 1;
  hda_pci_write_regw(d, CORBRP, rp.val);
  do {
    rp.val = hda_pci_read_regw(d, CORBRP);
  } while (!rp.corbrprst);

  rp.val = hda_pci_read_regw(d, CORBRP);
  rp.corbrprst = 0;
  hda_pci_write_regw(d, CORBRP, rp.val);
  do {
    rp.val = hda_pci_read_regw(d, CORBRP);
  } while (rp.corbrprst);

  DEBUG("CORB read pointer has been reset\n");

  // reset CORB write pointer; to reset, just set CORBWP bit to 0
  corbwp_t wp;
  wp.val = hda_pci_read_regw(d, CORBWP);
  wp.corbwp = 0;
  hda_pci_write_regw(d, CORBWP, wp.val);

  DEBUG("CORB write pointer has been reset\n");

  // set CORBRUN bit to 1 to enable CORB operation
  cc.val = hda_pci_read_regb(d, CORBCTL);
  cc.corbrun = 1;
  hda_pci_write_regb(d, CORBCTL, cc.val);

  DEBUG("CORB turned on\n");

  return 0;
}

// the steps are defined by the specification, revision 1.0a, section 4.4.1.3
// on "Initializing the RIRB", page 68
static int setup_rirb(struct hda_pci_dev *d) {
  rirbctl_t rc;
  rirbsize_t rs;

  // stop RIRB by turning off DMA
  rc.val = hda_pci_read_regb(d, RIRBCTL);
  rc.rirbdmaen = 0;
  hda_pci_write_regb(d, RIRBCTL, rc.val);
  DEBUG("RIRB turned off\n");

  // determine RIRB size and allocate RIRB buffer in memory
  rs.val = hda_pci_read_regb(d, RIRBSIZE);
  if (RIRBSIZECAP_HAS_256(rs)) {
    d->rirb.size = 256;
    rs.rirbsize = 2;
  } else if (RIRBSIZECAP_HAS_16(rs)) {
    d->rirb.size = 16;
    rs.rirbsize = 1;
  } else if (RIRBSIZECAP_HAS_2(rs)) {
    d->rirb.size = 2;
    rs.rirbsize = 0;
  } else {
    ERROR("Cannot determine RIRB available sizes from CODEC\n");
    return -1;
  }
  hda_pci_write_regb(d, RIRBSIZE, rs.val);
  DEBUG("RIRB sizeset to %d\n", d->rirb.size);

  // update RIRBUBASE and RRIRBLBASE registers
  rirbubase_t ru = (uint32_t)(((uint64_t)d->rirb.buf) >> 32);
  rirblbase_t rl = (uint32_t)(((uint64_t)d->rirb.buf));
  hda_pci_write_regl(d, RIRBUBASE, ru);
  hda_pci_write_regl(d, RIRBLBASE, rl);
  DEBUG("RIRB DMA address set to %x:%x (%p)\n", ru, rl, d->rirb.buf);

  // since RIRB write pointer is inittialized to point to 0th entry, its
  // read pointer set to 0. note that the offset for each response is 8 bytes
  d->rirb.cur_read = 0;

  // ==================== HACK WORKAROUND ====================
  // we are not using the RIRB interrupt count feature at all, so it is not
  // entirely clear why RINTCNT has to be set.
  // HOWEVER, there appears to be a bug with QEMU's implementation of Intel
  // HDA the number of responses our device can send to RIRB is limited by
  // RINTCNT we are currently setting RINTCNT to the max value of 255, meaning
  // throughout the lifetime of our device, only 255 responses can be received
  rintcnt_t ri;
  ri.val = hda_pci_read_regw(d, RINTCNT);
  ri.rintcnt = -1;
  hda_pci_write_regw(d, RINTCNT, ri.val);
  // =========================================================

  DEBUG("RIRB interrupt count has been set to %d\n", ri.rintcnt);

  // start RIRB and start DMA (which runs when response queue is not empty)
  rc.val = hda_pci_read_regb(d, RIRBCTL);
  rc.rirbdmaen = 1;
  // we are NOT using interrupts to decide when to read from the RIRB
  // the "transact()" method always writes one command to CORB and then reads
  // the response (thus conusming it) from RIRB immediately
  rc.rirboic = 0; // interrupt from response overrun
  rc.rintctl = 0; // interrupt after N number of responses
  hda_pci_write_regb(d, RIRBCTL, rc.val);

  DEBUG("RIRB turned on\n");

  return 0;
}

static int setup_interrupts(struct hda_pci_dev *d) {
  // set all bits to ones to enable global, controller, and stream interrupts
  intctl_t c;
  c.val = 0;
  c.cie = 0;
  c.gie = 1;
  c.sie = -1;
  hda_pci_write_regl(d, INTCTL, c.val);

  DEBUG("Enable interupts: global, controller, and streams\n");

  return 0;
}

// see specification, figure 10, section 4.4.1.4, page 65
static int corb_push_request(struct hda_pci_dev *d, codec_req_t *r) {
  // make sure there are enough slots in the CORB buffer to push a new request
  // CORB buffer runs out of space when the write and read pointers are equal
  corbrp_t rp;
  corbwp_t wp;
  uint8_t new_wp;
  do {
    rp.val = hda_pci_read_regw(d, CORBRP);
    wp.val = hda_pci_read_regw(d, CORBWP);
    new_wp = (wp.corbwp + 1) % d->corb.size;
  } while (rp.corbrp == new_wp);

  // write command into the CORB buffer
  d->corb.buf[new_wp].val = r->val;

  // make sure this write becomes visible to other cpus
  // this should also make it visible to anything else that is
  // coherent.
  __asm__ __volatile__("mfence" : : : "memory");

  // update CORPWP to indicate index of last command in buffer
  wp.corbwp = new_wp;
  hda_pci_write_regw(d, CORBWP, wp.val);

  return 0;
}

static int rirb_pop_response(struct hda_pci_dev *d, codec_resp_t *r) {
  // make sure there is at least one response in the RIRB buffer to read
  // responses from; there is at least a response when write and read pointers
  // are not equal
  rirbwp_t wp;
  do {
    wp.val = hda_pci_read_regw(d, RIRBWP);
  } while (wp.rirbwp == d->rirb.cur_read);

  // read response from the RIRB buffer
  *r = d->rirb.buf[wp.rirbwp];

  // update internal "read pointer" state for bookkeeping sake
  d->rirb.cur_read = (d->rirb.cur_read + 1) % d->rirb.size;

  return 0;
}

static int transact(struct hda_pci_dev *d, int codec, int nid, int indirect,
                    uint32_t verb, codec_resp_t *rp) {
  codec_req_t rq;
  rq.val = 0;
  rq.CAd = codec;
  rq.nid = nid;
  rq.indirect = indirect;
  rq.verb = verb;

  if (corb_push_request(d, &rq)) {
    ERROR("Unable to send CORB request\n");
    return -1;
  }

  if (rirb_pop_response(d, rp)) {
    ERROR("Unable to get RIRB response\n");
    return -1;
  }

  return 0;
}

void print_debug_pcm(uint32_t resp) {
  DEBUG("      PCM sizes and rates: %08x\n", resp);
  if (PCM_IS_SUPPORTED(resp, PCM_BIT_DEPTH_32)) {
    DEBUG("        Depth: %s\n", "32 bits");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_BIT_DEPTH_24)) {
    DEBUG("        Depth: %s\n", "24 bits");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_BIT_DEPTH_20)) {
    DEBUG("        Depth: %s\n", "20 bits");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_BIT_DEPTH_16)) {
    DEBUG("        Depth: %s\n", "16 bits");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_BIT_DEPTH_8)) {
    DEBUG("        Depth: %s\n", "8 bits");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_8kHZ)) {
    DEBUG("        Rate: %s\n", "8 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_11kHZ025)) {
    DEBUG("        Rate: %s\n", "11.025 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_16kHZ)) {
    DEBUG("        Rate: %s\n", "16 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_22kHZ05)) {
    DEBUG("        Rate: %s\n", "22.05 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_32kHZ)) {
    DEBUG("        Rate: %s\n", "32 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_44kHZ1)) {
    DEBUG("        Rate: %s\n", "44.1 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_48kHZ)) {
    DEBUG("        Rate: %s\n", "48 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_88kHZ2)) {
    DEBUG("        Rate: %s\n", "88.2 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_96kHZ)) {
    DEBUG("        Rate: %s\n", "96 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_176kHZ4)) {
    DEBUG("        Rate: %s\n", "176 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_192kHZ)) {
    DEBUG("        Rate: %s\n", "192 kHz");
  }
  if (PCM_IS_SUPPORTED(resp, PCM_SAMPLE_RATE_384kHZ)) {
    DEBUG("        Rate: %s\n", "384 kHz");
  }
}

static int scan_codec(struct hda_pci_dev *d, int codec) {
  DEBUG("Scanning codec %d\n", codec);

  // Initialize available_modes_list, which is what we will use to store all
  // available modes supported by our HDA device
  INIT_LIST_HEAD(&d->available_modes_list);

  codec_resp_t rp;

  // Root Node has NID = 0
  // specification: section 7.2.1, page 132
  int root_node = 0;

  // get Root Node's Vendor and Device IDs
  // specification: section 7.3.4.1, page 198
  transact(d, codec, root_node, 0, MAKE_VERB_8(GET_PARAM, VENDOR), &rp);
  DEBUG("Root Node: codec vendor %04x device %04x\n", rp.resp >> 16 & 0xffff,
        rp.resp & 0xffff);

  // get function group nodes associated with the codec (root node)
  // specification: section 7.3.4.3, page 199
  transact(d, codec, root_node, 0, MAKE_VERB_8(GET_PARAM, SUBORD_NODE_COUNT),
           &rp);
  int first_func_node = rp.resp >> 16 & 0xff;
  int num_func_nodes = rp.resp & 0xff;

  // for each function group, we find its widgets
  uint8_t first_widget_node;
  uint8_t num_widget_nodes;
  uint8_t widget_type;
  for (int curr_func = first_func_node;
       curr_func < first_func_node + num_func_nodes; curr_func++) {
    // get widget associated with the function group
    // specification: section 7.3.4.3, page 199
    transact(d, codec, curr_func, 0, MAKE_VERB_8(GET_PARAM, SUBORD_NODE_COUNT),
             &rp);
    first_widget_node = rp.resp >> 16 & 0xff;
    num_widget_nodes = rp.resp & 0xff;

    // get functional group type of the current node
    // specification: section 7.3.4.4, page 199
    transact(d, codec, curr_func, 0, MAKE_VERB_8(GET_PARAM, FUNC_GROUP_TYPE),
             &rp);
    widget_type = rp.resp & 0xff;

    DEBUG("  Functional Group: NID %d type '%s'\n", curr_func,
          (widget_type == NODE_TYPE_AUDIO_FUNCTION_GROUP
               ? "Audio Function Group"
               : (widget_type == NODE_TYPE_AUDIO_VENDOR_DEFINED_MODEM
                      ? "Vendor Defined Modem"
                      : ((widget_type >= NODE_TYPE_AUDIO_VENDOR_DEFINED_START &&
                          widget_type <= NODE_TYPE_AUDIO_VENDOR_DEFINED_END)
                             ? "Vendor Defined Function Group"
                             : "Reserved"))));

    // we only care about the "Audio Function Group", which is responsible for
    // playing sound
    if (widget_type != 0x1) {
      continue;
    }

    // for each widget in the audio function group, get its paramters
    for (int curr_widget = first_widget_node;
         curr_widget < first_widget_node + num_widget_nodes; curr_widget++) {

      // get audio widget capabilities
      // specification: section 7.3.4.6, page 201
      transact(d, codec, curr_widget, 0,
               MAKE_VERB_8(GET_PARAM, AUDIO_WIDGET_CAPS), &rp);
      widget_type = rp.resp >> 20 & 0xf;

      // get string representation of the widget type
      char widget_type_str[10];
      switch (widget_type) {
      case WIDGET_TYPE_AUDIO_OUTPUT:
        snprintf(widget_type_str, 10, "'output'");
        break;
      case WIDGET_TYPE_AUDIO_INPUT:
        snprintf(widget_type_str, 10, "'input'");
        break;
      case WIDGET_TYPE_AUDIO_MIXER:
        snprintf(widget_type_str, 10, "'mixer'");
        break;
      case WIDGET_TYPE_AUDIO_SELECTOR:
        snprintf(widget_type_str, 10, "'selector'");
        break;
      case WIDGET_TYPE_PIN_COMPLEX:
        snprintf(widget_type_str, 10, "'pin'");
        break;
      case WIDGET_TYPE_POWER:
        snprintf(widget_type_str, 10, "'power'");
        break;
      case WIDGET_TYPE_VOLUME_KNOB:
        snprintf(widget_type_str, 10, "'volume'");
        break;
      case WIDGET_TYPE_BEEP_GENERATOR:
        snprintf(widget_type_str, 10, "'beep'");
        break;
      case WIDGET_TYPE_VENDOR_DEFINED:
        snprintf(widget_type_str, 10, "'vendor'");
      }
      DEBUG("    Widget Node: NID %d type %s\n", curr_widget, widget_type_str);

      // get number of channels supported by this widget
      uint8_t num_of_channels = 1 + (((rp.resp >> 12) & 0xe) | (rp.resp & 0x1));
      DEBUG("      Number of channels: %d\n", num_of_channels);

      // if widget is not output or input, skip
      if (widget_type != WIDGET_TYPE_AUDIO_OUTPUT &&
          widget_type != WIDGET_TYPE_AUDIO_INPUT) {
        continue;
      }

      // see if stream supports PCM
      // specification: section 7.3.4.8, page 205
      transact(d, codec, curr_widget, 0, MAKE_VERB_8(GET_PARAM, STREAM_FORMATS),
               &rp);
      if (rp.resp & 0xf) {
        DEBUG("      Supports PCM\n");
      } else {
        ERROR("      Does not support PCM\n");
        continue;
      }

      // get sample rate and bits per sample
      // specification: section 7.3.4.7, page 204
      transact(d, codec, curr_widget, 0,
               MAKE_VERB_8(GET_PARAM, PCM_SIZES_AND_RATES), &rp);
      uint32_t pcm_supported = rp.resp;
      print_debug_pcm(pcm_supported);

      // populate list of available modes from PCM
      for (uint8_t bit_depth = PCM_BIT_DEPTH_8; bit_depth <= PCM_BIT_DEPTH_32;
           bit_depth++) {
        for (uint8_t sample_rate = PCM_SAMPLE_RATE_8kHZ;
             sample_rate <= PCM_SAMPLE_RATE_384kHZ; sample_rate++) {
          // if given PCM bit depth and sample rate are supported, then add it
          // to our list of available modes
          if (PCM_IS_SUPPORTED(pcm_supported, bit_depth) &&
              PCM_IS_SUPPORTED(pcm_supported, sample_rate)) {
            // the params struct to be stored in our list of available modes
            struct nk_sound_dev_params params = {
                .type = widget_type == WIDGET_TYPE_AUDIO_INPUT
                            ? NK_SOUND_DEV_INPUT_STREAM
                            : NK_SOUND_DEV_OUTPUT_STREAM,
                .num_of_channels = num_of_channels,
                .sample_rate =
                    PCM_SAMPLE_RATES[sample_rate - PCM_SAMPLE_RATE_OFFSET],
                .sample_resolution =
                    PCM_BIT_DEPTHS[bit_depth - PCM_BIT_DEPTH_OFFSET],
                .scale = NK_SOUND_DEV_SCALE_LINEAR,
            };

            // create new mode struct on heap
            struct available_mode *mode;
            mode = malloc(sizeof(struct available_mode));

            if (!mode) {
              ERROR("Cannot allocate new available mode\n");
              continue;
            }

            // store the new mode in our list of available modes
            mode->params = params;
            list_add(&mode->node, &d->available_modes_list);

            DEBUG("Added new available mode: type %d channels %d sample_rate "
                  "%d sample_resolution %d scale %d\n",
                  mode->params.type, mode->params.num_of_channels,
                  mode->params.sample_rate, mode->params.sample_resolution,
                  mode->params.scale);
          }
        }
      }
    }
  }
  return 0;
}

static int scan_codecs(struct hda_pci_dev *d) {
  DEBUG("Scanning for all codecs\n");
  for (int i = 0; i < SDIMAX; i++) {
    if (d->codecs[i]) {
      if (scan_codec(d, i)) {
        ERROR("Codecs scan failed\n");
        return -1;
      }
    }
  }
  return 0;
}

static int bringup_device(struct hda_pci_dev *dev) {
  DEBUG("Bringing up device %u:%u.%u. Starting Address is: %x\n",
        dev->pci_dev->bus->num, dev->pci_dev->num, dev->pci_dev->fun,
        dev->mem_start);

  if (configure_msi_interrupts(dev)) {
    ERROR("Failed to configure MSI interrupts\n");
    return -1;
  }

  // make sure pci config space command register is acceptable
  uint16_t cmd = pci_dev_cfg_readw(dev->pci_dev, HDA_PCI_COMMAND_OFFSET);
  cmd &= ~0x0400; // turn off interrupt disable
  cmd |= 0x7;     // make sure bus master, memory, and io space are enabled
  DEBUG("Writing PCI command register to 0x%x\n", cmd);
  pci_dev_cfg_writew(dev->pci_dev, HDA_PCI_COMMAND_OFFSET, cmd);

  uint16_t status = pci_dev_cfg_readw(dev->pci_dev, HDA_PCI_STATUS_OFFSET);
  DEBUG("Reading PCI status register as 0x%x\n", status);

  DEBUG("Initiate HDA device reset\n");
  if (reset(dev)) {
    ERROR("HDA device reset failed\n");
    return -1;
  }
  DEBUG("HDA device reset completed\n");

  DEBUG("Initiate codec discovery\n");
  if (discover_codecs(dev)) {
    ERROR("Codec discovery failed\n");
    return -1;
  }
  DEBUG("Codec discovery completed\n");

  DEBUG("Initiate CORB setup\n");
  if (setup_corb(dev)) {
    ERROR("CORB setup failed\n");
    return -1;
  }
  DEBUG("CORB setup completed\n");

  DEBUG("Initate RIRB setup\n");
  if (setup_rirb(dev)) {
    ERROR("RIRB setup failed\n");
    return -1;
  }
  DEBUG("RIRB setup completed\n");

  DEBUG("Initiate interrupts setup\n");
  if (setup_interrupts(dev)) {
    ERROR("Interrupts setup failed\n");
    return -1;
  }
  DEBUG("Interrupts setup completed\n");

  DEBUG("Initiate codecs scan\n");
  if (scan_codecs(dev)) {
    ERROR("Codecs scan failed\n");
    return -1;
  }
  DEBUG("Codecs scan completed\n");

  // ========== TEST CODE, REMOVE AFTER ==========

  struct nk_sound_dev_params params;

  params.type = NK_SOUND_DEV_OUTPUT_STREAM;
  params.scale = NK_SOUND_DEV_SCALE_LINEAR;
  params.sample_rate = NK_SOUND_DEV_SAMPLE_RATE_44kHZ1;
  params.sample_resolution = NK_SOUND_DEV_SAMPLE_RESOLUTION_16;
  params.num_of_channels = 2;

  // should be able to open first 15 streams and not the last last 2
  for (int j = 0; j < 17; j++) {
    hda_open_stream(dev, &params);
  }

  struct nk_sound_dev_params available_modes[100];
  hda_get_avaiable_modes(dev, available_modes, 100);

  // =============================================

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

// ========== STREAM HELPER FUNCTIONS ==========
static void write_sd_control(struct hda_pci_dev *dev, sdnctl_t *sd_control,
                             uint8_t stream_offset) {
  uint16_t sdnctl_offset = SDNCTL + stream_offset * STREAM_OFFSET_CONST;
  hda_pci_write_regb(dev, sdnctl_offset, sd_control->byte_1);
  hda_pci_write_regb(dev, sdnctl_offset + 1, sd_control->byte_2);
  hda_pci_write_regb(dev, sdnctl_offset + 2, sd_control->byte_3);
}

static void read_sd_control(struct hda_pci_dev *dev, sdnctl_t *sd_control,
                            uint8_t stream_offset) {
  uint16_t sdnctl_offset = SDNCTL + stream_offset * STREAM_OFFSET_CONST;
  sd_control->byte_1 = hda_pci_read_regb(dev, sdnctl_offset);
  sd_control->byte_2 = hda_pci_read_regb(dev, sdnctl_offset + 1);
  sd_control->byte_3 = hda_pci_read_regb(dev, sdnctl_offset + 2);
}

static int check_valid_params(struct hda_pci_dev *dev,
                              struct nk_sound_dev_params *params) {
  struct list_head *curmode;
  struct available_mode *mode;
  list_for_each(curmode, &(dev->available_modes_list)) {
    mode = list_entry(curmode, struct available_mode, node);
    if (mode->params.type == params->type &&
        mode->params.num_of_channels == params->num_of_channels &&
        mode->params.sample_rate == params->sample_rate &&
        mode->params.sample_resolution == params->sample_resolution) {
      return 0;
    }
  }
  return -1;
}

static int reset_stream(struct hda_pci_dev *dev,
                        struct nk_sound_dev_stream *stream) {

  DEBUG("Resetting stream\n");

  sdnctl_t sd_control;

  // specification: section 3.3.35, page 45
  // clear the run bit and reset stream descriptor control by setting srst to 1
  DEBUG("Clearing RUN bit and resetting stream descriptor\n");
  read_sd_control(dev, &sd_control, stream->stream_id);
  sd_control.run = 0;
  sd_control.srst = 1;
  write_sd_control(dev, &sd_control, stream->stream_id);

  // ********** IMPORTANT NOTE **********
  // While testing, we found that the version of QEMU on Moore (version 2.5.0)
  // seems to have a bug with its implementation of the Intel HDA device.
  //
  // More specifically, the srst big in the SDnCTL register does not ever get
  // set to 1. This is not the expected behavior; after writing "1" to the srst
  // bit, we expect to see the srst bit get updated to 1 after the device has
  // finished restting. The software then has to clear this bit to take the
  // device out of reset.
  //
  // Therefore, we have two implementations. Use the first one if you are
  // running on newer versions of QEMU, which has the correct Intel HDA
  // behavior. If running this on Moore, use the latter approach. The version
  // of QEMU we tested the CORRECT behavior with is version 7.2.1.

  DEBUG("Putting stream into reset state\n");

  // =================================================================
  // Uncomment the three lines below if using newer QEMU versions

  do {
    read_sd_control(dev, &sd_control, stream->stream_id);
  } while (sd_control.srst != 1);

  // =================================================================
  // Uncomment the three lines below if using QEMU 2.5.0

  // do {
  //   read_sd_control(dev, &sd_control);
  // } while (sd_control.srst == 1);

  // =================================================================

  // move stream descriptor out of the reset state by setting srst to 0
  read_sd_control(dev, &sd_control, stream->stream_id);
  sd_control.srst = 0;
  write_sd_control(dev, &sd_control, stream->stream_id);

  DEBUG("Taking stream out of reset state\n");

  // software must read a 0 from srst before accessing any of the stream
  // registers
  do {
    read_sd_control(dev, &sd_control, stream->stream_id);
  } while (sd_control.srst != 0);

  DEBUG("Completed reseting stream\n");
  return 0;
}

static int configure_stream(struct hda_pci_dev *dev,
                            struct nk_sound_dev_stream *stream,
                            struct nk_sound_dev_params *params) {
  DEBUG("Configure stream descriptor with given parameters\n");

  sdnfmt_t sd_format;
  uint16_t sdnfmt_offset = SDNFMT + stream->stream_id * STREAM_OFFSET_CONST;

  sd_format.val = hda_pci_read_regw(dev, sdnfmt_offset);
  sd_format.chan = params->num_of_channels - 1;
  sd_format.bits = params->sample_resolution;
  sd_format.div = HDA_SAMPLE_RATES[params->sample_rate][2];
  sd_format.mult = HDA_SAMPLE_RATES[params->sample_rate][1];
  sd_format.base = HDA_SAMPLE_RATES[params->sample_rate][0];
  hda_pci_write_regw(dev, sdnfmt_offset, sd_format.val);

  sd_format.val = hda_pci_read_regw(dev, sdnfmt_offset);
  DEBUG("Stream Descriptor Format: 0x%x\n", sd_format.val);
  DEBUG("Completed configuring stream descriptor\n");
  return 0;
}

static int initialize_bdl(struct hda_pci_dev *dev,
                          struct nk_sound_dev_stream *stream, bdl_t *bdl) {
  DEBUG("Allocating space for BDL\n");
  bdl = malloc(sizeof(bdl_t));
  if (!bdl) {
    ERROR("Cannot allocate space for BDL\n");
    return -1;
  }
  DEBUG("Allocated %d bytes for BDL\n", sizeof(bdl));
  DEBUG("BDL address: 0x%016lx\n", bdl);

  memset(bdl, 0, sizeof(*bdl));

  uint32_t bdl_u = (uint32_t)(((uint64_t)bdl) >> 32);
  // Note: the previous group use 0xFFFFC0, but from specification the first
  // 7 (6:0) bits are reserved. Shouldn't we use 0xFFFFFF80 instead?
  uint32_t bdl_l = ((uint32_t)(((uint64_t)bdl))) & 0xFFFFFF80;
  uint16_t bdl_upper = SDNBDPU + stream->stream_id * STREAM_OFFSET_CONST;
  uint16_t bdl_lower = SDNBDPL + stream->stream_id * STREAM_OFFSET_CONST;
  hda_pci_write_regl(dev, bdl_upper, bdl_u);
  hda_pci_write_regl(dev, bdl_lower, bdl_l);

  DEBUG("Wrote 0x%08x to BDL upper address\n",
        hda_pci_read_regl(dev, bdl_upper));
  DEBUG("Wrote 0x%08x to BDL lower address\n",
        hda_pci_read_regl(dev, bdl_lower));
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
