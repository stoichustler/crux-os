#include <crux/delay.h>
#include <crux/init.h>
#include <crux/lib.h>
#include <crux/softirq.h>
#include <crux/warning.h>

#define WARNING_ARRAY_SIZE 20
static unsigned int __initdata nr_warnings;
static const char *__initdata warnings[WARNING_ARRAY_SIZE];

void __init warning_add(const char *warning)
{
    if ( nr_warnings >= WARNING_ARRAY_SIZE )
        panic("Too many pieces of warning text\n");

    warnings[nr_warnings] = warning;
    nr_warnings++;
}

void __init warning_print(void)
{
    unsigned int i, j, countdown;

    if ( !nr_warnings )
        return;

    for ( i = 0; i < nr_warnings; i++ )
    {
        /* printk("%s", warnings[i]); */
        process_pending_softirqs();
    }

    for ( i = 0; i < 3; i++ )
    {
        countdown = 3 - i;
        printk("%s%u...",
            countdown == 3 ? "Kicking crux in " : "",
            countdown);
        for ( j = 0; j < 100; j++ )
        {
            process_pending_softirqs();
            mdelay(10);
        }
    }
    printk("\n");
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
