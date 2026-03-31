#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <services.h>

#define MAXSTR 100

int main(void)

{

    char s[MAXSTR];

    pa_getenv("bark", s, MAXSTR);

    printf("This is services_test1 \"%s\"\n", s);

    return (0);

}
