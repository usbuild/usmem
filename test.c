#include <stdio.h>
#include <string.h>
#include "usmem.h"
int main() {
    us_allocator ar = new_us_allocator();
    char *p = (char*)us_alloc(ar, 16);
    strcpy(p, "Hello World");
    puts(p);
    us_dealloc(ar, p, 16);
    return 0;
}
