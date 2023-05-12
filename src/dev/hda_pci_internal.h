#ifndef __HDA_PCI_INTERNAL
#define __HDA_PCI_INTERNAL

#include <nautilus/dev.h>
#include <nautilus/list.h>
#include <nautilus/sounddev.h>
#include <nautilus/spinlock.h>

// ========== PCI CONFIG SPACE ==========

#define HDA_MAX_NUM_OF_STREAMS 15

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
// Specification: section 3.3.9, page 32
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

// CORBLBASE: CORB Lower Base Address
// Specification: section 3.3.18, page 36
#define CORBLBASE 0x40
#define CORBLBASE_LEN 0x4
typedef uint32_t corblbase_t; // has to be 128-byte alignment

// CORBUBASE: CORB Upper Base Address
// Specification: section 3.3.19, page 36
#define CORBUBASE 0x44
#define CORBUBASE_LEN 0x4
typedef uint32_t corbubase_t;

// CORBWP - CORB Write Pointer
// Specification: section 3.3.20, page 37
#define CORBWP 0x48
#define CORBWP_LEN 0x2
typedef union {
  uint16_t val;
  struct {
    uint8_t corbwp;
    uint8_t res;
  };
} __attribute__((packed)) corbwp_t;

// CORBRP - CORB Read Pointer
// Specification: section 3.3.21, page 37
#define CORBRP 0x4a
#define CORBRP_LEN 0x2
typedef union {
  uint16_t val;
  struct {
    uint8_t corbrp : 8;
    uint8_t res : 7;
    uint8_t corbrprst : 1;
  };
} __attribute__((packed)) corbrp_t;

// CORBCTL - Corb Control
// Specification: section 3.3.22, page 37
#define CORBCTL 0x4c
#define CORBCTL_LEN 0x1
typedef union {
  uint8_t val;
  struct {
    uint8_t cmeie : 1;
    uint8_t corbrun : 1;
    uint8_t res : 6;
  };
} __attribute__((packed)) corbctl_t;

// CORBSIZE - CORB Size
// Specification: section 3.3.24, page 38
#define CORBSIZE 0x4e
#define CORBSIZE_LEN 0x1
typedef union {
  uint8_t val;
  struct {
    uint8_t corbsize : 2;
#define CORBSIZE_DECODE(x)                                                     \
  ((x->corbsize == 0 ? 2 : x->corbsize == 1 ? 16 : x->corbsize == 2 : 256 : 0))
    uint8_t res : 2;
    uint8_t corbszcap : 4;
#define CORBSIZECAP_HAS_2(x) (!!(x.corbszcap & 0x1))
#define CORBSIZECAP_HAS_16(x) (!!(x.corbszcap & 0x2))
#define CORBSIZECAP_HAS_256(x) (!!(x.corbszcap & 0x4))
  };
} __attribute__((packed)) corbsize_t;

// RIRBLBASE - RIRB Lower Base Address
// Specification: section 3.3.25, page 39
#define RIRBLBASE 0x50
#define RIRBLBASE_LEN 0x4
typedef uint32_t rirblbase_t; // has to be 128-byte alignment

// RIRBUBASE - RIRB Upper Base Address
// Specification: section 3.3.26, page 39
#define RIRBUBASE 0x54
#define RIRBUBASE_LEN 0x4
typedef uint32_t rirbubase_t;

// RIRBWP - RIRB Write Pointer
// Specification: section 3.3.27, page 39
#define RIRBWP 0x58
#define RIRBWP_LEN 0x2
typedef union {
  uint16_t val;
  struct {
    uint8_t rirbwp;
    uint8_t res : 7;
    uint8_t rirbwprst : 1;
  };
} __attribute__((packed)) rirbwp_t;

// RINTCNT - RIRB Response Interupt Count
// Specification: section 3.3.28, page 40
#define RINTCNT 0x5a
#define RINTCNT_LEN 0x2
typedef union {
  uint16_t val;
  struct {
    uint8_t rintcnt;
    uint8_t res;
  };
} __attribute__((packed)) rintcnt_t;

// RIRBCTL - RIRB Control
// Specification: section 3.3.29, page 40
#define RIRBCTL 0x5c
#define RIRBCTL_LEN 0x1
typedef union {
  uint8_t val;
  struct {
    uint8_t rintctl : 1;
    uint8_t rirbdmaen : 1;
    uint8_t rirboic : 1;
  };
} __attribute__((packed)) rirbctl_t;

