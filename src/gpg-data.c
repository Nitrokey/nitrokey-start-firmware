#include "config.h"
#include "ch.h"
#include "gnuk.h"

const char const select_file_TOP_result[] __attribute__ ((aligned (1))) = {
  0x00, 0x00,			/* unused */
  0x0b, 0x10,			/* number of bytes in this directory */
  0x3f, 0x00,			/* field of selected file: MF, 3f00 */
  0x38,			/* it's DF */
  0xff,			/* unused */
  0xff,	0x44, 0x44,	/* access conditions */
  0x01,			/* status of the selected file (OK, unblocked) */
  0x05,			/* number of bytes of data follow */
    0x03,			/* Features: unused */
    0x01,			/* number of subdirectories (OpenPGP) */
    0x01,			/* number of elementary files (SerialNo) */
    0x00,			/* number of secret codes */
    0x00,			/* Unused */
  0x00, 0x00		/* PIN status: OK, PIN blocked?: No */
};

const char const do_5e[] __attribute__ ((aligned (1))) =  {
  6,
  'g', 'n', 'i', 'i', 'b', 'e'
};

const char const do_5b[] __attribute__ ((aligned (1))) = {
  12,
  'N', 'I', 'I', 'B', 'E', ' ', 'Y', 'u', 't', 'a', 'k', 'a'
};

const char const do_5f2d[] __attribute__ ((aligned (1))) = {
  2,
  'j', 'a'
};

const char const do_5f35[] __attribute__ ((aligned (1))) = {
  1,
  '1'
};

const char const do_5f50[] __attribute__ ((aligned (1))) = {
  20,
  'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
  '.', 'f', 's', 'i', 'j', '.', 'o', 'r', 'g', '/'
};

const char const get_data_rb_result[] __attribute__ ((aligned (1))) = {
  0x5a, 0x4, 0x01, 0x02, 0x03, 0x04
};
