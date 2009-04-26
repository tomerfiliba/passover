/*
 * High Performance Time
 */

#ifndef HPTIME_H_INCLUDED
#define HPTIME_H_INCLUDED

#include <stdint.h>
#include "errors.h"


typedef uint64_t usec_t;
#define HPTIME_SYNC_INTERVAL    (1000000)

errcode_t hptime_init(void);
errcode_t hptime_fini(void);
usec_t hptime_get_time(void);

#endif /* HPTIME_H_INCLUDED */
