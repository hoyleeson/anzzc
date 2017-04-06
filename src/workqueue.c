/*
 * src/workqueue.c
 * 
 * 2016-01-01  written by Hoyleeson <hoyleeson@gmail.com>
 *	Copyright (C) 2015-2016 by Hoyleeson.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 */

#include <stdlib.h>
#include <pthread.h>

#include <include/timer.h>
#include <include/list.h>
#include <include/wait.h>
#include <include/bitops.h>
#include <include/workqueue.h>
#include <include/compiler.h>
#include <include/fake_atomic.h>

enum {
    /* global_wq flags */
    GWQ_MANAGE_WORKERS = 1 << 0,   /* need to manage workers */
    GWQ_MANAGING_WORKERS   = 1 << 1,   /* managing workers */
    GWQ_HIGHPRI_PENDING = 1 << 2,

    /* worker flags */
    WORKER_STARTED      = 1 << 0,   /* started */
    WORKER_DIE      = 1 << 1,   /* die die die */
    WORKER_IDLE     = 1 << 2,   /* is idle */
    WORKER_PREP     = 1 << 3,   /* preparing to run works */
    WORKER_CPU_INTENSIVE = 1 << 4,  /* cpu intensive */

    WORKER_NOT_RUNNING  = WORKER_PREP | WORKER_CPU_INTENSIVE,

    BUSY_WORKER_HASH_ORDER  = 6,        /* 64 pointers */
    BUSY_WORKER_HASH_SIZE   = 1 << BUSY_WORKER_HASH_ORDER,
    BUSY_WORKER_HASH_MASK   = BUSY_WORKER_HASH_SIZE - 1,

    MAX_IDLE_WORKERS_RATIO  = 4,        /* 1/4 of busy can be idle */

    IDLE_WORKER_TIMEOUT = 300 * MSEC_PER_SEC,
};

/*
 * Structure fields follow one of the following exclusion rules.
 *
 * I: Modifiable by initialization/destruction paths and read-only for
 *    everyone else.
 *
 * P: Preemption protected.  Disabling preemption is enough and should
 *    only be modified and accessed from the local cpu.
 *
 * L: gwq->lock protected.  Access with gwq->lock held.
 *
 * X: During normal operation, modification requires gwq->lock and
 *    should be done only from local cpu.  Either disabling preemption
 *    on local cpu or grabbing gwq->lock is enough for read access.
 *    If GWQ_DISASSOCIATED is set, it's identical to L.
 *
 * F: wq->flush_mutex protected.
 *
 * W: workqueue_lock protected.
 */

struct global_wq;

/*
 * The poor guys doing the actual heavy lifting.  All on-duty workers
 * are either serving the manager role, on idle list or on busy hash.
 */
struct worker {
    int	id;		/* I: worker id */
    /* on idle list while idle, on busy hash table while busy */
    union {
        struct list_head    entry;  /* L: while idle */
        struct hlist_node   hentry; /* L: while busy */
    };

    struct work_struct	*current_work;	/* L: work being processed */
    work_func_t     current_func;   /* L: current_work's fn */
    struct workqueue_struct *current_wq; /* L: current_work's wq */
    struct list_head	scheduled;	/* L: scheduled works XXX*/
    struct global_wq	*gwq;		/* I: the associated gwq */
    /* 64 bytes boundary on 64bit, 32 on 32bit */
    uint64_t last_active;	/* L: last active timestamp */
    unsigned int		flags;		/* X: flags */

    pthread_t 			task;		/* I: worker task */
    wait_queue_head_t   waitq;
};

/*
 * Global workqueue.  There's one and only one for
 * and all works are queued and processed here regardless of their
 * target workqueues.
 */
struct global_wq {
    pthread_mutex_t		lock;		/* the gwq lock */
    struct list_head	worklist;	/* L: list of pending works */
    unsigned int		flags;		/* L: GWQ_* flags */

    int			nr_workers;	/* L: total number of workers */
    int			nr_idle;	/* L: currently idle ones */
    int			nr_running;	/* L: currently running ones */

