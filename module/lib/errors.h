#ifndef PASSOVER_ERRORS_H_INLCUDED
#define PASSOVER_ERRORS_H_INLCUDED

#include <stdlib.h>
#include <stdio.h>

typedef enum {
	ERR_UNKNOWN = -1,
	ERR_SUCCESS = 0,
	#define ERROR_DEF(err) err,
	#include "errors.xh"
	#undef ERROR_DEF
	MAX_ERROR_CODE
} errcode_t;

#define OUT
#define IS_ERROR(__code)       ((__code) != ERR_SUCCESS)
#define RETURN_SUCCESSFUL    return ERR_SUCCESS
#define PROPAGATE(__expr) \
	{errcode_t __code = __expr; if (IS_ERROR(__code)) return __code;}
#define PROPAGATE_TO(__label, __expr) \
	{errcode_t __code = __expr; if (IS_ERROR(__code)) goto __label;}
#define ASSERT(__expr) \
	{ \
	errcode_t __code = __expr; \
	if (IS_ERROR(__code)) { \
		printf("%s(%d): %s\n    %s\n", __FILE__, __LINE__, #__expr, errcode_get_name(__code)); \
		abort(); \
	} \
	}

const char * errcode_get_name(errcode_t code);


#endif // PASSOVER_ERRORS_H_INLCUDED
