#include <porting/netport.h>
#include <porting/callout.h>

int callout_reset_on(struct callout *a, int b, void (*c)(void *), void *d, int e)
{
    return 1;
}

int _callout_stop_safe(struct callout *a, int b)
{
    return 1;
}
