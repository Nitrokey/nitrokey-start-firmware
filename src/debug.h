struct stdout {
  chopstx_mutex_t m;
  /**/
  chopstx_mutex_t m_dev;
  chopstx_cond_t cond_dev;
  uint8_t connected;
};

extern struct stdout stdout;
