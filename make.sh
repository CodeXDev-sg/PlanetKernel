nasm -f bin boot.asm -o boot.bin
nasm -f elf32 kernel_entry.asm -o kernel_entry.o
i686-elf-g++ -fno-leading-underscore -mno-80387 -mno-fp-ret-in-387 -mno-mmx -mno-sse -mno-sse2 -O2 -ffreestanding -fno-pie -nostdlib -m16 -c kernel.c -o kernel.o
i686-elf-ld -T linker.ld -o KERNEL.BIN kernel_entry.o kernel.o --oformat binary
dd if=/dev/zero of=boot.img bs=512 count=2880 conv=notrunc status=none
mkfs.vfat boot.img -F 12 -n "PlanetOS"
dd status=none if=boot.bin of=boot.img conv=notrunc
mcopy -i boot.img KERNEL.BIN ::/
