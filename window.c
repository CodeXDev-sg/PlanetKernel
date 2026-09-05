typedef struct {
    uint8_t visible;
    uint8_t type;
    int x, y, w, h;
    const char *title;
    void *drawPixelAt;
    void *custom_data;
} Window;

/*
    What is Window.type?
        It is a type of renderer:

        0x00 Renderer not initialized
        0x01 Pixel Renderer
        0x02 Full Renderer
        0x03 Iteration Renderer
*/

// Структура для хранения сегментного (Far) указателя в Real-Mode
typedef struct {
    uint16_t offset;
    uint16_t segment;
} __attribute__((packed)) FarPointer;

/**
 * Получить указатель на растровые данные символа из BIOS
 * @param ascii_char - ASCII код символа (например, 'A')
 * @param font_height - Высота шрифта: 8 (для шрифта 8x8) или 16 (для 8x16)
 */
// Функция возвращает БАЗОВЫЙ адрес начала всей таблицы шрифтов из BIOS
__attribute__((noinline))
FarPointer get_bios_font_base(uint8_t font_height) {
    uint16_t out_seg = 0;
    uint16_t out_off = 0;
    
    // Подготавливаем параметр BH для прерывания
    uint16_t bh_param = (font_height == 8) ? 0x03 : 0x06;

    __asm__ __volatile__ (
        "push %%bp\n\t"
        "push %%es\n\t"
        "push %%bx\n\t"

        // Загружаем параметр в BX и сдвигаем в BH, как требует BIOS
        "mov %w2, %%bx\n\t"
        "mov %%bl, %%bh\n\t"
        "mov $0x1130, %%ax\n\t"  // AH = 11h, AL = 30h
        "int $0x10\n\t"          // Вызов BIOS. ES:BP = Начало таблицы (символ 0)

        "mov %%bp, %%dx\n\t"     // Сохраняем базовое смещение в DX
        "mov %%es, %%cx\n\t"     // Сохраняем сегмент в CX

        "pop %%bx\n\t"
        "pop %%es\n\t"
        "pop %%bp\n\t"

        "mov %%cx, %w0\n\t"       // out_seg = CX
        "mov %%dx, %w1\n\t"       // out_off = DX
        
        : "=m"(out_seg), "=m"(out_off)
        : "m"(bh_param)
        : "ax", "cx", "dx", "cc"
    );

    FarPointer ptr = { .offset = out_off, .segment = out_seg };
    return ptr;
}

// Безопасное чтение байта
__attribute__((noinline))
uint8_t far_peekb(uint16_t segment, uint16_t offset) {
    uint8_t val;
    __asm__ __volatile__ (
        "push %%fs\n\t"
        "push %%si\n\t"
        
        "mov %1, %%fs\n\t"
        "mov %2, %%si\n\t"
        "mov %%fs:(%%si), %0\n\t"
        
        "pop %%si\n\t"
        "pop %%fs\n\t"
        : "=q"(val)
        : "m"(segment), "m"(offset)
        : "cc"
    );
    return val;
}

void draw_char(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t font_height = 16; // Или 8, если выбрали шрифт 8x8
    
    // 1. Получаем базу всей таблицы (символ с кодом 0)
    FarPointer font_base = get_bios_font_base(font_height);

    // 2. Считаем смещение конкретного символа
    // Используем 32-битное число для предотвращения 16-битного переполнения
    uint32_t total_offset = (uint32_t)font_base.offset + ((uint32_t)(uint8_t)c * font_height);
    
    // В реальном режиме смещение не может быть больше 64КБ (0xFFFF).
    // Если при расчете мы вышли за границы сегмента, нужно скорректировать сегмент!
    uint16_t segment = font_base.segment + (total_offset >> 4);
    uint16_t offset = total_offset & 0x0F;

    // 3. Рендерим по строкам
    for (int row = 0; row < font_height; row++) {
        // Читаем байт текущей строки из BIOS
        uint8_t data = far_peekb(segment, offset + row);

        // Перебираем 8 бит слева направо
        for (int col = 0; col < 8; col++) {
            // Проверяем бит с помощью маски
            if (data & (0x80 >> col)) {
                uint8_t tr, tg, tb;
                getpixel(x + col, y + row, &tr, &tg, &tb);
                tr = (tr << 1) + (r << 1);
                tg = (tg << 1) + (g << 1);
                tb = (tb << 1) + (b << 1);
                putpixel_asm(x + col, y + row, tr, tg, tb);
            }
        }
    }
}


void testWindowRenderer(int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, void *custom_data) {
    unsigned short colr = sinmat[x];
    unsigned short colg = sinmat[(x+80) % 320];
    unsigned short colb = sinmat[(x+160) % 320];
    colr += sinmat[y];
    colg += sinmat[(y+80) % 320];
    colb += sinmat[(y+160) % 320];
    colr = colr>>1;
    colg = colg>>1;
    colb = colb>>1;
    *r = colr;
    *g = colg;
    *b = colb;
}

void metaRender(int x, int y, unsigned char *r, unsigned char *g, unsigned char *b, void *vcustom_data) {
    unsigned long *custom_data = (long unsigned int*)vcustom_data;
    unsigned long t = custom_data[0];
    int vx = x - t;
    int vy = y - t;
    unsigned short col = (vx^vy)%256;
    vx = x - t;
    vy = y - t;
    col += (vx^vy)%256;
    vx = x + t;
    vy = y + t;
    col += (vx^vy)%256;
    vx = x - t;
    vy = y + t;
    col += (vx^vy)%256;
    vx = x + t;
    vy = y - t;
    col = col >> 2;
    *r = col;
    *g = col;
    *b = col;
    if(x == 99 && y == 99) {
        custom_data[0]++;
    }
}