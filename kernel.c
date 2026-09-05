asm(".code16gcc");

unsigned char sinmat[] = {{0},{5},{10},{15},{20},{25},{30},{35},{40},{45},{50},{55},{60},{64},{69},{74},{79},{84},{88},{93},{98},{102},{107},{111},{116},{120},{125},{129},{133},{137},{142},{146},{150},{154},{158},{162},{166},{169},{173},{177},{180},{184},{187},{191},{194},{197},{200},{203},{206},{209},{212},{215},{217},{220},{222},{225},{227},{229},{232},{234},{236},{237},{239},{241},{243},{244},{245},{247},{248},{249},{250},{251},{252},{253},{253},{254},{254},{255},{255},{255},{255},{255},{255},{255},{254},{254},{253},{253},{252},{251},{250},{249},{248},{247},{245},{244},{243},{241},{239},{237},{236},{234},{232},{229},{227},{225},{222},{220},{217},{215},{212},{209},{206},{203},{200},{197},{194},{191},{187},{184},{180},{177},{173},{169},{166},{162},{158},{154},{150},{146},{142},{137},{133},{129},{125},{120},{116},{111},{107},{102},{98},{93},{88},{84},{79},{74},{69},{64},{60},{55},{50},{45},{40},{35},{30},{25},{20},{15},{10},{5},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},};

#include "stddef.h"
#include "stdint.h"

#define COM1_BASE          0x3F8

#define COM_DATA(base)     (base + 0) // Data register (read/write)
#define COM_IER(base)      (base + 1) // Interrupt Enable Register
#define COM_DLR_LSB(base)  (base + 0) // Divisor Latch LSB (when DLAB=1)
#define COM_DLR_MSB(base)  (base + 1) // Divisor Latch MSB (when DLAB=1)
#define COM_FCR(base)      (base + 2) // FIFO Control Register
#define COM_LCR(base)      (base + 3) // Line Control Register
#define COM_MCR(base)      (base + 4) // Modem Control Register
#define COM_LSR(base)      (base + 5) // Line Status Register

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
inline char inb(unsigned short port) {
    char ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

unsigned char kbd_us[256] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,      
 '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',

 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void bios_putchar(char c) {
    __asm__ __volatile__ (
        "int $0x10"
        :
        : "a"((0x0E << 8) | (unsigned char)c), "b"(0x0007)
    );
}

void bios_print(const char* str) {
    while (*str) {
        bios_putchar(*str++);
    }
}

void init_serial(unsigned short base) {
    outb(COM_IER(base), 0x00);    // Disable all UART interrupts
    
    // Set Baud Rate to 115200
    // 1. Enable DLAB (Divisor Latch Access Bit) to set baud rate
    outb(COM_LCR(base), 0x80);    
    // 2. Set divisor to 1 (1152000 base clock / 115200 baud = 1)
    outb(COM_DLR_LSB(base), 0x01); 
    outb(COM_DLR_MSB(base), 0x00); 
    
    // Configure Line parameters: 8 bits, no parity, 1 stop bit (8N1)
    // This also clears the DLAB bit (0x80) so we can write data again
    outb(COM_LCR(base), 0x03);    
    
    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM_FCR(base), 0xC7);    
    
    // Configure Modem Control: RTS/DSR set (Ready to Transmit)
    outb(COM_MCR(base), 0x0B);    
}

// Check if the transmit holding register is empty
int is_transmit_empty(unsigned short base) {
    return inb(COM_LSR(base)) & 0x20;
}

// Write a single character
void write_serial_char(unsigned short base, char c) {
    while (is_transmit_empty(base) == 0); // Wait until empty
    outb(COM_DATA(base), c);             // Write byte to data register
}

void com_print(const char* str) {
    while (*str) {
        write_serial_char(COM1_BASE, *str++);
    }
}

char temp[512];
int is_unreal_mode = 0;
int low_speed = 0;

unsigned char read_key() {
    while ((inb(0x64) & 1) == 0);
    return kbd_us[inb(0x60)];
}

