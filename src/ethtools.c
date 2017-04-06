/*
 * src/ethtools.c
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <linux/sockios.h>
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <net/if_arp.h>

#include <include/log.h>
#include <include/ethtools.h>


#define DEFAULT_ETH     "eth0"

int get_ipaddr(const char* eth, char* ipaddr)
{
    int i = 0;
    int sockfd;
    struct ifconf ifconf;
    char buf[512];
    struct ifreq *ifreq;
    char *dev = (char *)eth;

    if(!dev) {
        dev = DEFAULT_ETH;
    }

    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0) {
        perror("socket");
        exit(1);
    }

    ioctl(sockfd, SIOCGIFCONF, &ifconf);
    ifreq = (struct ifreq*)buf;

    for(i=(ifconf.ifc_len/sizeof(struct ifreq)); i>0; i--) {
        if(strcmp(ifreq->ifr_name, dev)==0) {
            strcpy(ipaddr, inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr));
            return 0;
        }

        ifreq++;
    }
    return -EINVAL;
}

int detect_mii(int skfd, const char *ifname)
{
    struct ifreq ifr;
    unsigned short *data, mii_val;

    /* Get the vitals from the interface. */
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0) {
		loge("SIOCGMIIPHY on %s failed: %s\n", ifname, strerror(errno));
		(void) close(skfd);
		return 2;
    }

    data = (unsigned short *)(&ifr.ifr_data);
    //phy_id = data[0];
    data[1] = 1;

    if (ioctl(skfd, SIOCGMIIREG, &ifr) < 0) {
        loge("SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
        return 2;
    }

    mii_val = data[3];

    return(((mii_val & 0x0016) == 0x0004) ? 0 : 1);
}

/*
 * retval: 0 interface link down ,1 interface link up
 **/
int detect_ethtool(int skfd, const char *ifname)
{
    struct ifreq ifr;
    struct ethtool_value edata;

    memset(&ifr, 0, sizeof(ifr));
    edata.cmd = ETHTOOL_GLINK;
  	edata.data = 0;
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name)-1);
    ifr.ifr_data = (char *) &edata;

    if (ioctl(skfd, SIOCETHTOOL, &ifr) == -1) {
        loge("ETHTOOL_GLINK failed: %s\n", strerror(errno));
        return 2;
    }

    return (edata.data ? 0 : 1);
}


int detect_eth_status(const char * ifname)
{  
	int skfd = -1;
	int retval;

    /* Open a socket. */
	if (( skfd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
		loge("socket error\n"); //and return -1
		return -1;
	}

	retval = detect_ethtool(skfd, ifname); //check interface link

    /* interface link error */
	if (retval == 2) {
		retval = detect_mii(skfd, ifname);//check interface link
	}

	close(skfd);  // get the retval and close the socket 
	return retval;
}


/*
*description: string to ip address.
*
*@arg1:ip address string.
*
*return:ipaddr.
*/
uint32_t str_to_ipaddr(const char* ipaddr)
{
	return inet_addr(ipaddr);
}

/*
* get this machine ip address.
* arg1: out param, local ip buffer.
* ret: local ip buffer.
*/
char* get_local_ip(_out char* ipaddr)
{
	get_ipaddr(DEFAULT_ETH, ipaddr);
	return ipaddr;
}

/*
*arg1:eth name
*arg2:timeout second. unit:second
*ret:-1:timeout;0:success
*
*/
static int wait_eth_up_timeout(const char *ifname, int seconds)
{
	int i_tmp = 0;
	int eth_state = -1;
	logi("wait for network start.\n");
	while(1)
	{
		eth_state = detect_eth_status(ifname);
		if(eth_state == 0)
		{
			break;
		}

		if(seconds && i_tmp++ == seconds)
		{
			return -1;
		}		
		usleep(1000*1000);
	}
	logi("network normal starting.\n");
	return 0;
}


/*
* wait until the network is ready.
* 
*ret:-1:timeout;0:success
*/
int wait_for_network_ready(void)
{
	return wait_eth_up_timeout(DEFAULT_ETH, 0);
}
/*
*wait the network is ready.
*arg1:timeout second. unit:second
*ret:-1:timeout;0:success
*/
int wait_for_network_ready_timeout(int seconds)
{
	return wait_eth_up_timeout(DEFAULT_ETH, seconds);
}