    /* workers are chained either in the idle_list or busy_hash */
    struct list_head	idle_list;	/* X: list of idle workers */
    struct hlist_head    busy_hash[BUSY_WORKER_HASH_SIZE];
    /* L: hash of busy workers */

    struct timer_list	idle_timer;	/* L: worker idle timeout */

    int		worker_ids;	/* L: for worker IDs */

    struct worker		*first_idle;	/* L: first idle worker */

    struct list_head 	workqueues;
};

/*
 * workqueue.  The lower WORK_STRUCT_FLAG_BITS of
 * work_struct->data are used for flags and thus wqs need to be
 * aligned at two's power of the number of flag bits.
 */
struct workqueue_struct {
    struct global_wq	*gwq;		/* I: the associated gwq */
    unsigned int		flags;		/* I: WQ_* flags */
    struct list_head	list;		/* W: list of all workqueues */

    int			nr_active;	/* L: nr of active works */
    int			max_active;	/* L: max active works */
    struct list_head	delayed_works;	/* L: delayed works */
};

static struct global_wq _global_wq;
static LIST_HEAD(workqueues);
static pthread_mutex_t workqueue_lock = PTHREAD_MUTEX_INITIALIZER;


static void *worker_thread(void *__worker);

static inline struct global_wq *get_global_wq(void)
{
    return &_global_wq;
}


static inline void set_work_wq(struct work_struct *work, 
        struct workqueue_struct *wq, unsigned long extra_flags)
{
    fake_atomic_long_set(&work->data, 
            (long)wq | WORK_STRUCT_PENDING | (extra_flags & WORK_STRUCT_FLAG_MASK));
}


static inline struct workqueue_struct *get_work_wq(struct work_struct *work)
{
    return (void *)(fake_atomic_long_get(&work->data) & WORK_STRUCT_WQ_DATA_MASK);
}


/*
 * Policy functions.  These define the policies on how the global
 * worker pool is managed.  Unless noted otherwise, these functions
 * assume that they're being called with gwq->lock held.
 */
static bool __need_more_worker(struct global_wq *gwq)
{
    return !gwq->nr_running || (gwq->flags & GWQ_HIGHPRI_PENDING);
}

/*
 * Need to wake up a worker?  Called from anything but currently
 * running workers.
 */
static bool need_more_worker(struct global_wq *gwq)
{
    return !list_empty(&gwq->worklist) && __need_more_worker(gwq);
}


/* Return the first worker.  Safe with preemption disabled */
static struct worker *first_worker(struct global_wq *gwq)
{
    if (unlikely(list_empty(&gwq->idle_list)))
        return NULL;

    return list_first_entry(&gwq->idle_list, struct worker, entry);
}


/**
 * wake_up_worker - wake up an idle worker
 * @gwq: gwq to wake worker for
 *
 * Wake up the first idle worker of @gwq.
 *
 * CONTEXT:
 */
static void wake_up_worker(struct global_wq *gwq)
{
    struct worker *worker = first_worker(gwq);

    if (likely(worker))
        wake_up(&worker->waitq);
}


/**
 * worker_set_flags - set worker flags and adjust nr_running accordingly
 * @worker: self
 * @flags: flags to set
 * @wakeup: wakeup an idle worker if necessary
 *
 * Set @flags in @worker->flags and adjust nr_running accordingly.  If
 * nr_running becomes zero and @wakeup is %true, an idle worker is
 * woken up.
 *
 * CONTEXT:
 * pthread_mutex_lock(gwq->lock)
 */
static inline void worker_set_flags(struct worker *worker, unsigned int flags)
{
    struct global_wq *gwq = worker->gwq;

    /*
     * If transitioning into NOT_RUNNING, adjust nr_running and
     * wake up an idle worker as necessary if requested by
     * @wakeup.
     */
    if ((flags & WORKER_NOT_RUNNING) &&
        !(worker->flags & WORKER_NOT_RUNNING)) {

        gwq->nr_running--;
    }

    worker->flags |= flags;
}

