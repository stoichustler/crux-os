/******************************************************************************
 * ns16550.c
 *
 * Driver for 16550-series UARTs. This driver is to be kept within crux as
 * it permits debugging of seriously-toasted machines (e.g., in situations
 * where a device driver within a guest OS would be inaccessible).
 *
 * Copyright (c) 2003-2005, K A Fraser
 */

/*
 * The PCI part of the code in this file currently is only known to
 * work on x86. Undo this hack once the logic has been suitably
 * abstracted.
 */

#include <crux/console.h>
#include <crux/init.h>
#include <crux/irq.h>
#include <crux/param.h>
#include <crux/sched.h>
#include <crux/sections.h>
#include <crux/timer.h>
#include <crux/serial.h>
#include <crux/iocap.h>
#ifdef NS16550_PCI
#include <crux/pci.h>
#include <crux/pci_regs.h>
#include <crux/pci_ids.h>
#endif
#include <crux/8250-uart.h>
#include <crux/vmap.h>
#include <asm/io.h>
#ifdef CONFIG_HAS_DEVICE_TREE_DISCOVERY
#include <asm/device.h>
#endif

static struct ns16550 {
    int baud, clock_hz, data_bits, parity, stop_bits, fifo_size, irq;
    u64 io_base;   /* I/O port or memory-mapped I/O address. */
    u64 io_size;
    int reg_shift; /* Bits to shift register offset by */
    int reg_width; /* Size of access to use, the registers
                    * themselves are still bytes */
    char __iomem *remapped_io_base;  /* Remapped virtual address of MMIO. */
    /* UART with IRQ line: interrupt-driven I/O. */
    struct irqaction irqaction;
    u8 lsr_mask;
#ifdef CONFIG_ARM
    struct vuart_info vuart;
#endif
    /* UART with no IRQ line: periodically-polled I/O. */
    struct timer timer;
#ifdef CONFIG_SYSTEM_SUSPEND
    struct timer resume_timer;
#endif
    unsigned int timeout_ms;
    bool intr_works;
    bool dw_usr_bsy;
#ifdef NS16550_PCI
    /* PCI card parameters. */
    bool pb_bdf_enable;     /* if =1, pb-bdf effective, port behind bridge */
    bool ps_bdf_enable;     /* if =1, ps_bdf effective, port on pci card */
    unsigned int pb_bdf[3]; /* pci bridge BDF */
    unsigned int ps_bdf[3]; /* pci serial port BDF */
    u32 bar;
    u32 bar64;
    u16 cr;
    u8 bar_idx;
    bool msi;
    const struct ns16550_config_param *param; /* Points into .init.*! */
#endif
} ns16550_com[2] = {};

#ifdef NS16550_PCI
struct ns16550_config {
    u16 vendor_id;
    u16 dev_id;
    enum {
        param_default, /* Must not be referenced by any table entry. */
        param_trumanage,
        param_oxford,
        param_oxford_2port,
        param_pericom_1port,
        param_pericom_2port,
        param_pericom_4port,
        param_pericom_8port,
        param_exar_xr17v352,
        param_exar_xr17v354,
        param_exar_xr17v358,
        param_intel_lpss,
    } param;
};

/* Defining uart config options for MMIO devices */
struct ns16550_config_param {
    unsigned int reg_shift;
    unsigned int reg_width;
    unsigned int fifo_size;
    u8 lsr_mask;
    bool mmio;
    bool bar0;
    unsigned int max_ports;
    unsigned int base_baud;
    unsigned int uart_offset;
    unsigned int first_offset;
};

static void enable_exar_enhanced_bits(const struct ns16550 *uart);
#endif

#ifdef CONFIG_SYSTEM_SUSPEND
static void cf_check ns16550_delayed_resume(void *data);
#endif

static u8 ns_read_reg(const struct ns16550 *uart, unsigned int reg)
{
    const volatile void __iomem *addr;

#ifdef CONFIG_HAS_IOPORTS
    if ( uart->remapped_io_base == NULL )
        return inb(uart->io_base + reg);
#endif

    addr = uart->remapped_io_base + (reg << uart->reg_shift);
    switch ( uart->reg_width )
    {
    case 1:
        return readb(addr);
    case 4:
        return readl(addr);
    default:
        return 0xff;
    }
}

