#ifndef __CACHE_LOG_HH__
#define __CACHE_LOG_HH__

#include <syslog.h>

#define EMPTY		-1
#define UNAVAILABLE	-2
#define FATALERROR	-3

#define logErr(...) \
	do{\
		fprintf(stderr,"!!!Error!!! %s",__PRETTY_FUNCTION__);\
		fprintf(stderr,__VA_ARGS__);\
		exit(-1);\
	}while(0)

/*
#define logFunc(...) \
	do{\
		syslog(LOG_DEBUG,"%s",__PRETTY_FUNCTION__);\
		syslog(LOG_DEBUG,__VA_ARGS__);\
	}while(0)
*/
#define logFunc(...) \
	do{\
		if(0)fprintf(stderr,"%s",__PRETTY_FUNCTION__);\
		if(0)fprintf(stderr,__VA_ARGS__);\
	}while(0)
#endif
