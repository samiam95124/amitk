/*******************************************************************************

Getty server

Simple server example that produces Lincon's Gettysberg address when connected
to, using port 42 (of course). Copied from online example.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <localdefs.h>
#include <network.h>
#include <option.h>

#define BUFLEN 250

string gettys[] = {

    "Now we are engaged in a great civil war, testing whether that nation, or",
    "any nation so conceived and so dedicated, can long endure. We are met on",
    "a great battle-field of that war. We have come to dedicate a portion of",
    "that field, as a final resting place for those who here gave their lives",
    "that that nation might live. It is altogether fitting and proper that we",
    "should do this.",
    "",
    "But, in a larger sense, we can not dedicate -- we can not consecrate --",
    "we can not hallow -- this ground. The brave men, living and dead, who",
    "struggled here, have consecrated it, far above our poor power to add or",
    "detract. The world will little note, nor long remember what we say here,",
    "but it can never forget what they did here. It is for us the living,",
    "rather, to be dedicated here to the unfinished work which they who fought",
    "here have thus far so nobly advanced. It is rather for us to be here",
    "dedicated to the great task remaining before us -- that from these",
    "honored dead we take increased devotion to that cause for which they gave",
    "the last full measure of devotion -- that we here highly resolve that",
    "these dead shall not have died in vain -- that this nation, under God,",
    "shall have a new birth of freedom -- and that government of the people,",
    "by the people, for the people, shall not perish from the earth.",
    "",
    "Abraham Lincoln",
    "November 19, 1863",
    NULL

};

int secure = FALSE;

ami_optrec opttbl[] = {

    { "secure", &secure, NULL, NULL, NULL },
    { "s",      &secure, NULL, NULL, NULL },
    { NULL,     NULL,    NULL, NULL, NULL }

};

int main(int argc, char **argv)
{

    FILE* fp;
    char buff[BUFLEN];
    unsigned long addr;
    int port = 4433;
    int s;
    int argi = 1;

    /* parse user options */
    ami_options(&argi, &argc, argv, opttbl, TRUE);

    if (argc != 1 && argc != 2) {

        fprintf(stderr, "Usage: gettys [--secure|-s] [<port>]\n");
        exit(1);
    }

    /* if user supplied a port, get that */
    if (argc == 2) port = atoi(argv[argi]);

    while (1) { /* serve this port until cancelled */

        printf("gettys server waits on port 4433 for connections\n");
        /* wait on port 42 */
        fp = ami_waitnet(port, secure);

        printf("Inbound connection\n");

        /* print contents of text */
        s = 0;
        while (gettys[s]) fprintf(fp, "%s\n", gettys[s++]);

        printf("Connection finished\n");

        /* close the connection and wait for next */
        fclose(fp);

    }

    return (0);

}
