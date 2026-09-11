/*******************************************************************************
*                                                                              *
*                              CRASH DUMP                                      *
*                                                                              *
* Says where a program died. On a fatal signal -- a segmentation fault, a bus  *
* error, an illegal instruction, a floating point fault, an abort -- the       *
* thread that took it writes to the error channel what the signal was, the    *
* address it faulted on, which thread it was, and the stack of calls that     *
* led there, then lets the signal take the process down as it would have, so  *
* the core file is still made.                                                 *
*                                                                              *
* The stack comes from glibc's backtrace(), which walks the frames of the      *
* running thread and is safe to call from a signal handler. Its names are the *
* names the program exports, which is why the Makefile links the programs     *
* with --export-dynamic when this is on: without that a static function is a  *
* bare offset. For the rest, and for line numbers, the handler runs           *
* addr2line over the frames that fall in the program itself, when it is       *
* there; a stripped or an unfamiliar frame is left as its offset, which       *
* addr2line -e <program> can be asked about by hand.                          *
*                                                                              *
* Compiled in when the Makefile has CRASHDUMP=1, which is the default on      *
* Linux; CRASHDUMP=0 leaves it out. It is a module of its own and calls       *
* nothing else in the library, so any program that links a Petit-Ami          *
* library gets it, whether or not it uses anything else. AMI_CRASHDUMP=0 in   *
* the environment turns it off in a program built with it, for a debugger    *
* that wants the signal raw.                                                   *
*                                                                              *
* The handler runs on a stack of its own, so a stack overflow is reported     *
* like any other fault rather than faulting again in the report.              *
*                                                                              *
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <link.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdint.h>

#define MAXFRAMES 64  /* frames reported */
#define STACKSIZE 65536 /* the handler's own stack */

static char      exepath[1024];  /* the program file, for addr2line */
static uintptr_t exebase;        /* where it is loaded */
static uintptr_t exeend;
static char      altstack[STACKSIZE];

/* Writes that are safe in a signal handler: no stdio. */
static void put(const char* s)

{

    ssize_t r = write(2, s, strlen(s));

    (void)r;

}

static void puthex(uintptr_t v)

{

    char b[2+sizeof(v)*2+1];
    int  i = sizeof(b)-1;

    b[i] = 0;
    do { b[--i] = "0123456789abcdef"[v&15]; v >>= 4; } while (v);
    b[--i] = 'x'; b[--i] = '0';
    put(b+i);

}

static void putdec(long v)

{

    char b[24];
    int  i = sizeof(b)-1;
    int  neg = v < 0;

    b[i] = 0;
    if (neg) v = -v;
    do { b[--i] = '0'+v%10; v /= 10; } while (v);
    if (neg) b[--i] = '-';
    put(b+i);

}

static const char* signame(int sig)

{

    switch (sig) {

        case SIGSEGV: return ("SIGSEGV, segmentation fault");
        case SIGBUS:  return ("SIGBUS, bus error");
        case SIGILL:  return ("SIGILL, illegal instruction");
        case SIGFPE:  return ("SIGFPE, arithmetic fault");
        case SIGABRT: return ("SIGABRT, abort");
        default:      return ("fatal signal");

    }

}

/* the program's own frames, by name and line, through addr2line: the
   frames outside it are left as backtrace wrote them */
static void symbolic(void* const* frames, int n)

{

    char* argv[MAXFRAMES+8];
    char  offs[MAXFRAMES][2+sizeof(uintptr_t)*2+1];
    int   ac = 0, i, k;
    pid_t pid;

    if (!exebase || !*exepath || access("/usr/bin/addr2line", X_OK)) return;
    argv[ac++] = "addr2line";
    argv[ac++] = "-f";   /* the function */
    argv[ac++] = "-C";   /* names as written */
    argv[ac++] = "-p";   /* one line a frame */
    argv[ac++] = "-a";   /* with the address asked about */
    argv[ac++] = "-e";
    argv[ac++] = exepath;
    for (i = 0, k = 0; i < n && k < MAXFRAMES; i++) {

        uintptr_t a = (uintptr_t)frames[i];

        if (a < exebase || a >= exeend) continue; /* another module's */
        a -= exebase;
        a--; /* the call, not the return: the return may be the next line */
        {   /* hex, by hand */
            char* b = offs[k]+sizeof(offs[k])-1;
            uintptr_t v = a;
            *b = 0;
            do { *--b = "0123456789abcdef"[v&15]; v >>= 4; } while (v);
            *--b = 'x'; *--b = '0';
            argv[ac++] = b;
            k++;
        }

    }
    argv[ac] = NULL;
    if (!k) return;
    put("\n  by line, the program's own frames (offsets in the program):\n");
    pid = fork();
    if (pid == 0) {

        dup2(2, 1); /* addr2line's lines go where the rest went */
        execv("/usr/bin/addr2line", argv);
        _exit(127);

    } else if (pid > 0) {

        int st;

        while (waitpid(pid, &st, 0) < 0 && errno == EINTR) ;

    }

}

static void crashhandler(int sig, siginfo_t* si, void* uc)

{

    void* frames[MAXFRAMES];
    int   n;
    struct sigaction sa;

    (void)uc;
    put("\n*** ");
    put(*exepath? strrchr(exepath, '/')? strrchr(exepath, '/')+1: exepath: "program");
    put(" took ");
    put(signame(sig));
    if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE) {

        put(" at address ");
        puthex((uintptr_t)si->si_addr);

    }
    put(", thread ");
    putdec((long)gettid());
    put(" of process ");
    putdec((long)getpid());
    put(" ***\n  stack, innermost first:\n");
    n = backtrace(frames, MAXFRAMES);
    /* the first two frames are this handler and the trampoline the
       kernel returns through: the fault is the frame after them */
    if (n > 2) { backtrace_symbols_fd(frames+2, n-2, 2); symbolic(frames+2, n-2); }
    else { backtrace_symbols_fd(frames, n, 2); symbolic(frames, n); }
    put("  (the core file, if one is kept, holds the rest)\n\n");
    /* the signal takes its course: the default action, and the core */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
    raise(sig);

}

/* where the program is loaded, from the loader's list: the first entry
   is the program itself */
static int phdrcb(struct dl_phdr_info* info, size_t size, void* data)

{

    int i;

    (void)size; (void)data;
    exebase = info->dlpi_addr;
    exeend = exebase;
    for (i = 0; i < info->dlpi_phnum; i++)
        if (info->dlpi_phdr[i].p_type == PT_LOAD) {

            uintptr_t e = exebase+info->dlpi_phdr[i].p_vaddr+info->dlpi_phdr[i].p_memsz;

            if (e > exeend) exeend = e;

        }

    return (1); /* the first is enough */

}

__attribute__((constructor))
static void crashdump_init(void)

{

    struct sigaction sa;
    stack_t          ss;
    ssize_t          l;
    const char*      e = getenv("AMI_CRASHDUMP");

    if (e && !strcmp(e, "0")) return;
    l = readlink("/proc/self/exe", exepath, sizeof(exepath)-1);
    if (l > 0) exepath[l] = 0; else exepath[0] = 0;
    dl_iterate_phdr(phdrcb, NULL);
    /* a stack of its own, so a stack overflow can be reported */
    ss.ss_sp = altstack;
    ss.ss_size = sizeof(altstack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crashhandler;
    sa.sa_flags = SA_SIGINFO|SA_ONSTACK|SA_NODEFER|SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

}