static void ns_write_reg(const struct ns16550 *uart, unsigned int reg, u8 c)
{
    volatile void __iomem *addr;

#ifdef CONFIG_HAS_IOPORTS
    if ( uart->remapped_io_base == NULL )
        return outb(c, uart->io_base + reg);
#endif

    addr = uart->remapped_io_base + (reg << uart->reg_shift);
    switch ( uart->reg_width )
    {
    case 1:
        writeb(c, addr);
        break;
    case 4:
        writel(c, addr);
        break;
    default:
        /* Ignored */
        break;
    }
}

static int ns16550_ioport_invalid(struct ns16550 *uart)
{
    return ns_read_reg(uart, UART_IER) == 0xff;
}

static void handle_dw_usr_busy_quirk(struct ns16550 *uart)
{
    if ( uart->dw_usr_bsy &&
         (ns_read_reg(uart, UART_IIR) & UART_IIR_BSY) == UART_IIR_BSY )
    {
        /* DesignWare 8250 detects if LCR is written while the UART is
         * busy and raises a "busy detect" interrupt. Read the UART
         * Status Register to clear this state.
         *
         * Allwinner/sunxi UART hardware is similar to DesignWare 8250
         * and also contains a "busy detect" interrupt. So this quirk
         * fix will also be used for Allwinner UART.
         */
        ns_read_reg(uart, UART_USR);
    }
}

static void cf_check ns16550_interrupt(int irq, void *dev_id)
{
    struct serial_port *port = dev_id;
    struct ns16550 *uart = port->uart;

    uart->intr_works = 1;

    while ( !(ns_read_reg(uart, UART_IIR) & UART_IIR_NOINT) )
    {
        u8 lsr = ns_read_reg(uart, UART_LSR);

        if ( (lsr & uart->lsr_mask) == uart->lsr_mask )
            serial_tx_interrupt(port);
        if ( lsr & UART_LSR_DR )
            serial_rx_interrupt(port);

        /* A "busy-detect" condition is observed on Allwinner/sunxi UART
         * after LCR is written during setup. It needs to be cleared at
         * this point or UART_IIR_NOINT will never be set and this loop
         * will continue forever.
         *
         * This state can be cleared by calling the dw_usr_busy quirk
         * handler that resolves "busy-detect" for  DesignWare uart.
         */
        handle_dw_usr_busy_quirk(uart);
    }
}

/* Safe: ns16550_poll() runs as softirq so not reentrant on a given CPU. */
static DEFINE_PER_CPU(struct serial_port *, poll_port);

static void cf_check __ns16550_poll(const struct cpu_user_regs *regs)
{
    struct serial_port *port = this_cpu(poll_port);
    struct ns16550 *uart = port->uart;
    const struct cpu_user_regs *old_regs;

    if ( uart->intr_works )
        return; /* Interrupts work - no more polling */

    /* Mimic interrupt context. */
    old_regs = set_irq_regs(regs);

    while ( ns_read_reg(uart, UART_LSR) & UART_LSR_DR )
    {
        if ( ns16550_ioport_invalid(uart) )
            goto out;

        serial_rx_interrupt(port);
    }

    if ( ( ns_read_reg(uart, UART_LSR) & uart->lsr_mask ) == uart->lsr_mask )
        serial_tx_interrupt(port);

out:
    set_irq_regs(old_regs);
    set_timer(&uart->timer, NOW() + MILLISECS(uart->timeout_ms));
}

static void cf_check ns16550_poll(void *data)
{
    this_cpu(poll_port) = data;
    run_in_exception_handler(__ns16550_poll);
}

static int cf_check ns16550_tx_ready(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;

    if ( ns16550_ioport_invalid(uart) )
        return -EIO;

    return ( (ns_read_reg(uart, UART_LSR) &
              uart->lsr_mask ) == uart->lsr_mask ) ? uart->fifo_size : 0;
}