/**
 * worker_clr_flags - clear worker flags and adjust nr_running accordingly
 * @worker: self
 * @flags: flags to clear
 *
 * Clear @flags in @worker->flags and adjust nr_running accordingly.
 *
 * CONTEXT:
 * pthread_mutex_lock(gwq->lock)
 */
static inline void worker_clr_flags(struct worker *worker, unsigned int flags)
{
    struct global_wq *gwq = worker->gwq;
    unsigned int oflags = worker->flags;

    worker->flags &= ~flags;

    /*
     * If transitioning out of NOT_RUNNING, increment nr_running.  Note
     * that the nested NOT_RUNNING is not a noop.  NOT_RUNNING is mask
     * of multiple flags, not a single flag.
     */
    if ((flags & WORKER_NOT_RUNNING) && (oflags & WORKER_NOT_RUNNING))
        if (!(worker->flags & WORKER_NOT_RUNNING))
            gwq->nr_running++;
}


/*
 * busy_worker_head - return the busy hash head for a work
 * @gwq: gwq of interest
 * @work: work to be hashed
 *
 * Return hash head of @gwq for @work.
 *
 * CONTEXT:
 * spin_lock_irq(gwq->lock).
 *
 * RETURNS:
 * Pointer to the hash head.
 */
static struct hlist_head *busy_worker_head(struct global_wq *gwq,
        struct work_struct *work)
{
    const int base_shift = ilog2(sizeof(struct work_struct));
    unsigned long v = (unsigned long)work;

    /* simple shift and fold hash, do we need something better? */
    v >>= base_shift;
    v += v >> BUSY_WORKER_HASH_ORDER;
    v &= BUSY_WORKER_HASH_MASK;

    return &gwq->busy_hash[v];
}


/**
 * __find_worker_executing_work - find worker which is executing a work
 * @gwq: gwq of interest
 * @bwh: hash head as returned by busy_worker_head()
 * @work: work to find worker for
 *
 * Find a worker which is executing @work on @gwq.  @bwh should be
 * the hash head obtained by calling busy_worker_head() with the same
 * work.
 *
 * CONTEXT:
 * spin_lock_irq(gwq->lock).
 *
 * RETURNS:
 * Pointer to worker which is executing @work if found, NULL
 * otherwise.
 */
static struct worker *__find_worker_executing_work(struct global_wq *gwq,
        struct hlist_head *bwh,
        struct work_struct *work)
{
    struct worker *worker;
    struct hlist_node *tmp;

    hlist_for_each_entry(worker, tmp, bwh, hentry)
        if (worker->current_work == work &&
                worker->current_func == work->func)
            return worker;
    return NULL;
}


/**
 * find_worker_executing_work - find worker which is executing a work
 * @gwq: gwq of interest
 * @work: work to find worker for
 *
 * Find a worker which is executing @work on @gwq by searching
 * @gwq->busy_hash which is keyed by the address of @work.  For a worker
 * to match, its current execution should match the address of @work and
 * its work function.  This is to avoid unwanted dependency between
 * unrelated work executions through a work item being recycled while still
 * being executed.
 *
 * This is a bit tricky.  A work item may be freed once its execution
 * starts and nothing prevents the freed area from being recycled for
 * another work item.  If the same work item address ends up being reused
 * before the original execution finishes, workqueue will identify the
 * recycled work item as currently executing and make it wait until the
 * current execution finishes, introducing an unwanted dependency.
 *
 * This function checks the work item address, work function and workqueue
 * to avoid false positives.  Note that this isn't complete as one may
 * construct a work function which can introduce dependency onto itself
 * through a recycled work item.  Well, if somebody wants to shoot oneself
 * in the foot that badly, there's only so much we can do, and if such
 * deadlock actually occurs, it should be easy to locate the culprit work
 * function.
 *
 * RETURNS:
 * Pointer to worker which is executing @work if found, NULL
 * otherwise.
 */
static struct worker *find_worker_executing_work(struct global_wq *gwq,
        struct work_struct *work)
{
    return __find_worker_executing_work(gwq, busy_worker_head(gwq, work), work);
}


