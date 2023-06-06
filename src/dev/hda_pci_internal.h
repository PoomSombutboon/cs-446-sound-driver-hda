#ifndef __HDA_PCI_INTERNAL
#define __HDA_PCI_INTERNAL

#include <nautilus/dev.h>
#include <nautilus/list.h>
#include <nautilus/sounddev.h>
#include <nautilus/spinlock.h>

// ========== PCI CONFIG SPACE ==========

#define HDA_MAX_NUM_OF_STREAMS 30
#define HDA_MAX_NUM_OF_STREAM_TAGS 16
#define HDA_MAX_NUM_OF_BDLS 10

// ========== PCI CONFIG SPACE ==========

#define INTEL_VENDOR_ID 0x8086
#define HDA_DEVICE_ID 0x2668
#define HDA_PCI_COMMAND_OFFSET 0x4
#define HDA_PCI_STATUS_OFFSET 0x6

// ========== HDA CONTROLLER REGISTER SET ==========

// GCAP - Global Capabilities
// Specification: section 3.3.2, page 28
#define GCAP 0x0
#define GCAP_LEN 0x2
typedef union gcap {
  uint16_t val;
  struct {
    uint8_t ok64 : 1;
    uint8_t nsdo : 2;
    uint8_t bss : 5;
    uint8_t iss : 4;
    uint8_t oss : 4;
#define NUM_SDO(x)                                                             \
  (((x).nsdo) == 0 ? 1 : ((x).nsdo) == 1 ? 2 : ((x).nsdo) == 2 ? 4 : 0)
  };
} __attribute__((packed)) gcap_t;

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
#define GET_CONLIST 0xf02
#define GET_CONVCTL 0xf06
#define SET_CONVCTL 0x706
#define GET_POWSTATE 0xf05
#define GET_GAINMUTE 0xb

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

typedef union {
  uint8_t val;
  struct {
    uint8_t channel : 4; // Stream
    uint8_t stream : 4;  // Channel
  } __attribute__((packed));
} __attribute__((packed)) outwgctl_t;

// ========== AVAILABLE MODE NODE ==========

struct available_mode {
  // node associated with this struct
  struct list_head node;

  // parameters for this mode
  struct nk_sound_dev_params params;
};

// ========== STREAM CONTROL REGISTERS ==========

// First output stream in QEMU
// TODO: Remove this hard coded constant
#define OUTPUT_STREAM_NUM 4

// Specification: 3.3, page 27
#define STREAM_OFFSET_CONST 0x20

// SDNCTL - Stream Descriptor n Control
// Specification: 3.3.35, page 43
#define SDNCTL 0x80
#define SDNCTL_LEN 0x3
typedef union {
  struct {
    uint8_t byte_1;
    uint8_t byte_2;
    uint8_t byte_3;
  } __attribute__((packed));
  struct {
    uint8_t srst : 1;
    uint8_t run : 1;
    uint8_t ioce : 1;
    uint8_t feie : 1;
    uint8_t deie : 1;
    uint16_t resv : 11;
    uint8_t stripe : 2;
    uint8_t tp : 1;
    uint8_t dir : 1;
    uint8_t strm_num : 4;
  } __attribute__((packed));
} __attribute__((packed)) sdnctl_t;

// SDNLVI - Stream Descriptor n Last Valid Index
// Specification: section 3.3.39, page 47
#define SDNLVI 0x8c
#define SDNLVI_LEN 0x2
typedef union {
  uint16_t val;
  struct {
    uint8_t lvi : 8;
    uint8_t resv : 8;
  } __attribute__((packed));
} __attribute__((packed)) sdnlvi_t;

// SDNFMT - Stream Descriptor n Format
// Specification: section 3.3.41, page 47
#define SDNFMT 0x92
#define SDNFMT_LEN 0x2
typedef union {
  uint16_t val;
  struct {
    uint8_t chan : 4;
    uint8_t bits : 3;
    uint8_t resv2 : 1;
    uint8_t div : 3;
    uint8_t mult : 3;
    uint8_t base : 1;
    uint8_t resv1 : 1;
  } __attribute__((packed));
} __attribute__((packed)) sdnfmt_t;

#define BASE_48kHz 0
#define BASE_44kHz1 1
#define MULT_BY_1 0
#define MULT_BY_2 1
#define MULT_BY_3 2
#define MULT_BY_4 3
#define DIV_BY_1 0
#define DIV_BY_2 1
#define DIV_BY_3 2
#define DIV_BY_4 3
#define DIV_BY_5 4
#define DIV_BY_6 5
#define DIV_BY_7 6
#define DIV_BY_8 7

