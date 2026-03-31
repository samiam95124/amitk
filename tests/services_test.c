#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <services.h>

#define MAXSTR 100
#define SECOND 10000

static void prttimdat(long t)

{

    if (t == -LONG_MAX) printf("********** ******** ");
    else {

        pa_writedate(stdout, pa_local(t));
        putchar(' ');
        pa_writetime(stdout, pa_local(t));
        putchar(' ');

    }

}

static void prtperm(pa_permset p)

{

    if (INISET(p, pa_pmread)) putchar('r'); else putchar(' ');
    if (INISET(p, pa_pmwrite)) putchar('w'); else putchar(' ');
    if (INISET(p, pa_pmexec)) putchar('e'); else putchar(' ');
    if (INISET(p, pa_pmdel)) putchar('d'); else putchar(' ');
    if (INISET(p, pa_pmvis)) putchar('v'); else putchar(' ');
    if (INISET(p, pa_pmcopy)) putchar('c'); else putchar(' ');
    if (INISET(p, pa_pmren)) putchar('m'); else putchar(' ');
    putchar(' ');

}

static void waittime(int t)

{

    int ct = pa_clock();

    while (pa_elapsed(ct) < t);

}

int main(void)

{

    pa_filptr lp;
    pa_envptr ep;
    int       t;
    char      s[MAXSTR], s2[MAXSTR], s3[MAXSTR];
    int       c;
    int       err;
    char      p[MAXSTR], n[MAXSTR], e[MAXSTR];
    pa_filptr fla;
    FILE*     fp;
    int       i;
    pa_chrset sc;

    printf("Services module test v1.0\n");
    printf("\n");
    printf("test1:\n");
    pa_list("*", &lp);
    while (lp) {

        printf("%-25s %-10lld %-10lld ", lp->name, lp->size, lp->alloc);
        if (INISET(lp->attr, pa_atexec)) putchar('e'); else putchar(' ');
        if (INISET(lp->attr, pa_atarc)) putchar('a'); else putchar(' ');
        if (INISET(lp->attr, pa_atsys)) putchar('s'); else putchar(' ');
        if (INISET(lp->attr, pa_atdir)) putchar('d'); else putchar(' ');
        if (INISET(lp->attr, pa_atloop)) putchar('l'); else putchar(' ');
        putchar(' ');
        prttimdat(lp->create);
        prttimdat(lp->modify);
        prttimdat(lp->access);
        prttimdat(lp->backup);
        prtperm(lp->user);
        prtperm(lp->group);
        prtperm(lp->other);
        putchar('\n');
        lp = lp->next;

    }
    printf("s/b <the listing for the current directory>\n");
    printf("test1.l:\n");
    pa_listl("*", strlen("*"), &lp);
    while (lp) {

        printf("%-25s %-10lld %-10lld ", lp->name, lp->size, lp->alloc);
        if (INISET(lp->attr, pa_atexec)) putchar('e'); else putchar(' ');
        if (INISET(lp->attr, pa_atarc)) putchar('a'); else putchar(' ');
        if (INISET(lp->attr, pa_atsys)) putchar('s'); else putchar(' ');
        if (INISET(lp->attr, pa_atdir)) putchar('d'); else putchar(' ');
        if (INISET(lp->attr, pa_atloop)) putchar('l'); else putchar(' ');
        putchar(' ');
        prttimdat(lp->create);
        prttimdat(lp->modify);
        prttimdat(lp->access);
        prttimdat(lp->backup);
        prtperm(lp->user);
        prtperm(lp->group);
        prtperm(lp->other);
        putchar('\n');
        lp = lp->next;

    }
    printf("s/b <the listing for the current directory>\n");

    pa_times(s, MAXSTR, pa_time());
    printf("test 3: %s s/b <the current time in zulu>\n", s);
    pa_times(s, MAXSTR, pa_local(pa_time()));
    printf("test 5: %s s/b <the current time in local>\n", s);
    pa_dates(s, MAXSTR, pa_local(pa_time()));
    printf("test 7: %s s/b <the current date>\n", s);
    printf("test 9: ");
    pa_writetime(stdout, pa_local(pa_time()));
    printf(" s/b <the time>\n");
    printf("test 10: ");
    pa_writedate(stdout, pa_local(pa_time()));
    printf(" s/b <the date>\n");
    t = pa_clock();
    printf("test11: waiting 1 second\n");
    waittime(SECOND);
    printf("test 11: %ld s/b %d (approximate)\n", pa_elapsed(t), SECOND);
    printf("test 12: %d s/b 1\n", pa_validfile("c:\\just\\fargle.com"));
    printf("test 14: %d s/b 1\n", pa_wild("c:\\fargle.c?m"));
    printf("test 15: %d s/b 1\n", pa_validfile("c:\\far*gle.com"));
    printf("test 17  %d s/b 1\n", pa_wild("c:\\for?.txt"));
    printf("test 18: %d s/b 1\n", pa_wild("c:\\for*.txt"));
    printf("test 19: %d s/b 0\n", pa_wild("c:\\fork.txt"));
    pa_setenv("barkbark", "what is this");
    pa_getenv("barkbark", s, MAXSTR);
    printf("test20: %s s/b what is this\n", s);
    pa_remenv("barkbark");
    pa_getenv("barkbark", s, MAXSTR);
    printf("test22: \"%s\" s/b \"\"\n", s);
    pa_allenv(&ep);
    printf("test23:\n");
    i = 10;
    while (ep != 0 && i > 0) {

       printf("Name: %s Data: %s\n", ep->name, ep->data);
       ep = ep->next;
       i--;

    }
    printf("s/b <10 entries from the current environment>\n");
    printf("test24:\n");
    pa_exec("services_test1");
    printf("waiting 5 seconds for program to start\n");
    waittime(SECOND*5);
    printf("s/b This is services_test1 \"\" (empty string)\n");
    printf("test25:\n");
    pa_execw("services_test1", &err);
    printf("%d\n", err);
    printf("s/b\n");
    printf("This is services_test1 \"\"\n");
    printf("0\n");
    printf("test26:\n");
    ep = malloc(sizeof(pa_envrec));
    ep->name = malloc(5);
    ep->data = malloc(9);
    strcpy(ep->name, "bark");
    strcpy(ep->data, "hi there");
    ep->next = 0;
    pa_exece("services_test1", ep);
    printf("waiting 5 seconds\n");
    waittime(SECOND*5);
    printf("s/b This is services_test1: \"hi there\"\n");
    printf("test27:\n");
    pa_execew("services_test1", ep, &err);
    printf("%d\n", err);
    printf("s/b\n");
    printf("This is services_test1 \"hi there\"\n");
    printf("0\n");
    pa_getcur(s, MAXSTR);
    printf("test 29: %s s/b <the current path>\n", s);
    pa_getcur(s, MAXSTR);
    pa_getusr(s3, MAXSTR);
    pa_setcur(s3);
    pa_getcur(s2, MAXSTR);
    printf("test 30: %s s/b <the user path>\n", s2);
    pa_setcur(s);
    pa_getcur(s, MAXSTR);
    printf("test 31: %s s/b <the current path>\n", s);
    pa_brknam("c:\\what\\ho\\junk.com", p, MAXSTR, n, MAXSTR, e, MAXSTR);
    printf("test 32: Path: %s Name: %s Ext: %s ", p, n, e);
    printf("s/b: Path: c:\\what\\ho\\ Name: junk Ext: com\n");
    pa_maknam(s, MAXSTR, p, n, e);
    printf("test 33: %s s/b c:\\what\\ho\\junk.com\n", s);
    strcpy(s, "junk");
    pa_fulnam(s, MAXSTR);
    printf("test 36: %s s/b <path>junk\n", s);
    pa_getpgm(s, MAXSTR);
    printf("test 38: %s s/b <the program path>\n", s);
    pa_getusr(s, MAXSTR);
    printf("test 40: %s s/b <the user path>\n", s);
    fp = fopen("junk", "w");
    fclose(fp);
    /* Linux cannot set or reset attributes */
#ifndef __linux
    printf("test 42: ");
    pa_setatr("junk", BIT(pa_atarc));
    pa_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, pa_atarc));
    printf(" s/b junk 1\n");
    printf("test 43: ");
    pa_resatr("junk", BIT(pa_atarc));
    pa_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, pa_atarc));
    printf(" s/b junk 0\n");
    printf("test 44: ");
    pa_setatr("junk", BIT(pa_atsys));
    pa_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, pa_atsys));
    printf(" s/b junk 1\n");
    printf("test 45: ");
    pa_resatr("junk", BIT(pa_atsys));
    pa_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, pa_atsys));
    printf(" s/b junk 0\n");
