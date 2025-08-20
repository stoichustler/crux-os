#include <stdint.h>

__attribute__((noreturn))
__weak void _exit(int status)
{
	(void)status;

	while (1) {
		__asm__ volatile ("wfe");
	}
}
