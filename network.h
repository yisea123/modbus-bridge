#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdint.h>

int8_t network_init(void);
int8_t network_telnetsrv_start(void);
int8_t network_telnetsrv_stop(void);

#endif /* __NETWORK_H__ */
