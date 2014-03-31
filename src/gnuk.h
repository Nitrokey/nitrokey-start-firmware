/*
 * Application layer <-> CCID layer data structure
 */
struct apdu {
  uint8_t seq;

  /* command APDU */
  uint8_t *cmd_apdu_head;	/* CLS INS P1 P2 [ internal Lc ] */
  uint8_t *cmd_apdu_data;
  uint16_t cmd_apdu_data_len;	/* Nc, calculated by Lc field */
  uint16_t expected_res_size;	/* Ne, calculated by Le field */

  /* response APDU */
  uint16_t sw;
  uint8_t *res_apdu_data;
  uint16_t res_apdu_data_len;
};

extern struct apdu apdu;

#define CARD_CHANGE_INSERT 0
#define CARD_CHANGE_REMOVE 1
#define CARD_CHANGE_TOGGLE 2
void ccid_card_change_signal (int how);

/* CCID thread */
#define EV_RX_DATA_READY (1) /* USB Rx data available  */
#define EV_EXEC_FINISHED (2) /* OpenPGP Execution finished */
#define EV_TX_FINISHED   (4) /* CCID Tx finished  */
#define EV_CARD_CHANGE   (8)

/* OpenPGPcard thread */
#define EV_PINPAD_INPUT_DONE    (1)
#define EV_EXIT                 (2)
#define EV_CMD_AVAILABLE        (4)
#define EV_VERIFY_CMD_AVAILABLE (8)
#define EV_MODIFY_CMD_AVAILABLE (16)

/* Maximum cmd apdu data is key import 22+4+128+128 (proc_key_import) */
#define MAX_CMD_APDU_DATA_SIZE (22+4+128+128) /* without header */
/* Maximum res apdu data is public key 5+9+256 (gpg_do_public_key) */
#define MAX_RES_APDU_DATA_SIZE (5+9+256) /* without trailer */

#define ICC_MSG_HEADER_SIZE	10

#define res_APDU apdu.res_apdu_data
#define res_APDU_size apdu.res_apdu_data_len

/* USB buffer size of LL (Low-level): size of single Bulk transaction */
#define USB_LL_BUF_SIZE 64

enum icc_state
{
  ICC_STATE_NOCARD,		/* No card available */
  ICC_STATE_START,		/* Initial */
  ICC_STATE_WAIT,		/* Waiting APDU */
				/* Busy1, Busy2, Busy3, Busy5 */
  ICC_STATE_EXECUTE,		/* Busy4 */
  ICC_STATE_RECEIVE,		/* APDU Received Partially */
  ICC_STATE_SEND,		/* APDU Sent Partially */

  ICC_STATE_EXITED,		/* ICC Thread Terminated */
  ICC_STATE_EXEC_REQUESTED,	/* Exec requested */
};

extern enum icc_state *icc_state_p;

extern volatile uint8_t auth_status;
#define AC_NONE_AUTHORIZED	0x00
#define AC_PSO_CDS_AUTHORIZED	0x01  /* PW1 with 0x81 verified */
#define AC_OTHER_AUTHORIZED	0x02  /* PW1 with 0x82 verified */
#define AC_ADMIN_AUTHORIZED	0x04  /* PW3 verified */
#define AC_NEVER		0x80
#define AC_ALWAYS		0xFF

#define PW_ERR_PW1 0
#define PW_ERR_RC  1
#define PW_ERR_PW3 2
extern int gpg_pw_get_retry_counter (int who);
extern int gpg_pw_locked (uint8_t which);
extern void gpg_pw_reset_err_counter (uint8_t which);
extern void gpg_pw_increment_err_counter (uint8_t which);

extern int ac_check_status (uint8_t ac_flag);
extern int verify_pso_cds (const uint8_t *pw, int pw_len);
extern int verify_other (const uint8_t *pw, int pw_len);
extern int verify_user_0 (uint8_t access, const uint8_t *pw, int buf_len,
			  int pw_len_known, const uint8_t *ks_pw1, int saveks);
extern int verify_admin (const uint8_t *pw, int pw_len);
extern int verify_admin_0 (const uint8_t *pw, int buf_len, int pw_len_known,
			   const uint8_t *ks_pw3, int saveks);

extern void ac_reset_pso_cds (void);
extern void ac_reset_other (void);
extern void ac_reset_admin (void);
extern void ac_fini (void);


