#include <stdint.h>

/**
 * Structure to hold the decoded CHS geometry of a drive.
 */
typedef struct {
    unsigned short cylinders;
    unsigned char heads;
    unsigned char sectors_per_track;
    unsigned char drive_type; // Returns 1=360KB, 2=1.2MB, 4=1.44MB for floppies
} DiskGeometry;

DiskGeometry BootGeometry;

/**
 * Reads sectors from a disk/floppy using BIOS INT 0x13, AH=0x02.
 *
 * @param drive    Drive number (0x00 for Floppy A:)
 * @param cylinder Cylinder/Track number (0 - 79)
 * @param head     Head/Side number (0 - 1)
 * @param sector   Sector number (1-based index, usually 1 - 18)
 * @param count    Number of sectors to read
 * @param buffer   Pointer to the destination memory buffer
 * @return         0 on success, or the BIOS error code in AH on failure.
 */
unsigned char read_sector(unsigned char drive, 
                                 unsigned char cylinder, 
                                 unsigned char head, 
                                 unsigned char sector, 
                                 unsigned char count, 
                                 void *buffer) 
{
    unsigned char error_code;
    unsigned char sectors_read;
    __asm__ __volatile__ (
        "int $0x13\n\t"
        "setc %b0\n\t"
        : "=q" (error_code), "=a" (sectors_read)
        : "a" ((0x02 << 8) | count),
          "b" ((unsigned short)(unsigned long)buffer),
          "c" (((cylinder & 0xFF) << 8) | sector),
          "d" ((head << 8) | drive)
        : "cc", "memory"
    );
    if (error_code) {
        return (sectors_read >> 8); 
    }
    return 0;
}

/**
 * Gets the CHS parameters of a specific drive.
 * 
 * @param drive    Drive number (0x00 for Floppy A:, 0x80 for Hard Drive 0)
 * @param geo      Pointer to a DiskGeometry structure to store the results
 * @return         0 on success, or the BIOS error code on failure.
 */
