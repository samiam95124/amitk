/* Does the client of a window still see keys when it holds widgets, and
   what changes after one of them is clicked? A compose window wants an
   editing area of its own alongside its fields, and that only works if
   the keys reach the window. */
#include <stdio.h>
#include <string.h>
#include <localdefs.h>
#include <graphics.h>
#define EB 1
#define BT 2
int main(void)
{
    ami_evtrec er;
    ami_long w, h, bw, bh;
    ami_title(stdout, "Keys and widgets");
    ami_auto(stdout, FALSE);
    ami_curvis(stdout, FALSE);
    ami_editboxsizg(stdout, "0", &w, &h);
    ami_editboxg(stdout, 20, 20, 300, 20+h, EB);
    ami_buttonsizg(stdout, "Send", &bw, &bh);
    ami_buttong(stdout, 20, 80, 20+bw, 80+bh, "Send", BT);
    do {
        ami_event(stdin, &er);
        if (er.etype == ami_etchar)
            fprintf(stderr, "window got char '%c'\n", er.echar);
        else if (er.etype == ami_etbutton)
            fprintf(stderr, "button %lld\n", AMI_LONG_CAST(er.butid));
        else if (er.etype == ami_etmouba)
            fprintf(stderr, "click\n");
        else if (er.etype == ami_etenter) fprintf(stderr, "window got enter\n");
    } while (er.etype != ami_etterm);
    return (0);
}
