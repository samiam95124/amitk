/*******************************************************************************
*                                                                              *
*                              CRASH TEST                                      *
*                                                                              *
* Dies on purpose, to show the crash dump at work: see linux/crashdump.c.     *
* Given no argument it divides by zero, three calls deep, so the dump has a   *
* stack to show; given one it takes that way out instead:                     *
*                                                                              *
*     crash_test            divide by zero (SIGFPE)                            *
*     crash_test null       read through a null pointer (SIGSEGV)              *
*     crash_test abort      abort()                                            *
*     crash_test stack      recurse without end (a stack overflow: SIGSEGV     *
*                           on the guard page, reported from the alternate    *
*                           stack)                                             *
*     crash_test error      a library error: the services module is asked to  *
*                           run an empty command. Without AMI_ERRABORT in the *
*                           environment that is a message and an exit, no     *
*                           dump; with it the module aborts where it is and   *
*                           the dump shows the stack.                          *
*                                                                              *
* Each is a function of its own, called through two more, so the dump's       *
* frames read innermost first as the fault, the caller, the caller's caller,  *
* and main. The divisor and the pointer come from the command line's length   *
* so the compiler cannot fold the fault away.                                  *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <localdefs.h>
#include <services.h>

static volatile int zero;      /* set from main: the compiler cannot see it is zero */
static volatile int* nowhere;  /* and this null */

static int divide(int n) { return (n/zero); }

static int fetch(void) { return (*nowhere); }

static int descend(int n) { char pad[4096]; pad[0] = (char)n; return (descend(n+1)+pad[0]); }

static int liberror(void) { ami_long e; ami_execw("", &e); return ((int)e); }

static int middle(const char* how)

{

    if (!strcmp(how, "null")) return (fetch());
    if (!strcmp(how, "abort")) abort();
    if (!strcmp(how, "stack")) return (descend(0));
    if (!strcmp(how, "error")) return (liberror());

    return (divide(100));

}

static int outer(const char* how)

{

    printf("crash_test: about to die by %s\n", how);
    fflush(stdout);

    return (middle(how));

}

int main(int argc, char* argv[])

{

    const char* how = argc > 1? argv[1]: "divide";

    zero = argc-argc;     /* zero, but not to the compiler */
    nowhere = (int*)(long)(argc-argc);

    return (outer(how));

}
