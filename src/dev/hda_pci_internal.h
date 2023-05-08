#ifndef __HDA_PCI_INTERNAL
#define __HDA_PCI_INTERNAL

#include "nautilus/spinlock.h"

// ========== PCI CONFIG SPACE ==========
#define INTEL_VENDOR_ID 0x8086
#define HDA_DEVICE_ID 0x2668

// ========== HDA DEVICE STATES ==========
struct hda_pci_dev {
  // for protection of per-device state
  spinlock_t lock;
};

#endif
