cmake_minimum_required(VERSION 3.13)


#Set pico-sdk source directory. This is probably different for you!
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk")

include (${PICO_SDK_PATH}/pico_sdk_init.cmake)

pico_sdk_init()


project(numlocked)





add_executable(numlocked
	main.c
	usb_descriptors.c
	ssd1306.c
)

#Include directories
target_include_directories(numlocked PUBLIC
	${CMAKE_CURRENT_LIST_DIR}
)

target_link_options(numlocked PRIVATE 
    -Wl,-Map=${CMAKE_BINARY_DIR}/firmware.map
    -Wl,--print-memory-usage
)


target_compile_options(numlocked PRIVATE
	-Wall
	-Wextra
	#	-Wpedantic
	-Wshadow
	-Wunused
	-Wuninitialized
	-fstack-usage
	-fstack-check
	-Wstack-usage=512
)



# Link libraries
target_link_libraries(numlocked
	pico_stdlib
	pico_unique_id
	pico_multicore
	pico_time
	tinyusb_device
	tinyusb_board
	hardware_i2c
)

#Enable USB
pico_enable_stdio_usb(numlocked 1)

# Generate .uf2
pico_add_extra_outputs(numlocked)



find_program(ARM_SIZE arm-none-eabi-size)
find_program(ARM_OBJDUMP arm-none-eabi-objdump)


# Only add memory analysis if tools are available
if(ARM_SIZE)
    add_custom_command(TARGET numlocked POST_BUILD
        COMMAND ${ARM_SIZE} $<TARGET_FILE:numlocked>
        COMMENT "Memory usage:"
    )
    
    add_custom_target(memory_report
        COMMAND ${ARM_SIZE} -A -d $<TARGET_FILE:numlocked>
        DEPENDS numlocked
    )
endif()

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(static_analysis
        COMMAND ${CPPCHECK} --enable=all --inconclusive 
                --std=c11 --platform=unix32
                ${CMAKE_SOURCE_DIR}/src
    )
endif()



add_custom_target(stack_report
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.su" -exec cat {} +
    DEPENDS numlocked
    COMMENT "Stack usage per function:"
)


