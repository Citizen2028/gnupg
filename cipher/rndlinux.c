/* rndlinux.c  -  raw random number for OSes with /dev/random
 *	Copyright (C) 1998 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_GETTIMEOFDAY
  #include <sys/times.h>
#endif
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "types.h"
#ifdef IS_MODULE
  #include "g10lib.h"
  #define _(a) (a)
#else
  #include "util.h"
  #include "ttyio.h"
  #include "i18n.h"
  #include "dynload.h"
#endif

static int open_device( const char *name, int minor );
static int gather_random( byte *buffer, size_t *r_length, int level );

#ifdef IS_MODULE
static void tty_printf(const char *fmt, ... )
{
    g10_log_info("tty_printf not available (%s)\n", fmt );
}
#endif



/****************
 * Used to open the Linux and xBSD /dev/random devices
 */
static int
open_device( const char *name, int minor )
{
    int fd;
    struct stat sb;

    fd = open( name, O_RDONLY );
    if( fd == -1 )
	g10_log_fatal("can't open %s: %s\n", name, strerror(errno) );
    if( fstat( fd, &sb ) )
	g10_log_fatal("stat() off %s failed: %s\n", name, strerror(errno) );
  #if defined(__sparc__) && defined(__linux__)
    #warning something is wrong with UltraPenguin /dev/random
  #else
    if( !S_ISCHR(sb.st_mode) )
	g10_log_fatal("invalid random device!\n" );
  #endif
    return fd;
}


static int
gather_random( byte *buffer, size_t *r_length, int level )
{
    static int fd_urandom = -1;
    static int fd_random = -1;
    int fd;
    int n;
    int warn=0;
    size_t length = *r_length;
    /* note: we will always return the requested length */

    if( level >= 2 ) {
	if( fd_random == -1 )
	    fd_random = open_device( NAME_OF_DEV_RANDOM, 8 );
	fd = fd_random;
    }
    else {
	if( fd_urandom == -1 )
	    fd_urandom = open_device( NAME_OF_DEV_URANDOM, 9 );
	fd = fd_urandom;
    }
    do {
	fd_set rfds;
	struct timeval tv;
	int rc;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	if( !(rc=select(fd+1, &rfds, NULL, NULL, &tv)) ) {
	    if( !warn )
		tty_printf( _(
"\n"
"Not enough random bytes available.  Please do some other work to give\n"
"the OS a chance to collect more entropy! (Need %d more bytes)\n"), length );
	    warn = 1;
	    continue;
	}
	else if( rc == -1 ) {
	    tty_printf("select() error: %s\n", strerror(errno));
	    continue;
	}

	do {
	    n = read(fd, buffer, length );
	    if( n >= 0 && n > length ) {
		g10_log_error("bogus read from random device (n=%d)\n", n );
		n = length;
	    }
	} while( n == -1 && errno == EINTR );
	if( n == -1 )
	    g10_log_fatal("read error on random device: %s\n", strerror(errno));
	assert( n <= length );
	buffer += n;
	length -= n;
    } while( length );

    return 100; /* 100% useful at the requested level */
}



#ifndef IS_MODULE
static
#endif
const char * const gnupgext_version = "RNDLINUX ($Revision$)";

static struct {
    int class;
    int version;
    void *func;
} func_table[] = {
    { 40, 1, gather_random },
};



/****************
 * Enumerate the names of the functions together with informations about
 * this function. Set sequence to an integer with a initial value of 0 and
 * do not change it.
 * If what is 0 all kind of functions are returned.
 * Return values: class := class of function:
 *			   10 = message digest algorithm info function
 *			   11 = integer with available md algorithms
 *			   20 = cipher algorithm info function
 *			   21 = integer with available cipher algorithms
 *			   30 = public key algorithm info function
 *			   31 = integer with available pubkey algorithms
 *			   40 = get gather_random function
 *			   41 = get fast_random_poll function
 *		  version = interface version of the function/pointer
 *			    (currently this is 1 for all functions)
 */

#ifndef IS_MODULE
static
#endif
void *
gnupgext_enum_func( int what, int *sequence, int *class, int *vers )
{
    void *ret;
    int i = *sequence;

    do {
	if ( i >= DIM(func_table) || i < 0 ) {
	    return NULL;
	}
	*class = func_table[i].class;
	*vers  = func_table[i].version;
	ret = func_table[i].func;
	i++;
    } while ( what && what != *class );

    *sequence = i;
    return ret;
}

#ifndef IS_MODULE
void
rndlinux_constructor(void)
{
    register_internal_cipher_extension( gnupgext_version,
					gnupgext_enum_func );
}
#endif

