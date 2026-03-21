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

        services_writedate(stdout, services_local(t));
        putchar(' ');
        services_writetime(stdout, services_local(t));
        putchar(' ');

    }

}

static void prtperm(services_permset p)

{

    if (INISET(p, services_pmread)) putchar('r'); else putchar(' ');
    if (INISET(p, services_pmwrite)) putchar('w'); else putchar(' ');
    if (INISET(p, services_pmexec)) putchar('e'); else putchar(' ');
    if (INISET(p, services_pmdel)) putchar('d'); else putchar(' ');
    if (INISET(p, services_pmvis)) putchar('v'); else putchar(' ');
    if (INISET(p, services_pmcopy)) putchar('c'); else putchar(' ');
    if (INISET(p, services_pmren)) putchar('m'); else putchar(' ');
    putchar(' ');

}

static void waittime(int t)

{

    int ct = services_clock();

    while (services_elapsed(ct) < t);

}

int main(void)

{

    services_filptr lp;
    services_envptr ep;
    int       t;
    char      s[MAXSTR], s2[MAXSTR], s3[MAXSTR];
    int       c;
    int       err;
    char      p[MAXSTR], n[MAXSTR], e[MAXSTR];
    services_filptr fla;
    FILE*     fp;
    int       i;
    services_chrset sc;

    printf("Services module test v1.0\n");
    printf("\n");
    printf("test1:\n");
    services_list("*", &lp);
    while (lp) {

        printf("%-25s %-10lld %-10lld ", lp->name, lp->size, lp->alloc);
        if (INISET(lp->attr, services_atexec)) putchar('e'); else putchar(' ');
        if (INISET(lp->attr, services_atarc)) putchar('a'); else putchar(' ');
        if (INISET(lp->attr, services_atsys)) putchar('s'); else putchar(' ');
        if (INISET(lp->attr, services_atdir)) putchar('d'); else putchar(' ');
        if (INISET(lp->attr, services_atloop)) putchar('l'); else putchar(' ');
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
    services_listl("*", strlen("*"), &lp);
    while (lp) {

        printf("%-25s %-10lld %-10lld ", lp->name, lp->size, lp->alloc);
        if (INISET(lp->attr, services_atexec)) putchar('e'); else putchar(' ');
        if (INISET(lp->attr, services_atarc)) putchar('a'); else putchar(' ');
        if (INISET(lp->attr, services_atsys)) putchar('s'); else putchar(' ');
        if (INISET(lp->attr, services_atdir)) putchar('d'); else putchar(' ');
        if (INISET(lp->attr, services_atloop)) putchar('l'); else putchar(' ');
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

    services_times(s, MAXSTR, services_time());
    printf("test 3: %s s/b <the current time in zulu>\n", s);
    services_times(s, MAXSTR, services_local(services_time()));
    printf("test 5: %s s/b <the current time in local>\n", s);
    services_dates(s, MAXSTR, services_local(services_time()));
    printf("test 7: %s s/b <the current date>\n", s);
    printf("test 9: ");
    services_writetime(stdout, services_local(services_time()));
    printf(" s/b <the time>\n");
    printf("test 10: ");
    services_writedate(stdout, services_local(services_time()));
    printf(" s/b <the date>\n");
    t = services_clock();
    printf("test11: waiting 1 second\n");
    waittime(SECOND);
    printf("test 11: %ld s/b %d (approximate)\n", services_elapsed(t), SECOND);
    printf("test 12: %d s/b 1\n", services_validfile("c:\\just\\fargle.com"));
    printf("test 14: %d s/b 1\n", services_wild("c:\\fargle.c?m"));
    printf("test 15: %d s/b 1\n", services_validfile("c:\\far*gle.com"));
    printf("test 17  %d s/b 1\n", services_wild("c:\\for?.txt"));
    printf("test 18: %d s/b 1\n", services_wild("c:\\for*.txt"));
    printf("test 19: %d s/b 0\n", services_wild("c:\\fork.txt"));
    services_setenv("barkbark", "what is this");
    services_getenv("barkbark", s, MAXSTR);
    printf("test20: %s s/b what is this\n", s);
    services_remenv("barkbark");
    services_getenv("barkbark", s, MAXSTR);
    printf("test22: \"%s\" s/b \"\"\n", s);
    services_allenv(&ep);
    printf("test23:\n");
    i = 10;
    while (ep != 0 && i > 0) {

       printf("Name: %s Data: %s\n", ep->name, ep->data);
       ep = ep->next;
       i--;

    }
    printf("s/b <10 entries from the current environment>\n");
    printf("test24:\n");
    services_exec("services_test1");
    printf("waiting 5 seconds for program to start\n");
    waittime(SECOND*5);
    printf("s/b This is services_test1 \"\" (empty string)\n");
    printf("test25:\n");
    services_execw("services_test1", &err);
    printf("%d\n", err);
    printf("s/b\n");
    printf("This is services_test1 \"\"\n");
    printf("0\n");
    printf("test26:\n");
    ep = malloc(sizeof(services_envrec));
    ep->name = malloc(5);
    ep->data = malloc(9);
    strcpy(ep->name, "bark");
    strcpy(ep->data, "hi there");
    ep->next = 0;
    services_exece("services_test1", ep);
    printf("waiting 5 seconds\n");
    waittime(SECOND*5);
    printf("s/b This is services_test1: \"hi there\"\n");
    printf("test27:\n");
    services_execew("services_test1", ep, &err);
    printf("%d\n", err);
    printf("s/b\n");
    printf("This is services_test1 \"hi there\"\n");
    printf("0\n");
    services_getcur(s, MAXSTR);
    printf("test 29: %s s/b <the current path>\n", s);
    services_getcur(s, MAXSTR);
    services_getusr(s3, MAXSTR);
    services_setcur(s3);
    services_getcur(s2, MAXSTR);
    printf("test 30: %s s/b <the user path>\n", s2);
    services_setcur(s);
    services_getcur(s, MAXSTR);
    printf("test 31: %s s/b <the current path>\n", s);
    services_brknam("c:\\what\\ho\\junk.com", p, MAXSTR, n, MAXSTR, e, MAXSTR);
    printf("test 32: Path: %s Name: %s Ext: %s ", p, n, e);
    printf("s/b: Path: c:\\what\\ho\\ Name: junk Ext: com\n");
    services_maknam(s, MAXSTR, p, n, e);
    printf("test 33: %s s/b c:\\what\\ho\\junk.com\n", s);
    strcpy(s, "junk");
    services_fulnam(s, MAXSTR);
    printf("test 36: %s s/b <path>junk\n", s);
    services_getpgm(s, MAXSTR);
    printf("test 38: %s s/b <the program path>\n", s);
    services_getusr(s, MAXSTR);
    printf("test 40: %s s/b <the user path>\n", s);
    fp = fopen("junk", "w");
    fclose(fp);
    /* Linux cannot set or reset attributes */
#ifndef __linux
    printf("test 42: ");
    services_setatr("junk", BIT(services_atarc));
    services_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, services_atarc));
    printf(" s/b junk 1\n");
    printf("test 43: ");
    services_resatr("junk", BIT(services_atarc));
    services_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, services_atarc));
    printf(" s/b junk 0\n");
    printf("test 44: ");
    services_setatr("junk", BIT(services_atsys));
    services_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, services_atsys));
    printf(" s/b junk 1\n");
    printf("test 45: ");
    services_resatr("junk", BIT(services_atsys));
    services_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, services_atsys));
    printf(" s/b junk 0\n");