static void cf_check ns16550_putc(struct serial_port *port, char c)
{
    struct ns16550 *uart = port->uart;
    ns_write_reg(uart, UART_THR, c);
}

static int cf_check ns16550_getc(struct serial_port *port, char *pc)
{
    struct ns16550 *uart = port->uart;

    if ( ns16550_ioport_invalid(uart) ||
        !(ns_read_reg(uart, UART_LSR) & UART_LSR_DR) )
        return 0;

    *pc = ns_read_reg(uart, UART_RBR);
    return 1;
}

static void pci_serial_early_init(struct ns16550 *uart)
{
#ifdef NS16550_PCI
    if ( uart->bar && uart->io_base >= 0x10000 )
    {
        pci_conf_write16(PCI_SBDF(0, uart->ps_bdf[0], uart->ps_bdf[1],
                                  uart->ps_bdf[2]),
                         PCI_COMMAND, PCI_COMMAND_MEMORY);
        return;
    }

    if ( !uart->ps_bdf_enable || uart->io_base >= 0x10000 )
        return;

    if ( uart->pb_bdf_enable )
        pci_conf_write16(PCI_SBDF(0, uart->pb_bdf[0], uart->pb_bdf[1],
                                  uart->pb_bdf[2]),
                         PCI_IO_BASE,
                         (uart->io_base & 0xF000) |
                         ((uart->io_base & 0xF000) >> 8));

    pci_conf_write32(PCI_SBDF(0, uart->ps_bdf[0], uart->ps_bdf[1],
                              uart->ps_bdf[2]),
                     PCI_BASE_ADDRESS_0,
                     uart->io_base | PCI_BASE_ADDRESS_SPACE_IO);
    pci_conf_write16(PCI_SBDF(0, uart->ps_bdf[0], uart->ps_bdf[1],
                              uart->ps_bdf[2]),
                     PCI_COMMAND, PCI_COMMAND_IO);
#endif
}

static void ns16550_setup_preirq(struct ns16550 *uart)
{
    unsigned char lcr;
    unsigned int  divisor;

    uart->intr_works = 0;

    pci_serial_early_init(uart);

    lcr = (uart->data_bits - 5) | ((uart->stop_bits - 1) << 2) | uart->parity;

    /* No interrupts. */
    ns_write_reg(uart, UART_IER, 0);

    /* Handle the DesignWare 8250 'busy-detect' quirk. */
    handle_dw_usr_busy_quirk(uart);

#ifdef NS16550_PCI
    /* Enable Exar "Enhanced function bits" */
    enable_exar_enhanced_bits(uart);
#endif

    /* Line control and baud-rate generator. */
    ns_write_reg(uart, UART_LCR, lcr | UART_LCR_DLAB);
    if ( uart->baud != BAUD_AUTO )
    {
        /* Baud rate specified: program it into the divisor latch. */
        divisor = uart->clock_hz / (uart->baud << 4);
        ns_write_reg(uart, UART_DLL, (char)divisor);
        ns_write_reg(uart, UART_DLM, (char)(divisor >> 8));
    }
    else
    {
        /* Baud rate already set: read it out from the divisor latch. */
        divisor  = ns_read_reg(uart, UART_DLL);
        divisor |= ns_read_reg(uart, UART_DLM) << 8;
        if ( divisor )
            uart->baud = uart->clock_hz / (divisor << 4);
        else
            printk(CRUXLOG_ERR
                   "Automatic baud rate determination was requested,"
                   " but a baud rate was not set up\n");
    }
    ns_write_reg(uart, UART_LCR, lcr);

    /* No flow ctrl: DTR and RTS are both wedged high to keep remote happy. */
    ns_write_reg(uart, UART_MCR, UART_MCR_DTR | UART_MCR_RTS);

    /* Enable and clear the FIFOs. Set a large trigger threshold. */
    ns_write_reg(uart, UART_FCR,
                 UART_FCR_ENABLE | UART_FCR_CLRX | UART_FCR_CLTX | UART_FCR_TRG14);
}

