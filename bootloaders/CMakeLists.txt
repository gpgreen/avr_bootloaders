##################################################################################
# "THE ANY BEVERAGE-WARE LICENSE" (Revision 42 - based on beer-ware
# license):
# <dev@layer128.net> wrote this file. As long as you retain this notice
# you can do whatever you want with this stuff. If we meet some day, and
# you think this stuff is worth it, you can buy me a be(ve)er(age) in
# return. (I don't like beer much.)
#
# Matthias Kleemann
##################################################################################

cmake_minimum_required(VERSION 2.8)

option(PYPILOT_CONTROLLER_BOOTLOADER "build for pypilot-controller board" OFF)

option(POWER_MONITOR_BOOTLOADER "build for power monitor hat" OFF)

option(NANO_PROTO_BOOTLOADER "build for nano breadboard prototype" OFF)

### TOOLCHAIN SETUP AREA #################################################
# Set any variables used in the toolchain prior project() call. In that
# case they are already set and used.
##########################################################################

##################################################################################
# tools to be used for programming the AVR
##################################################################################
set(AVR_UPLOADTOOL avrdude)
set(AVR_PROGRAMMER usbtiny)
#set(AVR_UPLOADTOOL_PORT /dev/ttyACM0)
set(AVR_UPLOADTOOL_OPTIONS "-C/home/ggreen/apps/arduino-1.8.15/hardware/tools/avr/etc/avrdude.conf")
# AVR and fuses needs to be set
set(AVR_MCU atmega328p)
set(AVRDUDE_MCU m328p)
set(AVR_USE_E_FUSE on)
set(AVR_E_FUSE 0xfd)
set(AVR_H_FUSE 0xd8)
set(AVR_L_FUSE 0xe2)
set(BOOTSTART 0x3800)

if(POWER_MONITOR_BOOTLOADER)
  set(AVR_E_FUSE 0xfd)
  set(AVR_H_FUSE 0xd8)
  set(AVR_L_FUSE 0xe2)
  set(MCU_SPEED "8000000UL")
endif()

if(NANO_PROTO_BOOTLOADER)
  set(AVR_E_FUSE 0xff)
  set(AVR_H_FUSE 0xd8)
  set(AVR_L_FUSE 0xc7)
  set(MCU_SPEED "16000000UL")
endif()

### END TOOLCHAIN SETUP AREA #############################################

##########################################################################
# name your project
##########################################################################
project(avr_bootloaders)

##################################################################################
# status messages
##################################################################################
message(STATUS "Current uploadtool is: ${AVR_UPLOADTOOL}")
message(STATUS "Current programmer is: ${AVR_PROGRAMMER}")
message(STATUS "Current upload port is: ${AVR_UPLOADTOOL_PORT}")
message(STATUS "Current uploadtool options are: ${AVR_UPLOADTOOL_OPTIONS}")
message(STATUS "Current MCU is set to: ${AVR_MCU}")
message(STATUS "Current MCU_SPEED is set to: ${MCU_SPEED}")
message(STATUS "Current Programmer MCU is set to: ${AVRDUDE_MCU}")
message(STATUS "Current use E fuse is set to: ${AVR_USE_E_FUSE}")
message(STATUS "Current E_FUSE is set to: ${AVR_E_FUSE}")
message(STATUS "Current H_FUSE is set to: ${AVR_H_FUSE}")
message(STATUS "Current L_FUSE is set to: ${AVR_L_FUSE}")
message(STATUS "Current boot start address is set to: ${BOOTSTART}")

##################################################################################
# set build type, if not already set at cmake command line
##################################################################################
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

##################################################################################
# some cmake cross-compile necessities
##################################################################################
if(DEFINED ENV{AVR_FIND_ROOT_PATH})
    set(CMAKE_FIND_ROOT_PATH $ENV{AVR_FIND_ROOT_PATH})
else(DEFINED ENV{AVR_FIND_ROOT_PATH})
    if(EXISTS "/opt/local/avr")
      set(CMAKE_FIND_ROOT_PATH "/opt/local/avr")
    elseif(EXISTS "/usr/avr")
      set(CMAKE_FIND_ROOT_PATH "/usr/avr")
    elseif(EXISTS "/usr/lib/avr")
      set(CMAKE_FIND_ROOT_PATH "/usr/lib/avr")
    elseif(EXISTS "/usr/local/CrossPack-AVR")
      set(CMAKE_FIND_ROOT_PATH "/usr/local/CrossPack-AVR")
    else(EXISTS "/opt/local/avr")
      message(FATAL_ERROR "Please set AVR_FIND_ROOT_PATH in your environment.")
    endif(EXISTS "/opt/local/avr")