/**
 * gwq_determine_ins_pos - find insertion position
 * @gwq: gwq of interest
 * @wq: wq a work is being queued for
 *
 * A work for @wq is about to be queued on @gwq, determine insertion
 * position for the work.  If @wq is for HIGHPRI wq, the work is
 * queued at the head of the queue but in FIFO order with respect to
 * other HIGHPRI works; otherwise, at the end of the queue.  This
 * function also sets GWQ_HIGHPRI_PENDING flag to hint @gwq that
 * there are HIGHPRI works pending.
 *
 * CONTEXT:
 *
 * RETURNS:
 * Pointer to inserstion position.
 */
static inline struct list_head *gwq_determine_ins_pos(struct global_wq *gwq,
        struct workqueue_struct *wq)
{
    struct work_struct *twork;

    if (likely(!(wq->flags & WQ_HIGHPRI)))
        return &gwq->worklist;

    list_for_each_entry(twork, &gwq->worklist, entry) {
        struct workqueue_struct *twq = get_work_wq(twork);

        if (!(twq->flags & WQ_HIGHPRI))
            break;
    }

    gwq->flags |= GWQ_HIGHPRI_PENDING;
    return &twork->entry;
}


/**
 * insert_work - insert a work into gwq
 * @wq: wq @work belongs to
 * @work: work to insert
 * @head: insertion point
 * @extra_flags: extra WORK_STRUCT_* flags to set
 *
 * Insert @work which belongs to @wq into @gwq after @head.
 * @extra_flags is or'd to work_struct flags.
 *
 * CONTEXT:
 */
static void insert_work(struct workqueue_struct *wq,
        struct work_struct *work, struct list_head *head,
        unsigned int extra_flags)
{
    struct global_wq *gwq = wq->gwq;

    /* we own @work, set data and link */
    set_work_wq(work, wq, extra_flags);

    list_add_tail(&work->entry, head);

    if (__need_more_worker(gwq))
        wake_up_worker(gwq);
}



/**
 * queue_work - queue work on a workqueue
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to the CPU on which it was submitted, but if the CPU dies
 * it can be processed by another CPU.
 */
static void __queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
    struct list_head *worklist;
    unsigned int work_flags = 0;
    struct global_wq *gwq = wq->gwq;

    pthread_mutex_lock(&gwq->lock);

    BUG_ON(!list_empty(&work->entry));

    if (likely(wq->nr_active < wq->max_active)) {
        wq->nr_active++;
        worklist = gwq_determine_ins_pos(gwq, wq);
    } else {
        work_flags |= WORK_STRUCT_DELAYED;
        worklist = &wq->delayed_works;
    }

    insert_work(wq, work, worklist, work_flags);

    pthread_mutex_unlock(&gwq->lock);
}

int queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
    /*XXX*/
    int ret = 0; 

    if(!fake_test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
        __queue_work(wq, work);
        ret = 1;
    }
    return ret;
}

static void delayed_work_timer_fn(unsigned long __data)
{
    struct delayed_work *dwork = (struct delayed_work *)__data;
    struct workqueue_struct *wq = get_work_wq(&dwork->work);

    __queue_work(wq, &dwork->work);
}


/**
 * queue_delayed_work - queue work on a workqueue after delay
 * @wq: workqueue to use
 * @dwork: delayable work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
int queue_delayed_work(struct workqueue_struct *wq,
        struct delayed_work *dwork, unsigned long delay)
{
    struct timer_list *timer = &dwork->timer;
    int now;

    if (delay == 0)
        return queue_work(wq, &dwork->work);

    now = curr_time_ms();

    timer->expires = now + delay;
    timer->data = (unsigned long)dwork;
    timer->function = delayed_work_timer_fn;

    add_timer(timer);
    return 0;
}


/**
 * work_busy - test whether a work is currently pending or running
 * @work: the work to be tested
 *
 * Test whether @work is currently pending or running.  There is no
 * synchronization around this function and the test result is
 * unreliable and only useful as advisory hints or for debugging.
 * Especially for reentrant wqs, the pending state might hide the
 * running state.
 *
 * RETURNS:
 * OR'd bitmask of WORK_BUSY_* bits.
 */
