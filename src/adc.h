extern chopstx_mutex_t adc_mtx;
extern chopstx_cond_t adc_cond;
extern int adc_waiting;
extern int adc_data_available;

void adc_init (void);
void adc_start (void);
void adc_stop (void);

#define ADC_SAMPLE_MODE 0
#define ADC_CRC32_MODE       1
void adc_start_conversion (int mode, uint32_t *p, int size);
void adc_wait (chopstx_intr_t *intr);
