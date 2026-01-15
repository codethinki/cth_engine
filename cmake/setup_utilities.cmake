function(set_compiler_specifics)
message("Using compiler ${CMAKE_CXX_COMPILER}")

if(MSVC)
	add_compile_options(/utf-8)
else()
	add_compile_options(-fexceptions)
endif()
endfunction()



macro(set_cxx_standard)
if(MSVC)
	set(CMAKE_CXX_STANDARD 23)
else()
	set(CMAKE_CXX_STANDARD 26)
endif()

set(CMAKE_C_STANDARD ${CMAKE_CXX_STANDARD})

set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Set cxx standard to ${CMAKE_CXX_STANDARD}")
endmacro()