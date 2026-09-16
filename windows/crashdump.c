/*******************************************************************************
*                                                                              *
*                              CRASH DUMP                                      *
*                                                                              *
* Windows version. Says where a program died, as linux/crashdump.c does: on a  *
* fatal exception -- an access violation, an illegal instruction, an integer   *
* or floating point fault, a stack overflow -- or on an abort, the thread     *
* that took it writes to the error channel what it was, the address it        *
* faulted on, which thread it was, and the stack of calls that led there,     *
* then lets the exception take its course as it would have, so Windows error  *
* reporting sees it as before.                                                 *
*                                                                              *
* The exceptions come through the unhandled exception filter, which Windows   *
* runs on the faulting thread with the thread's context at the fault; the     *
* stack is walked from that context with StackWalk64, so its first frame is   *
* the fault itself, not this handler. An abort is a signal, SIGABRT, raised   *
* by the C runtime, and is caught as one; its stack is this thread's, walked  *
* from here, with this handler's own frame left off. The frames are named by  *
* the module they fall in and the offset in it, and by symbol where DbgHelp   *
* has one. For the program's own frames the handler runs addr2line, where     *
* there is one on the path, which gives the function and the line from the    *
* debug information the build carries; the addresses are put back to where   *
* the linker placed the program, read from the program file, since the       *
* loader may have moved it.                                                    *
*                                                                              *
* DbgHelp is loaded when the report is made, not linked, so the module calls  *
* nothing else and any program that links a Petit-Ami library gets it,        *
* whether or not it uses anything else. Compiled in when the Makefile has     *
* CRASHDUMP=1, the default; CRASHDUMP=0 leaves it out. AMI_CRASHDUMP=0 in the *
* environment turns it off in a program built with it, for a debugger that    *
* wants the exception raw.                                                     *
*                                                                              *
* The library's own errors are reported by each module's error routine, which *
* says its piece and exits: no exception, so no dump of its own. A routine    *
* that wants the stack with its message calls ami_dumpstack(), through a weak *
* reference so that a build without this module calls nothing; the graphics  *
* module does. With AMI_ERRABORT set in the environment every module aborts   *
* there instead, and the abort comes through here like any other fault.       *
*                                                                              *
* A stack overflow leaves the thread no room to report in, so the main thread *
* is given a guaranteed reserve for the handler at the start; a thread of the *
* program's own making has none unless it asks, and its overflow is reported  *
* as far as the room allows.                                                   *
*                                                                              *
*******************************************************************************/

#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

#define MAXFRAMES 64     /* frames reported */
#define GUARANTEE 262144 /* the handler's reserve on the main thread's stack */

static char      exepath[MAX_PATH]; /* the program file, for addr2line */
static uintptr_t exebase;           /* where it is loaded */
static uintptr_t exeend;
static uintptr_t prefbase;          /* where the linker placed it */
static char      a2lpath[MAX_PATH]; /* addr2line, where there is one */
static int       reporting;         /* a report is under way: a fault in it is not reported */
static LPTOP_LEVEL_EXCEPTION_FILTER prevfilter; /* the filter that was there */

/* DbgHelp, loaded for the report */
typedef BOOL    (WINAPI *syminit_t)(HANDLE, PCSTR, BOOL);
typedef DWORD   (WINAPI *symopts_t)(DWORD);
typedef BOOL    (WINAPI *symaddr_t)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL    (WINAPI *stkwalk_t)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID,
                                    PREAD_PROCESS_MEMORY_ROUTINE64,
                                    PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                    PGET_MODULE_BASE_ROUTINE64,
                                    PTRANSLATE_ADDRESS_ROUTINE64);
static syminit_t                        symInitialize;
static symopts_t                        symSetOptions;
static symaddr_t                        symFromAddr;
static stkwalk_t                        stackWalk64;
static PFUNCTION_TABLE_ACCESS_ROUTINE64 symFunctionTableAccess64;
static PGET_MODULE_BASE_ROUTINE64       symGetModuleBase64;
static int                              symready; /* DbgHelp is up */

/* Writes to the error channel beneath any stdio: the terminal module owns
   standard output, and a report through it would land on the screen. */