// RIRBSIZE - RIRB Size
// Specification: section 3.3.31, page 41
#define RIRBSIZE 0x5e
#define RIRBSIZE_LEN 0x1
typedef union {
  uint8_t val;
  struct {
    uint8_t rirbsize : 2;
#define RIRBSIZE_DECODE(x)                                                     \
  ((x->rirbsize == 0 ? 2 : x->rirsize == 1 ? 16 : x->rirbsize == 2 : 256 : 0))
    uint8_t res : 2;
    uint8_t rirbszcap : 4;
#define RIRBSIZECAP_HAS_2(x) (!!(x.rirbszcap & 0x1))
#define RIRBSIZECAP_HAS_16(x) (!!(x.rirbszcap & 0x2))
#define RIRBSIZECAP_HAS_256(x) (!!(x.rirbszcap & 0x4))
  };
} __attribute__((packed)) rirbsize_t;

// INTCTL - Interrupt Control
// Specification: section 3.3.14, page 34
#define INTCTL 0x20
#define INTCTL_LEN 0x4
typedef union {
  uint32_t val;
  struct {
    uint32_t sie : 30;
    uint8_t cie : 1;
    uint8_t gie : 1;
  };
} __attribute__((packed)) intctl_t;

// ========== CODEC PARAMETERS AND CONTROLS ==========

// verb generators
#define MAKE_VERB_8(id, payload)                                               \
  ((((uint32_t)(id)) << 8) | (((uint32_t)(payload)) & 0xff))
#define MAKE_VERB_16(id, payload)                                              \
  ((((uint32_t)(id)) << 16) | (((uint32_t)(payload)) & 0xffff))

// Specification: section 7.3.3, page 141
// codec controls (12-bit identifiers)
#define GET_PARAM 0xf00

// Specication: section 7.3.4, page 198
// codec parameters (4-bit identifiers)
#define VENDOR 0x0
#define REVISION 0x2
#define SUBORD_NODE_COUNT 0x4
#define FUNC_GROUP_TYPE 0x5
#define AUDIO_FUNC_GROUP_CAPS 0x8
#define AUDIO_WIDGET_CAPS 0x9
#define PCM_SIZES_AND_RATES 0xa
#define STREAM_FORMATS 0xb
#define PIN_CAPS 0xc
#define AMP_CAPS 0xd
#define CON_LIST_LEN 0xe
#define POWER_STATES 0xf
#define PROC_CAPS 0x10
#define GPIO_COUNT 0x11
#define VOL_KNOB_CAPS 0x13

// Specification: section 7.3.4.4, page 200
#define NODE_TYPE_AUDIO_FUNCTION_GROUP 0x1
#define NODE_TYPE_AUDIO_VENDOR_DEFINED_MODEM 0x2
#define NODE_TYPE_AUDIO_VENDOR_DEFINED_START 0x80
#define NODE_TYPE_AUDIO_VENDOR_DEFINED_END 0xff

// Specification: section 7.3.4.6, page 202
#define WIDGET_TYPE_AUDIO_OUTPUT 0x0
#define WIDGET_TYPE_AUDIO_INPUT 0x1
#define WIDGET_TYPE_AUDIO_MIXER 0x2
#define WIDGET_TYPE_AUDIO_SELECTOR 0x3
#define WIDGET_TYPE_PIN_COMPLEX 0x4
#define WIDGET_TYPE_POWER 0x5
#define WIDGET_TYPE_VOLUME_KNOB 0x6
#define WIDGET_TYPE_BEEP_GENERATOR 0x7
#define WIDGET_TYPE_VENDOR_DEFINED 0xf

// Specification: section 7.3.4.7, page 205
#define PCM_BIT_DEPTH_32 20
#define PCM_BIT_DEPTH_24 19
#define PCM_BIT_DEPTH_20 18
#define PCM_BIT_DEPTH_16 17
#define PCM_BIT_DEPTH_8 16
#define PCM_SAMPLE_RATE_8kHZ 0
#define PCM_SAMPLE_RATE_11kHZ025 1
#define PCM_SAMPLE_RATE_16kHZ 2
#define PCM_SAMPLE_RATE_22kHZ05 3
#define PCM_SAMPLE_RATE_32kHZ 4
#define PCM_SAMPLE_RATE_44kHZ1 5
#define PCM_SAMPLE_RATE_48kHZ 6
#define PCM_SAMPLE_RATE_88kHZ2 7
#define PCM_SAMPLE_RATE_96kHZ 8
#define PCM_SAMPLE_RATE_176kHZ4 9
#define PCM_SAMPLE_RATE_192kHZ 10
#define PCM_SAMPLE_RATE_384kHZ 11
#define PCM_IS_SUPPORTED(rp, offset) (!!(rp >> offset & 0x1))
#define PCM_BIT_DEPTH_OFFSET 16
#define PCM_SAMPLE_RATE_OFFSET 0

