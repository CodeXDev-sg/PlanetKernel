[bits 16]
[extern kernel_main]
global _start

_start:
    ; Отключаем прерывания на время настройки стека (Важно!)
    cli
    
    ; Настраиваем сегменты данных и кода
    mov ax, cs
    mov ds, ax
    mov es, ax
    
    ; НАСТРОЙКА БЕЗОПАСНОГО СТЕКА
    mov ss, ax          ; SS теперь равен 0x0020 (стек в том же сегменте, что и ядро)
    mov sp, 0xFFFF      ; Указатель стека на самый конец сегмента (64 КБ памяти)

    ; Разрешаем прерывания обратно
    sti

    ; Вызываем ядро
    call kernel_main
    
    ; Бесконечный цикл на случай возврата
    jmp $
    ;mov ax, cs
    ;mov ds, ax
    ;mov es, ax
    ;call kernel_main
    ;jmp $