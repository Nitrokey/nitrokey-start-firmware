/*
 * ST32F103 memory setup.
 */
__main_stack_size__     = 0x0400;
__process_stack_size__  = 0x0200;
__stacks_total_size__   = __main_stack_size__ + __process_stack_size__;

MEMORY
{
    ram0 : org = 0x20000000, len = 0x1a00
    ram1 : org = 0x20001400, len = 20k - 0x1a00
}

vector = 0x08000000;
_flash_start = 0x08001000;
_flash_end   = 0x08020000;

__ram_start__           = ORIGIN(ram0);
__ram_size__            = 20k;
__ram_end__             = __ram_start__ + __ram_size__;

SECTIONS
{
    . = 0;

    .bss :
    {
        _bss_start = .;
        *(.bss)
        . = ALIGN(4);
        *(.bss.*)
        . = ALIGN(4);
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > ram0

    .text : ALIGN(16) SUBALIGN(16)
    {
        _text = .;
        KEEP(*(.vectors))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.glue_7t)
        *(.glue_7)
        *(.gcc*)
    } > ram1

    .ctors :
    {
        PROVIDE(_ctors_start_ = .);
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))
        PROVIDE(_ctors_end_ = .);
    } > ram1

    .dtors :
    {
        PROVIDE(_dtors_start_ = .);
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
        PROVIDE(_dtors_end_ = .);
    } > ram1

    .ARM.extab : {*(.ARM.extab* .gnu.linkonce.armextab.*)}

    __exidx_start = .;
    .ARM.exidx : {*(.ARM.exidx* .gnu.linkonce.armexidx.*)} > ram1
    __exidx_end = .;

    .eh_frame_hdr : {*(.eh_frame_hdr)}

    .eh_frame : ONLY_IF_RO {*(.eh_frame)}

    . = ALIGN(4);
    _etext = .;
    _textdata = _etext;

    .data :
    {
        _data = .;
        *(.data)
        . = ALIGN(4);
        *(.data.*)
        . = ALIGN(4);
        *(.ramtext)
        . = ALIGN(4);
        _edata = .;
    } > ram1

    PROVIDE(end = .);
    _end            = .;
}

__heap_base__   = _end;
__heap_end__    = __ram_end__ - __stacks_total_size__;
