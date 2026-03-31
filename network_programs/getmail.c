/*******************************************************************************
*                                                                              *
*                             POP EMAIL READER                                 *
*                                                                              *
*                   2006/05/23 Copyright (C) S. A. Franco                      *
*                                                                              *
* Shows an example of network access via netlib. Accesses a mail server, and   *
* orders a list of outstanding mail. This is then dumped to the console.       *
*                                                                              *
* Netlib is a very simple method to access the Internet, because it simply     *
* connects to a server using the standard file methods.                        *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 100
#define NUMMSG 1000 /* maximum messages to store */

static FILE* mail;
static unsigned long addr;
static char buff[BUFLEN];
static char server[BUFLEN];
static char user[BUFLEN];
static char pass[BUFLEN];
static int  msgnum;
static int  msgsiz;
static int  msgarr[NUMMSG];
static int  port = 110; /* unsecured port */
static int  sport = 995; /* secured port */

int secure = FALSE;

ami_optrec opttbl[] = {

    { "secure", &secure, NULL,   NULL, NULL },
    { "s",      &secure, NULL,   NULL, NULL },
    { "port",   NULL,    &port,  NULL, NULL },
    { "p",      NULL,    &port,  NULL, NULL },
    { "sport",  NULL,    &sport, NULL, NULL },
    { "sp",     NULL,    &sport, NULL, NULL },
    { NULL,     NULL,    NULL,   NULL, NULL }

};

void waitresp(void)

{

    char t[100];

    fgets(t, BUFLEN, mail); /* get response */
    if (t[0] != '+') {

      fprintf(stderr, "*** Error: protocol error\n");
      exit(1);

    }

}

int main(int argc, char **argv)

{

    int i, top;
    int argi = 1;
    int pt;

    printf("Mail server access test program\n");
    printf("\n");

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc < 4) {

        fprintf(stderr, "Usage: getmail [--secure|-s] [--port|p=<port>] "
                        "[--sport|sp=<port>] <server> <user> <pass>\n");
        exit(1);

    }
    ami_addrnet(argv[argi], &addr);
    mail = ami_opennet(addr, secure?sport:port, secure);
    waitresp();
    fprintf(mail, "user %s\r\n", argv[argi+1]);
    waitresp();
    fprintf(mail, "pass %s\r\n", argv[argi+2]);
    waitresp();
    fprintf(mail, "list\r\n");
    waitresp();
    top = 0; /* set top of message array */
    /* gather message numbers */
    do {

        if (fgets(buff, BUFLEN, mail)) {

            if (strcmp(buff, ".\r\n")) {

                sscanf(buff, "%d %d\n", &msgnum, &msgsiz);
                msgarr[top] = msgnum; /* save message number */
                if (top < NUMMSG) top++; /* advance */
                //printf("%7d %8d\n", msgnum, msgsiz);

            }

        }

   } while (buff[0] && strcmp(buff, ".\r\n"));
   /* now print those messages */
   for (i = 0; i < top; i++) {

       printf("Message: %d\n\n", msgarr[i]);
       fprintf(mail, "retr %d\r\n", msgarr[i]);
       waitresp();
       do {

           if (fgets(buff, BUFLEN, mail))
               if (strcmp(buff, ".\r\n")) printf("%s", buff);

       } while (buff[0] && strcmp(buff, ".\r\n"));

   }

   fclose(mail);

   return (0);

}