extern void set_res_sw (uint8_t sw1, uint8_t sw2);
extern uint16_t data_objects_number_of_bytes;

#define CHALLENGE_LEN	32

extern void gpg_data_scan (const uint8_t *p);
extern void gpg_data_copy (const uint8_t *p);
extern void gpg_do_get_data (uint16_t tag, int with_tag);
extern void gpg_do_put_data (uint16_t tag, const uint8_t *data, int len);
extern void gpg_do_public_key (uint8_t kk_byte);
extern void gpg_do_keygen (uint8_t kk_byte);

extern const uint8_t *gpg_get_firmware_update_key (uint8_t keyno);


enum kind_of_key {
  GPG_KEY_FOR_SIGNING = 0,
  GPG_KEY_FOR_DECRYPTION,
  GPG_KEY_FOR_AUTHENTICATION,
};

extern const uint8_t *flash_init (void);
extern void flash_do_release (const uint8_t *);
extern const uint8_t *flash_do_write (uint8_t nr, const uint8_t *data, int len);
extern uint8_t *flash_key_alloc (enum kind_of_key);
extern void flash_key_release (uint8_t *);
extern int flash_key_write (uint8_t *key_addr, const uint8_t *key_data,
			    const uint8_t *pubkey, int pubkey_len);
extern void flash_set_data_pool_last (const uint8_t *p);
extern void flash_clear_halfword (uint32_t addr);
extern void flash_increment_counter (uint8_t counter_tag_nr);
extern void flash_reset_counter (uint8_t counter_tag_nr);

#define FILEID_SERIAL_NO	0
#define FILEID_UPDATE_KEY_0	1
#define FILEID_UPDATE_KEY_1	2
#define FILEID_UPDATE_KEY_2	3
#define FILEID_UPDATE_KEY_3	4
#define FILEID_CH_CERTIFICATE	5
extern int flash_erase_binary (uint8_t file_id);
extern int flash_write_binary (uint8_t file_id, const uint8_t *data, uint16_t len, uint16_t offset);

#define FLASH_CH_CERTIFICATE_SIZE 2048

/* Linker set these two symbols */
extern uint8_t ch_certificate_start;
extern uint8_t random_bits_start;

#define KEY_CONTENT_LEN 256	/* p and q */
#define INITIAL_VECTOR_SIZE 16
#define DATA_ENCRYPTION_KEY_SIZE 16

struct key_data {
  uint8_t *key_addr;		 /* Pointer to encrypted data, and public */
  uint8_t data[KEY_CONTENT_LEN]; /* decrypted data content */
};

struct key_data_internal {
  uint32_t data[KEY_CONTENT_LEN/4]; /* p and q */
  uint32_t checksum[DATA_ENCRYPTION_KEY_SIZE/4];
};

struct prvkey_data {
  /*
   * IV: Initial Vector
   */
  uint8_t iv[INITIAL_VECTOR_SIZE];
  /*
   * Checksum
   */
  uint8_t checksum_encrypted[DATA_ENCRYPTION_KEY_SIZE];
  /*
   * DEK (Data Encryption Key) encrypted
   */
  uint8_t dek_encrypted_1[DATA_ENCRYPTION_KEY_SIZE]; /* For user */
  uint8_t dek_encrypted_2[DATA_ENCRYPTION_KEY_SIZE]; /* For resetcode */
  uint8_t dek_encrypted_3[DATA_ENCRYPTION_KEY_SIZE]; /* For admin */
};

#define BY_USER		1
#define BY_RESETCODE	2
#define BY_ADMIN	3

/*
 * Maximum length of pass phrase is 127.
 * We use the top bit (0x80) to encode if keystring is available within DO.
 */
#define PW_LEN_MAX            127
#define PW_LEN_MASK          0x7f
#define PW_LEN_KEYSTRING_BIT 0x80

#define SALT_SIZE 8

void s2k (const unsigned char *salt, size_t slen,
	  const unsigned char *input, size_t ilen, unsigned char output[32]);

#define KEYSTRING_PASSLEN_SIZE  1
#define KEYSTRING_SALT_SIZE     SALT_SIZE
#define KEYSTRING_MD_SIZE       32
#define KEYSTRING_SIZE        (KEYSTRING_PASSLEN_SIZE + KEYSTRING_SALT_SIZE \
			       + KEYSTRING_MD_SIZE)
