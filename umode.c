#include <stdint.h>

// Элемент GDT. В GCC обязательно используем packed, 
// чтобы компилятор не добавил лишнее выравнивание (padding).
struct gdt_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Делаем выравнивание GDT, чтобы процессору было легче ее читать
struct gdt_descriptor gdt[2] __attribute__((aligned(8)));
struct gdt_ptr gp;

void init_gdt() {
    gdt[0] = (struct gdt_descriptor){0, 0, 0, 0, 0, 0};
    gdt[1] = (struct gdt_descriptor){0xFFFF, 0, 0, 0x92, 0xCF, 0};

    // Ваш DS = 0x2000. Линейный адрес = 0x20000 + смещение &gdt
    uint32_t linear_gdt = (0x2000 << 4) + (uint32_t)&gdt;

    gp.limit = (sizeof(struct gdt_descriptor) * 2) - 1;
    gp.base = linear_gdt;
}

void enter_unreal_mode() {
    __asm__ __volatile__ (
        ".code16gcc\n\t"
        "cli\n\t"

        "pushw %%ds\n\t"
        "pushw %%es\n\t"
        "pushw %%cs\n\t"
        "popw %%ds\n\t"             // Делаем DS = CS (0x2000), чтобы адресовать локальную GDT

        // Загружаем GDT. Используем префикс data32 (так как мы в 16-битном коде)
        // Адрес вычисляется как (CS * 16) + смещение метки 2f внутри сегмента
        "data32 lgdt (2f)\n\t"

        // Включаем Protected Mode
        "mov %%cr0, %%eax\n\t"
        "or $1, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"

        // Короткий прыжок для очистки конвейера
        "jmp 1f\n"
        "1:\n\t"

        // Загружаем селектор данных (0x08)
        "mov $0x08, %%bx\n\t"
        "mov %%bx, %%ds\n\t"
        "mov %%bx, %%es\n\t"

        // Выключаем Protected Mode
        "mov %%cr0, %%eax\n\t"
        "and $0xFFFFFFFE, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"

        "popw %%es\n\t"             // Восстанавливаем оригинальный ES
        "popw %%ds\n\t"             // Восстанавливаем оригинальный DS (но лимит уже 4ГБ!)
        "sti\n\t"
        "jmp 3f\n\t"                // Обходим данные стороной

        // --- ДАННЫЕ GDT ВНУТРИ КОДА ---
        ".align 4\n"
        "gdt_start:\n\t"
        // Селектор 0x00: Null Descriptor
        ".word 0, 0\n\t"
        ".byte 0, 0, 0, 0\n\t"
        
        // Селектор 0x08: Flat Data (база=0, лимит=4GB, доступ=0x92, гранулярность=0xCF)
        ".word 0xFFFF\n\t"          // Limit low
        ".word 0x0000\n\t"          // Base low
        ".byte 0x00\n\t"            // Base middle
        ".byte 0x92\n\t"            // Access byte
        ".byte 0xCF\n\t"            // Granularity + Limit high
        ".byte 0x00\n\t"            // Base high
        "gdt_end:\n"

        // Структура GDTR (псевдо-дескриптор)
        "2:\n\t"
        ".word (gdt_end - gdt_start) - 1\n\t" // Лимит таблицы
        ".long 0x20000 + gdt_start\n\t"       // АБСОЛЮТНЫЙ физический адрес таблицы (0x2000 * 16 + gdt_start)

        "3:\n\t"
        :
        :
        : "eax", "ebx", "cc", "memory"
    );
}

// Пример прямой записи в память выше 1 МБ (например, в область 2 МБ)
void write_high_memory(uint32_t physical_address, uint8_t value) {
    // В Unreal Mode мы обнуляем сегмент ES, чтобы использовать его как плоский 32-битный указатель
    __asm__ __volatile__ (
        "push %%es\n\t"
        "xor %%ax, %%ax\n\t"
        "mov %%ax, %%es\n\t"      // ES = 0, базовая адресация от 0x0
        "movb %1, %%es:(%0)\n\t"  // Запись байта по абсолютному 32-битному адресу
        "pop %%es\n\t"
        :
        : "r" (physical_address), "r" (value)
        : "ax"
    );
}