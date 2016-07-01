CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(context-service)
INCLUDE(GNUInstallDirs)

# Targets
SET(target "contextd")

# Source Lists
FILE(GLOB SRCS src/*.cpp src/access_control/*.cpp src/policy/*.cpp)
IF(${TRIGGER_SUPPORT})
	FILE(GLOB SRCS ${SRCS} src/trigger/*.cpp)
	ADD_DEFINITIONS(-DTRIGGER_SUPPORT)
ENDIF(${TRIGGER_SUPPORT})
MESSAGE("Sources: ${SRCS}")

# Dependencies
SET(DEPS
	gmodule-2.0
	vconf
	capi-system-device
	capi-appfw-app-manager
	capi-appfw-package-manager
	notification
	context-common
	context
)

IF("${PROFILE}" STREQUAL "mobile")
	ADD_DEFINITIONS("-D_MOBILE_")
ENDIF("${PROFILE}" STREQUAL "mobile")

IF("${PROFILE}" STREQUAL "wearable")
	ADD_DEFINITIONS("-D_WEARABLE_")
ENDIF("${PROFILE}" STREQUAL "wearable")

IF(${LEGACY_SECURITY})
	SET(DEPS ${DEPS} security-server)
	ADD_DEFINITIONS(-DLEGACY_SECURITY)
ELSE(${LEGACY_SECURITY})
	SET(DEPS ${DEPS} cynara-creds-gdbus cynara-client cynara-session)
ENDIF(${LEGACY_SECURITY})

IF(${SYSTEM_SERVICE})
	ADD_DEFINITIONS(-DSYSTEM_SERVICE)
ENDIF(${SYSTEM_SERVICE})

# Common Options
INCLUDE(FindPkgConfig)
INCLUDE_DIRECTORIES(
	/usr/include
	/usr/include/glib-2.0
)
ADD_DEFINITIONS(-O2 -Wall -fPIC -fPIE -fdata-sections -ffunction-sections -fvisibility=hidden)
ADD_DEFINITIONS(-DLOG_TAG="CONTEXT")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIC -pie -Wl,--as-needed -Wl,--gc-section -Wl,--print-gc-section")

# Building Daemon
pkg_check_modules(daemon_pkg REQUIRED ${DEPS})

FOREACH(flag ${daemon_pkg_CFLAGS})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
ENDFOREACH(flag)

ADD_EXECUTABLE(${target} ${SRCS})
TARGET_LINK_LIBRARIES(${target} ${daemon_pkg_LDFLAGS})

# Installing Daemon
INSTALL(TARGETS ${target} DESTINATION ${CMAKE_INSTALL_BINDIR})