// Specification: section 7.3.4.7, page 205
const uint8_t HDA_SAMPLE_RATES[][3] = {
    {BASE_48kHz, MULT_BY_1, DIV_BY_6},  // 8.0 kHz
    {BASE_44kHz1, MULT_BY_1, DIV_BY_4}, // 11.025 kHz
    {BASE_48kHz, MULT_BY_1, DIV_BY_3},  // 16.0 kHz
    {BASE_44kHz1, MULT_BY_1, DIV_BY_2}, // 22.05 kHz
    {BASE_48kHz, MULT_BY_2, DIV_BY_3},  // 32.0 kHz
    {BASE_44kHz1, MULT_BY_1, DIV_BY_1}, // 44.1 kHz
    {BASE_48kHz, MULT_BY_1, DIV_BY_1},  // 48.0 kHz
    {BASE_44kHz1, MULT_BY_2, DIV_BY_1}, // 88.2 kHz
    {BASE_48kHz, MULT_BY_2, DIV_BY_1},  // 96.0 kHz
    {BASE_44kHz1, MULT_BY_4, DIV_BY_1}, // 176.4 kHz
    {BASE_48kHz, MULT_BY_4, DIV_BY_1},  // 192.0 kHz
};

// SD0STS - Stream Descriptor n Status
// Specification: section
#define SDNSTS 0x83
#define SDNSTS_LEN 0x1
typedef union {
  uint8_t val;
  struct {
    uint8_t resv2 : 2;
    uint8_t bcis : 1;
    uint8_t fifoe : 1;
    uint8_t dese : 1;
    uint8_t fifordy : 1;
    uint8_t resv1 : 2;
  } __attribute__((packed));
} __attribute__((packed)) sdnsts_t;

// SDNBDPL - Stream Descriptor n Lower Base Address
// Specification: section 3.3.42, page 49
#define SDNBDPL 0x98

// SDNBDPU - Stream Descriptor n Upper Base Address
// Specification: section 3.3.43, page 49
#define SDNBDPU 0x9c

// ========== STREAM MANAGEMENT OBJECTS ==========

// BDLE - Buffer Descriptor List Entry
// Specification: 3.6.3, page 55
typedef struct {
  uint64_t address : 64;
  uint32_t length : 32;
  uint8_t ioc : 1;
  uint32_t reserved : 31;
} __attribute__((packed, aligned(16))) bdle_t;

// BDL - Buffer Descriptor List
// specification: 3.6.2, page 55
#define MAX_BDL_ENTIRES 256
typedef struct {
  bdle_t buf[MAX_BDL_ENTIRES];
} __attribute__((aligned(128))) bdl_t;

// struct to store callbacks and their contexts
typedef struct hda_pci_callback {
  void (*callback)(nk_sound_dev_status_t, void *);
  uint64_t *context;
} hda_pci_callback_t;

struct hda_stream_info {
  struct nk_sound_dev_stream stream;

  // store buffer descriptor lists in a ring buffer
  bdl_t *bdls[HDA_MAX_NUM_OF_BDLS];
  uint32_t bdls_size[HDA_MAX_NUM_OF_BDLS];
  uint8_t bdls_lvi[HDA_MAX_NUM_OF_BDLS];
  uint8_t bdls_start_index;
  uint8_t bdls_length;

  // store callback into ring buffer
  hda_pci_callback_t callbacks[HDA_MAX_NUM_OF_BDLS];

  // This is one of the 30 possible stream numbers defined in section 3.3.2 of
  // the specification. There is a maximum of 30 possible streams that can be
  // supported by Intel HDA: 15 may be configured as output and another 15 as
  // input.
  // As defined by the specification, the input streams come first, followed by
  // the output streams. For example, if we have 5 input streams and 2 output
  // streams, stream number 0 to 4 will be the input streams and 5 to 6 the
  // output. IDs can range from 0 to 29.
  uint8_t stream_id;

  // This refers to the "Tag" associated with the data being transferred on this
  // link. It is used when setting the "STRM - stream number" bit inside the
  // SDnCTL register in section 3.3.35 of the specification.
  // This is used to identify the link a converter is to be used with by the
  // "Converter Control" as defined in section 7.3.3.11 of the specification.
  // Because there can only be a maximum of 15 input/output streams, this number
  // can only range from 1-15. Tag 0 is reserved for "unused" streams by
  // convention.
  uint8_t stream_tag;
};

// SDNCBL - Stream Descriptor n Cyclic Buffer Length
// Specification: section 3.3.38, page 46
#define SDNCBL 0x88

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

  // input and output stream number ranges
  uint8_t input_stream_start;
  uint8_t input_stream_end;
  uint8_t output_stream_start;
  uint8_t output_stream_end;

  // store all streams; the index corresponds to the "stream_id" of whatever
  // hda_stream_info struct stored at that index
  struct hda_stream_info *streams[HDA_MAX_NUM_OF_STREAMS];

  // stream tags
  struct hda_stream_info *output_stream_tags[HDA_MAX_NUM_OF_STREAM_TAGS];
  struct hda_stream_info *input_stream_tags[HDA_MAX_NUM_OF_STREAM_TAGS];

  // widget output and input node IDs
  int codec_id;
  uint8_t audio_input_node_id;
  uint8_t input_pin_node_id;
  uint8_t audio_output_node_id;
  uint8_t output_pin_node_id;

  // store available mdoes
  struct list_head available_modes_list;

  // current running stream number
  uint8_t current_stream;
};

// ========== AUDIO DATA ==========

struct audio_data {
  uint8_t *buffer;
  uint64_t size;
};

#endif
