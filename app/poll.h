/*********************************************************
 *                                                       *
 * 		V o o d o o  T r a c k e r				 *
 *                                                       *
 *	     - poll() emulation using select() -		 *
 *												 *
 *   Copyright (C) 1999 Greg Hayes, all rights reserved  *
 *                                                       *
 *              See COPYING for information              *
 *                                                       *
 *********************************************************/

#ifndef _ST_POLL_H
#define _ST_POLL_H

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#else

/* Data structure describing a polling request.  */
struct pollfd
  {
    int fd;			/* File descriptor to poll.  */
    short int events;		/* Types of events poller cares about.  */
    short int revents;		/* Types of events that actually occurred.  */
  };

/* Event types that can be polled for.  These bits may be set in `events'
   to indicate the interesting event types; they will appear in `revents'
   to indicate the status of the file descriptor.  */
#define POLLIN		01              /* There is data to read.  */
#define POLLPRI		02              /* There is urgent data to read.  */
#define POLLOUT		04              /* Writing now will not block.  */

int poll(struct pollfd *ufds, unsigned int nfds, int timeout);

#endif /* HAVE_SYS_POLL_H */

#endif /* _ST_POLL_H */