#define KS_META_SIZE          (KEYSTRING_PASSLEN_SIZE + KEYSTRING_SALT_SIZE)
#define KS_GET_SALT(ks)       (ks + KEYSTRING_PASSLEN_SIZE)
#define KS_GET_KEYSTRING(ks)  (ks + KS_META_SIZE)

extern void gpg_do_clear_prvkey (enum kind_of_key kk);
extern int gpg_do_load_prvkey (enum kind_of_key kk, int who, const uint8_t *keystring);
extern int gpg_do_chks_prvkey (enum kind_of_key kk,
			       int who_old, const uint8_t *old_ks,
			       int who_new, const uint8_t *new_ks);

extern int gpg_change_keystring (int who_old, const uint8_t *old_ks,
				 int who_new, const uint8_t *new_ks);

extern struct key_data kd[3];

#ifdef DEBUG
#define DEBUG_MORE 1
/*
 * Debug functions in debug.c
 */
extern void put_byte (uint8_t b);
extern void put_byte_with_no_nl (uint8_t b);
extern void put_short (uint16_t x);
extern void put_word (uint32_t x);
extern void put_int (uint32_t x);
extern void put_string (const char *s);
extern void put_binary (const char *s, int len);

#define DEBUG_INFO(msg)	    put_string (msg)
#define DEBUG_WORD(w)	    put_word (w)
#define DEBUG_SHORT(h)	    put_short (h)
#define DEBUG_BYTE(b)       put_byte (b)
#define DEBUG_BINARY(s,len) put_binary ((const char *)s,len)
#else
#define DEBUG_INFO(msg)
#define DEBUG_WORD(w)
#define DEBUG_SHORT(h)
#define DEBUG_BYTE(b)
#define DEBUG_BINARY(s,len)
#endif

extern int rsa_sign (const uint8_t *, uint8_t *, int, struct key_data *);
extern uint8_t *modulus_calc (const uint8_t *, int);
extern int rsa_decrypt (const uint8_t *, uint8_t *, int, struct key_data *);
extern int rsa_verify (const uint8_t *pubkey, const uint8_t *hash,
		       const uint8_t *signature);
extern uint8_t *rsa_genkey (void);

extern int ecdsa_sign_p256r1  (const uint8_t *hash, uint8_t *output,
			       const uint8_t *key_data);
extern uint8_t *ecdsa_compute_public_p256r1 (const uint8_t *key_data);

extern int ecdsa_sign_p256k1  (const uint8_t *hash, uint8_t *output,
			       const uint8_t *key_data);
extern uint8_t *ecdsa_compute_public_p256k1 (const uint8_t *key_data);

extern const uint8_t *gpg_do_read_simple (uint8_t);
extern void gpg_do_write_simple (uint8_t, const uint8_t *, int);
extern void gpg_increment_digital_signature_counter (void);


extern void fatal (uint8_t code) __attribute__ ((noreturn));
#define FATAL_FLASH  1
#define FATAL_RANDOM 2

extern uint8_t keystring_md_pw3[KEYSTRING_MD_SIZE];
extern uint8_t admin_authorized;

/*** Flash memory tag values ***/
#define NR_NONE			0x00
/* Data objects */
/*
 * Representation of data object:
 *
 *   <-1 halfword-> <--len/2 halfwords->
 *   <-tag-><-len-> <---data content--->
 */
#define NR_DO__FIRST__		0x01
#define NR_DO_SEX		0x01
#define NR_DO_FP_SIG		0x02
#define NR_DO_FP_DEC		0x03
#define NR_DO_FP_AUT		0x04
#define NR_DO_CAFP_1		0x05
#define NR_DO_CAFP_2		0x06
#define NR_DO_CAFP_3		0x07
#define NR_DO_KGTIME_SIG	0x08
#define NR_DO_KGTIME_DEC	0x09
#define NR_DO_KGTIME_AUT	0x0a
#define NR_DO_LOGIN_DATA	0x0b
#define NR_DO_URL		0x0c
#define NR_DO_NAME		0x0d
#define NR_DO_LANGUAGE		0x0e
#define NR_DO_PRVKEY_SIG	0x0f
#define NR_DO_PRVKEY_DEC	0x10
#define NR_DO_PRVKEY_AUT	0x11
#define NR_DO_KEYSTRING_PW1	0x12
#define NR_DO_KEYSTRING_RC	0x13
#define NR_DO_KEYSTRING_PW3	0x14
#define NR_DO__LAST__		21   /* == 0x15 */
/* 14-bit counter for DS: Recorded in flash memory by 1-halfword (2-byte).  */
/*
 * Representation of 14-bit counter:
 *      0: 0x8000
 *      1: 0x8001
 *     ...
 *  16383: 0xbfff
 */
