 /*******************************************
  *          _______  __  __
  *         /__\_  _\/  \/ _\
  *        /  \ / / / / /\ \
  *        \/\/ \/  \__/\__/ DEUX 2025
  *
  ******************************************/

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>

void _exit(int status)
{
    while (1) {
        ;
    }
}

void *_sbrk(intptr_t count)
{
    /* not implemented */
}
