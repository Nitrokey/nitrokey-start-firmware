#include "config.h"
#include "ch.h"
#include "gnuk.h"

/*
 * XXX: I have tried havege_rand, but it requires too much memory...
 */

/*
 * Multiply-with-carry method by George Marsaglia
 */
static uint32_t m_w;
static uint32_t m_z;

uint32_t
get_random (void)
{
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);

  return (m_z << 16) + m_w;
}

void
random_init (void)
{
  static uint8_t s = 0;

 again:
  if ((s & 1))
    m_w = (m_w << 8) ^ hardclock ();
  else
    m_z = (m_z << 8) ^ hardclock ();

  s++;
  if (m_w == 0 || m_z == 0)
    goto again;
}

uint8_t dek[16];
uint8_t *get_data_encryption_key (void)
{
  uint32_t r;
  r = get_random ();
  memcpy (dek, &r, 4);
  r = get_random ();
  memcpy (dek+4, &r, 4);
  r = get_random ();
  memcpy (dek+8, &r, 4);
  r = get_random ();
  memcpy (dek+12, &r, 4);
  return dek;
}

void
dek_free (uint8_t *dek)
{
  (void)dek;
}
