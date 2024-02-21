//
// Created by ndraey on 21.01.24.
//

#include "drv/pci.h"
#include "drv/audio/hda.h"
#include "io/ports.h"
#include "io/tty.h"
#include "mem/pmm.h"
#include "mem/vmm.h"

uint8_t hda_bus = 0,
        hda_slot = 0,
        hda_func = 0;

uint16_t hda_vendor = 0,
         hda_device = 0;

uint32_t hda_addr = 0;

void* hda_corb = 0;
void* hda_rirb = 0;

#define WRITE32(reg, value) *(volatile uint32_t*)(hda_addr + (reg)) = (value)
#define READ32(reg) (*(volatile uint32_t*)(hda_addr + (reg)))
#define WRITE16(reg, value) *(volatile uint16_t*)(hda_addr + (reg)) = (value)
#define READ16(reg) (*(volatile uint16_t*)(hda_addr + (reg)))
#define WRITE8(reg, value) *(volatile uint8_t*)(hda_addr + (reg)) = (value)
#define READ8(reg) (*(volatile uint8_t*)(hda_addr + (reg)))

void hda_init() {
    pci_find_device_by_class_and_subclass(4, 3, &hda_vendor, &hda_device, &hda_bus, &hda_slot, &hda_func);

    if(hda_vendor && hda_device) {
        qemu_ok("Found Intel HDA! (%x:%x)", hda_vendor, hda_device);
    } else {
        return;
    }

    hda_addr = pci_read32(hda_bus, hda_slot, hda_func, 0x10) & ~0b1111;

    qemu_ok("HDA address: %x", hda_addr);
    tty_printf("HDA address: %x\n", hda_addr);

    map_pages(
            get_kernel_page_directory(),
            hda_addr,
            hda_addr,
            PAGE_SIZE,
            PAGE_WRITEABLE | PAGE_CACHE_DISABLE // PAGE_PRESENT is set automatically
    );

    hda_reset();

    tty_printf("HDA RESET OKAY!\n");

    size_t data = READ16(0x00);

    size_t input_streams = (data >> 8) & 0b1111;
    size_t output_streams = (data >> 12) & 0b1111;

    tty_printf("HDA: I: %d; O: %d;\n", input_streams, output_streams);

    hda_disable_interrupts();

    //turn off dma position transfer
    WRITE32(0x70, 0);
    WRITE32(0x74, 0);

    //disable synchronization
    WRITE32(0x34, 0);
    WRITE32(0x38, 0);

    //stop CORB and RIRB
    WRITE8(0x4C, 0x0);
    WRITE8(0x5C, 0x0);
    
    hda_corb = kmalloc_common(1024, PAGE_SIZE);
    hda_rirb = kmalloc_common(1024, PAGE_SIZE);

    memset(hda_corb, 0, 1024);
    memset(hda_rirb, 0, 1024);

	WRITE32(0x40, (uint32_t)hda_corb); // First 32 bits
	WRITE32(0x44, 0); // Last 32 bits (we are 32-bit, so we don't need it)

	size_t entries = READ8(0x4E);

	if((entries & 0x40) == 0x40) {
		tty_printf("CORB: 256 entries\n");
	} else if((entries & 0x20) == 0x20) {
		tty_printf("CORB: 16 entries\n");
	} else if((entries & 0x10) == 0x10) {
		tty_printf("CORB: 2 entries\n");
	} else {
		tty_printf("CORB: NO CORB\n");
	}

}

void hda_reset() {
    if(!hda_vendor)
        return;

    WRITE32(0x8, 0);

    while((READ32(0x08) & 1) != 0);

    WRITE32(0x8, 1);

    while ((READ32(0x08) & 1) != 1);

    qemu_ok("Reset ok!");
}

void hda_disable_interrupts() {
    WRITE32(0x20, 0);
}
