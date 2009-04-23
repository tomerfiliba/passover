#ifndef PASSOVER_ERRORS_H_INLCUDED
#define PASSOVER_ERRORS_H_INLCUDED

#include <stdlib.h>
#include <stdio.h>

typedef enum {
	ERR_UNKNWON = -1,
	ERR_SUCCESS = 0,
	#define ERROR_DEF(err) err,
	#include "errors.xh"
	#undef ERROR_DEF
	MAX_ERROR_CODE
} errcode_t;

#define OUT
#define IS_ERROR(code)       ((code) != ERR_SUCCESS)
#define RETURN_SUCCESSFUL    return ERR_SUCCESS
#define PROPAGATE(expr) \
	{errcode_t code = expr; if (IS_ERROR(code)) return code;}
#define PROPAGATE_TO(label, expr) \
	{errcode_t code = expr; if (IS_ERROR(code)) goto label;}
#define ASSERT(expr) \
	{ \
	errcode_t code = expr; \
	if (IS_ERROR(code)) { \
		printf("%s(%d): %s\n    %s\n", __FILE__, __LINE__, #expr, errcode_get_name(code)); \
		abort(); \
	} \
	}

const char * errcode_get_name(errcode_t code);


#endif // PASSOVER_ERRORS_H_INLCUDED
