/*******************************************************************************

List network certificate

Fetches the network certificate tree by ordinal from the command line, and
prints the entire tree.

Command format:

listcertnet <server> <port> <certno>

The given certificate, from 1 to n, is listed in tree format. If there are not
that many certificates in the server chain, an error results.

This program demonstrates getting a server certificate as an ASCII name/value
tree, then printing it out in indented format. It can also be modified to find
and or select any number of specific fields in the certificate tree.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <localdefs.h>
#include <network.h>

/* number of spaces in each indentation level */
#define INDENTLVL 4

void prtcert(
    /* certificate tree */          ami_certptr cp,
    /* current indentation level */ int        indent
)

{

    int i;
    char* p;

    while (cp) { /* traverse the list */

        for (i = 0; i < indent; i++) putchar(' ');
        /* anonymous entry/branch is possible, not sure what good that is */
        if (cp->name) {

            if (cp->critical) printf("%s(critical): ", cp->name);
            else printf("%s: ", cp->name);

        }
        /* branches usually don't have data */
        if (cp->data) {

            /* see if we print multiline data or single */
            if (strchr(cp->data, '\n')) {

                p = cp->data;
                while (*p) { /* traverse */

                    printf("\n"); /* next line */
                    for (i = 0; i < indent+INDENTLVL; i++) putchar(' ');
                    while (*p && *p != '\n') { putchar(*p); p++; }
                    if (*p == '\n') p++; /* skip line end */

                }

            } else printf("%s", cp->data);

        }
        printf("\n");

        /* if this is a branch, we recurse to a new indent level */
        if (cp->fork) prtcert(cp->fork, indent+INDENTLVL);

        cp = cp->next; /* go next list entry */

    }

}

int main(
    /* argument count */       int  argc,
    /* argument string list */ char **argv
)

{

    /* server IPv4 address */ unsigned long addr;
    /* server SSL file */     FILE*         fp;
    /* certificate tree */    ami_certptr    list;
    /* port number */         int           port;
    int num;

    if (argc != 4) {

        fprintf(stderr, "Usage: listcertnet <server> <port> <certno>\n");
        exit(1);

    }

    port = atoi(argv[2]); /* get port */
    num = atoi(argv[3]); /* get which certificate to fetch */

    /* open server SSL */
    ami_addrnet(argv[1], &addr);
    fp = ami_opennet(addr, port, TRUE);

    /* Get the certificate indicated. Note that if you only want certain
       entries from the tree, fill the names in with NULL data pointers. */
    list = NULL;
    ami_certlistnet(fp, 1, &list);

    /* print certificate tree at 0 indent */
    prtcert(list, 0);

    fclose(fp); /* close server port */

    return (0);

}
