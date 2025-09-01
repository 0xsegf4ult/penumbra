add_executable(vmpenumbra)

target_compile_options(vmpenumbra PRIVATE -target x86_64-none-elf -ffreestanding -fno-rtti -fno-exceptions -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -nostdlib -nostdlib++ -nodefaultlibs -fPIC)
target_link_options(vmpenumbra PRIVATE -T ${CMAKE_CURRENT_LIST_DIR}/linker.ld -fuse-ld=lld -nostdlib -nostdlib++ -nodefaultlibs)
target_include_directories(vmpenumbra PRIVATE ${CMAKE_SOURCE_DIR}/buildroot/usr/include)
target_sources(vmpenumbra
	PUBLIC FILE_SET CXX_MODULES FILES
 	arch/x86_64/cpu.cppm
 	arch/x86_64/io.cppm
 	arch/x86_64/interrupts.cppm
	arch/x86_64/paging.cppm
	arch/x86_64/serial.cppm
	arch/x86_64/module.cppm
	mm/memory_map.cppm
	mm/module.cppm
	kfmt.cppm
	kstd.cppm
	klog.cppm
	types.cppm
	PRIVATE 
	arch/x86_64/gdt.S
	arch/x86_64/isr_stubs.S
	kstd_impl.cpp
	init.cpp)
set_target_properties(vmpenumbra PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/boot")