#endif
    printf("test 46: ");
    services_setuper("junk", BIT(services_pmwrite));
    services_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->user, services_pmwrite));
    printf(" s/b junk 1\n");
    printf("test 47: ");
    services_resuper("junk", BIT(services_pmwrite));
    services_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->user, services_pmwrite));
    printf(" s/b junk 0\n");
    services_setuper("junk", BIT(services_pmwrite));
    unlink("junk");
    printf("test 48: ");
    services_makpth("junk");
    services_list("junk", &fla);
    if (fla != 0) printf("%s %d", fla->name, INISET(fla->attr, services_atdir));
    printf(" s/b junk 1\n");
    printf("test 49: ");
    services_rempth("junk");
    services_list("junk", &fla);
    printf("%d s/b 1\n", fla == 0);
    services_filchr(sc);
    printf("test 50: Set of valid characters: ");
    for (i = 0; i < 126; i++) if (INCSET(sc, i)) putchar(i);
    printf("\n");

    printf("test 51: Option character: %c\n", services_optchr());
    printf("test 52: Path character: %c\n", services_pthchr());
    printf("test 53: Latitude: %d\n", services_latitude());
    printf("test 54: longitude: %d\n", services_longitude());
    printf("test 55: Altitude: %d\n", services_altitude());
    printf("test 56: Country code: %d\n", services_country());
    services_countrys(s, 100, services_country());
    printf("test 57: Country name: %s\n", s);
    printf("test 58: Timezone: %d\n", services_timezone());
    printf("test 59: Daysave: %d\n", services_daysave());
    printf("test 60: 24 hour time: %d\n", services_time24hour());
    printf("test 61: Language: %d\n", services_language());
    services_languages(s, 100, services_language());
    printf("test 62: Language name: %s\n", s);
    printf("test 63: Decimal character: %c\n", services_decimal());
    printf("test 64: Separator character: %c\n", services_numbersep());
    printf("test 65: Time order: %d\n", services_timeorder());
    printf("test 66: Date order: %d\n", services_dateorder());
    printf("test 67: Date separator: %c\n", services_datesep());
    printf("test 68: time separator: %c\n", services_timesep());
    printf("test 69: Currency character: %c\n", services_currchr());

    return (0); /* exit no error */

}