unsigned char mouse_icon[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,1,1,0,0,0,0,0,0,0},
    {0,0,0,0,1,2,1,0,0,0,0,0,0},
    {0,0,0,0,1,2,2,1,0,0,0,0,0},
    {0,0,0,0,1,2,2,2,1,0,0,0,0},
    {0,0,0,0,1,2,2,2,2,1,0,0,0},
    {0,0,0,0,1,2,2,2,2,2,1,0,0},
    {0,0,0,0,1,2,2,2,2,2,1,1,0},
    {0,0,0,0,1,2,2,2,2,1,0,0,0},
    {0,0,0,0,1,2,1,2,2,1,0,0,0},
    {0,0,0,0,1,1,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,0,1,2,2,1,0},
    {0,0,0,0,0,0,0,0,0,1,2,1,0},
    {0,0,0,0,0,0,0,0,0,0,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0}
};

int mouse[2] = {160, 100};
int prev_mouse[2] = {0, 0};
int sensivity = 5;
uint8_t pressed[128];
uint8_t arrows[4];
char test_zone[24];

void char_to_booleans(char c, int bools[8]) {
    for (int i = 0; i < 8; i++) {
        // Shift the bit to the lowest position and mask it
        // (7 - i) extracts from Most Significant Bit (MSB) to Least Significant Bit (LSB)
        bools[i] = (c >> (7 - i)) & 1;
    }
}

char* custom_itoa(int value, char* buffer, int base) {
    // Проверка корректности системы счисления
    if (base < 2 || base > 16) {
        buffer[0] = '\0';
        return buffer;
    }

    char* ptr = buffer;
    char* ptr1 = buffer;
    char tmp_char;
    int tmp_value;

    // Обработка отрицательных чисел для десятичной системы
    bool is_negative = false;
    if (value < 0 && base == 10) {
        is_negative = true;
        // Используем беззнаковое смещение во избежание переполнения для INT_MIN
        unsigned int uvalue = -static_cast<long long>(value);
        
        do {
            tmp_value = uvalue % base;
            *ptr++ = "0123456789abcdef"[tmp_value];
            uvalue /= base;
        } while (uvalue);
        
        *ptr++ = '-';
    } else {
        // Для других систем счисления трактуем число как беззнаковое
        unsigned int uvalue = static_cast<unsigned int>(value);
        do {
            tmp_value = uvalue % base;
            *ptr++ = "0123456789abcdef"[tmp_value];
            uvalue /= base;
        } while (uvalue);
    }

    // Терминирующий ноль
    *ptr--;
    
    // Переворачиваем строку в буфере, так как цифры записались задом наперед
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr = *ptr1;
        *ptr1 = tmp_char;
        ptr1++;
        ptr--;
    }

    return buffer;
}

char itoabuf[20];

void* memcpy(void* dest, const void* src, unsigned long n) {
    if (dest == src || n == 0) return dest;

    // Встроенный ассемблер GCC для x86/x64
    __asm__ __volatile__ (
        "cmp %[src], %[dest]\n\t"  // Сравниваем указатели
        "jae 1f\n\t"               // Если dest >= src, копируем с конца (назад)
        "cld\n\t"                  // Иначе: DF=0, копируем вперед
        "rep movsb\n\t"
        "jmp 2f\n\t"
        "1:\n\t"
        "lea -1(%[src], %[n]), %[src]\n\t"   // Сдвигаем src на последний байт
        "lea -1(%[dest], %[n]), %[dest]\n\t" // Сдвигаем dest на последний байт
        "std\n\t"                            // DF=1, копируем назад
        "rep movsb\n\t"
        "cld\n\t"                            // Сбрасываем DF обратно в 0 (важно для GCC!)
        "2:\n\t"
        : [dest] "+D" (dest), [src] "+S" (src), [n] "+c" (n) // Назначаем регистры EDI/RDI, ESI/RSI, ECX/RCX
        :
        : "memory"
    );

    return dest;
}
void bare_metal_memcpy(void* dest, const void* src, size_t n) {
    // Входные указатели и счетчик обновляются самой инструкцией, 
    // поэтому используются как input/output операнды ("+") с правильными регистрами.
    __asm__ volatile (
        "cld\n\t"             // Сброс флага направления (DF=0) для копирования вперед
        "rep movsb"           // Повторять копирование байта из [rsi] в [rdi] пока rcx != 0
        : "+D"(dest),         // %0 -> EDI/RDI (Destination Pointer)
          "+S"(src),          // %1 -> ESI/RSI (Source Pointer)
          "+c"(n)             // %2 -> ECX/RCX (Counter)
        :                     // Нет чисто входных операндов
        : "memory"            // Сообщает компилятору об изменении памяти по указателям
    );
}

