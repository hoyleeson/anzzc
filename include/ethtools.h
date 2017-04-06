/*
 * include/ethtools.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_ETHTOOLS_H
#define _ANZZC_ETHTOOLS_H

#include <stdint.h>

#include "types.h"

#define ETHTOOL_GLINK		0x0000000a  /* Get link status (ethtool_value) */

#ifdef __cplusplus
extern "C" {
#endif



/* for passing single values */
struct ethtool_value {
	uint32_t	cmd;
	uint32_t	data;
};


/**
* @brief   get_ip_addr
* 
* Get local ip address.  
* @author Li_Xinhai
* @date 2012-06-26
* @param[in] eth:  network I/F.
* @param[out] ipaddr: local ip address.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
int get_ipaddr(const char* eth, _out char* ipaddr);


/**
* @brief   detect_mii
* 
* Check MII value.  
* @author Li_Xinhai
* @date 2012-06-26
* @param[in] skfd:  network I/F handle.
* @param[in] ifname: network I/F name.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
int detect_mii(int skfd, const char *ifname);


/**
* @brief   detect_ethtool
* 
* detect_ethtool.  
* @author Li_Xinhai
* @date 2012-06-26
* @param[in] skfd:  network I/F handle.
* @param[in] ifname: network I/F name.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
int detect_ethtool(int skfd, const char *ifname);


/**
* @brief   detect_eth_status
* 
* detect_eth_status.  
* @author Li_Xinhai
* @date 2012-06-26
* @param[in] ifname: network I/F name.
* @return int return success or failed
* @retval returns zero on success
* @retval return a non-zero error code if failed
*/
int detect_eth_status(const char * ifname);

/*
*description: string to ip address.
*
*@arg1:ip address string.
*
*return:ipaddr.
*/
uint32_t str_to_ipaddr(const char* ipaddr);

/*
*description: get this machine ip address.
*
*@arg1:out param, local ip buffer.
*
*return:local ip buffer.
*/
char* get_local_ip(_out char* ipaddr);

/*
*description: wait until the network is ready.
*
*return:-1:error;0:success
*/
int wait_for_network_ready(void);

/*
*description: wait the network is ready until timeout.
*@arg1:timeout second. unit:second
*return: -1:timeout;0:success
*/
int wait_for_network_ready_timeout(int seconds);

#ifdef __cplusplus
}
#endif


#endif
