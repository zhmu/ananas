#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/resourceset.h"
#include "kernel/vm.h"

const Resource*
ResourceSet::GetResource(const Resource::Type& type, size_t index) const
{
	for(const Resource& res: rs_Resource) {
		if (res.r_Type != type)
			continue;
		if (index-- > 0)
			continue;
		return &res;
	}

	return nullptr;
}

void*
ResourceSet::AllocateResource(const Resource::Type& type, Resource::Length length, size_t index)
{
	auto res = const_cast<Resource*>(GetResource(type, index));
	if (res == nullptr)
		return nullptr; // no such resource

	res->r_Length = length;
	switch(type) {
		case Resource::RT_Memory:
			return (void*)kmem_map(res->r_Base, res->r_Length, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
		case Resource::RT_IO:
		case Resource::RT_ChildNum:
		case Resource::RT_IRQ: // XXX should allocate, not just return
		case Resource::RT_USB_Device: // XXX should allocate, not just return
			return reinterpret_cast<void*>(res->r_Base);
		default:
			panic("resource type %u exists, but can't allocate", type);
	}

	/* NOTREACHED */
	return nullptr;
}

bool
ResourceSet::AddResource(const Resource& resource)
{
	Resource* res = &rs_Resource[0];
	for (size_t i = 0; i < rs_MaxEntries; i++, res++) {
		if (res->r_Type != Resource::RT_Unused)
			continue;
		*res = resource;
		return true;
	}

	return false;
}

void
ResourceSet::Print() const
{
	for(const Resource& res: rs_Resource) {
		bool hex = false;
		switch (res.r_Type) {
			case Resource::RT_Memory: kprintf("memory "); hex = true; break;
			case Resource::RT_IO: kprintf("io "); hex = true; break;
			case Resource::RT_IRQ: kprintf("irq "); break;
			case Resource::RT_ChildNum: kprintf("child "); break;
			case Resource::RT_PCI_Bus: kprintf("bus "); break;
			case Resource::RT_PCI_Device: kprintf("dev "); break;
			case Resource::RT_PCI_Function: kprintf("func "); break;
			case Resource::RT_PCI_VendorID: kprintf("vendor "); hex = true; break;
			case Resource::RT_PCI_DeviceID: kprintf("device "); hex = true; break;
			case Resource::RT_PCI_ClassRev: kprintf("class/revision "); break;
			default: continue;
		}
		kprintf(hex ? "0x%x" : "%u", res.r_Base);
		if (res.r_Length > 0)
			kprintf(hex ? "-0x%x" : "-%u", res.r_Base + res.r_Length);
		kprintf(" ");
	}
}

/* vim:set ts=2 sw=2: */