endif(DEFINED ENV{AVR_FIND_ROOT_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# not added automatically, since CMAKE_SYSTEM_NAME is "generic"
set(CMAKE_SYSTEM_INCLUDE_PATH "${CMAKE_FIND_ROOT_PATH}/include")
set(CMAKE_SYSTEM_LIBRARY_PATH "${CMAKE_FIND_ROOT_PATH}/lib")

##################################################################################
# status messages for generating
##################################################################################
message(STATUS "Set CMAKE_FIND_ROOT_PATH to ${CMAKE_FIND_ROOT_PATH}")
message(STATUS "Set CMAKE_SYSTEM_INCLUDE_PATH to ${CMAKE_SYSTEM_INCLUDE_PATH}")
message(STATUS "Set CMAKE_SYSTEM_LIBRARY_PATH to ${CMAKE_SYSTEM_LIBRARY_PATH}")

##################################################################################
# set compiler options for build types
##################################################################################
if(CMAKE_BUILD_TYPE MATCHES Release)
   set(CMAKE_C_FLAGS_RELEASE "-Os")
   set(CMAKE_CXX_FLAGS_RELEASE "-Os")
endif(CMAKE_BUILD_TYPE MATCHES Release)

if(CMAKE_BUILD_TYPE MATCHES Debug)
   set(CMAKE_C_FLAGS_DEBUG "-Os -save-temps -g -gdwarf-3 -gstrict-dwarf")
   set(CMAKE_CXX_FLAGS_DEBUG "-Os -save-temps -g -gdwarf-3 -gstrict-dwarf")
endif(CMAKE_BUILD_TYPE MATCHES Debug)

##################################################################################
# compiler options for all build types
##################################################################################
add_definitions("-DF_CPU=${MCU_SPEED}")
add_definitions("-fpack-struct")
add_definitions("-fshort-enums")
add_definitions("-Wall")
# http://gcc.gnu.org/onlinedocs/gcc-4.8.2/gcc/Alternate-Keywords.html#Alternate-Keywords
# [...]-pedantic and other options cause warnings for many GNU C extensions. You can prevent such warnings within
# one expression by writing __extension__ before the expression. __extension__ has no effect aside from this.[...] 
add_definitions("-pedantic")
add_definitions("-pedantic-errors")
add_definitions("-funsigned-char")
add_definitions("-funsigned-bitfields")
add_definitions("-ffunction-sections")
add_definitions("-c")
add_definitions("-std=gnu99")
add_definitions("-DBAUD_RATE=57600")

##################################################################################
# option builds
##################################################################################

if(PYPILOT_CONTROLLER_BOOTLOADER)
  add_link_options("-Wl,--section-start=.text=0x3800")
endif()

if(POWER_MONITOR_BOOTLOADER)
  add_definitions("-DPOWER_MONITOR_FW=1")
  add_definitions("-DNUM_LED_FLASHES=1")
  add_definitions("-DMAX_TIME_COUNT=F_CPU>>4")
  add_link_options("-Wl,--section-start=.text=${BOOTSTART}")
endif()

if(NANO_PROTO_BOOTLOADER)
  add_definitions("-DNANO_PROTO_FW=1")
  add_definitions("-DNUM_LED_FLASHES=1")
  add_definitions("-DMAX_TIME_COUNT=F_CPU>>5")
  add_link_options("-Wl,--section-start=.text=${BOOTSTART}")
endif()

##########################################################################
# include search paths
##########################################################################
include_directories(
	"${CMAKE_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/../libs/avr_drivers/canlibrary"
	"${CMAKE_SOURCE_DIR}/../libs/avr_drivers/drivers"
	"${CMAKE_SOURCE_DIR}/../libs/avr_drivers/i2cmaster"
	"${CMAKE_SOURCE_DIR}/../libs/avr_drivers/utils"
#	"${CMAKE_SOURCE_DIR}/../libs/libcanard"
#	"${CMAKE_SOURCE_DIR}/../libs/libcanard/drivers/avr"
#	"${CMAKE_SOURCE_DIR}/../libs/libcanard/drivers/avr/avr-can-lib"
        "/usr/local/include/simavr"
)

##################################################################################
# add AVR executable
##################################################################################

if(PYPILOT_CONTROLLER_BOOTLOADER)
  add_avr_executable(
    pypilot-controller-bootloader
    atmega_bootloader.c
    )
endif()

if(POWER_MONITOR_BOOTLOADER)
  add_avr_executable(
    power-monitor-bootloader
    atmega_spi_bootloader.c
    )
  set_target_properties(
    power-monitor-bootloader-atmega328p.elf
    PROPERTIES
      LINK_FLAGS "-mmcu=${AVR_MCU} -Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000 -Wl,-Map,power-monitor-bootloader-atmega328p.map"
    )
endif()

if(NANO_PROTO_BOOTLOADER)
  add_avr_executable(
    nano-bootloader
    atmega_spi_bootloader.c
    )
  set_target_properties(
    nano-bootloader-atmega328p.elf
    PROPERTIES
      LINK_FLAGS "-mmcu=${AVR_MCU} -Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000 -Wl,-Map,nano-bootloader-atmega328p.map"
    )
endif()


##################################################################################
# link library to executable
# NOTE: It needs to be the elf target.
##################################################################################

set(CAN_LIB "${CMAKE_AVRLIBS_PATH}/canlibrary/libavrcanlib-${AVR_MCU}.a")
message(STATUS "avrcanlib: ${CAN_LIB}")
set(DRIVER_LIB "${CMAKE_AVRLIBS_PATH}/drivers/libavrdrivers-${AVR_MCU}.a")
message(STATUS "avrdriverslib: ${DRIVER_LIB}")
set(I2CMASTER_LIB "${CMAKE_AVRLIBS_PATH}/i2cmaster/libavri2cmaster-${AVR_MCU}.a")
message(STATUS "avri2cmaster: ${I2CMASTER_LIB}")
set(UTILS_LIB "${CMAKE_AVRLIBS_PATH}/utils/libavrutils-${AVR_MCU}.a")
message(STATUS "avrutilslib: ${UTILS_LIB}")

#avr_target_link_libraries(
#	power-monitor
#	${CAN_LIB}
#	${DRIVER_LIB}
#	${I2CMASTER_LIB}
#	${UTILS_LIB}
#)

