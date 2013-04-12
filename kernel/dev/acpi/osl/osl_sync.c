#include <ananas/lock.h>
#include <ananas/mm.h>
#include "../acpica/acpi.h"

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE* OutHandle)
{
	semaphore_t* sem = kmalloc(sizeof(semaphore_t));
	if (sem == NULL)
		return AE_NO_MEMORY;

	sem_init(sem, InitialUnits);
	*OutHandle = sem;
	return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
	if (Handle == NULL)
		return AE_BAD_PARAMETER;
	kfree(Handle);
	return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
	KASSERT(Units == 1, "unsupported number of units");
	/*KASSERT(Timeout == -1, "unsupported timout value");*/
	if (Handle == NULL)
		return AE_BAD_PARAMETER;
	sem_wait(Handle);
	return AE_OK;
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
	KASSERT(Units == 1, "unsupported number of units");
	if (Handle == NULL)
		return AE_BAD_PARAMETER;
	sem_signal(Handle);
	return AE_OK;
}

ACPI_STATUS
AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle)
{
	spinlock_t* sl = kmalloc(sizeof(spinlock_t));
	if (sl == NULL)
		return AE_NO_MEMORY;

	spinlock_init(sl);
	*OutHandle = sl;
	return AE_OK;
}

void
AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
	kfree(Handle);
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
	return spinlock_lock_unpremptible(Handle);
}

void
AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
	spinlock_unlock_unpremptible(Handle, Flags);
}

/* vim:set ts=2 sw=2: */
