# Ladybird Instruction Level Simulator

## Build

`$ make`

## Simple Run

`$ ./launch_sim [ELF Executable]`

## Run with Disk Image for virtio_disk

`$ ./launch_sim [ELF Executable] [Disk Image]`

## Run [xv6 (RV32IMA ported)](https://github.com/harihitode/ladybird_xv6)

`$ make xv6`

You can get xv6 kernel and disk image from

* Kernel: [Google Drive](https://drive.google.com/file/d/1puGVLrPvhocKS7GaNAFHNTrkyb2ep5cg/view?usp=sharing)
* Disk Image: [Google Drive](https://drive.google.com/file/d/16-acP4p0-iX1lPEzncfZYNl0aEYHX6qk/view?usp=sharing)

## RUN Linux (see [Embedded Linux from Scratch](http://mcu.cz/images_articles/4980-opdenacker-embedded-linux-45minutes-riscv.pdf) to build kernel)

`$ make linux`

You can get Linux kernel and disk image containing Busybox from

* Kernel: [Google Drive](https://drive.google.com/file/d/1oOPNRAD00Be6UgMubbiiEAo7N7YIpDIT/view?usp=sharing)
* Disk Image: [Google Drive](https://drive.google.com/file/d/19EXahB4r7oPIqCfxiMAORns9LJltDOyp/view?usp=sharing)
