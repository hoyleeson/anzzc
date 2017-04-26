
AM_CFLAGS = @GLOBAL_CFLAGS@ -fPIC -I$(top_srcdir)

ANZZC_SRCS = common.c log.c notifier.c timer.c utils.c wait.c console.c workqueue.c \
			 completion.c parser.c configs.c mempool.c queue.c fifo.c bsearch.c rbtree.c \
			 bitmap.c find_bit.c hweight.c idr.c daemon.c dump_stack.c poller.c parcel.c \
			 ioasync.c init.c hbeat.c data_frag.c packet.c pack_head.c iowait.c args.c \
			 netsock.c sock_stream.c sock_dgram.c ethtools.c sockets.c cmds.c \
			 parser.h keywords.h 


lib_LTLIBRARIES = libanzzc.la
libanzzc_la_SOURCES = $(ANZZC_SRCS)
