/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/device.h"
#include "kernel/irq.h"
#include "kernel/kmem.h"
#include "kernel/mm.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "kernel-md/io.h"
#include "kernel-md/pcihb.h"
#include "../acpica/acpi.h"

struct ACPI_IRQ_INFO : irq::IHandler {
    ACPI_OSD_HANDLER i_handler;
    void* i_context;

    irq::IRQResult OnIRQ() override
    {
        if (i_handler(i_context) == ACPI_INTERRUPT_HANDLED)
            return irq::IRQResult::Processed;
        return irq::IRQResult::Ignored;
    }
};

ACPI_STATUS
AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void* Context)
{
    auto info = new ACPI_IRQ_INFO;
    info->i_handler = Handler;
    info->i_context = Context;

    if (auto result = irq::Register(InterruptLevel, NULL, irq::type::Default, *info);
        result.IsFailure()) {
        kfree(info);
        return AE_BAD_PARAMETER;
    }
    return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler)
{
    /* XXX How are we going to locate the private info part? */
    panic("AcpiOsRemoveInterruptHandler()");
    return AE_OK;
}

ACPI_STATUS
AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64* Value, UINT32 Width)
{
    void* ptr = kmem_map(Address, Width, VM_FLAG_READ);
    if (ptr == NULL)
        return AE_BAD_ADDRESS;
    switch (Width) {
        case 8:
            *(UINT8*)Value = *(UINT8*)ptr;
            break;
        case 16:
            *(UINT16*)Value = *(UINT16*)ptr;
            break;
        case 32:
            *(UINT32*)Value = *(UINT32*)ptr;
            break;
        case 64:
            *(UINT64*)Value = *(UINT64*)ptr;
            break;
        default:
            panic("Unsupported width %u", Width);
    }
    kmem_unmap(ptr, Width);
    return AE_OK;
}

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    void* ptr = kmem_map(Address, Width, VM_FLAG_WRITE);
    if (ptr == NULL)
        return AE_BAD_ADDRESS;
    switch (Width) {
        case 8:
            *(UINT8*)ptr = (UINT8)Value;
            break;
        case 16:
            *(UINT16*)ptr = (UINT16)Value;
            break;
        case 32:
            *(UINT32*)ptr = (UINT32)Value;
            break;
        default:
            panic("Unsupported width %u", Width);
    }
    kmem_unmap(ptr, Width);
    return AE_OK;
}

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32* Value, UINT32 Width)
{
    switch (Width) {
        case 8:
            *(UINT8*)Value = inb(Address);
            break;
        case 16:
            *(UINT16*)Value = inw(Address);
            break;
        case 32:
            *(UINT32*)Value = inl(Address);
            break;
        default:
            panic("Unsupported width %u", Width);
    }
    return AE_OK;
}

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    switch (Width) {
        case 8:
            outb(Address, (UINT8)Value);
            break;
        case 16:
            outw(Address, (UINT16)Value);
            break;
        case 32:
            outl(Address, (UINT32)Value);
            break;
        default:
            panic("Unsupported width %u", Width);
    }
    return AE_OK;
}

ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Register, UINT64* Value, UINT32 Width)
{
    *Value = pci_read_config(PciId->Bus, PciId->Device, PciId->Function, Register, Width);
    return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID* PciId, UINT32 Register, UINT64 Value, UINT32 Width)
{
    pci_write_config(PciId->Bus, PciId->Device, PciId->Function, Register, Value, Width);
    return AE_OK;
}
