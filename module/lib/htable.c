#include <stdint.h>
#include <malloc.h>

#include "htable.h"

//#define HTABLE_BOOST_GETS
//#define HTABLE_BOOST_MINIMAL_LENGTH  (2)
//#define HTABLE_COLLECT_STATS


errcode_t htable_init(htable_t * self, int size)
{
	int i;

	if (size <= 0) {
		return ERR_HTABLE_INVALID_SIZE;
	}
	self->size = size;
	self->count = 0;
	self->next_free_index = 0;

	#ifdef HTABLE_COLLECT_STATS
	self->total_gets = 0;
	self->total_get_boosts = 0;
	for (i = 0; i < HTABLE_HISTOGRAM_SIZE; i++) {
		self->get_histogram[i] = 0;
		self->set_histogram[i] = 0;
	}
	self->total_get_fails = 0;
	self->total_sets = 0;
	self->total_set_replaces = 0;
	self->total_set_collisions = 0;
	self->total_set_fails = 0;
	#endif

	self->heads = (int*)malloc(sizeof(int) * size);
	if (self->heads == NULL) {
		return ERR_HTABLE_MALLOC_FAILED;
	}

	self->buckets = (htable_bucket_t*)malloc(sizeof(htable_bucket_t) * size);
	if (self->buckets == NULL) {
		free(self->heads);
		self->heads = NULL;
		return ERR_HTABLE_MALLOC_FAILED;
	}
	for (i = 0; i < size; i++) {
		self->heads[i] = -1;
		self->buckets[i].next = -1;
	}

	RETURN_SUCCESSFUL;
}

void htable_print_stats(htable_t * self);

errcode_t htable_fini(htable_t * self)
{
	if (self->heads != NULL) {
		#ifdef HTABLE_COLLECT_STATS
		htable_print_stats(self);
		#endif

		free(self->heads);
		self->heads = NULL;
	}
	if (self->buckets != NULL) {
		free(self->buckets);
		self->buckets = NULL;
	}
	RETURN_SUCCESSFUL;
}

inline errcode_t htable_get(htable_t * self, int hash, htable_key_t key,
		OUT htable_value_t * value)
{
	int size = self->size;
	int index = self->heads[((unsigned)hash) % size];
	htable_bucket_t * bucket;
	#ifdef HTABLE_BOOST_GETS
	htable_bucket_t * last_bucket = NULL;
	#endif
	#if defined(HTABLE_COLLECT_STATS) || defined(HTABLE_BOOST_GETS)
	int length = 0;
	#endif

	while (index >= 0) {
		bucket = &self->buckets[index];
		if (bucket->key == key) {
			#ifdef HTABLE_COLLECT_STATS
			int histindex = length;
			if (histindex >= HTABLE_HISTOGRAM_SIZE) {
				histindex = HTABLE_HISTOGRAM_SIZE - 1;
			}
			self->get_histogram[histindex] += 1;
			self->total_gets += 1;
			#endif

			#ifdef HTABLE_BOOST_GETS
			if (last_bucket != NULL && length >= HTABLE_BOOST_MINIMAL_LENGTH) {
				// in order to make finding this key faster next time, lift
				// it up one spot in the linked-list (last_bucket != NULL
				// means it is not the head already)
				#ifdef HTABLE_COLLECT_STATS
				self->total_get_boosts += 1;
				#endif
				*value = bucket->value;
				bucket->key = last_bucket->key;
				bucket->value = last_bucket->key;
				last_bucket->key = key;
				last_bucket->value = *value;
				RETURN_SUCCESSFUL;
			}
			#endif
			*value = bucket->value;
			RETURN_SUCCESSFUL;
		}
		#ifdef HTABLE_BOOST_GETS
		last_bucket = bucket;
		#endif
		#if defined(HTABLE_COLLECT_STATS) || defined(HTABLE_BOOST_GETS)
		length += 1;
		#endif
		index = bucket->next;
	}

	#ifdef HTABLE_COLLECT_STATS
	self->total_get_fails += 1;
	#endif
	return ERR_HTABLE_GET_KEY_MISSING;
}

inline int htable_contains(htable_t * self, int hash, htable_key_t key)
{
	htable_value_t v;
	return (htable_get(self, hash, key, &v) == 0) ? 1 : 0;
}