static void put(const char* s)

{

    DWORD w;

    WriteFile(GetStdHandle(STD_ERROR_HANDLE), s, (DWORD)strlen(s), &w, NULL);

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

static const char* exename(void)

{

    const char* s = strrchr(exepath, '\\');

    if (!s) s = strrchr(exepath, '/');
    return (s? s+1: *exepath? exepath: "program");

}

static const char* excname(DWORD code)

{

    switch (code) {

        case EXCEPTION_ACCESS_VIOLATION:      return ("an access violation");
        case EXCEPTION_IN_PAGE_ERROR:         return ("an in-page error");
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return ("an array bounds fault");
        case EXCEPTION_DATATYPE_MISALIGNMENT: return ("a misaligned access");
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return ("an illegal instruction");
        case EXCEPTION_PRIV_INSTRUCTION:      return ("a privileged instruction");
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return ("an integer divide by zero");
        case EXCEPTION_INT_OVERFLOW:          return ("an integer overflow");
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return ("a floating point divide by zero");
        case EXCEPTION_FLT_INVALID_OPERATION: return ("a floating point invalid operation");
        case EXCEPTION_FLT_OVERFLOW:          return ("a floating point overflow");
        case EXCEPTION_FLT_UNDERFLOW:         return ("a floating point underflow");
        case EXCEPTION_FLT_DENORMAL_OPERAND:  return ("a floating point denormal operand");
        case EXCEPTION_FLT_INEXACT_RESULT:    return ("a floating point inexact result");
        case EXCEPTION_FLT_STACK_CHECK:       return ("a floating point stack fault");
        case EXCEPTION_STACK_OVERFLOW:        return ("a stack overflow");
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return ("a noncontinuable exception");
        default:                              return ("a fatal exception");

    }

}

/* DbgHelp: loaded and started for the report. Without it the frames are
   named by module and offset alone, from the loader. */
static void symup(void)

{

    HMODULE h;

    if (symready) return;
    h = LoadLibraryA("dbghelp.dll");
    if (!h) return;
    symInitialize = (syminit_t)GetProcAddress(h, "SymInitialize");
    symSetOptions = (symopts_t)GetProcAddress(h, "SymSetOptions");
    symFromAddr = (symaddr_t)GetProcAddress(h, "SymFromAddr");
    stackWalk64 = (stkwalk_t)GetProcAddress(h, "StackWalk64");
    symFunctionTableAccess64 =
        (PFUNCTION_TABLE_ACCESS_ROUTINE64)GetProcAddress(h, "SymFunctionTableAccess64");
    symGetModuleBase64 =
        (PGET_MODULE_BASE_ROUTINE64)GetProcAddress(h, "SymGetModuleBase64");
    if (!symInitialize || !stackWalk64 || !symFunctionTableAccess64 ||
        !symGetModuleBase64) return;
    if (symSetOptions) symSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_LOAD_ANYTHING);
    if (!symInitialize(GetCurrentProcess(), NULL, TRUE)) return;
    symready = 1;

}

/* the stack from a context, innermost first: the frames' program counters */
static int walk(CONTEXT* cx, uintptr_t* frames, int max)

{

    STACKFRAME64 sf;
    DWORD        mach;
    int          n = 0;

    if (!symready) return (0);
    memset(&sf, 0, sizeof(sf));
#ifdef _WIN64
    mach = IMAGE_FILE_MACHINE_AMD64;
    sf.AddrPC.Offset = cx->Rip;
    sf.AddrFrame.Offset = cx->Rbp;
    sf.AddrStack.Offset = cx->Rsp;
#else
    mach = IMAGE_FILE_MACHINE_I386;
    sf.AddrPC.Offset = cx->Eip;
    sf.AddrFrame.Offset = cx->Ebp;
    sf.AddrStack.Offset = cx->Esp;
#endif
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Mode = AddrModeFlat;
    while (n < max &&
           stackWalk64(mach, GetCurrentProcess(), GetCurrentThread(), &sf, cx, NULL,
                       symFunctionTableAccess64, symGetModuleBase64, NULL)) {

        if (!sf.AddrPC.Offset) break;
        /* a stack overflow leaves frames the walk misreads, as addresses
           where no code can be: those are left out */
        if (sf.AddrPC.Offset < 0x10000) continue;
        frames[n++] = (uintptr_t)sf.AddrPC.Offset;

    }

    return (n);

}

