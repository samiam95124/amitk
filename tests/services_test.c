#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <services.h>

#define MAXSTR 100
#define SECOND 10000

static void prttimdat(ami_long t)

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

static void waittime(ami_long t)

{

    ami_long ct = ami_clock();

    while (ami_elapsed(ct) < t);

}

int main(void)

{

    ami_filptr lp;
    ami_envptr ep;
    ami_long  t;
    char      s[MAXSTR], s2[MAXSTR], s3[MAXSTR];
    int       c;
    ami_long  err;
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
    printf("test 11: %lld s/b %d (approximate)\n", AMI_LONG_CAST(ami_elapsed(t)), SECOND);
    printf("test 12: %lld s/b 1\n", AMI_LONG_CAST(ami_validfile("c:\\just\\fargle.com")));
    printf("test 14: %lld s/b 1\n", AMI_LONG_CAST(ami_wild("c:\\fargle.c?m")));
    printf("test 15: %lld s/b 1\n", AMI_LONG_CAST(ami_validfile("c:\\far*gle.com")));
    printf("test 17  %lld s/b 1\n", AMI_LONG_CAST(ami_wild("c:\\for?.txt")));
    printf("test 18: %lld s/b 1\n", AMI_LONG_CAST(ami_wild("c:\\for*.txt")));
    printf("test 19: %lld s/b 0\n", AMI_LONG_CAST(ami_wild("c:\\fork.txt")));
    ami_setenv("barkbark", "what is this");
    ami_getenv("barkbark", s, MAXSTR);
    printf("test20: %s s/b what is this\n", s);
    /* Set the same name again. This takes the "found" path in setenv,
       which replaces the value in place rather than making a new entry,
       and is where a one byte heap overflow lived: the replacement was
       allocated without room for the terminator. A longer value than the
       first makes the overflow certain rather than incidental. */
    ami_setenv("barkbark", "a considerably longer replacement value");
    ami_getenv("barkbark", s, MAXSTR);
    printf("test21: %s s/b a considerably longer replacement value\n", s);
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
    printf("%lld\n", AMI_LONG_CAST(err));
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
    printf("%lld\n", AMI_LONG_CAST(err));
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
    /* Build the path with the system's own separator. The test used a
       Windows path with backslashes, which is not a separator on Unix, so
       brknam correctly returned the whole string as the name and the test
       appeared to fail everywhere but Windows. */
    sprintf(s2, "%cwhat%cho%cjunk.com", ami_pthchr(), ami_pthchr(),
            ami_pthchr());
    ami_brknam(s2, p, MAXSTR, n, MAXSTR, e, MAXSTR);
    printf("test 32: Path: %s Name: %s Ext: %s ", p, n, e);
    printf("s/b: Path: %cwhat%cho%c Name: junk Ext: com\n", ami_pthchr(),
           ami_pthchr(), ami_pthchr());
    ami_maknam(s, MAXSTR, p, n, e);
    printf("test 33: %s s/b %cwhat%cho%cjunk.com\n", s, ami_pthchr(),
           ami_pthchr(), ami_pthchr());
    strcpy(s, "junk");
    ami_fulnam(s, MAXSTR);
    printf("test 36: %s s/b <path>junk\n", s);
    ami_getpgm(s, MAXSTR);
    printf("test 38: %s s/b <the program path>\n", s);
    ami_getusr(s, MAXSTR);
    printf("test 40: %s s/b <the user path>\n", s);
    fp = fopen("junk", "w");
    fclose(fp);
    /* only Windows can set or reset these attributes: the archive and system
       attributes are Windows ones, and the Unix systems have no way to set
       them */
#ifdef _WIN32
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
    printf("test 53: Latitude: %lld\n", AMI_LONG_CAST(ami_latitude()));
    printf("test 54: longitude: %lld\n", AMI_LONG_CAST(ami_longitude()));
    printf("test 55: Altitude: %lld\n", AMI_LONG_CAST(ami_altitude()));
    printf("test 56: Country code: %lld\n", AMI_LONG_CAST(ami_country()));
    ami_countrys(s, 100, ami_country());
    printf("test 57: Country name: %s\n", s);
    printf("test 58: Timezone: %lld\n", AMI_LONG_CAST(ami_timezone()));
    printf("test 59: Daysave: %lld\n", AMI_LONG_CAST(ami_daysave()));
    printf("test 60: 24 hour time: %lld\n", AMI_LONG_CAST(ami_time24hour()));
    printf("test 61: Language: %lld\n", AMI_LONG_CAST(ami_language()));
    ami_languages(s, 100, ami_language());
    printf("test 62: Language name: %s\n", s);
    printf("test 63: Decimal character: %c\n", ami_decimal());
    printf("test 64: Separator character: %c\n", ami_numbersep());
    printf("test 65: Time order: %lld\n", AMI_LONG_CAST(ami_timeorder()));
    printf("test 66: Date order: %lld\n", AMI_LONG_CAST(ami_dateorder()));
    printf("test 67: Date separator: %c\n", ami_datesep());
    printf("test 68: time separator: %c\n", ami_timesep());
    printf("test 69: Currency character: %c\n", ami_currchr());

    return (0); /* exit no error */

}