static inline errcode_t _htable_add(htable_t * self, htable_key_t key, htable_value_t value, OUT int * outindex)
{
	int index = self->next_free_index;

	if (self->count >= self->size) {
		#ifdef HTABLE_COLLECT_STATS
		self->total_set_fails += 1;
		#endif
		return ERR_HTABLE_TABLE_FULL;
	}
	#ifdef HTABLE_COLLECT_STATS
	self->total_sets += 1;
	#endif
	self->next_free_index += 1;
	self->count += 1;
	self->buckets[index].key = key;
	self->buckets[index].value = value;

	*outindex = index;
	RETURN_SUCCESSFUL;
}

inline errcode_t htable_set(htable_t * self, int hash, htable_key_t key,
		htable_key_t value)
{
	int size = self->size;
	htable_bucket_t * bucket;
	int head_index = ((unsigned)hash) % size;
	int index = self->heads[head_index];

	if (index < 0) {
		// no collision -- this bucket is free
		PROPAGATE(_htable_add(self, key, value, &index));
		self->heads[head_index] = index;
	}
	else {
		// a collision -- find the terminal bucket. if we find that the key
		// already exists, just replace the value
		#ifdef HTABLE_COLLECT_STATS
		int length = 0;
		#endif

		while (index >= 0) {
			bucket = &self->buckets[index];
			if (key == bucket->key) {
				// replace existing key
				#ifdef HTABLE_COLLECT_STATS
				self->total_set_replaces += 1;
				#endif
				bucket->value = value;
				RETURN_SUCCESSFUL;
			}
			index = bucket->next;
			#ifdef HTABLE_COLLECT_STATS
			length += 1;
			#endif
		}

		// we have found the terminal bucket
		PROPAGATE(_htable_add(self, key, value, &index));

		#ifdef HTABLE_COLLECT_STATS
		self->total_set_collisions += 1;
		#endif
		bucket->next = index;

		#ifdef HTABLE_COLLECT_STATS
		int histindex = length;
		if (histindex >= HTABLE_HISTOGRAM_SIZE) {
			histindex = HTABLE_HISTOGRAM_SIZE - 1;
		}
		self->set_histogram[histindex] += 1;
		#endif
	}

	RETURN_SUCCESSFUL;
}

#ifdef HTABLE_COLLECT_STATS
#include <stdio.h>

void htable_print_stats(htable_t * self)
{
	int i;
	printf("statistics for htable %p (size = %d, count = %d)\n", self, self->size, self->count);
	printf("  gets:\n");
	printf("    successful = %d\n", self->total_gets);
	printf("    failed = %d\n", self->total_get_fails);
	printf("    boosts = %d\n", self->total_get_boosts);
	printf("    histogram:\n");
	for (i = 0; i < HTABLE_HISTOGRAM_SIZE - 1; i++) {
		printf("      (%d)     %d\n", i, self->get_histogram[i]);
	}
	printf("      (%d+)    %d\n", i, self->get_histogram[i]);
	printf("  sets:\n");
	printf("    insertions = %d\n", self->total_sets);
	printf("    replaces = %d\n", self->total_set_replaces);
	printf("    failed = %d\n", self->total_set_fails);
	printf("    collisions = %d\n", self->total_set_collisions);
	for (i = 0; i < HTABLE_HISTOGRAM_SIZE - 1; i++) {
		printf("      (%d)     %d\n", i, self->set_histogram[i]);
	}
	printf("      (%d+)    %d\n", i, self->set_histogram[i]);
}
#endif

/*
int main()
{
	htable_t ht;
	htable_value_t v;

	ASSERT(htable_init(&ht, 65535));
	if (htable_get(&ht, 771622341, 771622341, &v) != ERR_HTABLE_GET_KEY_MISSING) {
		printf("nonexistent key was found!\n");
		abort();
	}
	ASSERT(htable_set(&ht, 771622341, 771622341, 1));
	ASSERT(htable_set(&ht, 771622343, 771622343, 2));
	ASSERT(htable_set(&ht, 771622343, 771622343, 3));
	ASSERT(htable_set(&ht, 771622341, 771622345, 4));
	ASSERT(htable_get(&ht, 771622341, 771622341, &v));
	printf("v1 = %d\n", v);
	ASSERT(htable_get(&ht, 771622343, 771622343, &v));
	printf("v2 = %d\n", v);
	ASSERT(htable_get(&ht, 771622341, 771622345, &v));
	printf("v3 = %d\n", v);
	ASSERT(htable_get(&ht, 771622341, 771622345, &v));
	printf("v3 = %d\n", v);
	ASSERT(htable_get(&ht, 771622341, 771622345, &v));
	printf("v3 = %d\n", v);
	ASSERT(htable_fini(&ht));

	return 0;
}
*/