unsigned char get_disk_geometry(unsigned char drive, DiskGeometry *geo) {
    unsigned char error_flag;
    unsigned char error_code;
    unsigned short bx_val, cx_val, dx_val;
    /*__asm__ __volatile__ (
        "clc\n\t"
        "int $0x13\n\t"
        "setc %0\n\t"
        : "=q" (error_flag), "=a" (error_code), "=b" (bx_val), "=c" (cx_val), "=d" (dx_val)
        : "a" (0x08 << 8),
          "d" (drive)
        : "cc", "memory"
    );*/
    __asm__ __volatile__ (
        "clc\n\t"
        "int $0x13\n\t"
        "setc %b0\n\t"
        : "=m" (error_flag), "=a" (error_code), "=b" (bx_val), "=c" (cx_val), "=d" (dx_val)
        : "a" (0x08 << 8),
          "d" (drive)
        : "cc", "memory"
    );
    if (error_flag) {
        return (error_code >> 8);
    }
    geo->heads = (dx_val >> 8) + 1;
    geo->sectors_per_track = cx_val & 0x3F;
    unsigned short cyl_high = (cx_val & 0xC0) << 2;
    unsigned short cyl_low = (cx_val >> 8) & 0xFF;
    geo->cylinders = (cyl_high | cyl_low) + 1;
    geo->drive_type = bx_val & 0xFF;
    return 0;
}
/*
unsigned char read_lba(unsigned char drive, unsigned char lba, unsigned char count, void *buffer) {
    unsigned char chs_sector = (lba % BootGeometry.sectors_per_track) + 1;
    unsigned char chs_head = (lba / BootGeometry.sectors_per_track) % BootGeometry.heads;
    unsigned char chs_cylinder = lba / (BootGeometry.sectors_per_track * BootGeometry.heads);
    return read_sector(drive, chs_cylinder, chs_head, chs_sector, count, buffer);
}

#include <stdint.h>

#define PACKED __attribute__((packed))

// Структура записи каталога (32 байта)
typedef struct PACKED {
    char     filename[8];
    char     extension[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high; // Всегда 0 в FAT12
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t first_cluster_low;  // Индекс первого кластера данных
    uint32_t file_size;          // Размер файла в байтах
} FAT12_DirEntry;

typedef struct PACKED {
    uint8_t  boot_jmp[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;    // Обычно 512
    uint8_t  sectors_per_cluster; // Обычно 1
    uint16_t reserved_sectors;    // Сектора перед первой FAT (обычно 1 - загрузочный)
    uint8_t  fat_count;           // Количество таблиц FAT (обычно 2)
    uint16_t dir_entries_count;   // Макс. количество файлов в Root Dir (обычно 224)
    uint16_t total_sectors_short; // Общее число секторов (если < 65535)
    uint8_t  media_descriptor;    // Для 1.44MB флоппи равен 0xF0
    uint16_t sectors_per_fat;     // Размер одной таблицы FAT в секторах (обычно 9)
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;
    
    // Extended Boot Record
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;      // 0x29
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];          // "FAT12   "
} FAT12_BPB;

// Временные буферы в памяти Stage 2
// Разместите их глобально, чтобы сберечь стек Real Mode
static uint8_t fat_buffer[9 * 512];        // 4608 байт для таблицы FAT1
static uint8_t root_dir_buffer[14 * 512];   // 7168 байт для корневого каталога

// Функция побайтового сравнения имени файла (формат 8.3, ровно 11 символов)
static int memcmp11(const char* s1, const char* s2) {
    for (int i = 0; i < 11; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

// Получение следующего кластера из загруженной в память FAT1
static uint16_t fat12_get_next_cluster(uint16_t current_cluster) {
    uint32_t fat_offset = (current_cluster * 3) / 2;
    
    // Безопасное чтение 16-битного слова (учитывая возможное невыровненное смещение)
    uint16_t raw_value = *(uint16_t*)(fat_buffer + fat_offset);

    if (current_cluster % 2 == 0) {
        return raw_value & 0x0FFF;       // Четный: младшие 12 бит
    } else {
        return raw_value >> 4;           // Нечетный: старшие 12 бит
    }
}

// Главная функция загрузки файла для Stage 2
// target_name_83 - строка из 11 символов без точки. Пример: "KRNL    SYS" или "SETUP   BIN"
// dest_buffer    - адрес, куда будет скопирован файл (например, 0x10000 или сегментированный адрес)
int stage2_load_file(uint8_t drive, const char* target_name_83, void* dest_buffer) {
    // 1. Загружаем всю таблицу FAT1 (начинается с LBA 1, длина 9 секторов)
    if (read_lba(drive, 1, 9, fat_buffer) != 0) {
        return -1; // Ошибка чтения таблицы FAT
    }

    // 2. Загружаем корневой каталог (начинается с LBA 19, длина 14 секторов)
    if (read_lba(drive, 19, 14, root_dir_buffer) != 0) {
        return -2; // Ошибка чтения корневого каталога
    }

    // 3. Линейный поиск файла в каталоге
    FAT12_DirEntry* entries = (FAT12_DirEntry*)root_dir_buffer;
    uint16_t current_cluster = 0;
    int file_found = 0;

    for (int i = 0; i < 224; i++) {
        if (entries[i].filename[0] == 0x00) break;      // Конец списка файлов на диске
        if (entries[i].filename[0] == 0xE5) continue;   // Файл помечен как удаленный
        if (entries[i].attributes & 0x18)   continue;   // Пропускаем Volume Label (0x08) и Directory (0x10)

        // Имя в структуре идет подряд (8 байт имя + 3 байта расширение = 11 байт)
        if (memcmp11((const char*)entries[i].filename, target_name_83)) {
            current_cluster = entries[i].first_cluster_low;
            file_found = 1;
            break;
        }
    }

    if (!file_found) {
        return -3; // Файл не найден
    }

    // 4. Последовательное чтение цепочки кластеров файла
    uint8_t* write_ptr = (uint8_t*)dest_buffer;
    const uint32_t lba_data_start = 33; // Стартовый LBA зоны данных (19 + 14)

    while (current_cluster < 0xFF8) {
        if (current_cluster == 0xFF7) {
            return -4; // Обнаружен Bad-кластер
        }

        // Вычисляем LBA для текущего кластера (в FAT12 для флоппи 1 кластер = 1 сектор)
        uint32_t cluster_lba = lba_data_start + (current_cluster - 2);

        // Читаем 1 сектор кластера напрямую в целевой буфер
        if (read_lba(drive, cluster_lba, 1, write_ptr) != 0) {
            return -5; // Ошибка чтения сектора данных
        }

        // Сдвигаем указатель записи на размер сектора (512 байт)
        write_ptr += 512;

        // Запрашиваем из FAT адрес следующего фрагмента файла
        current_cluster = fat12_get_next_cluster(current_cluster);
    }

    return 0; // Файл успешно считан!
}

// Функция форматирования имени FAT (8.3) в красивую строку с точкой
// raw_name  - указатель на 11 байт имени из структуры FAT12_DirEntry
// out_str   - буфер, куда запишется результат (минимум 13 байт: 8 + 1 + 3 + '\0')
void format_fat_name(const char* raw_name, char* out_str) {
    int out_idx = 0;

    // 1. Копируем основное имя (до 8 символов), пропуская trailing пробелы
    for (int i = 0; i < 8; i++) {
        if (raw_name[i] != ' ') {
            out_str[out_idx++] = raw_name[i];
        }
    }

    // 2. Если есть расширение, добавляем точку и само расширение
    if (raw_name[8] != ' ') {
        out_str[out_idx++] = '.';
        for (int i = 8; i < 11; i++) {
            if (raw_name[i] != ' ') {
                out_str[out_idx++] = raw_name[i];
            }
        }
    }

    // 3. Закрываем строку нулем
    out_str[out_idx] = '\0';
}


// Предполагаем, что у вас есть базовая функция вывода строк на экран в Stage 2.
// Если ее нет, обычно она пишется через BIOS Int 0x10 (AH=0x0E)
void print_string(const char* str) {
    com_print(str);
    bios_print(str);
}

char macbuf[24];

void print_uint32(uint32_t num) {
    custom_itoa(num, macbuf, 10);
    com_print(macbuf);
    bios_print(macbuf);
} // Для вывода размера файлов

// Буфер для чтения Root Directory (14 секторов * 512 байт = 7168 байт)
//static uint8_t root_dir_buffer[14 * 512];

int stage2_list_root_directory(uint8_t drive) {
    // 1. Загружаем корневой каталог (LBA 19, длина 14 секторов для 1.44MB дискеты)
    if (read_lba(drive, 19, 14, root_dir_buffer) != 0) {
        return -1; // Ошибка чтения с диска
    }

    FAT12_DirEntry* entries = (FAT12_DirEntry*)root_dir_buffer;
    char pretty_name[13]; // Буфер для отформатированного имени

    print_string("--- ROOT DIRECTORY LIST ---\r\n");

    // В корневом каталоге стандартной дискеты максимум 224 записи
    for (int i = 0; i < 224; i++) {
        uint8_t first_char = (uint8_t)entries[i].filename[0];

        // Проверка специальных маркеров FAT
        if (first_char == 0x00) {
            // 0x00 означает, что эта запись свободная И все последующие записи тоже свободны.
            // Это конец списка файлов на диске.
            break; 
        }
        if (first_char == 0xE5) {
            // 0xE5 означает, что файл был удален. Пропускаем его.
            continue; 
        }

        // Фильтрация по атрибутам файла
        uint8_t attr = entries[i].attributes;

        if (attr == 0x0F) {
            // 0x0F — это маркер Long File Name (LFN) в Windows. 
            // Чистый FAT12 их не использует, пропускаем эти метаданные.
            continue; 
        }
        if (attr & 0x08) {
            // 0x08 — Volume Label (метка тома/имя дискеты). Тоже пропускаем в списке файлов.
            continue; 
        }

        // Форматируем имя файла в читаемый вид
        format_fat_name(entries[i].filename, pretty_name);

        // Выводим имя файла
        print_string(pretty_name);

        // Проверяем, папка это или файл, и выводим размер
        if (attr & 0x10) {
            print_string("    [DIR]\r\n");
        } else {
            print_string("    FILE    Размер: ");
            print_uint32(entries[i].file_size);
            print_string(" байт\r\n");
        }
    }

    print_string("---------------------------\r\n");
    return 0;
}

int stage2_load_file_dynamic(uint8_t drive, const char* target_name_83, void* dest_buffer) {
    // Локальный буфер для структуры BPB (512 байт, чтобы прочесть сектор 0)
    static uint8_t bpb_sector[512];
    
    // 1. Читаем самый первый сектор диска (Boot Sector)
    if (read_lba(drive, 0, 1, bpb_sector) != 0) {
        return -10;
    }
    
    // Накладываем структуру BPB на прочитанный сектор
    FAT12_BPB* bpb = (FAT12_BPB*)bpb_sector;

    // 2. ДИНАМИЧЕСКИЙ РАСЧЕТ АДРЕСОВ (работает на любом FAT12/FAT16)
    // FAT1 начинается сразу после зарезервированных секторов (обычно LBA 1)
    uint32_t fat_lba = bpb->reserved_sectors; 
    uint32_t fat_size = bpb->sectors_per_fat;

    // Корневой каталог идет сразу за всеми таблицами FAT
    uint32_t root_lba = fat_lba + (bpb->fat_count * fat_size);
    
    // Вычисляем, сколько секторов занимает корневой каталог
    uint32_t root_sectors = (bpb->dir_entries_count * 32) / bpb->bytes_per_sector;
    
    // Зона данных начинается сразу за корневым каталогом
    uint32_t data_lba = root_lba + root_sectors;

    // 3. Загружаем FAT1 и Root Directory, используя динамические адреса
    if (read_lba(drive, fat_lba, fat_size, fat_buffer) != 0) return -1;
    if (read_lba(drive, root_lba, root_sectors, root_dir_buffer) != 0) return -2;

    // 4. Поиск файла в корневом каталоге (цикл до bpb->dir_entries_count)
    FAT12_DirEntry* entries = (FAT12_DirEntry*)root_dir_buffer;
    uint16_t current_cluster = 0;
    int file_found = 0;

    for (int i = 0; i < bpb->dir_entries_count; i++) {
        if (entries[i].filename[0] == 0x00) break;
        if (entries[i].filename[0] == 0xE5) continue;
        if (entries[i].attributes & 0x18) continue;

        if (memcmp11((const char*)entries[i].filename, target_name_83)) {
            current_cluster = entries[i].first_cluster_low;
            file_found = 1;
            break;
        }
    }

    if (!file_found) return -3; // Теперь точно должен найти, если имя "KERNEL  BIN"

    // 5. Покластерное чтение цепочки
    uint8_t* write_ptr = (uint8_t*)dest_buffer;
    while (current_cluster < 0xFF8) {
        if (current_cluster == 0xFF7) return -4;

        // Вычисляем LBA с учетом динамического старта зоны данных
        // sectors_per_cluster для флоппи равен 1, но для надежности перемножаем
        uint32_t cluster_lba = data_lba + (current_cluster - 2) * bpb->sectors_per_cluster;

        if (read_lba(drive, cluster_lba, bpb->sectors_per_cluster, write_ptr) != 0) {
            return -5;
        }

        write_ptr += (bpb->sectors_per_cluster * bpb->bytes_per_sector);
        current_cluster = fat12_get_next_cluster(current_cluster); // (Использует fat_buffer)
    }

    return 0; // Успех!
}
*/