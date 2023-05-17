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
#include <math.h>
#include <nautilus/cpu.h>
#include <nautilus/idt.h>
#include <nautilus/irq.h>
#include <nautilus/list.h>
#include <nautilus/mm.h>
#include <nautilus/nautilus.h>
#include <nautilus/printk.h>
#include <nautilus/shell.h>
#include <nautilus/sounddev.h>
#include <nautilus/spinlock.h>

#include "hda_pci_internal.h"

// ========== FUNCTIONS TO IMPLEMENT ==========

int hda_close_stream(void *state, struct nk_sound_dev_stream *stream) {
  return -1;
}

int hda_read_from_stream(void *state, struct nk_sound_dev_stream *stream,
                         uint8_t *dst, uint64_t len,
                         void (*callback)(nk_sound_dev_status_t status,
                                          void *context),
                         void *context) {
  return -1;
}

// ========== FORWARD DECLRATION FOR INTERFACE HELPERS ==========
static inline uint8_t hda_pci_read_regb(struct hda_pci_dev *dev,
                                        uint32_t offset);

static inline void hda_pci_write_regl(struct hda_pci_dev *dev, uint32_t offset,
                                      uint32_t data);

static void write_sd_control(struct hda_pci_dev *dev, sdnctl_t *sd_control,
                             uint8_t stream_offset);

static void read_sd_control(struct hda_pci_dev *dev, sdnctl_t *sd_control,
                            uint8_t stream_offset);

static int check_valid_params(struct hda_pci_dev *dev,
                              struct nk_sound_dev_params *params);

static int create_new_stream(struct hda_pci_dev *dev,
                             struct nk_sound_dev_params *params);

static int reset_stream(struct hda_pci_dev *dev, uint8_t stream_id);

static int configure_stream(struct hda_pci_dev *dev, uint8_t stream_id);

static int initialize_bdl(struct hda_pci_dev *dev, uint8_t stream_id,
                          uint8_t *src, uint64_t len);

static int set_output_stream(struct hda_pci_dev *dev, uint8_t stream_id);

static uint64_t get_chunk_size(uint64_t current_offset, uint64_t total_size);

static void create_sine_wave(uint8_t *buffer, uint64_t buffer_len,
                             uint64_t tone_frequency,
                             uint64_t sampling_frequency);

static struct audio_data create_empty_audio(struct nk_sound_dev_params *params);

// ========== READ AND WRITE REGISTERS ==========

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

// ========== INTERFACE ==========

