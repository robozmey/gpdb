/*-------------------------------------------------------------------------
 *
 * libpq.h
 *	  POSTGRES LIBPQ buffer structure definitions.
 *
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/libpq/libpq.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef LIBPQ_H
#define LIBPQ_H

#include <sys/types.h>
#include <netinet/in.h>

#include "lib/stringinfo.h"
#include "libpq/libpq-be.h"


/*
 * External functions.
 */

/*
 * prototypes for functions in pqcomm.c
 */
extern int StreamServerPort(int family, char *hostName,
				 unsigned short portNumber, char *unixSocketDir,
				 pgsocket ListenSocket[], int MaxListen);
extern int	StreamConnection(pgsocket server_fd, Port *port);
extern void StreamClose(pgsocket sock);
extern void TouchSocketFiles(void);
extern void RemoveSocketFiles(void);
extern void pq_init(void);
extern void pq_comm_reset(void);
extern void pq_comm_close_fatal(void);                                  /* GPDB only */
extern int	pq_getbytes(char *s, size_t len);
extern int	pq_getstring(StringInfo s);
extern void pq_startmsgread(void);
extern void pq_endmsgread(void);
extern bool pq_is_reading_msg(void);
extern int	pq_getmessage(StringInfo s, int maxlen);
extern int	pq_getbyte(void);
extern int	pq_peekbyte(void);
extern int	pq_getbyte_if_available(unsigned char *c);
extern bool pq_buffer_has_data(void);
extern int	pq_putbytes(const char *s, size_t len);
extern int	pq_flush(void);
extern int	pq_flush_if_writable(void);
extern bool pq_is_send_pending(void);
extern int	pq_putmessage(char msgtype, const char *s, size_t len);
extern void pq_putmessage_noblock(char msgtype, const char *s, size_t len);
extern void pq_startcopyout(void);
extern void pq_endcopyout(bool errorAbort);
extern bool pq_waitForDataUsingSelect(void);                /* GPDB only */
extern bool pq_check_connection(void);

/*
 * prototypes for functions in be-secure.c
 */
extern char *ssl_cert_file;
extern char *ssl_key_file;
extern char *ssl_ca_file;
extern char *ssl_crl_file;

extern int	secure_initialize(bool failOnError);
extern bool secure_loaded_verify_locations(void);
extern void secure_destroy(void);
extern int	secure_open_server(Port *port);
extern void secure_close(Port *port);
extern ssize_t secure_read(Port *port, void *ptr, size_t len);
extern ssize_t secure_write(Port *port, void *ptr, size_t len);

#endif   /* LIBPQ_H */
