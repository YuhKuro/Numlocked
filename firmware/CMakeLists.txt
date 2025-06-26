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

# Link libraries
target_link_libraries(numlocked
	pico_stdlib
	pico_unique_id
	pico_multicore
	tinyusb_device
	tinyusb_board
	hardware_i2c
)

#Enable USB
pico_enable_stdio_usb(numlocked 1)

# Generate .uf2
pico_add_extra_outputs(numlocked)



