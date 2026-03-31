/*******************************************************************************

Print server certificates

Gets and prints the certificate chain given by the server.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

int main(int argc, char **argv)
{

    int fn;
    unsigned long addr;
    char cbuff[4096];
    int len;
    int port;
    int i;

    if (argc < 3) {

        fprintf(stderr, "Usage: prtcertmsg <server> <port>\n");
        exit(1);
    }

    port = atoi(argv[2]);

    ami_addrnet(argv[1], &addr);
    fn = ami_openmsg(addr, port, TRUE);

    i = 1; /* set 1st certificate */
    do {

       len = ami_certmsg(fn, i, cbuff, sizeof(cbuff));
       if (len) {

           printf("Certificate %d:\n", i);
           printf("%.*s\n", len, cbuff);
           i++; /* next certificate */

       }

    } while (len);

    ami_clsmsg(fn);

    return (0);

}