unsigned int work_busy(struct work_struct *work)
{
    struct global_wq *gwq = get_global_wq();
    unsigned int ret = 0;

    if (!gwq)
        return false;

    pthread_mutex_lock(&gwq->lock);

    if (work_pending(work))
        ret |= WORK_BUSY_PENDING;

    if (find_worker_executing_work(gwq, work))
        ret |= WORK_BUSY_RUNNING;

    pthread_mutex_unlock(&gwq->lock);

    return ret;
}


/**
 * flush_workqueue - ensure that any scheduled work has run to completion.
 * @wq: workqueue to flush
 *
 * Forces execution of the workqueue and blocks until its completion.
 * This is typically used in driver shutdown handlers.
 *
 * We sleep until all works which were queued on entry have been handled,
 * but we are not livelocked by new incoming ones.
 */
void flush_workqueue(struct workqueue_struct *wq)
{

    /*XXX*/
}

/* Can I start working?  Called from busy but !running workers. */
static bool may_start_working(struct global_wq *gwq)
{
    return gwq->nr_idle;
}

/* Do I need to keep working?  Called from currently running workers. */
static bool keep_working(struct global_wq *gwq)
{
    return !list_empty(&gwq->worklist) &&
        (gwq->nr_running <= 1 || (gwq->flags & GWQ_HIGHPRI_PENDING));
}


/* Do we need a new worker?  Called from manager. */
static bool need_to_create_worker(struct global_wq *gwq)
{
    return need_more_worker(gwq) && !may_start_working(gwq);
}

/* Do I need to be the manager? */
static bool need_to_manage_workers(struct global_wq *gwq)
{
    return need_to_create_worker(gwq) || gwq->flags & GWQ_MANAGE_WORKERS;
}

/* Do we have too many workers and should some go away? */
static bool too_many_workers(struct global_wq *gwq)
{
    bool managing = gwq->flags & GWQ_MANAGING_WORKERS;
    int nr_idle = gwq->nr_idle + managing; /* manager is considered idle */
    int nr_busy = gwq->nr_workers - nr_idle;

    return nr_idle > 2 && (nr_idle - 2) * MAX_IDLE_WORKERS_RATIO >= nr_busy;
}


/**
 * worker_enter_idle - enter idle state
 * @worker: worker which is entering idle state
 *
 * @worker is entering idle state.  Update stats and idle timer if
 * necessary.
 *
 * LOCKING:
 * spin_lock_irq(gwq->lock).
 */
static void worker_enter_idle(struct worker *worker)
{
    struct global_wq *gwq = worker->gwq;

    BUG_ON(worker->flags & WORKER_IDLE);
    BUG_ON(!list_empty(&worker->entry) && 
            (worker->hentry.next || worker->hentry.pprev));

    /* can't use worker_set_flags(), also called from start_worker() */
    worker->flags |= WORKER_IDLE;
    gwq->nr_idle++;
    worker->last_active = curr_time_ms();

    /* idle_list is LIFO */
    list_add(&worker->entry, &gwq->idle_list);

    if (too_many_workers(gwq) && !timer_pending(&gwq->idle_timer))
        mod_timer(&gwq->idle_timer,
                curr_time_ms() + IDLE_WORKER_TIMEOUT);
}

/**
 * worker_leave_idle - leave idle state
 * @worker: worker which is leaving idle state
 *
 * @worker is leaving idle state.  Update stats.
 *
 * LOCKING:
 * spin_lock_irq(gwq->lock).
 */
static void worker_leave_idle(struct worker *worker)
{
    struct global_wq *gwq = worker->gwq;

    BUG_ON(!(worker->flags & WORKER_IDLE));
    worker->flags &= ~WORKER_IDLE;
    gwq->nr_idle--;
    list_del_init(&worker->entry);
}


static struct worker *alloc_worker(void)
{
    struct worker *worker;

    worker = malloc(sizeof(*worker));
    if (!worker) 
        return NULL;

    INIT_LIST_HEAD(&worker->entry);
    /* on creation a worker is in !idle && prep state */
    worker->flags = WORKER_PREP;

