#ifndef HTABLE_H_INCLUDED
#define HTABLE_H_INCLUDED

#include "errors.h"

// comment this out to disable propagating matched buckets up
//#define HTABLE_BOOST_GETS
#define HTABLE_BOOST_MINIMAL_LENGTH  (2)
// comment this out to disable statistics collecting
//#define HTABLE_COLLECT_STATS

#ifdef HTABLE_COLLECT_STATS
#define HTABLE_HISTOGRAM_SIZE   (10)
#endif

typedef uint64_t htable_key_t;
typedef uint16_t htable_value_t;

typedef struct _htable_bucket_t {
	htable_key_t      key;
	htable_value_t    value;
	int               next;
} htable_bucket_t;

typedef struct _htable_t {
	int               size;
	int               count;
	int *             heads;
	htable_bucket_t * buckets;
	int               next_free_index;
    #ifdef HTABLE_COLLECT_STATS
	int               total_gets;
	int               total_get_boosts;
	int               get_histogram[HTABLE_HISTOGRAM_SIZE];
	int               total_get_fails;
	int               total_sets;
	int               total_set_replaces;
	int               set_histogram[HTABLE_HISTOGRAM_SIZE];
	int               total_set_collisions;
	int               total_set_fails;
    #endif
} htable_t;

int htable_init(htable_t * self, int size);
int htable_fini(htable_t * self);
int htable_get(htable_t * self, int hash, htable_key_t key, htable_value_t * value);
int htable_contains(htable_t * self, int hash, htable_key_t key);
int htable_set(htable_t * self, int hash, htable_key_t key, htable_key_t value);
#ifdef HTABLE_COLLECT_STATS
void htable_print_stats(htable_t * self);
#endif

#endif // HTABLE_H_INCLUDED