static void __init cf_check ns16550_init_preirq(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;

#ifdef CONFIG_HAS_IOPORTS
    /* I/O ports are distinguished by their size (16 bits). */
    if ( uart->io_base >= 0x10000 )
#endif
    {
        uart->remapped_io_base = (char *)ioremap(uart->io_base, uart->io_size);
    }

    ns16550_setup_preirq(uart);

    /* Check this really is a 16550+. Otherwise we have no FIFOs. */
    if ( uart->fifo_size <= 1 &&
         ((ns_read_reg(uart, UART_IIR) & 0xc0) == 0xc0) &&
         ((ns_read_reg(uart, UART_FCR) & UART_FCR_TRG14) == UART_FCR_TRG14) )
        uart->fifo_size = 16;
}

static void __init cf_check ns16550_init_irq(struct serial_port *port)
{
#ifdef NS16550_PCI
    struct ns16550 *uart = port->uart;

    if ( uart->msi )
        uart->irq = create_irq(0, false);
#endif
}

static void ns16550_setup_postirq(struct ns16550 *uart)
{
    if ( uart->irq > 0 )
    {
        /* Master interrupt enable; also keep DTR/RTS asserted. */
        ns_write_reg(uart,
                     UART_MCR, UART_MCR_OUT2 | UART_MCR_DTR | UART_MCR_RTS);

        /* Enable receive interrupts. */
        ns_write_reg(uart, UART_IER, UART_IER_ERDAI);
    }

    if ( uart->irq >= 0 )
        set_timer(&uart->timer, NOW() + MILLISECS(uart->timeout_ms));
}

static void __init cf_check ns16550_init_postirq(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;
    int rc, bits;

    if ( uart->irq < 0 )
        return;

    serial_async_transmit(port);

    init_timer(&uart->timer, ns16550_poll, port, 0);
#ifdef CONFIG_SYSTEM_SUSPEND
    init_timer(&uart->resume_timer, ns16550_delayed_resume, port, 0);
#endif

    /* Calculate time to fill RX FIFO and/or empty TX FIFO for polling. */
    bits = uart->data_bits + uart->stop_bits + !!uart->parity;
    uart->timeout_ms = max_t(
        unsigned int, 1, (bits * uart->fifo_size * 1000) / uart->baud);

#ifdef NS16550_PCI
    if ( uart->bar || uart->ps_bdf_enable )
    {
        if ( uart->param && uart->param->mmio &&
             rangeset_add_range(mmio_ro_ranges, PFN_DOWN(uart->io_base),
                                PFN_UP(uart->io_base + uart->io_size) - 1) )
            printk(CRUXLOG_INFO "Error while adding MMIO range of device to mmio_ro_ranges\n");

        if ( pci_ro_device(0, uart->ps_bdf[0],
                           PCI_DEVFN(uart->ps_bdf[1], uart->ps_bdf[2])) )
            printk(CRUXLOG_INFO "Could not mark config space of %02x:%02x.%u read-only.\n",
                   uart->ps_bdf[0], uart->ps_bdf[1],
                   uart->ps_bdf[2]);

        if ( uart->msi )
        {
            struct msi_info msi = {
                .sbdf = PCI_SBDF(0, uart->ps_bdf[0], uart->ps_bdf[1],
                                 uart->ps_bdf[2]),
                .irq = uart->irq,
                .entry_nr = 1
            };

            rc = uart->irq;

            if ( rc > 0 )
            {
                struct msi_desc *msi_desc = NULL;
                struct pci_dev *pdev;

                pcidevs_lock();

                pdev = pci_get_pdev(NULL, msi.sbdf);
                rc = pdev ? pci_enable_msi(pdev, &msi, &msi_desc) : -ENODEV;

                if ( !rc )
                {
                    struct irq_desc *desc = irq_to_desc(msi.irq);
                    unsigned long flags;

                    spin_lock_irqsave(&desc->lock, flags);
                    rc = setup_msi_irq(desc, msi_desc);
                    spin_unlock_irqrestore(&desc->lock, flags);
                    if ( rc )
                        pci_disable_msi(msi_desc);
                }

                pcidevs_unlock();

                if ( rc )
                {
                    uart->irq = 0;
                    if ( msi_desc )
                        msi_free_irq(msi_desc);
                    else
                        destroy_irq(msi.irq);
                }
            }

            if ( rc )
                printk(CRUXLOG_WARNING
                       "MSI setup failed (%d) for %02x:%02x.%o\n",
                       rc, uart->ps_bdf[0], uart->ps_bdf[1], uart->ps_bdf[2]);
        }
    }
#endif

    if ( uart->irq > 0 )
    {
        uart->irqaction.handler = ns16550_interrupt;
        uart->irqaction.name    = "ns16550";
        uart->irqaction.dev_id  = port;
        if ( (rc = setup_irq(uart->irq, 0, &uart->irqaction)) != 0 )
            printk("ERROR: Failed to allocate ns16550 IRQ %d\n", uart->irq);
    }

    ns16550_setup_postirq(uart);
}

