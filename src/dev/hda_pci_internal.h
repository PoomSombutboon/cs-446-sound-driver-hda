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
  uint16_t mem_start;
  uint16_t mem_end;
};

#endif