/* a frame as the loader and DbgHelp know it: the module and the offset in
   it, and the symbol where there is one, then the address */
static void rawframe(uintptr_t a)

{

    HMODULE     hm = NULL;
    char        mod[MAX_PATH];
    const char* s;
    char        buf[sizeof(SYMBOL_INFO)+256];
    SYMBOL_INFO* si = (SYMBOL_INFO*)buf;
    DWORD64     disp = 0;

    put("  ");
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)a, &hm) && hm &&
        GetModuleFileNameA(hm, mod, sizeof(mod))) {

        s = strrchr(mod, '\\');
        put(s? s+1: mod);
        memset(buf, 0, sizeof(buf));
        si->SizeOfStruct = sizeof(SYMBOL_INFO);
        si->MaxNameLen = 255;
        if (symready && symFromAddr && symFromAddr(GetCurrentProcess(), a, &disp, si)) {

            put("("); put(si->Name); put("+"); puthex((uintptr_t)disp); put(")");

        } else { put("+"); puthex(a-(uintptr_t)hm); }
        put(" ");

    }
    put("["); puthex(a); put("]\n");

}

/* the frames, a recursion's repeats folded to one */
static void rawlist(const uintptr_t* frames, int n)

{

    int i, same = 0;

    for (i = 0; i < n; i++)
        if (i && frames[i] == frames[i-1]) same++;
        else rawframe(frames[i]);
    if (same) { put("  ("); putdec(same); put(" repeats of a frame left out)\n"); }

}

/* Where the linker placed the program, which is where addr2line's addresses
   are: read from the program file, since the loader, having moved the image,
   writes the base it chose into the headers in memory. */
static uintptr_t linkedbase(void)

