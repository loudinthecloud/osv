#ifndef OSV_IOCTL_H
#define OSV_IOCTL_H

#include <sys/ioctl.h>

/* Generic file-descriptor ioctl's. */
#define	FIOSETOWN	_IOW('f', 124, int)	/* set owner */
#define	FIOGETOWN	_IOR('f', 123, int)	/* get owner */
#define	FIONWRITE	_IOR('f', 119, int)	/* get # bytes (yet) to write */
#define	FIONSPACE	_IOR('f', 118, int)	/* get space in send queue */

/* Socket ioctl's. */
#define	SIOCATMARK	 _IOR('s',  7, int)		/* at oob mark? */
#define	SIOCSPGRP	 _IOW('s',  8, int)		/* set process group */
#define	SIOCGPGRP	 _IOR('s',  9, int)		/* get process group */

#define	SIOCAIFADDR	 _IOW('i', 26, struct ifaliasreq)/* add/chg IF alias */
#define	SIOCALIFADDR	 _IOW('i', 27, struct if_laddrreq) /* add IF addr */
#define	SIOCGLIFADDR	_IOWR('i', 28, struct if_laddrreq) /* get IF addr */
#define	SIOCDLIFADDR	 _IOW('i', 29, struct if_laddrreq) /* delete IF addr */
#define	SIOCSIFCAP	 _IOW('i', 30, struct ifreq)	/* set IF features */
#define	SIOCGIFCAP	_IOWR('i', 31, struct ifreq)	/* get IF features */
#define	SIOCGIFMAC	_IOWR('i', 38, struct ifreq)	/* get IF MAC label */
#define	SIOCSIFMAC	 _IOW('i', 39, struct ifreq)	/* set IF MAC label */
#define	SIOCSIFDESCR	 _IOW('i', 41, struct ifreq)	/* set ifnet descr */
#define	SIOCGIFDESCR	_IOWR('i', 42, struct ifreq)	/* get ifnet descr */

#define	SIOCGIFPHYS	_IOWR('i', 53, struct ifreq)	/* get IF wire */
#define	SIOCSIFPHYS	 _IOW('i', 54, struct ifreq)	/* set IF wire */
#define	SIOCSIFMEDIA	_IOWR('i', 55, struct ifreq)	/* set net media */
#define	SIOCGIFMEDIA	_IOWR('i', 56, struct ifmediareq) /* get net media */
#define	OSIOCGIFCONF	_IOWR('i', 20, struct ifconf)	/* get ifnet list */

#define	SIOCSIFGENERIC	 _IOW('i', 57, struct ifreq)	/* generic IF set op */
#define	SIOCGIFGENERIC	_IOWR('i', 58, struct ifreq)	/* generic IF get op */

#define	SIOCGIFSTATUS	_IOWR('i', 59, struct ifstat)	/* get IF status */
#define	SIOCSIFLLADDR	 _IOW('i', 60, struct ifreq)	/* set linklevel addr */

#define	SIOCSIFPHYADDR	 _IOW('i', 70, struct ifaliasreq) /* set gif addres */
#define	SIOCGIFPSRCADDR	_IOWR('i', 71, struct ifreq)	/* get gif psrc addr */
#define	SIOCGIFPDSTADDR	_IOWR('i', 72, struct ifreq)	/* get gif pdst addr */
#define	SIOCDIFPHYADDR	 _IOW('i', 73, struct ifreq)	/* delete gif addrs */
#define	SIOCSLIFPHYADDR	 _IOW('i', 74, struct if_laddrreq) /* set gif addrs */
#define	SIOCGLIFPHYADDR	_IOWR('i', 75, struct if_laddrreq) /* get gif addrs */

#define	SIOCGIFFIB	_IOWR('i', 92, struct ifreq)	/* get IF fib */
#define	SIOCSIFFIB	 _IOW('i', 93, struct ifreq)	/* set IF fib */

#define	SIOCIFCREATE	_IOWR('i', 122, struct ifreq)	/* create clone if */
#define	SIOCIFCREATE2	_IOWR('i', 124, struct ifreq)	/* create clone if */
#define	SIOCIFDESTROY	 _IOW('i', 121, struct ifreq)	/* destroy clone if */
#define	SIOCIFGCLONERS	_IOWR('i', 120, struct if_clonereq) /* get cloners */

#define	SIOCAIFGROUP	 _IOW('i', 135, struct ifgroupreq) /* add an ifgroup */
#define	SIOCGIFGROUP	_IOWR('i', 136, struct ifgroupreq) /* get ifgroups */
#define	SIOCDIFGROUP	 _IOW('i', 137, struct ifgroupreq) /* delete ifgroup */
#define	SIOCGIFGMEMB	_IOWR('i', 138, struct ifgroupreq) /* get members */

#endif /* !OSV_IOCTL_H */
