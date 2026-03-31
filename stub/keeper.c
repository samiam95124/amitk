/* This just makes sure that terminal or graphics stays resident */

#include <stdio.h>
#include <terminal.h>

void __keeper(void) { ami_maxx(stdout); }