#ifdef CONFIG_SYSTEM_SUSPEND

static void cf_check ns16550_suspend(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;

    stop_timer(&uart->timer);

#ifdef NS16550_PCI
    if ( uart->bar )
       uart->cr = pci_conf_read16(PCI_SBDF(0, uart->ps_bdf[0], uart->ps_bdf[1],
                                  uart->ps_bdf[2]), PCI_COMMAND);
#endif
}

static void _ns16550_resume(struct serial_port *port)
{
#ifdef NS16550_PCI
    struct ns16550 *uart = port->uart;

    if ( uart->bar )
    {
       pci_conf_write32(PCI_SBDF(0, uart->ps_bdf[0], uart->ps_bdf[1],
                                 uart->ps_bdf[2]),
                        PCI_BASE_ADDRESS_0 + uart->bar_idx*4, uart->bar);

        /* If 64 bit BAR, write higher 32 bits to BAR+4 */
        if ( uart->bar & PCI_BASE_ADDRESS_MEM_TYPE_64 )
            pci_conf_write32(PCI_SBDF(0, uart->ps_bdf[0],  uart->ps_bdf[1],
                                      uart->ps_bdf[2]),
                        PCI_BASE_ADDRESS_0 + (uart->bar_idx+1)*4, uart->bar64);

       pci_conf_write16(PCI_SBDF(0, uart->ps_bdf[0], uart->ps_bdf[1],
                                 uart->ps_bdf[2]),
                        PCI_COMMAND, uart->cr);
    }
#endif

    ns16550_setup_preirq(port->uart);
    ns16550_setup_postirq(port->uart);
}

static int delayed_resume_tries;
static void cf_check ns16550_delayed_resume(void *data)
{
    struct serial_port *port = data;
    struct ns16550 *uart = port->uart;

    if ( ns16550_ioport_invalid(port->uart) && delayed_resume_tries-- )
        set_timer(&uart->resume_timer, NOW() + RESUME_DELAY);
    else
        _ns16550_resume(port);
}

static void cf_check ns16550_resume(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;

    /*
     * Check for ioport access, before fully resuming operation.
     * On some systems, there is a SuperIO card that provides
     * this legacy ioport on the LPC bus.
     *
     * We need to wait for dom0's ACPI processing to run the proper
     * AML to re-initialize the chip, before we can use the card again.
     *
     * This may cause a small amount of garbage to be written
     * to the serial log while we wait patiently for that AML to
     * be executed. However, this is preferable to spinning in an
     * infinite loop, as seen on a Lenovo T430, when serial was enabled.
     */
    if ( ns16550_ioport_invalid(uart) )
    {
        delayed_resume_tries = RESUME_RETRIES;
        set_timer(&uart->resume_timer, NOW() + RESUME_DELAY);
    }
    else
        _ns16550_resume(port);
}

#endif /* CONFIG_SYSTEM_SUSPEND */

