extern Thread *blinker_thread;
extern Thread *icc_thread;
extern Thread *gpg_thread;

#define EV_EXEC_FINISHED (eventmask_t)2	 /* GPG Execution finished */

#define put_string(STR)

extern void _write (char *, int);
