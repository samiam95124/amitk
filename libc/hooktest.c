/*******************************************************************************

                                  STDIO HOOK TEST

Tests the ability to hook the I/O vector in standard I/O. This can also be used
as a template for a hooker module.

*******************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

/* file handle numbers at the system interface level */

#define INPFIL 0 /* handle to standard input */
#define OUTFIL 1 /* handle to standard output */
#define ERRFIL 2 /* handle to standard error */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef int (*punlink_t)(const char*);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_unlink(punlink_t nfp, punlink_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static punlink_t ofpunlink;
static plseek_t  ofplseek;

/*******************************************************************************

Individual hook passthrough routines

Any or all of the hookers can be intercepted and the data passing through acted
on, modified or replaced.

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)
{ return (*ofpread)(fd, buff, count); }

static ssize_t iwrite(int fd, const void* buff, size_t count)
{

    int i;
    char* s = (char*) buff;

    /*
     * All we do for this hook is copy stdout to the stderr file. This can
     * then be redirected to a file, etc.
     *
     * Note that you MUST NOT redirect stdout back to itself, or all time and
     * space would collapse (infinite loop).
     */
    if (fd == OUTFIL) for (i = 0; i < count; i++) putc(s[i], stderr);

    return (*ofpwrite)(fd, buff, count);

}

static int iopen(const char* pathname, int flags, int perm)
{ return (*ofpopen)(pathname, flags, perm); }

static int iclose(int fd)
{ return (*ofpclose)(fd); }

static int iunlink(const char* pathname)
{ return (*ofpunlink)(pathname); }

static off_t ilseek(int fd, off_t offset, int whence)
{ return (*ofplseek)(fd, offset, whence); }

/*******************************************************************************

Init and deinit routines

*******************************************************************************/

static void ami_init_hooker (void) __attribute__((constructor (102)));
static void ami_init_hooker()

{

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_unlink(iunlink, &ofpunlink);
    ovr_lseek(ilseek, &ofplseek);

}

static void ami_deinit_hooker (void) __attribute__((destructor (102)));
static void ami_deinit_hooker()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    punlink_t cppunlink;
    plseek_t cpplseek;

        /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose /* || cppunlink != iunlink */ || cpplseek != ilseek){

        fprintf(stderr, "Stdio hooks do not match the ones placed\n");
        exit(1);

    }

}
