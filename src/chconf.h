/* ChibiOS/RT configuration file */

#ifndef _CHCONF_H_
#define _CHCONF_H_

#define CH_FREQUENCY                    1000
#define CH_TIME_QUANTUM                 20
#define CH_USE_NESTED_LOCKS             FALSE
#define CH_MEMCORE_SIZE                 0 /* Whole RAM */
#define CH_OPTIMIZE_SPEED               TRUE
#define CH_USE_REGISTRY                 TRUE
#define CH_USE_WAITEXIT                 TRUE
#define CH_USE_SEMAPHORES               FALSE
#define CH_USE_SEMAPHORES_PRIORITY      FALSE
#define CH_USE_SEMSW                    FALSE
#define CH_USE_MUTEXES                  TRUE
#define CH_USE_CONDVARS                 TRUE
#define CH_USE_CONDVARS_TIMEOUT         TRUE
#define CH_USE_EVENTS                   TRUE /* We use this! */
#define CH_USE_EVENTS_TIMEOUT           TRUE /* We use this! */
#define CH_USE_MESSAGES                 TRUE
#define CH_USE_MESSAGES_PRIORITY        FALSE
#define CH_USE_MAILBOXES                FALSE
#define CH_USE_QUEUES                   FALSE
#define CH_USE_MEMCORE                  TRUE
#define CH_USE_HEAP                     TRUE
#define CH_USE_MALLOC_HEAP              FALSE
#define CH_USE_MEMPOOLS                 TRUE
#define CH_USE_DYNAMIC                  FALSE

/* Debug options */
#define CH_DBG_ENABLE_CHECKS            FALSE
#define CH_DBG_ENABLE_ASSERTS           FALSE
#define CH_DBG_ENABLE_TRACE             FALSE
#define CH_DBG_ENABLE_STACK_CHECK       FALSE
#define CH_DBG_FILL_THREADS             FALSE
#define CH_DBG_THREADS_PROFILING        TRUE

#define THREAD_EXT_FIELDS                                               \
struct {                                                                \
  /* Add threads custom fields here.*/                                  \
};

#define THREAD_EXT_INIT(tp) {                                           \
  /* Add threads initialization code here.*/                            \
}

#define THREAD_EXT_EXIT(tp) {                                           \
  /* Add threads finalization code here.*/                              \
}

#define IDLE_LOOP_HOOK() {                                              \
  /* Idle loop code here.*/                                             \
}

#endif  /* _CHCONF_H_ */
