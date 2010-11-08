extern Thread *blinker_thread;
#define EV_LED_ON  ((eventmask_t)1)
#define EV_LED_OFF ((eventmask_t)2)

extern Thread *stdout_thread;
#define EV_TX_READY ((eventmask_t)1)

extern void put_byte (uint8_t b);
extern void put_byte_with_no_nl (uint8_t b);
extern void put_short (uint16_t x);
extern void put_word (uint32_t x);
extern void put_int (uint32_t x);
extern void put_string (const char *s);
extern void put_binary (const char *s, int len);

extern void _write (const char *, int);

/*
 * We declare some of libc functions here, because we will
 * remove dependency on libc in future.
 */
extern size_t strlen (const char *s);
extern int strncmp(const char *s1, const char *s2, size_t n);
extern void *memcpy (void *dest, const void *src, size_t n);
extern void *memset (void *s, int c, size_t n);
extern int memcmp (const void *s1, const void *s2, size_t n);

/*
 * Interface between ICC<-->GPG
 */
extern Thread *icc_thread;
extern Thread *gpg_thread;

#define USB_BUF_SIZE 64

#define EV_EXEC_FINISHED ((eventmask_t)2)	 /* GPG Execution finished */

/* maximum cmd apdu data is key import 22+4+128+128 (proc_key_import) */
#define MAX_CMD_APDU_SIZE (7+282) /* header + data */
/* maximum res apdu data is public key 5+9+256+2 (gpg_do_public_key) */
#define MAX_RES_APDU_SIZE ((5+9+256)+2) /* Data + status */
extern uint8_t cmd_APDU[MAX_CMD_APDU_SIZE];
extern uint8_t res_APDU[MAX_RES_APDU_SIZE];
extern int cmd_APDU_size;
extern int res_APDU_size;

#define AC_NONE_AUTHORIZED	0x00
#define AC_PSO_CDS_AUTHORIZED	0x01  /* PW1 with 0x81 verified */
#define AC_PSO_OTHER_AUTHORIZED	0x02  /* PW1 with 0x82 verified */
#define AC_ADMIN_AUTHORIZED	0x04  /* PW3 verified */
#define AC_NEVER		0x80
#define AC_ALWAYS		0xFF

#define PW_ERR_PW1 0
#define PW_ERR_RC  1
#define PW_ERR_PW3 2
extern int gpg_passwd_locked (uint8_t which);
extern void gpg_reset_pw_err_counter (uint8_t which);
extern void gpg_increment_pw_err_counter (uint8_t which);

extern int ac_check_status (uint8_t ac_flag);
extern int verify_pso_cds (const uint8_t *pw, int pw_len);
extern int verify_pso_other (const uint8_t *pw, int pw_len);
extern int verify_admin (const uint8_t *pw, int pw_len);
extern int verify_admin_0 (const uint8_t *pw, int buf_len, int pw_len_known);

extern void ac_reset_pso_cds (void);
extern void ac_reset_pso_other (void);


extern void write_res_apdu (const uint8_t *p, int len,
			    uint8_t sw1, uint8_t sw2);
uint16_t data_objects_number_of_bytes;

extern int gpg_do_table_init (void);
extern void gpg_do_get_data (uint16_t tag);
extern void gpg_do_put_data (uint16_t tag, const uint8_t *data, int len);
extern void gpg_do_public_key (uint8_t kk_byte);



enum kind_of_key {
  GPG_KEY_FOR_SIGNING,
  GPG_KEY_FOR_DECRYPTION,
  GPG_KEY_FOR_AUTHENTICATION,
};

extern void flash_init (void);
extern void flash_do_release (const uint8_t *);
extern const uint8_t *flash_do_write (uint8_t nr, const uint8_t *data, int len);
extern uint8_t *flash_key_alloc (void);
extern void flash_key_release (const uint8_t *);
extern const uint8_t *flash_data_pool (void);
extern void flash_set_data_pool_last (const uint8_t *p);
extern void flash_clear_halfword (uint32_t addr);
extern void flash_increment_counter (uint8_t counter_tag_nr);
extern void flash_reset_counter (uint8_t counter_tag_nr);

#define KEY_MAGIC_LEN 8
#define KEY_CONTENT_LEN 256	/* p and q */
#define GNUK_MAGIC "Gnuk KEY"

/* encrypted data content */
struct key_data {
  uint8_t data[KEY_CONTENT_LEN]; /* p and q */
  uint32_t check;
  uint32_t random;
  char magic[KEY_MAGIC_LEN];
};

#define ADDITIONAL_DATA_SIZE 16
#define DATA_ENCRYPTION_KEY_SIZE 16
struct prvkey_data {
  const uint8_t *key_addr;
  /*
   * CRM: [C]heck, [R]andom, and [M]agic in struct key_data
   *
   */
  uint8_t crm_encrypted[ADDITIONAL_DATA_SIZE];
  /*
   * DEK: Data Encryption Key
   */
  uint8_t dek_encrypted_1[DATA_ENCRYPTION_KEY_SIZE]; /* For user */
  uint8_t dek_encrypted_2[DATA_ENCRYPTION_KEY_SIZE]; /* For resetcode */
  uint8_t dek_encrypted_3[DATA_ENCRYPTION_KEY_SIZE]; /* For admin */
};