void bare_metal_memcpy_16(uint16_t dest_seg, uint16_t dest_offset, 
                          uint16_t src_seg,  uint16_t src_offset, 
                          uint16_t n) 
{
    __asm__ volatile (
        "push %%ds\n\t"       // Сохраняем текущий DS
        "push %%es\n\t"       // Сохраняем текущий ES
        
        "movw %3, %%ds\n\t"   // Загружаем сегмент источника в DS
        "movw %1, %%es\n\t"   // Загружаем сегмент приемника в ES
        "cld\n\t"
        "rep movsb\n\t"
        
        "pop %%es\n\t"        // Восстанавливаем ES
        "pop %%ds"            // Восстанавливаем DS
        : "+D"(dest_offset), "+S"(src_offset), "+c"(n)
        : "r"(dest_seg), "r"(src_seg)
        : "memory"
    );
}

void disable_x86_caching(void) {
    uint32_t cr0_val;

    // 1. Read the current CR0 register value
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0_val));

    // 2. Set CD (Cache Disable, bit 30) and NW (Not Write-through, bit 29)
    cr0_val |= (1 << 30) | (1 << 29);

    // 3. Write back the modified value into CR0
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0_val) : "memory");

    // 4. Flush and invalidate all internal cache levels completely
    __asm__ __volatile__("wbinvd" : : : "memory");
}

void memcheck(int use_com) {
    int block = 0;
    unsigned long j = 0;
    if(use_com) {
        com_print("MEMCHECK 1.0 SCANNING");
    } else {
        bios_print("MEMCHECK 1.0 SCANNING");
    }
    for(unsigned long i = 0x0000FFFF; i < 134217727; i++) {
        unsigned long curblock = (i-0x0000FFFF)>>18;
        if(curblock > block) {
            if(use_com) com_print(".");
            else bios_print(".");
            block = curblock;
        }
        unsigned long segment = i<<4;
        *((unsigned char*)(segment)) = 0xFA;
        (*((unsigned char*)(segment)))++;
        if(*((unsigned char*)(segment)) != 0xFB) break;
        j = i;
    }
    custom_itoa(j<<4, itoabuf, 10);
    if(use_com) {
        com_print("\r\nMEM: ");
        com_print(itoabuf);
        com_print("\r\n");
    } else {
        bios_print("\r\nMEM: ");
        bios_print(itoabuf);
        bios_print("\r\n");
    }
}

void second(void) {
    // 1 секунда — это примерно 18.2065 «тиков» таймера при делителе 65535
    const int ticks_needed = 18; 

    // Настраиваем PIT: Канал 0, режим 0 (Interrupt on Terminal Count), доступ LSB/MSB
    outb(0x43, 0x30); 
    
    for (int i = 0; i < ticks_needed; i++) {
        // Загружаем максимальный делитель (0xFFFF = 65535)
        outb(0x40, 0xFF); // Младший байт (LSB)
        outb(0x40, 0xFF); // Старший байт (MSB)

        // Опрашиваем состояние, пока счетчик не обнулится
        while (1) {
            // Читаем текущее значение счетчика «на лету»
            outb(0x43, 0x00); // Команда фиксации счетчика канала 0
            uint8_t lsb = inb(0x40);
            uint8_t msb = inb(0x40);
            uint16_t count = (msb << 8) | lsb;

            // В режиме 0 счетчик убывает. Если он стал близок к 0, цикл прошел.
            if (count < 10) { 
                break;
            }
        }
    }
}