struct nk_sound_dev_stream *
hda_open_stream(void *state, struct nk_sound_dev_params *params) {
  DEBUG("Opening new data stream\n");

  if (!state) {
    ERROR("The device state pointer is null\n");
    return NULL;
  }

  if (!params) {
    ERROR("The device parameters pointer is null\n");
    return NULL;
  }

  struct hda_pci_dev *dev = (struct hda_pci_dev *)state;

  DEBUG("INITIATE: check_valid_params()\n");
  if (check_valid_params(dev, params)) {
    DEBUG("FAILED: check_valid_params()\n");
    return NULL;
  }
  DEBUG("COMPLETED: check_valid_params()\n");

  DEBUG("INITIATE: create_new_stream()\n");
  int new_stream_id = create_new_stream(dev, params);
  if (new_stream_id == -1) {
    DEBUG("FAILED: create_new_stream()\n");
    return NULL;
  }
  uint8_t stream_id = (uint8_t)new_stream_id;
  DEBUG("COMPLETED: create_new_stream()\n");

  DEBUG("INITIATE: reset_stream()\n");
  if (reset_stream(dev, stream_id)) {
    DEBUG("FAILED: reset_stream()\n");
    return NULL;
  }
  DEBUG("COMPLETED: reset_stream()\n");

  DEBUG("INITIATE: configure_stream()\n");
  if (configure_stream(dev, stream_id)) {
    ERROR("FAILED: configure_stream()\n");
    return NULL;
  }
  DEBUG("COMPLETED: configure_stream()\n");

  DEBUG("INITIATE: initialize_bdl()\n");
  struct audio_data audio = create_empty_audio(params);
  if (initialize_bdl(dev, stream_id, audio.buffer, audio.size)) {
    ERROR("FAILED: initialize_bdl()\n");
    return NULL;
  }
  DEBUG("COMPLETED: initialize_bdl()\n");

  // program the BDL upper and lower address
  bdl_t *bdl = dev->streams[stream_id]->bdls[0];
  uint32_t bdl_u = (uint32_t)(((uint64_t)bdl) >> 32);
  uint32_t bdl_l = ((uint32_t)(((uint64_t)bdl))) & 0xFFFFFF80;
  uint16_t bdl_upper = SDNBDPU + stream_id * STREAM_OFFSET_CONST;
  uint16_t bdl_lower = SDNBDPL + stream_id * STREAM_OFFSET_CONST;
  hda_pci_write_regl(dev, bdl_upper, bdl_u);
  hda_pci_write_regl(dev, bdl_lower, bdl_l);
  DEBUG("Wrote 0x%08x to BDL upper address\n",
        hda_pci_read_regl(dev, bdl_upper));
  DEBUG("Wrote 0x%08x to BDL lower address\n",
        hda_pci_read_regl(dev, bdl_lower));

  // program the stream LVI (last valid index) of the BDL
  sdnlvi_t lvi;
  uint16_t lvi_offset = SDNLVI + stream_id * STREAM_OFFSET_CONST;
  lvi.val = hda_pci_read_regw(dev, lvi_offset);
  lvi.lvi = dev->streams[stream_id]->bdls_lvi[0];
  hda_pci_write_regw(dev, lvi_offset, lvi.val);
  lvi.val = hda_pci_read_regw(dev, lvi_offset);
  DEBUG("Stream %d's LVI: %d, %d\n", stream_id, lvi.val,
        dev->streams[stream_id]->bdls_lvi[0]);

  // program the length of samples in cyclic buffer
  uint32_t sdncbl_offset = SDNCBL + stream_id * STREAM_OFFSET_CONST;
  hda_pci_write_regl(dev, sdncbl_offset, (uint32_t)audio.size);
  DEBUG("Stream %d's CBL: %d\n", stream_id, audio.size);

  DEBUG("Done opening a new stream %d\n", stream_id);
  return &dev->streams[stream_id]->stream;
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

int hda_write_to_stream(void *state, struct nk_sound_dev_stream *stream,
                        uint8_t *src, uint64_t len,
                        void (*callback)(nk_sound_dev_status_t status,
                                         void *context),
                        void *context) {
  DEBUG("Writing to stream\n");

  if (!state) {
    ERROR("The device state pointer is null\n");
    return -1;
  }

  if (!stream) {
    ERROR("The stream pointer is null\n");
    return -1;
  }

  struct hda_pci_dev *dev = (struct hda_pci_dev *)state;
  uint8_t stream_id = stream->stream_id;

  if (stream_id >= HDA_MAX_NUM_OF_STREAMS || !dev->streams[stream_id]) {
    ERROR("stream id %d is an invalid stream\n");
    return -1;
  }

  // initialize BDL
  DEBUG("Initiate BDL initialization\n");
  if (initialize_bdl(dev, stream_id, src, len)) {
    ERROR("BDL initialization failed\n");
    return -1;
  }
  DEBUG("Completed BDL initialization\n");

  return 0;
}

int hda_get_stream_params(void *state, struct nk_sound_dev_stream *stream,
                          struct nk_sound_dev_params *p) {
  DEBUG("Getting stream parameters from stream\n");

  if (!state) {
    ERROR("The device state pointer is null\n");
    return -1;
  }

  if (!p) {
    ERROR("The device parameters pointer is null\n");
    return -1;
  }

  struct hda_pci_dev *dev = (struct hda_pci_dev *)state;

  struct hda_stream_info *hda_stream = dev->streams[stream->stream_id];
  *p = hda_stream->stream.params;

  return 0;
}

int hda_play_stream(void *state, struct nk_sound_dev_stream *stream) {
  DEBUG("Playing from stream\n");

  if (!state) {
    ERROR("The device state pointer is null\n");
    return -1;
  }

  if (!stream) {
    ERROR("The stream pointer is null\n");
    return -1;
  }

  struct hda_pci_dev *dev = (struct hda_pci_dev *)state;
  uint8_t stream_id = stream->stream_id;

  sdnctl_t sd_control;

  // if other stream is currently running, stop it
  if (dev->current_stream != 255) {
    DEBUG("Stopping stream %d\n", dev->current_stream);
    read_sd_control(dev, &sd_control, dev->current_stream);
    sd_control.run = 0;
    write_sd_control(dev, &sd_control, dev->current_stream);
  }

  // if stream is already running, do nothing
  read_sd_control(dev, &sd_control, stream_id);
  if (sd_control.run) {
    DEBUG("Stream %d is already running\n", stream_id);
    return 0;
  }

  // connect output converter (ADC) with the stream's tag
  set_output_stream(dev, stream_id);

  // set interrupt and start running stream
  read_sd_control(dev, &sd_control, stream_id);
  sd_control.stripe = 0;
  sd_control.ioce = 1;
  sd_control.run = 1;
  write_sd_control(dev, &sd_control, stream_id);
  DEBUG("Start running stream %d\n", stream_id);

  // update stream
  dev->current_stream = stream_id;

  return 0;
}

static int handler(excp_entry_t *e, excp_vec_t v, void *priv_data) {
  DEBUG("**** INSIDE HANDLER ****\n");

  struct hda_pci_dev *dev = (struct hda_pci_dev *)priv_data;

  uint8_t cur_stream_id = dev->current_stream;
  struct hda_stream_info *hda_stream = dev->streams[cur_stream_id];

  if (!hda_stream) {
    ERROR("INVALID: stream id %d\n", cur_stream_id);
    return -1;
  }

  sdnsts_t stream_status;
  stream_status.val =
      hda_pci_read_regb(dev, SDNSTS + cur_stream_id * STREAM_OFFSET_CONST);

  // check if interupt on completion (IOC) is triggered and the current stream
  // is an output stream
  if (hda_stream->stream.params.type == NK_SOUND_DEV_OUTPUT_STREAM &&
      stream_status.bcis) {
    stream_status.bcis = 1;
    hda_pci_write_regb(dev, SDNSTS + cur_stream_id * STREAM_OFFSET_CONST,
                       stream_status.val);
    stream_status.val =
        hda_pci_read_regb(dev, SDNSTS + cur_stream_id * STREAM_OFFSET_CONST);
    DEBUG("Reset BDCIS bit for stream %d status: %d\n", cur_stream_id,
          stream_status.bcis);

    uint8_t cur_index = hda_stream->bdls_start_index;
    DEBUG("IOC triggered: Buffer %d is completed\n", cur_index);

    sdnctl_t sd_control;
    DEBUG("Stop running stream %d\n", cur_stream_id);
    read_sd_control(dev, &sd_control, cur_stream_id);
    sd_control.run = 0;
    write_sd_control(dev, &sd_control, cur_stream_id);

    // free current BDL and the memory that store the audio data
    bdl_t *cur_bdl = hda_stream->bdls[cur_index];
    void *audio_ptr = (void *)(cur_bdl->buf[0].address);
    DEBUG("Freed audio data at 0x%016lx\n", audio_ptr);
    free(audio_ptr);
    DEBUG("Freed BDL %d at 0x%016lx\n", cur_index, cur_bdl);
    free(cur_bdl);

    // change freed pointer to NULL
    hda_stream->bdls[cur_index] = NULL;

    // check if current BDL is the last one in the ring buffer; if there
    // it is, create a new empty stream and insert it into the ring buffer
    if (hda_stream->bdls_length == 1) {
      DEBUG("Current BDL is the last one in the ring buffer, create a new "
            "empty BDL\n");
      struct audio_data audio = create_empty_audio(&hda_stream->stream.params);
      if (initialize_bdl(dev, cur_stream_id, audio.buffer, audio.size)) {
        ERROR("Failed to create new empty BDL\n");
        return -1;
      }
      DEBUG("Wrote new empty BDL to the ring buffer\n");
    }

    DEBUG("OLD BDLs Ring Buffer: length %d, index %d\n",
          hda_stream->bdls_length, hda_stream->bdls_start_index);
    hda_stream->bdls_length -= 1;
    hda_stream->bdls_start_index =
        (hda_stream->bdls_start_index + 1) % HDA_MAX_NUM_OF_BDLS;
    DEBUG("NEW BDLs Ring Buffer: length %d, index %d\n",
          hda_stream->bdls_length, hda_stream->bdls_start_index);

    // get the new (next) BDL in the ring buffer
    uint8_t new_index = hda_stream->bdls_start_index;
    bdl_t *new_bdl = hda_stream->bdls[new_index];

    DEBUG("New BDL address: 0x%016lx\n", new_bdl);

    // set current stream descriptor's BDL upper and lower address bit with
    // a new BDL address
    uint32_t bdl_u = (uint32_t)(((uint64_t)new_bdl) >> 32);
    uint32_t bdl_l = ((uint32_t)(((uint64_t)new_bdl))) & 0xFFFFFF80;
    uint16_t bdl_upper = SDNBDPU + cur_stream_id * STREAM_OFFSET_CONST;
    uint16_t bdl_lower = SDNBDPL + cur_stream_id * STREAM_OFFSET_CONST;
    hda_pci_write_regl(dev, bdl_upper, bdl_u);
    hda_pci_write_regl(dev, bdl_lower, bdl_l);
    DEBUG("Wrote 0x%08x to BDL upper address\n",
          hda_pci_read_regl(dev, bdl_upper));
    DEBUG("Wrote 0x%08x to BDL lower address\n",
          hda_pci_read_regl(dev, bdl_lower));

    // program the stream LVI (last valid index) of the new BDL
    sdnlvi_t lvi;
    uint16_t lvi_offset = SDNLVI + cur_stream_id * STREAM_OFFSET_CONST;
    lvi.val = hda_pci_read_regw(dev, lvi_offset);
    lvi.lvi = dev->streams[cur_stream_id]->bdls_lvi[new_index];
    hda_pci_write_regw(dev, lvi_offset, lvi.val);
    lvi.val = hda_pci_read_regw(dev, lvi_offset);
    DEBUG("Stream %d's new LVI: %d, %d\n", cur_stream_id, lvi.val,
          dev->streams[cur_stream_id]->bdls_lvi[new_index]);

    // program the length of samples in the new cyclic buffer
    uint32_t sdncbl_offset = SDNCBL + cur_stream_id * STREAM_OFFSET_CONST;
    hda_pci_write_regl(dev, sdncbl_offset,
                       dev->streams[cur_stream_id]->bdls_size[new_index]);
    DEBUG("Stream %d's new CBL: %d\n", cur_stream_id,
          dev->streams[cur_stream_id]->bdls_size[new_index]);

    // start stream
    DEBUG("Start running stream %d\n", cur_stream_id);
    read_sd_control(dev, &sd_control, cur_stream_id);
    // ===== TRIED SETTING STRIPE & IOCE AGAIN ========
    // sd_control.stripe = 0;
    // sd_control.ioce = 1;
    // ================================================
    sd_control.run = 1;
    write_sd_control(dev, &sd_control, cur_stream_id);
  }

  IRQ_HANDLER_END();
  return 0;
}

// ========== GLOBAL FIELDS ==========

// used for playing handlers from command line
struct hda_pci_dev *hda_dev;

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
        // 255 (or -1) to indicate that no stream is currently running
        hdev->current_stream = 255;

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

static int get_global_caps(struct hda_pci_dev *d) {
  gcap_t gcap;
  gcap.val = hda_pci_read_regw(d, GCAP);

  if (gcap.oss == 0) {
    ERROR("HDA device does not contain output streams\n");
    return -1;
  }

  if (gcap.iss == 0) {
    ERROR("HDA device does not contain input streams\n");
    return -1;
  }

  d->input_stream_start = 0;
  d->input_stream_end = gcap.iss - 1;
  d->output_stream_start = gcap.iss;
  d->output_stream_end = gcap.iss + gcap.oss - 1;

  return 0;
}

static int reset(struct hda_pci_dev *d) {
  gctl_t gctl;

  // CRST bit (offset 0x8, bit 0) should already be 0, but set it again here
  // to be sure
  gctl.val = hda_pci_read_regl(d, GCTL);
  gctl.crst = 0;
  hda_pci_write_regl(d, GCTL, gctl.val);

  DEBUG("Resetting HDA device, gctl = %08x\n", gctl.val);

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

  DEBUG("HDA device has been reset, gctl = %08x\n", gctl.val);
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
    ERROR("Did not find any codecs from SDI, statests = %x\n", statests.val);
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
  DEBUG("Turning off CORB DMA engine\n");

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

  DEBUG("Turned on CORB DMA engine\n");

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
  DEBUG("Turned off RIRB DMA engine\n");

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

  DEBUG("Turned on RIRB DMA engine\n");

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
    if (widget_type != NODE_TYPE_AUDIO_FUNCTION_GROUP) {
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

      // if widgets are OUTPUT, INPUT, or PIN, store pin numbers; otherwise skip
      switch (widget_type) {
      case WIDGET_TYPE_AUDIO_OUTPUT:
        d->audio_output_node_id = curr_widget;
        break;

      case WIDGET_TYPE_AUDIO_INPUT:
        d->audio_input_node_id = curr_widget;
        // find the pin complex to the input converter (ADC)
        transact(d, codec, curr_widget, 0, MAKE_VERB_8(GET_CONLIST, 0), &rp);
        if (rp.resp & 0xff) {
          d->input_pin_node_id = rp.resp & 0xff;
        }
        break;

      case WIDGET_TYPE_PIN_COMPLEX:
        // see if this pin complex is the one connected to ouput converter (DAC)
        transact(d, codec, curr_widget, 0, MAKE_VERB_8(GET_CONLIST, 0), &rp);
        if (rp.resp & 0xff) {
          d->output_pin_node_id = curr_widget;
        }
        continue;

      default:
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
        return -1;
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

            DEBUG("      Added new available mode: type %d channels %d "
                  "sample_rate %d sample_resolution %d scale %d\n",
                  mode->params.type, mode->params.num_of_channels,
                  mode->params.sample_rate, mode->params.sample_resolution,
                  mode->params.scale);
          }
        }
      }
    }
  }

  if (!d->audio_output_node_id || !d->audio_output_node_id) {
    ERROR("Could not find audio input/output widget\n");
    return -1;
  }

  DEBUG("Widget IDs:\n");
  DEBUG("  output converter: %d\n", d->audio_output_node_id);
  DEBUG("  output pin: %d\n", d->output_pin_node_id);
  DEBUG("  input converter: %d\n", d->audio_input_node_id);
  DEBUG("  input pin: %d\n", d->input_pin_node_id);
  return 0;
}

