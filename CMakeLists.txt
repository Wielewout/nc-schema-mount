cmake_minimum_required(VERSION 3.12)
project(nc-schema-mount C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ENV{PKG_CONFIG_ALL_STATIC} "1")
set(ENV{PKG_CONFIG_STATIC} "1")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

find_package(PkgConfig REQUIRED)

set(SYSREPO_MIN_VERSION 3.7.11)
set(LIBYANG_MIN_VERSION 3.13.5)

pkg_check_modules(SYSREPO REQUIRED sysrepo>=${SYSREPO_MIN_VERSION})
pkg_check_modules(LIBYANG REQUIRED libyang>=${LIBYANG_MIN_VERSION})
pkg_check_modules(PCRE2 REQUIRED libpcre2-8)

add_executable(nc-schema-mount
  src/main.c
)

target_include_directories(nc-schema-mount PRIVATE
  ${SYSREPO_INCLUDE_DIRS}
  ${LIBYANG_INCLUDE_DIRS}
)

target_link_libraries(nc-schema-mount
  ${SYSREPO_LIBRARIES}
  ${LIBYANG_LIBRARIES}
  ${PCRE2_LIBRARIES}
)

target_compile_options(nc-schema-mount PRIVATE
  ${SYSREPO_CFLAGS_OTHER}
  ${LIBYANG_CFLAGS_OTHER}
  ${PCRE2_CFLAGS_OTHER}
)

install(TARGETS nc-schema-mount RUNTIME DESTINATION bin)
