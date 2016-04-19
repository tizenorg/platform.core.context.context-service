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
	capi-system-info
	capi-system-device
	capi-appfw-app-manager
	capi-appfw-package-manager
	appsvc
	notification
	cynara-creds-gdbus
	cynara-client
	cynara-session
	context-provider
)

# Common Options
INCLUDE(FindPkgConfig)
INCLUDE_DIRECTORIES(
	/usr/include
	/usr/include/glib-2.0
)
ADD_DEFINITIONS(-O2 -Wall -fPIC -fdata-sections -ffunction-sections -fvisibility=hidden)
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIC -Wl,--as-needed -Wl,--gc-section -Wl,--print-gc-section")

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