static void __init cf_check ns16550_endboot(struct serial_port *port)
{
#ifdef CONFIG_HAS_IOPORTS
    struct ns16550 *uart = port->uart;
    int rv;

    if ( uart->remapped_io_base )
        return;
    rv = ioports_deny_access(hardware_domain, uart->io_base, uart->io_base + 7);
    if ( rv != 0 )
        BUG();
#endif
}

static int __init cf_check ns16550_irq(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;
    return ((uart->irq > 0) ? uart->irq : -1);
}

static void cf_check ns16550_start_tx(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;
    u8 ier = ns_read_reg(uart, UART_IER);

    /* Unmask transmit holding register empty interrupt if currently masked. */
    if ( !(ier & UART_IER_ETHREI) )
        ns_write_reg(uart, UART_IER, ier | UART_IER_ETHREI);
}

static void cf_check ns16550_stop_tx(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;
    u8 ier = ns_read_reg(uart, UART_IER);

    /* Mask off transmit holding register empty interrupt if currently unmasked. */
    if ( ier & UART_IER_ETHREI )
        ns_write_reg(uart, UART_IER, ier & ~UART_IER_ETHREI);
}

#ifdef CONFIG_ARM
static const struct vuart_info *ns16550_vuart_info(struct serial_port *port)
{
    struct ns16550 *uart = port->uart;

    return &uart->vuart;
}
#endif

static struct uart_driver __read_mostly ns16550_driver = {
    .init_preirq  = ns16550_init_preirq,
    .init_irq     = ns16550_init_irq,
    .init_postirq = ns16550_init_postirq,
    .endboot      = ns16550_endboot,
#ifdef CONFIG_SYSTEM_SUSPEND
    .suspend      = ns16550_suspend,
    .resume       = ns16550_resume,
#endif
    .tx_ready     = ns16550_tx_ready,
    .putc         = ns16550_putc,
    .getc         = ns16550_getc,
    .irq          = ns16550_irq,
    .start_tx     = ns16550_start_tx,
    .stop_tx      = ns16550_stop_tx,
#ifdef CONFIG_ARM
    .vuart_info   = ns16550_vuart_info,
#endif
};

static void ns16550_init_common(struct ns16550 *uart)
{
    uart->clock_hz  = UART_CLOCK_HZ;

    /* Default is no transmit FIFO. */
    uart->fifo_size = 1;

    /* Default lsr_mask = UART_LSR_THRE */
    uart->lsr_mask  = UART_LSR_THRE;
}

/* HUSTLER */

#ifdef CONFIG_HAS_DEVICE_TREE_DISCOVERY
static int __init ns16550_uart_dt_init(struct dt_device_node *dev,
                                       const void *data)
{
    struct ns16550 *uart;
    int res;
    u32 reg_shift, reg_width;

    uart = &ns16550_com[0];

    ns16550_init_common(uart);

    uart->baud      = BAUD_AUTO;
    uart->data_bits = 8;
    uart->parity    = UART_PARITY_NONE;
    uart->stop_bits = 1;

    res = dt_device_get_address(dev, 0, &uart->io_base, &uart->io_size);
    if ( res )
        return res;

    res = dt_property_read_u32(dev, "reg-shift", &reg_shift);
    if ( !res )
        uart->reg_shift = 0;
    else
        uart->reg_shift = reg_shift;

    res = dt_property_read_u32(dev, "reg-io-width", &reg_width);
    if ( !res )
        uart->reg_width = 1;
    else
        uart->reg_width = reg_width;

    if ( uart->reg_width != 1 && uart->reg_width != 4 )
        return -EINVAL;

    if ( dt_device_is_compatible(dev, "brcm,bcm2835-aux-uart") )
    {
        uart->reg_width = 4;
        uart->reg_shift = 2;
    }

    res = platform_get_irq(dev, 0);
    if ( ! res )
        return -EINVAL;
    uart->irq = res;

    uart->dw_usr_bsy = dt_device_is_compatible(dev, "snps,dw-apb-uart");

#ifdef CONFIG_ARM
    uart->vuart.base_addr = uart->io_base;
    uart->vuart.size = uart->io_size;
    uart->vuart.data_off = UART_THR <<uart->reg_shift;
    uart->vuart.status_off = UART_LSR<<uart->reg_shift;
    uart->vuart.status = UART_LSR_THRE|UART_LSR_TEMT;
#endif

    /* Register with generic serial driver. */
    serial_register_uart(uart - ns16550_com, &ns16550_driver, uart);

    dt_device_set_used_by(dev, DOMID_CRUX);

    return 0;
}

