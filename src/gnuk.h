extern Thread *blinker_thread;

extern void put_byte (uint8_t b);
extern void put_short (uint16_t x);
extern void put_string (const char *s);

extern void _write (const char *, int);

extern size_t strlen(const char *s);
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);

/*
 * Interface between ICC<-->GPG
 */
extern Thread *icc_thread;
extern Thread *gpg_thread;

#define USB_BUF_SIZE 64

#define EV_EXEC_FINISHED (eventmask_t)2	 /* GPG Execution finished */

/* maximum cmd apdu data is key import 22+4+128+128 (head, e, p, q)*/
#define MAX_CMD_APDU_SIZE (7+282) /* header + data */

#define MAX_RES_APDU_SIZE (256+2) /* Data + status */
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

extern int ac_check_status (uint8_t ac_flag);
extern int verify_pso_cds (uint8_t *pw, int pw_len);
extern int verify_pso_other (uint8_t *pw, int pw_len);
extern int verify_pso_admin (uint8_t *pw, int pw_len);


extern void write_res_apdu (const uint8_t *p, int len,
			    uint8_t sw1, uint8_t sw2);

extern int gpg_do_table_init (void);
extern void gpg_do_get_data (uint16_t tag);
extern void gpg_do_put_data (uint16_t tag, uint8_t *data, int len);

extern uint8_t * flash_do_write (uint16_t tag, uint8_t *data, int len);
