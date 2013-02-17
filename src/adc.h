#define ADC_DATA_AVAILABLE ((eventmask_t)1)
extern Thread *rng_thread;

void adc_init (void);
void adc_start (void);
void adc_stop (void);

#define ADC_SAMPLE_MODE 0
#define ADC_CRC32_MODE       1
void adc_start_conversion (int mode, uint32_t *p, int size);