#define BY_USER		1
#define BY_RESETCODE	2
#define BY_ADMIN	3

extern int flash_key_write (uint8_t *key_addr, const uint8_t *key_data, const uint8_t *modulus);

#define KEYSTRING_PASSLEN_SIZE  1
#define KEYSTRING_SALT_SIZE     8 /* optional */
#define KEYSTRING_ITER_SIZE     1 /* optional */
#define KEYSTRING_MD_SIZE       20
#define KEYSTRING_SIZE_PW1 (KEYSTRING_PASSLEN_SIZE+KEYSTRING_MD_SIZE)
#define KEYSTRING_SIZE_RC  (KEYSTRING_PASSLEN_SIZE+KEYSTRING_MD_SIZE)
#define KEYSTRING_SIZE_PW3 (KEYSTRING_PASSLEN_SIZE+KEYSTRING_SALT_SIZE \
  				+KEYSTRING_ITER_SIZE+KEYSTRING_MD_SIZE)

extern int gpg_do_load_prvkey (enum kind_of_key kk, int who, const uint8_t *keystring);
extern int gpg_do_chks_prvkey (enum kind_of_key kk,
			       int who_old, const uint8_t *old_ks,
			       int who_new, const uint8_t *new_ks);

extern int gpg_change_keystring (int who_old, const uint8_t *old_ks,
				 int who_new, const uint8_t *new_ks);

extern struct key_data kd;

#ifdef DEBUG
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

extern int rsa_sign (const uint8_t *, uint8_t *, int);
extern const uint8_t *modulus_calc (const uint8_t *, int);
extern void modulus_free (const uint8_t *);
extern int rsa_decrypt (const uint8_t *input, uint8_t *output, int msg_len);

extern int gpg_do_write_prvkey (enum kind_of_key kk, const uint8_t *key_data, int key_len, const uint8_t *keystring);

extern const uint8_t *gpg_do_read_simple (uint8_t);
extern void gpg_do_write_simple (uint8_t, const uint8_t *, int);
extern void gpg_increment_digital_signature_counter (void);


extern void gpg_set_pw3 (const uint8_t *newpw, int newpw_len);
extern void fatal (void) __attribute__ ((noreturn));

extern uint8_t keystring_md_pw3[KEYSTRING_MD_SIZE];

/*** Flash memory tag values ***/
#define NR_NONE			0x00
/* Data objects */
/*
 * Representation of data object:
 *
 *   <-1 word-> <--len/2 words->
 *   <tag><len> <-data content->
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
/* 14-bit counter for DS: Recorded in flash memory by 1-word (2-byte).  */
/*
 * Representation of 14-bit counter:
 *      0: 0x8000
 *      1: 0x8001
 *     ...
 *  16383: 0xbfff
 */
#define NR_COUNTER_DS		0x80 /* ..0xbf */
/* 10-bit counter for DS: Recorded in flash memory by 1-word (2-byte).  */
/*
 * Representation of 10-bit counter:
 *      0: 0xc000
 *      1: 0xc001
 *     ...
 *   1023: 0xc3ff
 */
#define NR_COUNTER_DS_LSB	0xc0 /* ..0xc3 */
/* 8-bit int or Boolean objects: Recorded in flash memory by 1-word (2-byte) */
/*
 * Representation of Boolean object:
 *   0: No record in flash memory
 *   1: 0xc?00
 */
#define NR_BOOL_PW1_LIFETIME	0xf0
/*
 * NR_BOOL_SOMETHING, NR_UINT_SOMETHING could be here...  Use 0xf?
 */
/* 123-counters: Recorded in flash memory by 2-word (4-byte).  */
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

/* 32-byte random bytes */
extern uint32_t get_random (void);
extern const uint8_t *random_bytes_get (void);
extern void random_bytes_free (const uint8_t *);

extern uint32_t hardclock (void);

extern void set_led (int);

#define NUM_ALL_PRV_KEYS 3	/* SIG, DEC and AUT */

extern uint8_t pw1_keystring[KEYSTRING_SIZE_PW1];

#if !defined(OPENPGP_CARD_INITIAL_PW1)
#define OPENPGP_CARD_INITIAL_PW1 "123456"
#endif

#if !defined(OPENPGP_CARD_INITIAL_PW3)
#define OPENPGP_CARD_INITIAL_PW3 "12345678"
#endif

extern const uint8_t openpgpcard_aid[17] __attribute__ ((aligned (1)));

extern int gpg_get_pw1_lifetime (void);

extern void flash_bool_clear (const uint8_t **addr_p);
extern const uint8_t *flash_bool_write (uint8_t nr);
extern int flash_cnt123_get_value (const uint8_t *p);
extern void flash_cnt123_increment (uint8_t which, const uint8_t **addr_p);
extern void flash_cnt123_clear (const uint8_t **addr_p);
extern void flash_put_data (uint16_t hw);
extern void flash_warning (const char *msg);
