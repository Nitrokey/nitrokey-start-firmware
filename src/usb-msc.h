#define MSC_CBW_SIGNATURE 0x43425355
#define MSC_CSW_SIGNATURE 0x53425355

#define MSC_GET_MAX_LUN_COMMAND        0xFE
#define MSC_MASS_STORAGE_RESET_COMMAND 0xFF

#define MSC_CSW_STATUS_PASSED 0
#define MSC_CSW_STATUS_FAILED 1

#define SCSI_INQUIRY                0x12
#define SCSI_MODE_SENSE6            0x1A
#define SCSI_ALLOW_MEDIUM_REMOVAL   0x1E
#define SCSI_READ10                 0x28
#define SCSI_READ_CAPACITY10        0x25
#define SCSI_REQUEST_SENSE          0x03
#define SCSI_START_STOP_UNIT        0x1B
#define SCSI_TEST_UNIT_READY        0x00
#define SCSI_WRITE10                0x2A
#define SCSI_VERIFY10               0x2F
#define SCSI_READ_FORMAT_CAPACITIES 0x23

#define SCSI_SYNCHRONIZE_CACHE      0x35

#define MSC_IDLE        0
#define MSC_DATA_OUT    1
#define MSC_DATA_IN     2
#define MSC_SENDING_CSW 3
#define MSC_ERROR       4

struct CBW {
  uint32_t dCBWSignature;
  uint32_t dCBWTag;
  uint32_t dCBWDataTransferLength;
  uint8_t bmCBWFlags;
  uint8_t bCBWLUN;
  uint8_t bCBWCBLength;
  uint8_t CBWCB[16];
} __attribute__((packed));

struct CSW {
  uint32_t dCSWSignature;
  uint32_t dCSWTag;
  uint32_t dCSWDataResidue;
  uint8_t bCSWStatus;
} __attribute__((packed));

#define SCSI_ERROR_NOT_READY 2
#define SCSI_ERROR_ILLEAGAL_REQUEST 5
#define SCSI_ERROR_UNIT_ATTENTION 6
#define SCSI_ERROR_DATA_PROTECT 7

extern uint8_t media_available;
extern chopstx_mutex_t *pinpad_mutex;
extern chopstx_cond_t *pinpad_cond;
