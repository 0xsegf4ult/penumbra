#!/bin/bash

if [ -z "$1" ] 
then qemu-system-x86_64 -no-shutdown -no-reboot -machine type=q35 -serial stdio -bios /usr/share/ovmf/x64/OVMF.4m.fd -usb boot.img
fi

if [ "$1" = "debug" ] 
then qemu-system-x86_64 -d int -no-shutdown -no-reboot -machine type=q35 -bios /usr/share/ovmf/x64/OVMF.4m.fd -usb boot.img -gdb tcp::26000 -S -nographic
fi
