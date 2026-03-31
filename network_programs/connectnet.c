/*******************************************************************************
*                                                                              *
*                             CONNECT VIA TCP/IP                               *
*                                                                              *
*                   2021/19/27 Copyright (C) S. A. Franco                      *
*                                                                              *
* Like telnet, but also can use TLS to secure the connection. SSH, the obvious *
* alternative, does not run the same protocol as SSL.                          *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

static unsigned long addr;

int secure = FALSE;

ami_optrec opttbl[] = {

    { "secure", &secure, NULL,   NULL, NULL },
    { "s",      &secure, NULL,   NULL, NULL },
    { NULL,     NULL,    NULL,   NULL, NULL }

};

int main(int argc, char **argv)

{

    int   argi = 1;
    FILE* fp;
    int   c;
    int   port;

    printf("Connect TCP/IP program\n");
    printf("\n");

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 3) {

        fprintf(stderr, "Usage: connectnet [--secure|-s] <server> <port>\n");
        exit(1);

    }

    /* get user specified port */
    port = atoi(argv[argi+1]);

    ami_addrnet(argv[argi], &addr);
    fp = ami_opennet(addr, port, secure);

    /* read input from far side */
    do {

        c = fgetc(fp);
        if (c != EOF) putchar(c);

    } while (c != EOF);

    fclose(fp);

    return (0);

}
