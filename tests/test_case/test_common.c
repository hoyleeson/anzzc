#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <include/list.h>
#include <include/log.h>
#include <include/configs.h>
#include <include/workqueue.h>


struct test_list_st
{
	int num;
	struct list_head node;
};

int test_list(int argc, char **argv)
{
	int i;
	struct test_list_st *tlist, *n;
	LIST_HEAD(list);

	for(i=0; i<10; i++) {
		tlist = malloc(sizeof(*tlist));
		tlist->num = i;
		list_add(&tlist->node, &list);	
	}
	list_for_each_entry_safe(tlist, n, &list, node) {
		printf("%d ", tlist->num);
		list_del(&tlist->node);
		free(tlist);
	}
	printf("\n");

	if(list_empty(&list))
		return 0;
	return -1;
}

int test_configs(int argc, char **argv)
{
	init_configs("configs/configs.conf");

	exec_commands();
	exec_daemons();
	return 0;
}

struct wq_test
{
    struct work_struct work;
    int val;
    int retval;
};

static void handle_work(struct work_struct *work)
{
    struct wq_test *twq; 

    twq = container_of(work, struct wq_test, work); 

    printf("workqueue handle, val:%d\n", twq->val);
    twq->retval = twq->val;
}

int test_workqueue(int argc, char **argv)
{
    int ret;
    struct workqueue_struct *wq;
    struct wq_test twq; 

    init_workqueues();
    wq = create_workqueue();

    INIT_WORK(&twq.work, handle_work);
    twq.val = 35;
    twq.retval = 0;

    queue_work(wq, &twq.work);
    sleep(1);
    ret = !(twq.val == twq.retval);

    printf("workqueue test %s.\n", ret ? "failed" : "success");
    return ret;
}


struct timer_test {
    int val;
    int retval;
};

static void handle_timer(unsigned long val) 
{
    struct timer_test *tt = (struct timer_test *)val;

    printf("timer handle, val:%d\n", tt->val);
    tt->retval = tt->val;
}

int test_timer(int argc, char **argv)
{
    int ret;
    struct timer_list timer;
    struct timer_test tt;

    tt.val = 18;
    tt.retval = 0;

    init_timers();

    init_timer(&timer);
    setup_timer(&timer, handle_timer, (unsigned long)&tt);
    mod_timer(&timer, curr_time_ms() + 3*MSEC_PER_SEC);
    sleep(4);

    ret = !(tt.val == tt.retval);
    printf("timer test %s.\n", ret ? "failed" : "success");
    return ret;
}

