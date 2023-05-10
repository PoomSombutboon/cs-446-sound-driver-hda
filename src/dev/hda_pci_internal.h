#ifndef __HDA_PCI_INTERNAL
#define __HDA_PCI_INTERNAL

#include <nautilus/dev.h>
#include <nautilus/list.h>
#include <nautilus/sounddev.h>
#include <nautilus/spinlock.h>

// ========== PCI CONFIG SPACE ==========

#define INTEL_VENDOR_ID 0x8086
#define HDA_DEVICE_ID 0x2668
#define HDA_PCI_COMMAND_OFFSET 0x4
#define HDA_PCI_STATUS_OFFSET 0x6

// ========== HDA CONTROLLER REGISTER SET ==========

// GCTL - Global Control
// Specification: section 3.3.7, page 30
#define GCTL 0x8
#define GCTL_LEN 0x4
typedef union {
  uint32_t val;
  struct {
    uint8_t crst : 1;
    uint8_t fcntrl : 1;
    uint8_t res2 : 6;
    uint8_t unsol : 1;
    uint32_t res1 : 23;
  };
} __attribute__((packed)) gctl_t;

// STATESTS - State Change Status
// Specification: sectoin 3.3.9, page 32
#define STATESTS 0xe
#define STATESTS_LEN 0x2
typedef union {
  uint16_t val;
  struct {
    uint16_t sdiwake : 15;
#define SDIMAX 15
#define SDIWAKE(s, i) ((((s).sdiwake) >> (i)) & 0x1)
    uint8_t res : 1;
  };
} __attribute__((packed)) statests_t;

// ========== HDA DEVICE STATES ==========

struct hda_pci_dev {
  // for protection of per-device state
  spinlock_t lock;

  // generic nk dev
  struct nk_sound_dev *nk_dev;

  // pci device
  struct pci_dev *pci_dev;

  // list of all hda devices will be stored here
  struct list_head hda_node;

  // the following is for legacy interrupts
  // we will use MSI
  uint8_t pci_intr;
  uint8_t intr_vec;

  // identifier to determine if pci device is mmio or pmio
  enum { NONE, IO, MEMORY } method;

  // pci registers region: only EITHER ioport or mem will be defined
  uint16_t ioport_start;
  uint16_t ioport_end;
  uint64_t mem_start;
  uint64_t mem_end;

  // valid codecs
  uint8_t codecs[SDIMAX];
};

#endif
