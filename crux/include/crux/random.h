#ifndef __CRUX_RANDOM_H__
#define __CRUX_RANDOM_H__

unsigned int get_random(void);

/* The value keeps unchange once initialized for each booting */
extern unsigned int boot_random;

#endif /* __CRUX_RANDOM_H__ */