#endif
    printf("test 46: ");
    pa_setuper("junk", BIT(pa_pmwrite));
    pa_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->user, pa_pmwrite));
    printf(" s/b junk 1\n");
    printf("test 47: ");
    pa_resuper("junk", BIT(pa_pmwrite));
    pa_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->user, pa_pmwrite));
    printf(" s/b junk 0\n");
    pa_setuper("junk", BIT(pa_pmwrite));
    unlink("junk");
    printf("test 48: ");
    pa_makpth("junk");
    pa_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, pa_atdir));
    printf(" s/b junk 1\n");
    printf("test 49: ");
    pa_rempth("junk");
    pa_list("junk", &fla);
    printf("%d s/b 1\n", fla == 0);
    pa_filchr(sc);
    printf("test 50: Set of valid characters: ");
    for (i = 0; i < 126; i++) if (INCSET(sc, i)) putchar(i);
    printf("\n");

    printf("test 51: Option character: %c\n", pa_optchr());
    printf("test 52: Path character: %c\n", pa_pthchr());
    printf("test 53: Latitude: %d\n", pa_latitude());
    printf("test 54: longitude: %d\n", pa_longitude());
    printf("test 55: Altitude: %d\n", pa_altitude());
    printf("test 56: Country code: %d\n", pa_country());
    pa_countrys(s, 100, pa_country());
    printf("test 57: Country name: %s\n", s);
    printf("test 58: Timezone: %d\n", pa_timezone());
    printf("test 59: Daysave: %d\n", pa_daysave());
    printf("test 60: 24 hour time: %d\n", pa_time24hour());
    printf("test 61: Language: %d\n", pa_language());
    pa_languages(s, 100, pa_language());
    printf("test 62: Language name: %s\n", s);
    printf("test 63: Decimal character: %c\n", pa_decimal());
    printf("test 64: Separator character: %c\n", pa_numbersep());
    printf("test 65: Time order: %d\n", pa_timeorder());
    printf("test 66: Date order: %d\n", pa_dateorder());
    printf("test 67: Date separator: %c\n", pa_datesep());
    printf("test 68: time separator: %c\n", pa_timesep());
    printf("test 69: Currency character: %c\n", pa_currchr());

    return (0); /* exit no error */

}
