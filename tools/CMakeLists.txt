find_package(slang)

set(PENUMBRA_BUILD_SHADER_COMPILER ON)

if(slang_FOUND)
	if(${slang_VERSION} VERSION_LESS "2026.2.2")
		message("Minimum required slang version is 2026.2.2"
			"Found ${slang_VERSION}"
			"Shader compiler will not be built"
		)
		set(PENUMBRA_BUILD_SHADER_COMPILER OFF)
	endif()
else()
	message("Could not find slang package" 
		"Shader compiler will not be built")
	set(PENUMBRA_BUILD_SHADER_COMPILER OFF)
endif()


if(PENUMBRA_BUILD_SHADER_COMPILER)
	add_subdirectory("shader_compiler")
endif()