static const struct dt_device_match ns16550_dt_match[] __initconst =
{
    DT_MATCH_COMPATIBLE("ns16550"),
    DT_MATCH_COMPATIBLE("ns16550a"),
    DT_MATCH_COMPATIBLE("snps,dw-apb-uart"),
    DT_MATCH_COMPATIBLE("brcm,bcm2835-aux-uart"),
    { /* sentinel */ },
};

DT_DEVICE_START(ns16550, "NS16550 UART", DEVICE_SERIAL)
        .dt_match = ns16550_dt_match,
        .init = ns16550_uart_dt_init,
DT_DEVICE_END

#endif /* HAS_DEVICE_TREE_DISCOVERY */

#if defined(CONFIG_ACPI) && defined(CONFIG_ARM)
#include <crux/acpi.h>

static int __init ns16550_acpi_uart_init(const void *data)
{
    struct acpi_table_header *table;
    struct acpi_table_spcr *spcr;
    acpi_status status;
    /*
     * Same as the DT part.
     * Only support one UART on ARM which happen to be ns16550_com[0].
     */
    struct ns16550 *uart = &ns16550_com[0];

    status = acpi_get_table(ACPI_SIG_SPCR, 0, &table);
    if ( ACPI_FAILURE(status) )
    {
        printk("ns16550: Failed to get SPCR table\n");
        return -EINVAL;
    }

    spcr = container_of(table, struct acpi_table_spcr, header);

    if ( unlikely(spcr->serial_port.space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY) )
    {
        printk("ns16550: Address space type is not mmio\n");
        return -EINVAL;
    }

    /*
     * The serial port address may be 0 for example
     * if the console redirection is disabled.
     */
    if ( unlikely(!spcr->serial_port.address) )
    {
        printk("ns16550: Console redirection is disabled\n");
        return -EINVAL;
    }

    ns16550_init_common(uart);

    /*
     * The baud rate is pre-configured by the firmware.
     * And currently the ACPI part is only targeting ARM so the flow_control
     * field and all PCI related ones which we do not care yet are ignored.
     */
    uart->baud = BAUD_AUTO;
    uart->data_bits = 8;
    uart->parity = spcr->parity;
    uart->stop_bits = spcr->stop_bits;
    uart->io_base = spcr->serial_port.address;
    uart->io_size = DIV_ROUND_UP(spcr->serial_port.bit_width, BITS_PER_BYTE);
    uart->reg_shift = spcr->serial_port.bit_offset;
    uart->reg_width = spcr->serial_port.access_width;

    /* The trigger/polarity information is not available in spcr. */
    irq_set_type(spcr->interrupt, IRQ_TYPE_LEVEL_HIGH);
    uart->irq = spcr->interrupt;

    uart->vuart.base_addr = uart->io_base;
    uart->vuart.size = uart->io_size;
    uart->vuart.data_off = UART_THR << uart->reg_shift;
    uart->vuart.status_off = UART_LSR << uart->reg_shift;
    uart->vuart.status = UART_LSR_THRE | UART_LSR_TEMT;

    /* Register with generic serial driver. */
    serial_register_uart(SERHND_DTUART, &ns16550_driver, uart);

    return 0;
}

ACPI_DEVICE_START(ans16550, "NS16550 UART", DEVICE_SERIAL)
    .class_type = ACPI_DBG2_16550_COMPATIBLE,
    .init = ns16550_acpi_uart_init,
ACPI_DEVICE_END

#endif /* CONFIG_ACPI && CONFIG_ARM */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
