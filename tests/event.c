/******************************************************************************
*                                                                             *
*                            EVENT DIAGNOSTIC                                 *
*                                                                             *
*                            10/02 S. A. Moore                                *
*                                                                             *
* Prints terminal level events.                                               *
*                                                                             *
******************************************************************************/

#include <string.h>
#include <stdio.h>

#include "terminal.h"

int main()

{

    ami_evtrec er;

    printf("Event diagnostic 1.0\n");
    do {

        ami_event(stdin, &er);
        switch (er.etype) {

            case ami_etchar: {

                if (er.echar < ' ') er.echar = '.';
                printf("ANSI character returned '%c'\n", er.echar);
                break;

              }
            case ami_etup:      printf("up one line\n"); break;
            case ami_etdown:    printf("down one line\n"); break;
            case ami_etleft:    printf("left one character\n"); break;
            case ami_etright:   printf("right one character\n"); break;
            case ami_etleftw:   printf("left one word\n"); break;
            case ami_etrightw:  printf("right one word\n"); break;
            case ami_ethome:    printf("home of document\n"); break;
            case ami_ethomes:   printf("home of screen\n"); break;
            case ami_ethomel:   printf("home of line\n"); break;
            case ami_etend:     printf("end of document\n"); break;
            case ami_etends:    printf("end of screen\n"); break;
            case ami_etendl:    printf("end of line\n"); break;
            case ami_etscrl:    printf("scroll left one character\n"); break;
            case ami_etscrr:    printf("scroll right one character\n"); break;
            case ami_etscru:    printf("scroll up one line\n"); break;
            case ami_etscrd:    printf("scroll down one line\n"); break;
            case ami_etpagd:    printf("page down\n"); break;
            case ami_etpagu:    printf("page up\n"); break;
            case ami_ettab:     printf("tab\n"); break;
            case ami_etenter:   printf("enter line\n"); break;
            case ami_etinsert:  printf("insert block\n"); break;
            case ami_etinsertl: printf("insert line\n"); break;
            case ami_etinsertt: printf("insert toggle\n"); break;
            case ami_etdel:     printf("delete block\n"); break;
            case ami_etdell:    printf("delete line\n"); break;
            case ami_etdelcf:   printf("delete character forward\n"); break;
            case ami_etdelcb:   printf("delete character backward\n"); break;
            case ami_etcopy:    printf("copy block\n"); break;
            case ami_etcopyl:   printf("copy line\n"); break;
            case ami_etcan:     printf("cancel current operation\n"); break;
            case ami_etstop:    printf("stop current operation\n"); break;
            case ami_etcont:    printf("continue current operation\n"); break;
            case ami_etprint:   printf("print document\n"); break;
            case ami_etprintb:  printf("print block\n"); break;
            case ami_etprints:  printf("print screen\n"); break;
            case ami_etfun:     printf("Function key, number: %d\n", er.fkey); break;
            case ami_etmenu:    printf("display menu\n"); break;
            case ami_etmouba:   printf("mouse button assertion, mouse: %d button: %d\n",
                                      er.amoun, er.amoubn); break;
            case ami_etmoubd:   printf("mouse button deassertion, mouse: %d button: %d\n",
                                      er.dmoun, er.dmoubn); break;
            case ami_etmoumov:  printf("mouse move, mouse: %d x: %d y: %d\n",
                                      er.mmoun, er.moupx, er.moupy); break;
            case ami_ettim:     printf("timer matures, timer: %d\n", er.timnum); break;
            case ami_etjoyba:   printf("joystick button assertion, stick: %d button: %d\n",
                                      er.ajoyn, er.ajoybn); break;
            case ami_etjoybd:   printf("joystick button deassertion, stick: %d button: %d\n",
                                      er.djoyn, er.djoybn); break;
            case ami_etjoymov:  printf("joystick move, stick: %d x: %d y: %d z: %d\n",
                                      er.mjoyn, er.joypx, er.joypy, er.joypz); break;
            case ami_etresize:
                printf("Window resized\n");
                printf("New size: x: %d y: %d\n", ami_maxx(stdout), ami_maxy(stdout));
                break;
            case ami_etterm:    printf("terminate program\n"); break;
            default: ;

        }

    } while (er.etype != ami_etterm);

    return 0;

}