    return worker;
}

/**
 * create_worker - create a new workqueue worker
 * @gwq: gwq the new worker will belong to
 * @bind: whether to set affinity to @cpu or not
 *
 * Create a new worker which is bound to @gwq.  The returned worker
 * can be started by calling start_worker() or destroyed using
 * destroy_worker().
 *
 * CONTEXT:
 * Might sleep.  Does GFP_KERNEL allocations.
 *
 * RETURNS:
 * Pointer to the newly created worker.
 */
static struct worker *create_worker(struct global_wq *gwq)
{
    int ret;
    struct worker *worker = NULL;
    pthread_attr_t attr;

    worker = alloc_worker();
    if (!worker)
        goto fail;

    init_waitqueue_head(&worker->waitq);
    worker->gwq = gwq;
    worker->id = gwq->worker_ids++;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&worker->task, &attr, worker_thread, worker);
    if(ret)
        goto create_fail;

    return worker;

create_fail:
    free(worker);
fail:
    return NULL;
}


/**
 * start_worker - start a newly created worker
 * @worker: worker to start
 *
 * Make the gwq aware of @worker and start it.
 *
 * CONTEXT:
 */
static void start_worker(struct worker *worker)
{
    worker->flags |= WORKER_STARTED;
    worker->gwq->nr_workers++;

    worker_enter_idle(worker);
    wake_up(&worker->waitq);
}

/**
 * destroy_worker - destroy a workqueue worker
 * @worker: worker to be destroyed
 *
 * Destroy @worker and adjust @gwq stats accordingly.
 *
 * CONTEXT:
 */
static void destroy_worker(struct worker *worker)
{
    struct global_wq *gwq = worker->gwq;
    /*XXX: add ida_remove*/
    //	int id = worker->id;

    /* sanity check frenzy */
    if(worker->current_work)
        return;
    /*XXX*/ //BUG_ON(!list_empty(&worker->scheduled));

    if (worker->flags & WORKER_STARTED)
        gwq->nr_workers--;
    if (worker->flags & WORKER_IDLE)
        gwq->nr_idle--;

    list_del_init(&worker->entry);
    worker->flags |= WORKER_DIE;

    //	kthread_stop(worker->task);
    free(worker);
}


/**
 * maybe_create_worker - create a new worker if necessary
 * @gwq: gwq to create a new worker for
 *
 * Create a new worker for @gwq if necessary.  @gwq is guaranteed to
 * have at least one idle worker on return from this function.  If
 * creating a new worker takes longer than MAYDAY_INTERVAL, mayday is
 * sent to all rescuers with works scheduled on @gwq to resolve
 * possible allocation deadlock.
 *
 * On return, need_to_create_worker() is guaranteed to be false and
 * may_start_working() true.
 *
 * RETURNS:
 * false if no action was taken and gwq->lock stayed locked, true
 * otherwise.
 */
static bool maybe_create_worker(struct global_wq *gwq)
{
    if (!need_to_create_worker(gwq))
        return false;

    while (true) {
        struct worker *worker;

        worker = create_worker(gwq);
        if (worker) {
            start_worker(worker);
            BUG_ON(need_to_create_worker(gwq));
            return true;
        }
    }

    return true;
}


/**
 * maybe_destroy_worker - destroy workers which have been idle for a while
 * @gwq: gwq to destroy workers for
 *
 * Destroy @gwq workers which have been idle for longer than
 * IDLE_WORKER_TIMEOUT.
 *
 * LOCKING:
 * spin_lock_irq(gwq->lock) which may be released and regrabbed
 * multiple times.  Called only from manager.
 *
 * RETURNS:
 * false if no action was taken and gwq->lock stayed locked, true
 * otherwise.
 */
static bool maybe_destroy_workers(struct global_wq *gwq)
{
    bool ret = false;

    while (too_many_workers(gwq)) {
        struct worker *worker;
        uint64_t expires;

        worker = list_entry(gwq->idle_list.prev, struct worker, entry);
        expires = worker->last_active + IDLE_WORKER_TIMEOUT;

        if (time_before(curr_time_ms(), expires)) {
            mod_timer(&gwq->idle_timer, expires);
            break;
        }

        destroy_worker(worker);
        ret = true;
    }

    return ret;
}



