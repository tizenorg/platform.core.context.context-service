CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(context-service)
INCLUDE(GNUInstallDirs)

# Targets
SET(target "contextd")

# Source Lists
FILE(GLOB_RECURSE SRCS src/*.cpp)
MESSAGE("Sources: ${SRCS}")

# Dependencies
SET(DEPS
	vconf
	libxml-2.0
	sqlite3
	capi-system-info
	capi-appfw-app-manager
	appsvc
	capi-security-privilege-manager
	alarm-service
	notification
	cynara-creds-gdbus
	clips
	context-common
	device-context-provider
	statistics-context-provider
	place-context-provider
)

# Dependencies regarding profiles
IF("${PROFILE}" STREQUAL "mobile")
	ADD_DEFINITIONS("-D_MOBILE")
ENDIF("${PROFILE}" STREQUAL "mobile")

IF("${PROFILE}" STREQUAL "wearable")
	ADD_DEFINITIONS("-D_WEARABLE")
ENDIF("${PROFILE}" STREQUAL "wearable")

# Target vs Emulator
IF("${ARCH}" STREQUAL "arm")
	ADD_DEFINITIONS("-D_TARGET")
ELSE("${ARCH}" STREQUAL "arm")
	ADD_DEFINITIONS("-D_EMULATOR")
ENDIF("${ARCH}" STREQUAL "arm")

# Common Options
INCLUDE(FindPkgConfig)
INCLUDE_DIRECTORIES(
	/usr/include
	/usr/include/glib-2.0
)
ADD_DEFINITIONS(-g -O2 -Wall -fPIC -fvisibility=hidden -Wl,--as-needed)

# Building Daemon
pkg_check_modules(daemon_pkg REQUIRED ${DEPS})

SET(DAEMON_EXTRA_CFLAGS -fPIE)
FOREACH(flag ${daemon_pkg_CFLAGS})
	SET(DAEMON_EXTRA_CFLAGS "${DAEMON_EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

ADD_EXECUTABLE(${target} ${SRCS})
TARGET_LINK_LIBRARIES(${target} ${daemon_pkg_LDFLAGS} -pie)
SET_TARGET_PROPERTIES(${target} PROPERTIES COMPILE_FLAGS ${DAEMON_EXTRA_CFLAGS})
SET_TARGET_PROPERTIES(${target} PROPERTIES COMPILE_DEFINITIONS "LOG_TAG=\"CONTEXT\"")

# Installing Daemon
INSTALL(TARGETS ${target} DESTINATION ${CMAKE_INSTALL_BINDIR})
