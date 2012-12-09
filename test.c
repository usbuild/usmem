#include "usmem.h"
#include <stdio.h>
#include <string.h>
int main() {
    us_allocator ar = new_obj_allocator();
    char *p = (char*)us_alloc(ar, 16);
    strcpy(p, "Hello World");
    puts(p);
    us_dealloc(ar, p, 16);
    return 0;
}
