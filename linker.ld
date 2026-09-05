OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386) /* Указываем базовую архитектуру для линкера */

SECTIONS
{
    . = 0x0000; /* Смещение 0 внутри сегмента 0x0020 */

    .text : {
        kernel_entry.o(.text) /* Первый код, который получит управление */
        *(.text)
    }

    .data : {
        *(.data)
        *(.rodata*)
    }

    .bss : {
        *(.bss)
    }
}