/**
 * manage_workers - manage worker pool
 * @worker: self
 *
 * Assume the manager role and manage gwq worker pool @worker belongs
 * to.  At any given time, there can be only zero or one manager per
 * gwq.  The exclusion is handled automatically by this function.
 *
 * The caller can safely start processing works on false return.  On
 * true return, it's guaranteed that need_to_create_worker() is false
 * and may_start_working() is true.
 *
 * CONTEXT:
 * spin_lock_irq(gwq->lock) which may be released and regrabbed
 * multiple times.  Does GFP_KERNEL allocations.
 *
 * RETURNS:
 * false if no action was taken and gwq->lock stayed locked, true if
 * some action was taken.
 */
static bool manage_workers(struct worker *worker)
{
    struct global_wq *gwq = worker->gwq;
    bool ret = false;

    if (gwq->flags & GWQ_MANAGING_WORKERS)
        return ret;

    gwq->flags &= ~GWQ_MANAGE_WORKERS;
    gwq->flags |= GWQ_MANAGING_WORKERS;

    /*
     * Destroy and then create so that may_start_working() is true
     * on return.
     */
    ret |= maybe_destroy_workers(gwq);
    ret |= maybe_create_worker(gwq);

    gwq->flags &= ~GWQ_MANAGING_WORKERS;

    return ret;
}


/**
 * worker_thread - the worker thread function
 * @__worker: self
 *
 * The gwq worker thread function.  There's a single dynamic pool of
 * these per each cpu.  These workers process all works regardless of
 * their specific target workqueue.  The only exception is works which
 * belong to workqueues with a rescuer which will be explained in
 * rescuer_thread().
 */
static void *worker_thread(void *__worker)
{
    struct worker *worker = __worker;
    struct global_wq *gwq = worker->gwq;
    struct hlist_head *bwh;

woke_up:
    pthread_mutex_lock(&gwq->lock);

    worker_leave_idle(worker);
recheck:
    /* no more worker necessary? */
    if (!need_more_worker(gwq))
        goto sleep;

    /* do we need to manage? */
    if (unlikely(!may_start_working(gwq)) && manage_workers(worker))
        goto recheck;

    worker_clr_flags(worker, WORKER_PREP);
    do {
        struct work_struct *work =
            list_first_entry(&gwq->worklist,
                    struct work_struct, entry);
        struct workqueue_struct *wq = get_work_wq(work);

        bool cpu_intensive = wq->flags & WQ_CPU_INTENSIVE;

        bwh = busy_worker_head(gwq, work);
        hlist_add_head(&worker->hentry, bwh);

        worker->current_work = work;
        worker->current_func = work->func;
        worker->current_wq = wq;

        list_del_init(&work->entry);

        /*
         * CPU intensive works don't participate in concurrency management.
         * They're the scheduler's responsibility.  This takes @worker out
         * of concurrency management and the next code block will chain
         * execution of the pending work items.
         */
        if (unlikely(cpu_intensive))
            worker_set_flags(worker, WORKER_CPU_INTENSIVE);

        if (need_more_worker(gwq))
            wake_up_worker(gwq);

        work_clear_pending(work);
        pthread_mutex_unlock(&gwq->lock);

        worker->current_func(work);

        pthread_mutex_lock(&gwq->lock);

        /* clear cpu intensive status */
        if (unlikely(cpu_intensive))
            worker_clr_flags(worker, WORKER_CPU_INTENSIVE);

        /* we're done with it, release */
        hlist_del_init(&worker->hentry);
        worker->current_work = NULL;
        worker->current_func = NULL;
        worker->current_wq = NULL;

        wq->nr_active--;
    } while (keep_working(gwq));
    worker_set_flags(worker, WORKER_PREP);

sleep:
    if (unlikely(need_to_manage_workers(gwq)) && manage_workers(worker))
        goto recheck;

    /*
     * gwq->lock is held and there's no work to process and no
     * need to manage, sleep.  Workers are woken up only while
     * holding gwq->lock or from local cpu, so setting the
     * current state before releasing gwq->lock is enough to
     * prevent losing any event.
     */
    worker_enter_idle(worker);

    pthread_mutex_unlock(&gwq->lock);
    wait_event(worker->waitq, need_more_worker(gwq));

    goto woke_up;

    return 0;
}