static int scan_codecs(struct hda_pci_dev *d) {
  DEBUG("Scanning for codecs\n");
  for (int i = 0; i < SDIMAX; i++) {
    if (d->codecs[i]) {
      if (scan_codec(d, i)) {
        ERROR("Codecs scan failed\n");
        return -1;
      }

      // FOR QEMU, there is only one codec. This should be the first one (at
      // index 0). We only use the first codec we find.
      d->codec_id = i;
      break;
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

  DEBUG("INITIATE: get_global_caps()\n");
  if (get_global_caps(dev)) {
    ERROR("FAILED: get_gloal_caps()\n");
    return -1;
  }
  DEBUG("COMPLETED: get_gloal_caps() completed\n");

  DEBUG("INITIATE: reset()\n");
  if (reset(dev)) {
    ERROR("FAILED: reset()\n");
    return -1;
  }
  DEBUG("COMPLETED: reset()\n");

  DEBUG("INITIATE: discover_codecs()\n");
  if (discover_codecs(dev)) {
    ERROR("FAILED: discover_codecs()\n");
    return -1;
  }
  DEBUG("COMPLETED: discover_codecs()\n");

  DEBUG("INITIATE: setup_corb()\n");
  if (setup_corb(dev)) {
    ERROR("FAILED: setup_corb()\n");
    return -1;
  }
  DEBUG("COMPLETED: setup_corb()\n");

  DEBUG("INITIATE: setup_rirb()\n");
  if (setup_rirb(dev)) {
    ERROR("FAILED: setup_rirb()\n");
    return -1;
  }
  DEBUG("COMPLETED: setup_rirb()\n");

  DEBUG("INITIATE: setup_interrupts()\n");
  if (setup_interrupts(dev)) {
    ERROR("FAILED: setup_interrupts()");
    return -1;
  }
  DEBUG("COMPLETED: setup_interrupts()\n");

  DEBUG("INITIATE: scan_codecs()\n");
  if (scan_codecs(dev)) {
    ERROR("FAILED: scan_codecs()\n");
    return -1;
  }
  DEBUG("COMPLETED: scan_codecs()\n");

  // ========== TEST CODE, REMOVE AFTER ==========

  /*
  struct nk_sound_dev_params params;

  params.type = NK_SOUND_DEV_OUTPUT_STREAM;
  params.scale = NK_SOUND_DEV_SCALE_LINEAR;
  params.sample_rate = NK_SOUND_DEV_SAMPLE_RATE_48kHZ;
  params.sample_resolution = NK_SOUND_DEV_SAMPLE_RESOLUTION_16;
  params.num_of_channels = 2;

  for (int j = 0; j < 2; j++) {
    struct nk_sound_dev_stream *stream = hda_open_stream(dev, &params);

    if (!stream) {
      ERROR("error lol\n");
      return -1;
    }

    uint64_t sampling_frequency = 48000;
    uint32_t duration = 2;
    uint64_t buf_len = sampling_frequency * duration * 4;
    uint64_t tone_frequency = (j + 1) * (j + 1) * 200;
    uint8_t *buf;
    // should see 2 errors since length of the buffer is 10
    for (int k = 1; k < 3; k++) {
      buf = (uint8_t *)malloc(buf_len);
      uint64_t cur_tone_frequency = tone_frequency * k;
      DEBUG("Create sin wave at: 0x%016lx\n", buf);
      create_sine_wave(buf, buf_len, cur_tone_frequency, sampling_frequency);
      hda_write_to_stream(dev, stream, buf, buf_len, 0, 0);
    }

    hda_play_stream(dev, stream);
  }
  */

  // =============================================

  // used for shell handlers
  hda_dev = dev;

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

static int reset_stream(struct hda_pci_dev *dev, uint8_t stream_id) {

  DEBUG("Resetting stream %d\n", stream_id);

  sdnctl_t sd_control;

  // specification: section 3.3.35, page 45
  // clear the run bit and reset stream descriptor control by setting srst to 1
  DEBUG("Clearing RUN bit and resetting stream descriptor for stream %d\n",
        stream_id);
  read_sd_control(dev, &sd_control, stream_id);
  sd_control.run = 0;
  sd_control.srst = 1;
  write_sd_control(dev, &sd_control, stream_id);

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

  DEBUG("Putting stream %d into reset state\n", stream_id);

  // =================================================================
  // Uncomment the three lines below if using newer QEMU versions

  do {
    read_sd_control(dev, &sd_control, stream_id);
  } while (sd_control.srst != 1);

  // =================================================================
  // Uncomment the three lines below if using QEMU 2.5.0

  // do {
  //   read_sd_control(dev, &sd_control);
  // } while (sd_control.srst == 1);

  // =================================================================

  // move stream descriptor out of the reset state by setting srst to 0
  read_sd_control(dev, &sd_control, stream_id);
  sd_control.srst = 0;
  write_sd_control(dev, &sd_control, stream_id);

  DEBUG("Taking stream %d out of reset state\n", stream_id);

  // software must read a 0 from srst before accessing any of the stream
  // registers
  do {
    read_sd_control(dev, &sd_control, stream_id);
  } while (sd_control.srst != 0);

  // clear interrupt enable bits for now; will enable them when running stream

  // set stream tag
  sd_control.strm_num = dev->streams[stream_id]->stream_tag;
  write_sd_control(dev, &sd_control, stream_id);

  DEBUG("Completed reseting stream %d\n", stream_id);
  return 0;
}

static int configure_stream(struct hda_pci_dev *dev, uint8_t stream_id) {
  DEBUG("Configure stream descriptor with given parameters\n");

  sdnfmt_t sd_format;
  uint16_t sdnfmt_offset = SDNFMT + stream_id * STREAM_OFFSET_CONST;

  struct nk_sound_dev_params params = dev->streams[stream_id]->stream.params;
  sd_format.val = hda_pci_read_regw(dev, sdnfmt_offset);
  sd_format.chan = params.num_of_channels - 1;
  sd_format.bits = params.sample_resolution;
  sd_format.div = HDA_SAMPLE_RATES[params.sample_rate][2];
  sd_format.mult = HDA_SAMPLE_RATES[params.sample_rate][1];
  sd_format.base = HDA_SAMPLE_RATES[params.sample_rate][0];
  hda_pci_write_regw(dev, sdnfmt_offset, sd_format.val);

  sd_format.val = hda_pci_read_regw(dev, sdnfmt_offset);
  DEBUG("Stream Descriptor Format: 0x%x\n", sd_format.val);
  DEBUG("Completed configuring stream descriptor\n");
  return 0;
}

static int initialize_bdl(struct hda_pci_dev *dev, uint8_t stream_id,
                          uint8_t *src, uint64_t len) {

  struct hda_stream_info *hda_stream = dev->streams[stream_id];

  // if given stream ID is invalid, print error and return
  if (!hda_stream) {
    ERROR("%d is an invalid stream id\n", stream_id);
    return -1;
  }

  // if there is no room in the ring buffer to hold BDL, print error and return
  if (hda_stream->bdls_length == HDA_MAX_NUM_OF_BDLS) {
    ERROR("BDL ring buffer is full\n");
    return -1;
  }

  // create new BDL struct to hold the BDL entries (BDLE)
  bdl_t *bdl;
  bdl = malloc(sizeof(bdl_t));

  // if we cannot allocate space for new BDL, print error and return
  if (!bdl) {
    ERROR("Cannot allocate BDL\n");
    return -1;
  }
  DEBUG("BDL address: 0x%016lx\n", bdl);

  // set all bytes in BDL to 0
  memset(bdl, 0, sizeof(*bdl));

  // write BDL to next available slot in the ring buffer
  uint8_t cur_index = (hda_stream->bdls_start_index + hda_stream->bdls_length) %
                      HDA_MAX_NUM_OF_BDLS;
  hda_stream->bdls[cur_index] = bdl;
  hda_stream->bdls_length += 1;
  DEBUG("Storing BDL to slot %d of the ring buffer\n", cur_index);

  // write source data to newly created BDL
  uint16_t index = 0;
  uint64_t curr_offset = 0;
  uint64_t chunksize = 0;
  while (curr_offset < len) {
    chunksize = get_chunk_size(curr_offset, len);

    bdl->buf[index].reserved = 0;
    bdl->buf[index].address = (((uint64_t)src) + curr_offset);
    bdl->buf[index].length = chunksize;
    bdl->buf[index].ioc = 0;

    DEBUG("BDLE %d address: 0x%016lx\n", index, &(bdl->buf[index]));
    DEBUG("Audio chunck address: 0x%016lx\n", bdl->buf[index].address);

    index++;
    curr_offset += chunksize;
  }

  // update IOC of the last BDLE to 1 to trigger interrupt on completion
  bdl->buf[index - 1].ioc = 1;

  // update size and LVI for this BDL
  hda_stream->bdls_size[cur_index] = len;
  hda_stream->bdls_lvi[cur_index] = index - 1;

  return 0;
}

static int create_new_stream(struct hda_pci_dev *dev,
                             struct nk_sound_dev_params *params) {
  uint8_t stream_id_start, stream_id_end;
  struct hda_stream_info **stream_tags;
  if (params->type == NK_SOUND_DEV_INPUT_STREAM) {
    stream_id_start = dev->input_stream_start;
    stream_id_end = dev->input_stream_end;
    stream_tags = dev->input_stream_tags;
  } else {
    stream_id_start = dev->output_stream_start;
    stream_id_end = dev->output_stream_end;
    stream_tags = dev->output_stream_tags;
  }

  uint8_t stream_id;
  uint8_t stream_tag;

  // search for next available stream ID
  int found_stream_id = 0;
  for (uint8_t i = stream_id_start; i <= stream_id_end; i++) {
    if (dev->streams[i]) {
      continue;
    }
    found_stream_id = 1;
    stream_id = i;
    break;
  }

  if (!found_stream_id) {
    ERROR("No stream ID available\n");
    return -1;
  }

  // search for next available stream tag
  int found_stream_tag = 0;
  for (uint8_t i = 1; i < HDA_MAX_NUM_OF_STREAM_TAGS; i++) {
    if (stream_tags[i]) {
      continue;
    }
    found_stream_tag = 1;
    stream_tag = i;
    break;
  }

  if (!found_stream_tag) {
    ERROR("No stream tag available\n");
    return -1;
  }

  // found available stream tag and id, we can now open a new stream
  struct hda_stream_info *stream = malloc(sizeof(struct hda_stream_info));

  if (!stream) {
    ERROR("Cannot allocate new stream\n");
    return -1;
  }

  stream->stream.stream_id = stream_id;
  stream->stream.params = *params;
  stream->stream_id = stream_id;
  stream->stream_tag = stream_tag;
  dev->streams[stream_id] = stream;
  stream_tags[stream_tag] = stream;

  DEBUG("Created new stream %d tag %d\n", stream->stream_id,
        stream->stream_tag);

  return stream_id;
}

static int set_output_stream(struct hda_pci_dev *dev, uint8_t stream_id) {
  DEBUG("Setting up audio output widget\n");

  codec_resp_t rp;

  outwgctl_t output_wg_ctl;
  transact(dev, dev->codec_id, dev->audio_output_node_id, 0,
           MAKE_VERB_8(GET_CONVCTL, 0), &rp);
  output_wg_ctl.val = (uint8_t)rp.resp;
  output_wg_ctl.stream = dev->streams[stream_id]->stream_tag;
  output_wg_ctl.channel = 0;

  transact(dev, dev->codec_id, dev->audio_output_node_id, 0,
           MAKE_VERB_8(SET_CONVCTL, output_wg_ctl.val), &rp);

  transact(dev, dev->codec_id, dev->audio_output_node_id, 0,
           MAKE_VERB_8(GET_CONVCTL, 0), &rp);
  DEBUG(
      "Node %d converter stream channel: codec %d, stream tag %d, channel %d\n",
      dev->audio_output_node_id, dev->codec_id, output_wg_ctl.stream,
      output_wg_ctl.channel);

  return 0;
}

static uint64_t get_chunk_size(uint64_t current_offset, uint64_t total_size) {
  const uint64_t max_length = 0xFFFFFFFF; // Length is a 32-bit quantity
  if (current_offset == 0) {
    return (total_size > max_length)
               ? max_length
               : total_size / 2; // Need at least two entries in the BDL
  } else {
    uint64_t remaining_size = total_size - current_offset;
    return (remaining_size > max_length) ? max_length : remaining_size;
  }
}

static struct audio_data
create_empty_audio(struct nk_sound_dev_params *params) {
  struct audio_data audio;

  // sample rate/frequency
  uint64_t sampling_frequency;
  switch (params->sample_rate) {
  case NK_SOUND_DEV_SAMPLE_RATE_8kHZ:
    sampling_frequency = 8000;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_11kHZ025:
    sampling_frequency = 11025;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_16kHZ:
    sampling_frequency = 16000;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_22kHZ05:
    sampling_frequency = 22050;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_32kHZ:
    sampling_frequency = 32000;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_44kHZ1:
    sampling_frequency = 44100;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_48kHZ:
    sampling_frequency = 48000;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_88kHZ2:
    sampling_frequency = 88200;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_96kHZ:
    sampling_frequency = 96000;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_176kHZ4:
    sampling_frequency = 176400;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_192kHZ:
    sampling_frequency = 192000;
    break;
  case NK_SOUND_DEV_SAMPLE_RATE_384kHZ:
    sampling_frequency = 384000;
    break;
  default:
    ERROR("Invalid sample rate\n");
    return audio;
  }

  // sample resolution
  uint64_t sample_size;
  switch (params->sample_resolution) {
  case NK_SOUND_DEV_SAMPLE_RESOLUTION_8:
    sample_size = 1;
    break;
  case NK_SOUND_DEV_SAMPLE_RESOLUTION_16:
    sample_size = 2;
    break;
  case NK_SOUND_DEV_SAMPLE_RESOLUTION_20:
  case NK_SOUND_DEV_SAMPLE_RESOLUTION_24:
  case NK_SOUND_DEV_SAMPLE_RESOLUTION_32:
    sample_size = 4;
    break;
  default:
    ERROR("Invalid sample resolution\n");
    return audio;
  }

  // buffer - set to duration of 0.5 seconds
  audio.size =
      (uint64_t)params->num_of_channels * sample_size * sampling_frequency / 2;
  audio.buffer = (uint8_t *)malloc(audio.size);
  memset(audio.buffer, 0, sizeof(audio.size));

  return audio;
}

// ========== COMMAND LINE FUNCTIONS FOR TESTING ==========

// Assumes 8-bit audio with 2 channels
static void create_sine_wave(uint8_t *buffer, uint64_t buffer_len,
                             uint64_t tone_frequency,
                             uint64_t sampling_frequency) {
  for (int i = 0, j = 0; i < buffer_len; i += 4, j++) {
    double x = (double)j * 2.0 * M_PI * (double)tone_frequency /
               (double)sampling_frequency;
    double sin_val = sin(x);

    buffer[i] = 0;
    buffer[i + 1] = (uint8_t)(sin_val * 127.0);
    buffer[i + 2] = 0;
    buffer[i + 3] = (uint8_t)(sin_val * 127.0);
  }
}

static int handle_write_to_stream(char *buf, void *priv) {
  uint64_t stream_id, frequency, duration;
  sscanf(buf, "write-stream %d %d %d", &stream_id, &frequency, &duration);

  uint64_t sampling_frequency = 48000;
  uint64_t buf_len = sampling_frequency * duration * 4;
  uint8_t *audio_buf;
  audio_buf = (uint8_t *)malloc(buf_len);
  create_sine_wave(audio_buf, buf_len, frequency, sampling_frequency);

  DEBUG("Creating audio data at 0x%016lx\n", audio_buf);

  struct nk_sound_dev_stream *stream = &hda_dev->streams[stream_id]->stream;
  hda_write_to_stream(hda_dev, stream, audio_buf, buf_len, 0, 0);

  nk_vc_printf("Writing to stream %d: freq %d duration %d\n", stream_id,
               frequency, duration);

  return 0;
}

static struct shell_cmd_impl write_stream_impl = {
    .cmd = "write-stream",
    .help_str = "write-stream stream_id frequency duration",
    .handler = handle_write_to_stream,
};
nk_register_shell_cmd(write_stream_impl);

static int handle_play_stream(char *buf, void *priv) {
  uint64_t stream_id;
  sscanf(buf, "play-stream %d", &stream_id);

  struct nk_sound_dev_stream *stream = &hda_dev->streams[stream_id]->stream;
  hda_play_stream(hda_dev, stream);

  nk_vc_printf("Playing stream %d\n", stream_id);

  return 0;
}

static struct shell_cmd_impl play_stream_impl = {
    .cmd = "play-stream",
    .help_str = "play-stream stream_id",
    .handler = handle_play_stream,
};
nk_register_shell_cmd(play_stream_impl);

static int handle_open_stream(char *buf, void *priv) {
  struct nk_sound_dev_params params;
  params.type = NK_SOUND_DEV_OUTPUT_STREAM;
  params.scale = NK_SOUND_DEV_SCALE_LINEAR;
  params.sample_rate = NK_SOUND_DEV_SAMPLE_RATE_48kHZ;
  params.sample_resolution = NK_SOUND_DEV_SAMPLE_RESOLUTION_16;
  params.num_of_channels = 2;

  struct nk_sound_dev_stream *stream = hda_open_stream(hda_dev, &params);

  nk_vc_printf("Opening stream %d\n", (uint64_t)stream->stream_id);

  return 0;
}

static struct shell_cmd_impl open_stream_impl = {
    .cmd = "open-stream",
    .help_str = "open-stream",
    .handler = handle_open_stream,
};
nk_register_shell_cmd(open_stream_impl);
