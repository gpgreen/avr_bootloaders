cmake_minimum_required(VERSION 2.8)

project(avr_bootloaders_test)

set(CMAKE_C_STANDARD 99)

include_directories(
  "/usr/local/include/simavr"
)

add_executable(
  tst_atmega_spi_bootloader
  tst_atmega_spi_bootloader.c
  )

target_link_libraries(
  tst_atmega_spi_bootloader
  PUBLIC
  simavr
  simavrparts
  util
  )
