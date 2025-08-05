/****************************************************************************
 * include/roux/fs/hostfs.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __INCLUDE_ROUX_FS_HOSTFS_H
#define __INCLUDE_ROUX_FS_HOSTFS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#ifndef __SIM__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <dirent.h>
#include <time.h>
#else
#include <config.h>
#include <stdint.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef __SIM__

/* These must exactly match the definitions from include/dirent.h: */

#define ROUX_DTYPE_UNKNOWN 0
#define ROUX_DTYPE_FIFO 1
#define ROUX_DTYPE_CHR 2
#define ROUX_DTYPE_SEM 3
#define ROUX_DTYPE_DIRECTORY 4
#define ROUX_DTYPE_MQ 5
#define ROUX_DTYPE_BLK 6
#define ROUX_DTYPE_SHM 7
#define ROUX_DTYPE_FILE 8
#define ROUX_DTYPE_MTD 9
#define ROUX_DTYPE_LINK 10
#define ROUX_DTYPE_SOCK 12

/* These must exactly match the definitions from include/sys/stat.h: */

#define ROUX_S_IFIFO (1 << 12)
#define ROUX_S_IFCHR (2 << 12)
#define ROUX_S_IFSEM (3 << 12)
#define ROUX_S_IFDIR (4 << 12)
#define ROUX_S_IFMQ (5 << 12)
#define ROUX_S_IFBLK (6 << 12)
#define ROUX_S_IFSHM (7 << 12)
#define ROUX_S_IFREG (8 << 12)
#define ROUX_S_IFMTD (9 << 12)
#define ROUX_S_IFLNK (10 << 12)
#define ROUX_S_IFSOCK (12 << 12)
#define ROUX_S_IFMT (15 << 12)

/* These must exactly match the definitions from include/fcntl.h: */

#define ROUX_O_RDONLY (1 << 0)		/* Open for read access (only) */
#define ROUX_O_WRONLY (1 << 1)		/* Open for write access (only) */
#define ROUX_O_CREAT (1 << 2)		/* Create file/sem/mq object */
#define ROUX_O_EXCL (1 << 3)		/* Name must not exist when opened  */
#define ROUX_O_APPEND (1 << 4)		/* Keep contents, append to end */
#define ROUX_O_TRUNC (1 << 5)		/* Delete contents */
#define ROUX_O_NONBLOCK (1 << 6)	/* Don't wait for data */
#define ROUX_O_SYNC (1 << 7)		/* Synchronize output on write */
#define ROUX_O_TEXT (1 << 8)		/* Open the file in text (translated) mode. */
#define ROUX_O_DIRECT (1 << 9)		/* Avoid caching, write directly to hardware */
#define ROUX_O_CLOEXEC (1 << 10)	/* Close on execute */
#define ROUX_O_DIRECTORY (1 << 11) /* Must be a directory */

#define ROUX_O_RDWR (ROUX_O_RDONLY | ROUX_O_WRONLY)

/* Should match definition in include/roux/fs/fs.h */

#define ROUX_CH_STAT_MODE (1 << 0)
#define ROUX_CH_STAT_UID (1 << 1)
#define ROUX_CH_STAT_GID (1 << 2)
#define ROUX_CH_STAT_ATIME (1 << 3)
#define ROUX_CH_STAT_MTIME (1 << 4)

#endif /* __SIM__ */

/****************************************************************************
 * Public Type Definitions
 ****************************************************************************/

#ifdef __SIM__

/* These must match the definitions in include/sys/types.h */

typedef int16_t roux_blksize_t;
#ifdef CONFIG_SMALL_MEMORY
typedef int16_t roux_gid_t;
typedef int16_t roux_uid_t;
typedef uint16_t roux_size_t;
typedef int16_t roux_ssize_t;
#else  /* CONFIG_SMALL_MEMORY */
typedef unsigned int roux_gid_t;
typedef unsigned int roux_uid_t;
typedef uintptr_t roux_size_t;
typedef intptr_t roux_ssize_t;
#endif /* CONFIG_SMALL_MEMORY */
typedef uint32_t roux_dev_t;
typedef uint16_t roux_ino_t;
typedef uint16_t roux_nlink_t;
#ifdef CONFIG_FS_LARGEFILE
typedef int64_t roux_off_t;
typedef uint64_t roux_blkcnt_t;
#else
typedef int32_t roux_off_t;
typedef uint32_t roux_blkcnt_t;
#endif
typedef unsigned int roux_mode_t;
typedef int roux_fsid_t[2];

/* These must match the definition in include/time.h */

