#ifndef _DEBUG_H
#define _DEBUG_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS
void debug(const char *msg);
void debug_write(const char *msg, size_t len);

int vkprintf(const char *__restrict fmt, va_list ap)
	__attribute__((format(printf, 1, 0)));
int kprintf(const char *__restrict fmt, ...)
	__attribute__((format(printf, 1, 2)));
__END_DECLS

#endif /* _DEBUG_H */
