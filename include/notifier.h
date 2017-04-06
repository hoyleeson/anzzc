/*
 * include/notifier.h
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#ifndef _ANZZC_NOTIFIER_H
#define _ANZZC_NOTIFIER_H

#include <pthread.h>

struct notifier_block {
	int (*notifier_call)(struct notifier_block *, unsigned long, void *);
	struct notifier_block *next;
	int priority;
};

struct notifier_head {
	pthread_rwlock_t lock;
	struct notifier_block *head;
};

#define NOTIFIER_INIT(name) { 	\
		PTHREAD_RWLOCK_INITIALIZER,\
		NULL,\
}

/*notifier_head initilizer macro*/
#define NOTIFIER_HEAD(name) 		\
		struct notifier_head name = \
	NOTIFIER_INIT(name)

#define NOTIFY_DONE		0x0000		/* Don't care */
#define NOTIFY_OK		0x0001		/* Suits me */
#define NOTIFY_STOP_MASK	0x8000		/* Don't call further */
#define NOTIFY_BAD		(NOTIFY_STOP_MASK|0x0002)

#ifdef __cplusplus
extern "C" {
#endif


int notifier_chain_register(struct notifier_head *nh, struct notifier_block *n);
int notifier_chain_cond_register(struct notifier_head *nh, struct notifier_block *n);
int notifier_chain_unregister(struct notifier_head *nh, struct notifier_block *n);

int notifier_call_chain_nr(struct notifier_head *nh, unsigned long val, void *v,
					int nr_to_call, int *nr_calls);
int notifier_call_chain(struct notifier_head *nh, unsigned long val, void *v);

#ifdef __cplusplus
}
#endif


#endif

