/* SPDX-License-Identifier: MIT */
/*
 * PCI Backend/Frontend Common Data Structures & Macros
 *
 *   Author: Ryan Wilson <hap9@epoch.ncsc.mil>
 */
#ifndef __CRUX_PCI_COMMON_H__
#define __CRUX_PCI_COMMON_H__

/* Be sure to bump this number if you change this file */
#define CRUX_PCI_MAGIC "7"

/* crux_pci_sharedinfo flags */
#define _CRUX_PCIF_active     (0)
#define CRUX_PCIF_active      (1<<_CRUX_PCIF_active)
#define _CRUX_PCIB_AERHANDLER (1)
#define CRUX_PCIB_AERHANDLER  (1<<_CRUX_PCIB_AERHANDLER)
#define _CRUX_PCIB_active     (2)
#define CRUX_PCIB_active      (1<<_CRUX_PCIB_active)

/* crux_pci_op commands */
#define CRUX_PCI_OP_conf_read    	(0)
#define CRUX_PCI_OP_conf_write   	(1)
#define CRUX_PCI_OP_enable_msi   	(2)
#define CRUX_PCI_OP_disable_msi  	(3)
#define CRUX_PCI_OP_enable_msix  	(4)
#define CRUX_PCI_OP_disable_msix 	(5)
#define CRUX_PCI_OP_aer_detected 	(6)
#define CRUX_PCI_OP_aer_resume		(7)
#define CRUX_PCI_OP_aer_mmio		(8)
#define CRUX_PCI_OP_aer_slotreset	(9)
#define CRUX_PCI_OP_enable_multi_msi	(10)

/* crux_pci_op error numbers */
#define CRUX_PCI_ERR_success          (0)
#define CRUX_PCI_ERR_dev_not_found   (-1)
#define CRUX_PCI_ERR_invalid_offset  (-2)
#define CRUX_PCI_ERR_access_denied   (-3)
#define CRUX_PCI_ERR_not_implemented (-4)
/* CRUX_PCI_ERR_op_failed - backend failed to complete the operation */
#define CRUX_PCI_ERR_op_failed       (-5)

/*
 * it should be PAGE_SIZE-sizeof(struct crux_pci_op))/sizeof(struct msix_entry))
 * Should not exceed 128
 */
#define SH_INFO_MAX_VEC     128

struct crux_msix_entry {
    uint16_t vector;
    uint16_t entry;
};
struct crux_pci_op {
    /* IN: what action to perform: CRUX_PCI_OP_* */
    uint32_t cmd;

    /* OUT: will contain an error number (if any) from errno.h */
    int32_t err;

    /* IN: which device to touch */
    uint32_t domain; /* PCI Domain/Segment */
    uint32_t bus;
    uint32_t devfn;

    /* IN: which configuration registers to touch */
    int32_t offset;
    int32_t size;

    /* IN/OUT: Contains the result after a READ or the value to WRITE */
    uint32_t value;
    /* IN: Contains extra infor for this operation */
    uint32_t info;
    /*IN:  param for msi-x */
    struct crux_msix_entry msix_entries[SH_INFO_MAX_VEC];
};

/*used for pcie aer handling*/
struct crux_pcie_aer_op
{

    /* IN: what action to perform: CRUX_PCI_OP_* */
    uint32_t cmd;
    /*IN/OUT: return aer_op result or carry error_detected state as input*/
    int32_t err;

    /* IN: which device to touch */
    uint32_t domain; /* PCI Domain/Segment*/
    uint32_t bus;
    uint32_t devfn;
};
struct crux_pci_sharedinfo {
    /* flags - CRUX_PCIF_* */
    uint32_t flags;
    struct crux_pci_op op;
    struct crux_pcie_aer_op aer_op;
};

#endif /* __CRUX_PCI_COMMON_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
