#ifndef TRACER_H_INCLUDED
#define TRACER_H_INCLUDED

#include "../lib/htable.h"
#include "../lib/listfile.h"
#include "../lib/rotdir.h"
#include "../lib/rotrec.h"


typedef struct {
	int        depth;
	rotrec_t   records;
	listfile_t codepoints;
	htable_t   table;
} tracer_t;


#endif // TRACER_H_INCLUDED