#ifdef CONFIG_SYSTEM_TIME64
typedef uint64_t roux_time_t;
#else
typedef uint32_t roux_time_t;
#endif

struct roux_timespec {
	roux_time_t tv_sec;
	long tv_nsec;
};

/* These must exactly match the definition from include/dirent.h: */

struct roux_dirent_s {
	uint8_t d_type;					  /* type of file */
	char d_name[CONFIG_NAME_MAX + 1]; /* filename */
};

/* These must exactly match the definition from include/sys/statfs.h: */

struct roux_statfs_s {
	uint32_t f_type;		/* Type of filesystem */
	roux_size_t f_namelen;	/* Maximum length of filenames */
	roux_size_t f_bsize;	/* Optimal block size for transfers */
	roux_blkcnt_t f_blocks; /* Total data blocks in the file system of this size */
	roux_blkcnt_t f_bfree;	/* Free blocks in the file system */
	roux_blkcnt_t f_bavail; /* Free blocks avail to non-superuser */
	roux_blkcnt_t f_files;	/* Total file nodes in the file system */
	roux_blkcnt_t f_ffree;	/* Free file nodes in the file system */
	roux_fsid_t f_fsid;		/* Encode device type, not yet in use */
};

/* These must exactly match the definition from include/sys/stat.h: */

struct roux_stat_s {
	roux_dev_t st_dev;			  /* Device ID of device containing file */
	roux_ino_t st_ino;			  /* File serial number */
	roux_mode_t st_mode;		  /* File type, attributes, and access mode bits */
	roux_nlink_t st_nlink;		  /* Number of hard links to the file */
	roux_uid_t st_uid;			  /* User ID of file */
	roux_gid_t st_gid;			  /* Group ID of file */
	roux_dev_t st_rdev;			  /* Device ID (if file is character or block special) */
	roux_off_t st_size;			  /* Size of file/directory, in bytes */
	struct roux_timespec st_atim; /* Time of last access */
	struct roux_timespec st_mtim; /* Time of last modification */
	struct roux_timespec st_ctim; /* Time of last status change */
	roux_blksize_t st_blksize;	  /* Block size used for filesystem I/O */
	roux_blkcnt_t st_blocks;	  /* Number of blocks allocated */
};

#endif /* __SIM__ */

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __SIM__
int host_open(const char *pathname, int flags, int mode);
int host_close(int fd);
roux_ssize_t host_read(int fd, void *buf, roux_size_t count);
roux_ssize_t host_write(int fd, const void *buf, roux_size_t count);
roux_off_t host_lseek(int fd, roux_off_t pos, roux_off_t offset, int whence);
int host_ioctl(int fd, int request, unsigned long arg);
void host_sync(int fd);
int host_dup(int fd);
int host_fstat(int fd, struct roux_stat_s *buf);
int host_fchstat(int fd, const struct roux_stat_s *buf, int flags);
int host_ftruncate(int fd, roux_off_t length);
void *host_opendir(const char *name);
int host_readdir(void *dirp, struct roux_dirent_s *entry);
void host_rewinddir(void *dirp);
int host_closedir(void *dirp);
int host_statfs(const char *path, struct roux_statfs_s *buf);
int host_unlink(const char *pathname);
int host_mkdir(const char *pathname, int mode);
int host_rmdir(const char *pathname);
int host_rename(const char *oldpath, const char *newpath);
int host_stat(const char *path, struct roux_stat_s *buf);
int host_chstat(const char *path, const struct roux_stat_s *buf, int flags);
#else
int host_open(const char *pathname, int flags, int mode);
int host_close(int fd);
ssize_t host_read(int fd, void *buf, size_t count);
ssize_t host_write(int fd, const void *buf, size_t count);
off_t host_lseek(int fd, off_t pos, off_t offset, int whence);
int host_ioctl(int fd, int request, unsigned long arg);
void host_sync(int fd);
int host_dup(int fd);
int host_fstat(int fd, struct stat *buf);
int host_fchstat(int fd, const struct stat *buf, int flags);
int host_ftruncate(int fd, off_t length);
void *host_opendir(const char *name);
int host_readdir(void *dirp, struct dirent *entry);
void host_rewinddir(void *dirp);
int host_closedir(void *dirp);
int host_statfs(const char *path, struct statfs *buf);
int host_unlink(const char *pathname);
int host_mkdir(const char *pathname, int mode);
int host_rmdir(const char *pathname);
int host_rename(const char *oldpath, const char *newpath);
int host_stat(const char *path, struct stat *buf);
int host_chstat(const char *path, const struct stat *buf, int flags);
#endif /* __SIM__ */

#endif /* __INCLUDE_ROUX_FS_HOSTFS_H */
