#include "errors.h"


static char * errcode_names[] = {
	"SUCCESS",
	#define ERROR_DEF(err) #err ,
	#include "errors.xh"
	#undef ERROR_DEF
};

const char * errcode_get_name(errcode_t code)
{
	if (code < 0 || code >= MAX_ERROR_CODE) {
		return "<error code out of range>";
	}
	return errcode_names[code];
}