#define NR_COUNTER_DS		0x80 /* ..0xbf */
/* 10-bit counter for DS: Recorded in flash memory by 1-halfword (2-byte).  */
/*
 * Representation of 10-bit counter:
 *      0: 0xc000
 *      1: 0xc001
 *     ...
 *   1023: 0xc3ff
 */
#define NR_COUNTER_DS_LSB	0xc0 /* ..0xc3 */
/* 8-bit int or Boolean objects: Recorded in flash memory by 1-halfword (2-byte) */
/*
 * Representation of Boolean object:
 *   0: No record in flash memory
 *   1: 0xf000
 */
#define NR_BOOL_PW1_LIFETIME	0xf0
/*
 * NR_BOOL_SOMETHING, NR_UINT_SOMETHING could be here...  Use 0xf?
 */
/* 123-counters: Recorded in flash memory by 2-halfword (4-byte).  */
/*
 * Representation of 123-counters:
 *   0: No record in flash memory
 *   1: 0xfe?? 0xffff
 *   2: 0xfe?? 0xc3c3
 *   3: 0xfe?? 0x0000
 *                    where <counter_id> is placed at second byte <??>
 */
#define NR_COUNTER_123		0xfe
#define NR_EMPTY		0xff

#define SIZE_PW_STATUS_BYTES 7


#define NUM_ALL_PRV_KEYS 3	/* SIG, DEC and AUT */

#if !defined(OPENPGP_CARD_INITIAL_PW1)
#define OPENPGP_CARD_INITIAL_PW1 "123456"
#endif

#if !defined(OPENPGP_CARD_INITIAL_PW3)
#define OPENPGP_CARD_INITIAL_PW3 "12345678"
#endif

extern const uint8_t openpgpcard_aid[14];

extern void flash_bool_clear (const uint8_t **addr_p);
extern const uint8_t *flash_bool_write (uint8_t nr);
extern int flash_cnt123_get_value (const uint8_t *p);
extern void flash_cnt123_increment (uint8_t which, const uint8_t **addr_p);
extern void flash_cnt123_clear (const uint8_t **addr_p);
extern void flash_put_data (uint16_t hw);
extern void flash_warning (const char *msg);

extern void flash_put_data_internal (const uint8_t *p, uint16_t hw);
extern void flash_bool_write_internal (const uint8_t *p, int nr);
extern void flash_cnt123_write_internal (const uint8_t *p, int which, int v);
extern void flash_do_write_internal (const uint8_t *p, int nr, const uint8_t *data, int len);

extern const uint8_t gnukStringSerial[];

#define LED_ONESHOT		(1)
#define LED_TWOSHOTS		(2)
#define LED_SHOW_STATUS		(4)
#define LED_START_COMMAND	(8)
#define LED_FINISH_COMMAND	(16)
#define LED_FATAL		(32)
extern void led_blink (int spec);

#if defined(PINPAD_SUPPORT)
# if defined(PINPAD_CIR_SUPPORT)
extern void cir_init (void);
# elif defined(PINPAD_DIAL_SUPPORT)
extern void dial_sw_disable (void);
extern void dial_sw_enable (void);
# elif defined(PINPAD_DND_SUPPORT)
extern void msc_init (void);
extern void msc_media_insert_change (int available);
extern int msc_scsi_write (uint32_t lba, const uint8_t *buf, size_t size);
extern int msc_scsi_read (uint32_t lba, const uint8_t **sector_p);
extern void msc_scsi_stop (uint8_t code);
# endif
#define PIN_INPUT_CURRENT 1
#define PIN_INPUT_NEW     2
#define PIN_INPUT_CONFIRM 3
#define MAX_PIN_CHARS 32
extern uint8_t pin_input_buffer[MAX_PIN_CHARS];
extern uint8_t pin_input_len;

extern int pinpad_getline (int msg_code, uint32_t timeout_usec);

#endif

extern uint8_t _regnual_start, __heap_end__[];
