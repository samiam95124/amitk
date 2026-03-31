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

        ami_writedate(stdout, ami_local(t));
        putchar(' ');
        ami_writetime(stdout, ami_local(t));
        putchar(' ');

    }

}

static void prtperm(ami_permset p)

{

    if (INISET(p, ami_pmread)) putchar('r'); else putchar(' ');
    if (INISET(p, ami_pmwrite)) putchar('w'); else putchar(' ');
    if (INISET(p, ami_pmexec)) putchar('e'); else putchar(' ');
    if (INISET(p, ami_pmdel)) putchar('d'); else putchar(' ');
    if (INISET(p, ami_pmvis)) putchar('v'); else putchar(' ');
    if (INISET(p, ami_pmcopy)) putchar('c'); else putchar(' ');
    if (INISET(p, ami_pmren)) putchar('m'); else putchar(' ');
    putchar(' ');

}

static void waittime(int t)

{

    int ct = ami_clock();

    while (ami_elapsed(ct) < t);

}

int main(void)

{

    ami_filptr lp;
    ami_envptr ep;
    int       t;
    char      s[MAXSTR], s2[MAXSTR], s3[MAXSTR];
    int       c;
    int       err;
    char      p[MAXSTR], n[MAXSTR], e[MAXSTR];
    ami_filptr fla;
    FILE*     fp;
    int       i;
    ami_chrset sc;

    printf("Services module test v1.0\n");
    printf("\n");
    printf("test1:\n");
    ami_list("*", &lp);
    while (lp) {

        printf("%-25s %-10lld %-10lld ", lp->name, lp->size, lp->alloc);
        if (INISET(lp->attr, ami_atexec)) putchar('e'); else putchar(' ');
        if (INISET(lp->attr, ami_atarc)) putchar('a'); else putchar(' ');
        if (INISET(lp->attr, ami_atsys)) putchar('s'); else putchar(' ');
        if (INISET(lp->attr, ami_atdir)) putchar('d'); else putchar(' ');
        if (INISET(lp->attr, ami_atloop)) putchar('l'); else putchar(' ');
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
    ami_listl("*", strlen("*"), &lp);
    while (lp) {

        printf("%-25s %-10lld %-10lld ", lp->name, lp->size, lp->alloc);
        if (INISET(lp->attr, ami_atexec)) putchar('e'); else putchar(' ');
        if (INISET(lp->attr, ami_atarc)) putchar('a'); else putchar(' ');
        if (INISET(lp->attr, ami_atsys)) putchar('s'); else putchar(' ');
        if (INISET(lp->attr, ami_atdir)) putchar('d'); else putchar(' ');
        if (INISET(lp->attr, ami_atloop)) putchar('l'); else putchar(' ');
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

    ami_times(s, MAXSTR, ami_time());
    printf("test 3: %s s/b <the current time in zulu>\n", s);
    ami_times(s, MAXSTR, ami_local(ami_time()));
    printf("test 5: %s s/b <the current time in local>\n", s);
    ami_dates(s, MAXSTR, ami_local(ami_time()));
    printf("test 7: %s s/b <the current date>\n", s);
    printf("test 9: ");
    ami_writetime(stdout, ami_local(ami_time()));
    printf(" s/b <the time>\n");
    printf("test 10: ");
    ami_writedate(stdout, ami_local(ami_time()));
    printf(" s/b <the date>\n");
    t = ami_clock();
    printf("test11: waiting 1 second\n");
    waittime(SECOND);
    printf("test 11: %ld s/b %d (approximate)\n", ami_elapsed(t), SECOND);
    printf("test 12: %d s/b 1\n", ami_validfile("c:\\just\\fargle.com"));
    printf("test 14: %d s/b 1\n", ami_wild("c:\\fargle.c?m"));
    printf("test 15: %d s/b 1\n", ami_validfile("c:\\far*gle.com"));
    printf("test 17  %d s/b 1\n", ami_wild("c:\\for?.txt"));
    printf("test 18: %d s/b 1\n", ami_wild("c:\\for*.txt"));
    printf("test 19: %d s/b 0\n", ami_wild("c:\\fork.txt"));
    ami_setenv("barkbark", "what is this");
    ami_getenv("barkbark", s, MAXSTR);
    printf("test20: %s s/b what is this\n", s);
    ami_remenv("barkbark");
    ami_getenv("barkbark", s, MAXSTR);
    printf("test22: \"%s\" s/b \"\"\n", s);
    ami_allenv(&ep);
    printf("test23:\n");
    i = 10;
    while (ep != 0 && i > 0) {

       printf("Name: %s Data: %s\n", ep->name, ep->data);
       ep = ep->next;
       i--;

    }
    printf("s/b <10 entries from the current environment>\n");
    printf("test24:\n");
    ami_exec("services_test1");
    printf("waiting 5 seconds for program to start\n");
    waittime(SECOND*5);
    printf("s/b This is services_test1 \"\" (empty string)\n");
    printf("test25:\n");
    ami_execw("services_test1", &err);
    printf("%d\n", err);
    printf("s/b\n");
    printf("This is services_test1 \"\"\n");
    printf("0\n");
    printf("test26:\n");
    ep = malloc(sizeof(ami_envrec));
    ep->name = malloc(5);
    ep->data = malloc(9);
    strcpy(ep->name, "bark");
    strcpy(ep->data, "hi there");
    ep->next = 0;
    ami_exece("services_test1", ep);
    printf("waiting 5 seconds\n");
    waittime(SECOND*5);
    printf("s/b This is services_test1: \"hi there\"\n");
    printf("test27:\n");
    ami_execew("services_test1", ep, &err);
    printf("%d\n", err);
    printf("s/b\n");
    printf("This is services_test1 \"hi there\"\n");
    printf("0\n");
    ami_getcur(s, MAXSTR);
    printf("test 29: %s s/b <the current path>\n", s);
    ami_getcur(s, MAXSTR);
    ami_getusr(s3, MAXSTR);
    ami_setcur(s3);
    ami_getcur(s2, MAXSTR);
    printf("test 30: %s s/b <the user path>\n", s2);
    ami_setcur(s);
    ami_getcur(s, MAXSTR);
    printf("test 31: %s s/b <the current path>\n", s);
    ami_brknam("c:\\what\\ho\\junk.com", p, MAXSTR, n, MAXSTR, e, MAXSTR);
    printf("test 32: Path: %s Name: %s Ext: %s ", p, n, e);
    printf("s/b: Path: c:\\what\\ho\\ Name: junk Ext: com\n");
    ami_maknam(s, MAXSTR, p, n, e);
    printf("test 33: %s s/b c:\\what\\ho\\junk.com\n", s);
    strcpy(s, "junk");
    ami_fulnam(s, MAXSTR);
    printf("test 36: %s s/b <path>junk\n", s);
    ami_getpgm(s, MAXSTR);
    printf("test 38: %s s/b <the program path>\n", s);
    ami_getusr(s, MAXSTR);
    printf("test 40: %s s/b <the user path>\n", s);
    fp = fopen("junk", "w");
    fclose(fp);
    /* Linux cannot set or reset attributes */
#ifndef __linux
    printf("test 42: ");
    ami_setatr("junk", BIT(ami_atarc));
    ami_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, ami_atarc));
    printf(" s/b junk 1\n");
    printf("test 43: ");
    ami_resatr("junk", BIT(ami_atarc));
    ami_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, ami_atarc));
    printf(" s/b junk 0\n");
    printf("test 44: ");
    ami_setatr("junk", BIT(ami_atsys));
    ami_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, ami_atsys));
    printf(" s/b junk 1\n");
    printf("test 45: ");
    ami_resatr("junk", BIT(ami_atsys));
    ami_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, ami_atsys));
    printf(" s/b junk 0\n");