{

    HANDLE            f;
    DWORD             r;
    IMAGE_DOS_HEADER  dos;
    IMAGE_NT_HEADERS  nt;
    uintptr_t         b = 0;

    f = CreateFileA(exepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return (0);
    if (ReadFile(f, &dos, sizeof(dos), &r, NULL) && r == sizeof(dos) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE &&
        SetFilePointer(f, dos.e_lfanew, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
        ReadFile(f, &nt, sizeof(nt), &r, NULL) && r == sizeof(nt) &&
        nt.Signature == IMAGE_NT_SIGNATURE)
        b = (uintptr_t)nt.OptionalHeader.ImageBase;
    CloseHandle(f);

    return (b);

}

/* the program's own frames, by name and line, through addr2line; the first
   frame is the fault itself where exact says so, a return address otherwise,
   and a return address is taken back one to the call, whose line it is */
static void symbolic(const uintptr_t* frames, int n, int exact)

{

    char      cmd[MAX_PATH*2+MAXFRAMES*24];
    char*     p;
    int       i, k = 0, same = 0;
    uintptr_t last = 0;
    STARTUPINFOA        si;
    PROCESS_INFORMATION pi;

    if (!exebase || !*exepath || !*a2lpath) return;
    if (!prefbase) prefbase = linkedbase();
    if (!prefbase) return;
    p = cmd;
    p += sprintf(p, "\"%s\" -f -C -p -a -e \"%s\"", a2lpath, exepath);
    for (i = 0; i < n && k < MAXFRAMES; i++) {

        uintptr_t a = frames[i];

        if (a < exebase || a >= exeend) continue; /* another module's */
        a = a-exebase+prefbase; /* as the linker placed it */
        if (!(exact && i == 0)) a--; /* the call, not the return */
        if (a == last) { same++; continue; }
        last = a;
        p += sprintf(p, " 0x%llx", (unsigned long long)a);
        k++;

    }
    if (!k) return;
    put("\n  by line, the program's own frames (addresses as linked");
    if (same) { put(", "); putdec(same); put(" repeats of a frame left out"); }
    put("):\n");
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_ERROR_HANDLE); /* its lines go where the rest went */
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {

        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

    }

}

static void header(const char* what)

{

    put("\n*** ");
    put(exename());
    put(" took ");
    put(what);

}

static void footer(void)

{

    put(", thread ");
    putdec((long)GetCurrentThreadId());
    put(" of process ");
    putdec((long)GetCurrentProcessId());
    put(" ***\n  stack, innermost first:\n");

}

/* the unhandled exception filter: the fault, with the thread's context at it */
static LONG WINAPI crashfilter(EXCEPTION_POINTERS* ep)

{

    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    CONTEXT           cx;
    uintptr_t         frames[MAXFRAMES];
    int               n;

    if (!reporting) {

        reporting = 1;
        header(excname(er->ExceptionCode));
        if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
            er->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {

            put(er->ExceptionInformation[0] == 1? ", writing":
                er->ExceptionInformation[0] == 8? ", executing": ", reading");
            put(" address ");
            puthex((uintptr_t)er->ExceptionInformation[1]);

        }
        put(" at ");
        puthex((uintptr_t)er->ExceptionAddress);
        footer();
        symup();
        cx = *ep->ContextRecord; /* the walk consumes its copy */
        n = walk(&cx, frames, MAXFRAMES);
        if (n) { rawlist(frames, n); symbolic(frames, n, 1); }
        else { rawframe((uintptr_t)er->ExceptionAddress); }
        put("  (the exception takes its course from here)\n\n");

    }

    return (prevfilter? prevfilter(ep): EXCEPTION_CONTINUE_SEARCH);

}

/* an abort: the C runtime raises SIGABRT, and this thread's stack is walked
   from here */
static void aborthandler(int sig)

{

    CONTEXT   cx;
    uintptr_t frames[MAXFRAMES];
    int       n;

    (void)sig;
    if (!reporting) {

        reporting = 1;
        header("SIGABRT, abort");
        footer();
        symup();
        RtlCaptureContext(&cx);
        n = walk(&cx, frames, MAXFRAMES);
        /* the first frame is this handler: the abort is the frames after */
        if (n > 1) { rawlist(frames+1, n-1); symbolic(frames+1, n-1, 0); }
        put("  (the abort takes its course from here)\n\n");

    }
    signal(SIGABRT, SIG_DFL);
    raise(SIGABRT);

}

/* The stack of the calling thread, on the error channel, for a module's
   error routine: where the program was when the module found the error.
   The program goes on to whatever the routine does next. Called through a
   weak reference, so a build without the module calls nothing. */
void ami_dumpstack(void)

{

    CONTEXT   cx;
    uintptr_t frames[MAXFRAMES];
    int       n;

    put("  stack, innermost first:\n");
    symup();
    RtlCaptureContext(&cx);
    n = walk(&cx, frames, MAXFRAMES);
    /* the first frame is this routine: the caller is the frames after */
    if (n > 1) { rawlist(frames+1, n-1); symbolic(frames+1, n-1, 0); }
    put("\n");

}

__attribute__((constructor))
static void crashdump_init(void)

{

    const char*        e = getenv("AMI_CRASHDUMP");
    HMODULE            hm;
    IMAGE_DOS_HEADER*  dos;
    IMAGE_NT_HEADERS*  nt;
    ULONG              room = GUARANTEE;

    if (e && !strcmp(e, "0")) return;
    if (!GetModuleFileNameA(NULL, exepath, sizeof(exepath))) exepath[0] = 0;
    hm = GetModuleHandleA(NULL);
    if (hm) {

        dos = (IMAGE_DOS_HEADER*)hm;
        nt = (IMAGE_NT_HEADERS*)((char*)hm+dos->e_lfanew);
        exebase = (uintptr_t)hm;
        exeend = exebase+nt->OptionalHeader.SizeOfImage;

    }
    if (!SearchPathA(NULL, "addr2line", ".exe", sizeof(a2lpath), a2lpath, NULL))
        a2lpath[0] = 0;
    /* room to report a stack overflow in */
    SetThreadStackGuarantee(&room);
    prevfilter = SetUnhandledExceptionFilter(crashfilter);
    signal(SIGABRT, aborthandler);

}