void unreal_memcpy(uint32_t dest_flat, uint32_t src_flat, uint32_t n) {
    // В Unreal Mode DS и ES уже должны быть равны 0 (плоская модель)
    __asm__ volatile (
        "cld\n\t"
        // Префикс .byte 0x67 заставляет процессор использовать 32-битные EDI/ESI/ECX
        // в 16-битном коде. GCC делает это автоматически, если использовать префикс "D" и "S"
        // для 32-битных переменных (uint32_t).
        "rep movsb"
        : "+D"(dest_flat), "+S"(src_flat), "+c"(n)
        :
        : "memory"
    );
}

unsigned int bootdrive = 0;

#include "fs.c"
#include "fat12/fat12.c"
#include "umode.c"
#include "intdrivers.c"
#include "window.c"

Window windows[16];

unsigned char tempr, tempg, tempb;

void drawPixelOnScreen(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
    for(int i = 0; i < 16; i++) {
        if(windows[i].visible) {
            if(windows[i].type != 0x01) {
                return;
            }
            if(x > windows[i].x && y > windows[i].y && x < windows[i].x+windows[i].w && y < windows[i].y+windows[i].h) {
                ((void (*)(int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, void *custom_data))(windows[i].drawPixelAt))(x - windows[i].x, y - windows[i].y, r, g, b, windows[i].custom_data);
                return;
            }
        }
    }
    *r = 0;
    *g = 0;
    *b = 0;
}

void redraw_screen() {
    for(int x = 0; x < 320; x++) {
        for(int y = 0; y < 200; y++) {
            /*unsigned short colr = sinmat[x];
            unsigned short colg = sinmat[(x+80) % 320];
            unsigned short colb = sinmat[(x+160) % 320];
            colr += sinmat[y];
            colg += sinmat[(y+80) % 320];
            colb += sinmat[(y+160) % 320];
            colr = colr>>1;
            colg = colg>>1;
            colb = colb>>1;*/
            drawPixelOnScreen(x, y, &tempr, &tempg, &tempb);
            putpixel_asm(x, y, tempr, tempg, tempb); // 0 255 255
        }
    }
}

char *ccharaerr[256];
/*volatile */fat_boot_sector_t boot_sector[512];
uint32_t root_dir_sector = boot_sector->reserved_sectors + (boot_sector->num_fats * boot_sector->sectors_per_fat);
uint32_t root_dir_size = ((boot_sector->root_entries * 32) +
                              (boot_sector->bytes_per_sector - 1)) /
                             boot_sector->bytes_per_sector;

