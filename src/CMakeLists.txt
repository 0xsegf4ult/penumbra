add_executable(vmpenumbra)

target_compile_options(vmpenumbra PRIVATE -target x86_64-none-elf -ffreestanding -fno-rtti -fno-exceptions -mcmodel=kernel -mno-red-zone -nostdlib -nostdlib++ -nodefaultlibs -fPIC)
target_link_options(vmpenumbra PRIVATE -T ${CMAKE_CURRENT_LIST_DIR}/linker.ld -fuse-ld=lld -nostdlib -nostdlib++ -nodefaultlibs)
target_include_directories(vmpenumbra PRIVATE ${CMAKE_SOURCE_DIR}/buildroot/usr/include)
target_sources(vmpenumbra
	PUBLIC FILE_SET CXX_MODULES FILES
 	arch/x86_64/cpu.cppm
 	arch/x86_64/io.cppm
 	arch/x86_64/serial.cppm
	arch/x86_64/mod.cppm	
	types.cppm
	PRIVATE init.cpp)
set_target_properties(vmpenumbra PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/buildroot/boot")