#endif
    printf("test 46: ");
    ami_setuper("junk", BIT(ami_pmwrite));
    ami_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->user, ami_pmwrite));
    printf(" s/b junk 1\n");
    printf("test 47: ");
    ami_resuper("junk", BIT(ami_pmwrite));
    ami_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->user, ami_pmwrite));
    printf(" s/b junk 0\n");
    ami_setuper("junk", BIT(ami_pmwrite));
    unlink("junk");
    printf("test 48: ");
    ami_makpth("junk");
    ami_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, ami_atdir));
    printf(" s/b junk 1\n");
    printf("test 49: ");
    ami_rempth("junk");
    ami_list("junk", &fla);
    printf("%d s/b 1\n", fla == 0);
    ami_filchr(sc);
    printf("test 50: Set of valid characters: ");
    for (i = 0; i < 126; i++) if (INCSET(sc, i)) putchar(i);
    printf("\n");

    printf("test 51: Option character: %c\n", ami_optchr());
    printf("test 52: Path character: %c\n", ami_pthchr());
    printf("test 53: Latitude: %d\n", ami_latitude());
    printf("test 54: longitude: %d\n", ami_longitude());
    printf("test 55: Altitude: %d\n", ami_altitude());
    printf("test 56: Country code: %d\n", ami_country());
    ami_countrys(s, 100, ami_country());
    printf("test 57: Country name: %s\n", s);
    printf("test 58: Timezone: %d\n", ami_timezone());
    printf("test 59: Daysave: %d\n", ami_daysave());
    printf("test 60: 24 hour time: %d\n", ami_time24hour());
    printf("test 61: Language: %d\n", ami_language());
    ami_languages(s, 100, ami_language());
    printf("test 62: Language name: %s\n", s);
    printf("test 63: Decimal character: %c\n", ami_decimal());
    printf("test 64: Separator character: %c\n", ami_numbersep());
    printf("test 65: Time order: %d\n", ami_timeorder());
    printf("test 66: Date order: %d\n", ami_dateorder());
    printf("test 67: Date separator: %c\n", ami_datesep());
    printf("test 68: time separator: %c\n", ami_timesep());
    printf("test 69: Currency character: %c\n", ami_currchr());

    return (0); /* exit no error */

}