extern "C" {
    void kernel_main() {
        for(int i = 0; i < 16; i++) {
            windows[i].visible = 0;
        }
        windows[0].visible = 1;
        windows[0].x = 10;
        windows[0].y = 20;
        windows[0].w = 100;
        windows[0].h = 50;
        windows[0].title = "Hello, World!";
        windows[0].drawPixelAt = (void*)testWindowRenderer;
        windows[0].type = 0x01;
        windows[1].visible = 1;
        windows[1].x = 50;
        windows[1].y = 50;
        windows[1].w = 100;
        windows[1].h = 100;
        windows[1].title = "Test";
        windows[1].drawPixelAt = (void*)metaRender;
        windows[1].custom_data = ccharaerr;
        windows[1].type = 0x01;
        init_serial(COM1_BASE);
        bios_print("Hello World!\r\n");
        com_print("Hello World!\r\n");
        unsigned char status = get_disk_geometry(bootdrive, &BootGeometry);
        if (status == 0) {
            com_print("BootGeometry = \n\r");
            com_print("C_base256:");
            write_serial_char(COM1_BASE, BootGeometry.cylinders);
            com_print("\n\r");
            com_print("H_base256:");
            write_serial_char(COM1_BASE, BootGeometry.heads);
            com_print("\n\r");
            com_print("SPT_base256:");
            write_serial_char(COM1_BASE, BootGeometry.sectors_per_track);
            com_print("\n\r");
            com_print("DT_base256:");
            write_serial_char(COM1_BASE, BootGeometry.drive_type);
            com_print("\n\r");
        } else {
            com_print("Failed to get BootGeometry!\r\n");
            com_print("Maybe we booting from floppy\r\n");
            BootGeometry.cylinders = 80;
            BootGeometry.heads = 2;
            BootGeometry.sectors_per_track = 18;
            BootGeometry.drive_type = 4;
        }
        bios_print("\r\nEnter Unreal Mode? (i286 and higher) [Y/n] ");
        unsigned char key = read_key();
        bios_putchar(key);
        read_key();
        if(!(key == 'n')) {
            enter_unreal_mode();
            is_unreal_mode = 1;
            com_print("is_unreal_mode: true");
        } else {
            com_print("is_unreal_mode: false");
        }
        bios_print("\r\nUse Slow Processor Mode? [Y/n] ");
        key = read_key();
        bios_putchar(key);
        read_key();
        if(!(key == 'n')) {
            //enter_unreal_mode();
            low_speed = 1;
            com_print("low_speed: true");
        } else {
            low_speed = 0;
            com_print("low_speed: false");
        }
        //disable_x86_caching();
        bios_print("\r\n");
        setup_video();//set_video_mode(0x10D);
        select_rgb_palette();
        bios_print("Reading configuration...\r\n");
        read_sector(bootdrive, 0, 0, 0, 1, (void*)boot_sector);
        com_print("\r\nOEM: ");
        for(int i = 0; i < 8; i++) {
            write_serial_char(COM1_BASE, boot_sector->oem_name[i]);
        }
        com_print("\r\nVOLUME: ");
        for(int i = 0; i < 11; i++) {
            write_serial_char(COM1_BASE, boot_sector->volume_label[i]);
        }
        com_print("\r\nFS: ");
        for(int i = 0; i < 8; i++) {
            write_serial_char(COM1_BASE, boot_sector->filesystem_type[i]);
        }
        uint8_t *buf = (uint8_t *)0x0700;
        int found = 0;
        for(uint32_t i = 0; i < root_dir_size; i++) {
            read_sectors_lba(root_dir_sector + i, 1, (void*)buf);
            //dump(buf, 512);
            for(uint8_t j = 0; j < 512 / sizeof(fat_dir_entry_t); j++) {
                fat_dir_entry_t* entry = (fat_dir_entry_t*)(buf + j * sizeof(fat_dir_entry_t));
                //puts(entry->filename);
                if(memcmp(entry->filename, "START   SYS", 8 + 3) == 0) {
                    //putchar('Y');
                    root_dir_sector += root_dir_size;
                    root_dir_sector += (entry->first_cluster_low - 2) * boot_sector->sectors_per_cluster;
                    read_sectors_lba(root_dir_sector, 3, (void*)buf);
                    //dump(buf);
                    //printhex(buf[0]);
                    //printhex(buf[1]);
                    //printhex(buf[2]);
                    //printhex(buf[3]);
                    found = 1;
                }
            }
            //dump(buf, boot_sector->bytes_per_sector);
        }
        if(!found) kernel_error(0x00000001); // START.SYS not found or boot drive error
        bios_print((const char*)buf);
        /*bios_print("Reading root...\r\n");
        int filestat = stage2_list_root_directory(bootdrive);
        if(filestat == 0) {
            bios_print("\r\n[DONE]\r\n");
        } else {
            bios_print("\r\n[ERROR ");
            custom_itoa(filestat, test_zone, 10);
            bios_print(test_zone);
            bios_print("]\r\n");
        }
        second();
        second();*/
        //memcheck(0);
        second();
        second();
        second();
        second();
        bios_print("Starting...");
        hook_keyboard();
        redraw_screen();
        //draw_char('T', 0, 0, 255, 255, 255);
        /*for(int x = 0; x < 592; x++) {
            for(int y = 0; y < 48; y++) {
                putpixel(48+x, 432+y, 0x07);
            }
        }
        for(int x = 0; x < 48; x++) {
            for(int y = 0; y < 48; y++) {
                putpixel(x, 432+y, 0x04);
            }
        }
        for(int x = 0; x < 8; x++) {
            for(int y = 0; y < 8; y++) {
                int o = mouse_icon[x+y*8]-1;
                if(o) {
                    putpixel(mouse.x+x, mouse.y+y, o-1);
                }
            }
        }*/
        /*read_key();
        for(int x = 0; x < 640; x++) {
            for(int y = 0; y < 480; y++) {
                putpixel(x, y, 0x0B);
            }
        }*/
        unsigned char keycode = 0;
        char ascii_char = 0;
        unsigned long t = 0;
        unsigned long cvt = low_speed ? 8192 : 256;
        unsigned long sus = low_speed ? 65536 : 32768;
        int bools[8];
        //ps2_flush_buffers();
        while(1) {
            if (inb(0x64) & 0x01) {
                keycode = inb(0x60); // Read and save the key scancode
                //keycode = kbd_us[keycode];
                pressed[kbd_us[keycode & 0b01111111]] = keycode > 127 ? 0 : 1;
                if (keycode == 0x48) {
                    arrows[0] = 1;
                } else if (keycode == 0x4D) {
                    arrows[1] = 1;
                } else if (keycode == 0x4B) {
                    arrows[2] = 1;
                } else if (keycode == 0x50) {
                    arrows[3] = 1;
                }
            }
            if(arrows[0]) {
                mouse[1] -= sensivity;
                arrows[0] = 0;
            }
            if(arrows[1]) {
                mouse[0] += sensivity;
                arrows[1] = 0;
            }
            if(arrows[2]) {
                mouse[0] -= sensivity;
                arrows[2] = 0;
            }
            if(arrows[3]) {
                mouse[1] += sensivity;
                arrows[3] = 0;
            }
            if(t % cvt == 0) {
                //screen_flip();
                //com_print("\r\nDRaW\r\n");
                for(int x = 0; x < 48; x++) {
                    for(int y = 0; y < 48; y++) {
                        drawPixelOnScreen(mouse[0]-20+x, mouse[1]-17+y, &tempr, &tempg, &tempb);
                        //unsigned short colr = sinmat[(mouse[0]-20+x+80) % 320];
                        //unsigned short colg = sinmat[(mouse[0]-20+x+80) % 320];
                        //unsigned short colb = sinmat[(mouse[0]-20+x+240) % 320];
                        //colr += sinmat[mouse[1]-17+y];
                        //colg += sinmat[(mouse[1]-17+y+160) % 320];
                        //colb += sinmat[(mouse[1]-17+y+160) % 320];
                        //colr = colr>>1;
                        //colg = colg>>1;
                        //colb = colb>>1;
                        if(x > 16 && y > 16 && x < 32 && y < 32) {
                            int vx = x - 16;
                            int vy = y - 16;
                            int o = mouse_icon[vy][vx];
                            if(o > 0) {
                                if(o == 1) {
                                    putpixel_asm(mouse[0]-4+vx, mouse[1]-1+vy, 0, 0, 0);
                                } else {
                                    putpixel_asm(mouse[0]-4+vx, mouse[1]-1+vy, 255, 255, 255);
                                }
                            } else {
                                putpixel_asm(mouse[0]-4+vx, mouse[1]-1+vy, tempr, tempg, tempb);
                            }
                        } else {
                            putpixel_asm(mouse[0]-20+x, mouse[1]-17+y, tempr, tempg, tempb);
                        }
                    }
                }
            }
            if(t % 8192 == 0) {
                //windows[0].x++;
                //windows[1].y++;
                for(int i = 0; i < 16; i++) {
                    if(windows[i].visible) {
                        int drawtitle = 1;
                        for(int j = 0; j < i; j++) {
                            if(windows[j].visible) {
                                if(windows[i].x > windows[j].x && windows[i].y-8 > windows[j].y && windows[i].x+windows[i].w > windows[j].x+windows[j].w && windows[i].y-8+windows[i].y > windows[j].y+windows[j].h) drawtitle = 0;
                            }
                        }
                        if(drawtitle) {
                            int j = 0;
                            for(char*str = (char*)windows[i].title; *str; str++) {
                                draw_char(*str, windows[i].x+j*8, windows[i].y-16, 255, 255, 255);
                                j++;
                            }
                        }
                    }
                }
            }
            if(t % sus == 0) redraw_screen();
            t++;
        }
        while(1);
    }
}