const nk_sound_dev_sample_rate_t PCM_SAMPLE_RATES[] = {
    NK_SOUND_DEV_SAMPLE_RATE_8kHZ,   NK_SOUND_DEV_SAMPLE_RATE_11kHZ025,
    NK_SOUND_DEV_SAMPLE_RATE_16kHZ,  NK_SOUND_DEV_SAMPLE_RATE_22kHZ05,
    NK_SOUND_DEV_SAMPLE_RATE_32kHZ,  NK_SOUND_DEV_SAMPLE_RATE_44kHZ1,
    NK_SOUND_DEV_SAMPLE_RATE_48kHZ,  NK_SOUND_DEV_SAMPLE_RATE_88kHZ2,
    NK_SOUND_DEV_SAMPLE_RATE_96kHZ,  NK_SOUND_DEV_SAMPLE_RATE_176kHZ4,
    NK_SOUND_DEV_SAMPLE_RATE_192kHZ, NK_SOUND_DEV_SAMPLE_RATE_384kHZ,
};

const nk_sound_dev_sample_resolution_t PCM_BIT_DEPTHS[] = {
    NK_SOUND_DEV_SAMPLE_RESOLUTION_8,  NK_SOUND_DEV_SAMPLE_RESOLUTION_16,
    NK_SOUND_DEV_SAMPLE_RESOLUTION_20, NK_SOUND_DEV_SAMPLE_RESOLUTION_24,
    NK_SOUND_DEV_SAMPLE_RESOLUTION_32,
};

// ========== CODEC COMMAND AND CONTROL ==========

#define MAX_CORB_ENTRIES 256
typedef union {
  uint32_t val;
  struct {
    uint32_t verb : 20;
    uint8_t nid : 7;
    uint8_t indirect : 1;
    uint8_t CAd : 4;
  } __attribute__((packed));
} __attribute__((packed)) corb_entry_t;
typedef corb_entry_t codec_req_t;

typedef struct {
  corb_entry_t buf[MAX_CORB_ENTRIES];
  int size;
  // use CORBWP->CORBWP instead; this is a write pointer maintained by the HDA
  // hardware itself int cur_write;
} __attribute__((aligned(128))) corb_state_t;

#define MAX_RIRB_ENTRIES 256
typedef struct {
  uint32_t resp;
  union {
    uint32_t val;
    struct {
      uint8_t codec : 4;
      uint8_t unsol : 1;
      uint32_t res : 27;
    } __attribute__((packed));
  } __attribute__((packed)) resp_ex;
} __attribute__((packed)) rirb_entry_t;
typedef rirb_entry_t codec_resp_t;

typedef struct {
  rirb_entry_t buf[MAX_RIRB_ENTRIES];
  int size;
  int cur_read;
} __attribute__((aligned(128))) rirb_state_t;

// ========== AVAILABLE MODE NODE ==========

struct available_mode {
  // node associated with this struct
  struct list_head node;

  // parameters for this mode
  struct nk_sound_dev_params params;
};

// ========== HDA DEVICE STATES ==========

struct hda_pci_dev {
  // for protection of per-device state
  spinlock_t lock;

  // generic nk dev
  struct nk_sound_dev *nk_dev;

  // pci device
  struct pci_dev *pci_dev;

  // node associated with this device
  // it is stored in "dev_list", a global variable inside hda_pci.c
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

  // CORB and RIRB
  corb_state_t corb;
  rirb_state_t rirb;

  // store all streams
  struct nk_sound_dev_stream *streams[HDA_MAX_NUM_OF_STREAMS + 1];

  // store available mdoes
  struct list_head available_modes_list;
  // TODO: Create a field to store avaliable modes
  // was thinking about dynamic array since we don't know how many set of params
  // is avaliable until as scan through the widges, but also not too sure about
  // this since c does not have dynamic array (pls correct me if im wrong about
  // this). so, was not sure if there is a better way to how store the avalible
  // modes.
};

#endif
