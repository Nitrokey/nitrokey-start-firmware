extern chopstx_mutex_t adc_mtx;
extern chopstx_cond_t adc_cond;
extern int adc_waiting;
extern int adc_data_available;

void adc_init (void);
void adc_start (void);
void adc_stop (void);

#define ADC_SAMPLE_MODE 0
#define ADC_CRC32_MODE  1

extern uint32_t adc_buf[64];

void adc_start_conversion (int offset, int count);
int adc_wait_completion (chopstx_intr_t *intr);
