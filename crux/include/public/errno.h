/* SPDX-License-Identifier: MIT */

/*
 * There are two expected ways of including this header.
 *
 * 1) The "default" case (expected from tools etc).
 *
 * Simply #include <public/errno.h>
 *
 * In this circumstance, normal header guards apply and the includer shall get
 * an enumeration in the CRUX_xxx namespace, appropriate for C or assembly.
 *
 * 2) The special case where the includer provides a CRUX_ERRNO() in scope.
 *
 * In this case, no inclusion guards apply and the caller is responsible for
 * their CRUX_ERRNO() being appropriate in the included context.  The header
 * will unilaterally #undef CRUX_ERRNO().
 */

/* SAF-8-safe inclusion procedure left to caller */
#ifndef CRUX_ERRNO

/*
 * Includer has not provided a custom CRUX_ERRNO().  Arrange for normal header
 * guards, an automatic enum (for C code) and constants in the CRUX_xxx
 * namespace.
 */
#ifndef __CRUX_PUBLIC_ERRNO_H__
#define __CRUX_PUBLIC_ERRNO_H__

#define CRUX_ERRNO_DEFAULT_INCLUDE

#ifndef __ASSEMBLY__

#define CRUX_ERRNO(name, value) CRUX_##name = (value),
enum crux_errno {

#elif __CRUX_INTERFACE_VERSION__ < 0x00040700

#define CRUX_ERRNO(name, value) .equ CRUX_##name, value

#endif /* __ASSEMBLY__ */

#endif /* __CRUX_PUBLIC_ERRNO_H__ */
#endif /* !CRUX_ERRNO */

/* ` enum neg_errnoval {  [ -Efoo for each Efoo in the list below ]  } */
/* ` enum errnoval { */

#ifdef CRUX_ERRNO

/*
 * Values originating from x86 Linux. Please consider using respective
 * values when adding new definitions here.
 *
 * The set of identifiers to be added here shouldn't extend beyond what
 * POSIX mandates (see e.g.
 * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/errno.h.html)
 * with the exception that we support some optional (XSR) values
 * specified there (but no new ones should be added).
 */

CRUX_ERRNO(EPERM,	 1)	/* Operation not permitted */
CRUX_ERRNO(ENOENT,	 2)	/* No such file or directory */
CRUX_ERRNO(ESRCH,	 3)	/* No such process */
#ifdef __CRUX__ /* Internal only, should never be exposed to the guest. */
CRUX_ERRNO(EINTR,	 4)	/* Interrupted system call */
#endif
CRUX_ERRNO(EIO,		 5)	/* I/O error */
CRUX_ERRNO(ENXIO,	 6)	/* No such device or address */
CRUX_ERRNO(E2BIG,	 7)	/* Arg list too long */
CRUX_ERRNO(ENOEXEC,	 8)	/* Exec format error */
CRUX_ERRNO(EBADF,	 9)	/* Bad file number */
CRUX_ERRNO(ECHILD,	10)	/* No child processes */
CRUX_ERRNO(EAGAIN,	11)	/* Try again */
CRUX_ERRNO(EWOULDBLOCK,	11)	/* Operation would block.  Aliases EAGAIN */
CRUX_ERRNO(ENOMEM,	12)	/* Out of memory */
CRUX_ERRNO(EACCES,	13)	/* Permission denied */
CRUX_ERRNO(EFAULT,	14)	/* Bad address */
CRUX_ERRNO(EBUSY,	16)	/* Device or resource busy */
CRUX_ERRNO(EEXIST,	17)	/* File exists */
CRUX_ERRNO(EXDEV,	18)	/* Cross-device link */
CRUX_ERRNO(ENODEV,	19)	/* No such device */
CRUX_ERRNO(ENOTDIR,	20)	/* Not a directory */
CRUX_ERRNO(EISDIR,	21)	/* Is a directory */
CRUX_ERRNO(EINVAL,	22)	/* Invalid argument */
CRUX_ERRNO(ENFILE,	23)	/* File table overflow */
CRUX_ERRNO(EMFILE,	24)	/* Too many open files */
CRUX_ERRNO(ENOSPC,	28)	/* No space left on device */
CRUX_ERRNO(EROFS,	30)	/* Read-only file system */
CRUX_ERRNO(EMLINK,	31)	/* Too many links */
CRUX_ERRNO(EDOM,		33)	/* Math argument out of domain of func */
CRUX_ERRNO(ERANGE,	34)	/* Math result not representable */
CRUX_ERRNO(EDEADLK,	35)	/* Resource deadlock would occur */
CRUX_ERRNO(EDEADLOCK,	35)	/* Resource deadlock would occur. Aliases EDEADLK */
CRUX_ERRNO(ENAMETOOLONG,	36)	/* File name too long */
CRUX_ERRNO(ENOLCK,	37)	/* No record locks available */
CRUX_ERRNO(ENOSYS,	38)	/* Function not implemented */
CRUX_ERRNO(ENOTEMPTY,	39)	/* Directory not empty */
CRUX_ERRNO(ENODATA,	61)	/* No data available */
CRUX_ERRNO(ETIME,	62)	/* Timer expired */
CRUX_ERRNO(EBADMSG,	74)	/* Not a data message */
CRUX_ERRNO(EOVERFLOW,	75)	/* Value too large for defined data type */
CRUX_ERRNO(EILSEQ,	84)	/* Illegal byte sequence */
#ifdef __CRUX__ /* Internal only, should never be exposed to the guest. */
CRUX_ERRNO(ERESTART,	85)	/* Interrupted system call should be restarted */
#endif
CRUX_ERRNO(ENOTSOCK,	88)	/* Socket operation on non-socket */
CRUX_ERRNO(EMSGSIZE,	90)	/* Message too large. */
CRUX_ERRNO(EOPNOTSUPP,	95)	/* Operation not supported on transport endpoint */
CRUX_ERRNO(EADDRINUSE,	98)	/* Address already in use */
CRUX_ERRNO(EADDRNOTAVAIL, 99)	/* Cannot assign requested address */
CRUX_ERRNO(ENOBUFS,	105)	/* No buffer space available */
CRUX_ERRNO(EISCONN,	106)	/* Transport endpoint is already connected */
CRUX_ERRNO(ENOTCONN,	107)	/* Transport endpoint is not connected */
CRUX_ERRNO(ETIMEDOUT,	110)	/* Connection timed out */
CRUX_ERRNO(ECONNREFUSED,	111)	/* Connection refused */

#undef CRUX_ERRNO
#endif /* CRUX_ERRNO */
/* ` } */

/* Clean up from a default include.  Close the enum (for C). */
#ifdef CRUX_ERRNO_DEFAULT_INCLUDE
#undef CRUX_ERRNO_DEFAULT_INCLUDE
#ifndef __ASSEMBLY__
};
#endif

#endif /* CRUX_ERRNO_DEFAULT_INCLUDE */
