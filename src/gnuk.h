extern Thread *blinker_thread;

extern void put_byte (uint8_t b);

extern void _write (const char *, int);

extern size_t strlen(const char *s);
extern void *memcpy(void *dest, const void *src, size_t n);

/*
 * Interface between ICC<-->GPG
 */
extern Thread *icc_thread;
extern Thread *gpg_thread;

#define USB_BUF_SIZE 64

#define EV_EXEC_FINISHED (eventmask_t)2	 /* GPG Execution finished */

#define MAX_CMD_APDU_SIZE (256)	/* XXX: Check OpenPGPcard protocol */
#define MAX_RES_APDU_SIZE (256+2) /* Data + status */
extern uint8_t cmd_APDU[MAX_CMD_APDU_SIZE];
extern uint8_t res_APDU[MAX_RES_APDU_SIZE];
extern int cmd_APDU_size;
extern int res_APDU_size;
