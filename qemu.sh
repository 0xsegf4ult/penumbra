#!/bin/bash

qemu-system-x86_64 -no-shutdown -no-reboot -machine type=q35 -serial stdio -bios /usr/share/ovmf/x64/OVMF.4m.fd -usb boot.img