struct workqueue_struct *alloc_workqueue(int max_active, unsigned int flags)
{
    struct workqueue_struct *wq;

    wq = (struct workqueue_struct *)malloc(sizeof(*wq));
    if(!wq)
        return NULL;

    max_active = max_active ?: WQ_DFL_ACTIVE;

    wq->flags = flags;
    wq->max_active = max_active;
    wq->nr_active = 0;
    wq->gwq = get_global_wq();

    INIT_LIST_HEAD(&wq->delayed_works);

    pthread_mutex_lock(&workqueue_lock);
    list_add(&wq->list, &workqueues);
    pthread_mutex_unlock(&workqueue_lock);

    return wq;
}

/**
 * destroy_workqueue - safely terminate a workqueue
 * @wq: target workqueue
 *
 * Safely destroy a workqueue. All work currently pending will be done first.
 */
void destroy_workqueue(struct workqueue_struct *wq)
{
    unsigned int flush_cnt = 0;
    bool drained;

    //	wq->flags |= WQ_DYING;

reflush:
    flush_workqueue(wq);

    pthread_mutex_lock(&wq->gwq->lock);
    drained = !wq->nr_active && list_empty(&wq->delayed_works);
    pthread_mutex_lock(&wq->gwq->lock);

    if (!drained) {
        if (++flush_cnt == 10 ||
                (flush_cnt % 100 == 0 && flush_cnt <= 1000))
            logw("workqueue: flush on destruction isn't complete"
                    " after %u tries\n", flush_cnt);
        goto reflush;
    }

    pthread_mutex_lock(&workqueue_lock);
    list_del(&wq->list);
    pthread_mutex_unlock(&workqueue_lock);

    BUG_ON(wq->nr_active);
    BUG_ON(!list_empty(&wq->delayed_works));

    free(wq);
}


static void idle_worker_timeout(unsigned long __gwq)
{
    struct global_wq *gwq = (void *)__gwq;

    pthread_mutex_lock(&gwq->lock);

    if (too_many_workers(gwq)) {
        struct worker *worker;
        uint64_t expires;

        /* idle_list is kept in LIFO order, check the last one */
        worker = list_entry(gwq->idle_list.prev, struct worker, entry);
        expires = worker->last_active + IDLE_WORKER_TIMEOUT;

        if (time_before(curr_time_ms(), expires))
            mod_timer(&gwq->idle_timer, expires);
        else {
            /* it's been idle for too long, wake up manager */
            gwq->flags |= GWQ_MANAGE_WORKERS;
            wake_up_worker(gwq);
        }
    }

    pthread_mutex_unlock(&gwq->lock);
}



int init_workqueues(void)
{
    int i;
    struct worker *worker;
    struct global_wq *gwq = get_global_wq();

    pthread_mutex_init(&gwq->lock, NULL);	

    INIT_LIST_HEAD(&gwq->worklist);
    gwq->flags = 0;
    gwq->nr_workers = 0;
    gwq->nr_idle = 0;
    gwq->worker_ids = 0;

    INIT_LIST_HEAD(&gwq->idle_list);
    for (i = 0; i < BUSY_WORKER_HASH_SIZE; i++)
        INIT_HLIST_HEAD(&gwq->busy_hash[i]);

    init_timer(&gwq->idle_timer);
    gwq->idle_timer.function = idle_worker_timeout;
    gwq->idle_timer.data = (unsigned long)gwq;

    gwq->first_idle = NULL;

    INIT_LIST_HEAD(&gwq->workqueues);

    worker = create_worker(gwq);
    start_worker(worker);

    return 0;
}

