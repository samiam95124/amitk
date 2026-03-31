/*******************************************************************************

Get web server page

Retrives the specified page from a web server. Either http or https can be
specified. The resulting page can either be just printed, or piped to a file to
be used.

Note that this version does not automatically clip the header info.

getpage [--secure|-s] [--v6] [--ne] [--html] <website> <page> [<port>]

Options:

--secure|-s

If secure is specified, an SSL connection will be created, otherwise plaintext.
the port number defaults to 443 for a https connection, otherwise 80.

--v6

Either the IPv4 address or IPv6 address can be selected. Note that the IPv6
address will only work if the server has IPv6 addressing (many servers will do
both).

--ne

Normally getpage stops if it sees the </html> or end tag. This option overrides
that.

--html

Only print the html tagged content. Otherwise all content is printed, usually
including the html page statistics.

*******************************************************************************/

/* posix includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Petit-Ami includes */
#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 250

/* do/do not secure connection */
int secure = FALSE;

/* use IPv6 or IPv4 */
int ipv6 = FALSE;

/* do not check html end of file */
int ncend = FALSE;

/* only print html content */
int htmlonly = FALSE;

ami_optrec opttbl[] = {

    { "secure", &secure,   NULL, NULL, NULL },
    { "s",      &secure,   NULL, NULL, NULL },
    { "v6",     &ipv6,     NULL, NULL, NULL },
    { "ne",     &ncend,    NULL, NULL, NULL },
    { "html",   &htmlonly, NULL, NULL, NULL },
    { NULL,     NULL,      NULL, NULL, NULL }

};

int main(int argc, char **argv)
{

    FILE* fp;
    char buff[BUFLEN];
    unsigned long addr;
    unsigned long long addrh, addrl;
    int port;
    int argi = 1;
    int end;
    int inhtml;

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc < 3) {

        fprintf(stderr, "Usage: getpage [--secure|-s] [--v6] [--ne] [--html] <website> <page> [<port>]\n");
        exit(1);

    }

    /* set default port */
    if (secure) port = 443; /* https standard port */
    else port = 80; /* http standard port */

    /* get user specified port */
    if (argc == 4) port = atoi(argv[argi+2]);

    /* open the server file */
    if (ipv6) {

        ami_addrnetv6(argv[argi], &addrh, &addrl);
        fp = ami_opennetv6(addrh, addrl, port, secure);

    } else {

        ami_addrnet(argv[argi], &addr);
        fp = ami_opennet(addr, port, secure);

    }

    /* send request to get root page */
    fprintf(fp, "GET %s HTTP/1.1\r\n", argv[argi+1]);
    fprintf(fp, "Host: %s\r\n\r\n", argv[argi]);

    end = FALSE; /* set not at html end */
    inhtml = FALSE; /* set not in html contents */

    /* print result */
    while (!feof(fp) && (!end || ncend)) {

        if (fgets(buff, BUFLEN, fp)) {

            if (!strcmp(buff, "<html>\n"))
                inhtml = TRUE; /* set in html content */
            if (!htmlonly || inhtml) printf("%s", buff);
            if (!strcmp(buff, "</html>\n"))
                inhtml = FALSE; /* set not in html content */
            /* this flush forces the printout even if we cancel out of the run.
               This is frequently necessary if the server does not time out
               automatically. This only matters if the output is piped. */
            fflush(stdout);
            /* set end of html data */
            end = !strcmp(buff, "</html>\n");

        }

    }

    return (0);

}
