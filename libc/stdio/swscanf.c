#define _GNU_SOURCE // avoid __REDIRECT(swscanf) in glibc headers
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include "libc.h"

int swscanf(const wchar_t *restrict s, const wchar_t *restrict fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vswscanf(s, fmt, ap);
	va_end(ap);
	return ret;
}

weak_alias(swscanf,__isoc99_swscanf);
