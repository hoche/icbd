cmake_minimum_required(VERSION 3.16)

project(icbd C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- User-configurable options ----
option(ICBD_ENABLE_SSL "Enable TLS listener support via OpenSSL" OFF)
option(ICBD_BRICK "Enable brick support" OFF)

set(ADMIN_PWD "" CACHE STRING "Admin password (compiled in). Required.")
if(ADMIN_PWD STREQUAL "")
  message(FATAL_ERROR "ADMIN_PWD is required. Configure with -DADMIN_PWD=... (note: this gets compiled into the binary).")
endif()

set(MAX_USERS "255" CACHE STRING "Maximum number of users. Must be <= FD_SETSIZE-1 on platforms that care.")

# ---- Feature checks for config.h ----
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckLibraryExists)

check_include_file("fcntl.h"       HAVE_FCNTL_H)
check_include_file("unistd.h"      HAVE_UNISTD_H)
check_include_file("stdarg.h"      HAVE_STDARG_H)
check_include_file("time.h"        HAVE_TIME_H)
check_include_file("sys/time.h"    HAVE_SYS_TIME_H)
check_include_file("sys/select.h"  HAVE_SYS_SELECT_H)

check_function_exists(gethostname  HAVE_GETHOSTNAME)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(select       HAVE_SELECT)
check_function_exists(socket       HAVE_SOCKET)
check_function_exists(strdup       HAVE_STRDUP)
check_function_exists(strerror     HAVE_STRERROR)
check_function_exists(snprintf     HAVE_SNPRINTF)

check_c_source_compiles("
#include <sys/types.h>
#include <sys/socket.h>
int main(void) { socklen_t x = 0; (void)x; return 0; }
" HAVE_SOCKLEN_T)

# resolv / _res (used in pktserv/getrname.c behind HAVE_LIBRESOLV)
check_library_exists(resolv _res "" HAVE_LIBRESOLV)

# gdbm (required now that autoconf is gone)
check_include_file("gdbm.h" HAVE_GDBM_H)
if(NOT HAVE_GDBM_H)
  message(FATAL_ERROR "Missing gdbm.h. Install libgdbm-dev.")
endif()
check_library_exists(gdbm gdbm_open "" HAVE_LIBGDBM)
if(NOT HAVE_LIBGDBM)
  message(FATAL_ERROR "Missing libgdbm. Install libgdbm-dev.")
endif()
find_library(GDBM_LIB gdbm REQUIRED)

# OpenSSL (optional)
if(ICBD_ENABLE_SSL)
  find_package(OpenSSL)
  if(OpenSSL_FOUND)
    set(HAVE_SSL 1)
    check_symbol_exists(TLS_server_method "openssl/ssl.h" HAVE_TLS_SERVER_METHOD)
  else()
    message(FATAL_ERROR "ICBD_ENABLE_SSL=ON but OpenSSL was not found.")
  endif()
endif()

if(ICBD_BRICK)
  set(BRICK 1)
endif()

# ---- Generate config.h ----
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/config.h.in"
  "${GENERATED_DIR}/config.h"
  @ONLY
)

# ---- Build outputs (keep familiar layout inside build dir) ----
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# ---- pktserv (static lib) ----
add_library(pktserv STATIC
  pktserv/pktbuffers.c
  pktserv/pktserv.c
  pktserv/pktsocket.c
  pktserv/sslconf.c
  pktserv/sslsocket.c
  pktserv/getrname.c
)
target_compile_definitions(pktserv PRIVATE PKTSERV_INTERNAL)
target_include_directories(pktserv PRIVATE
  "${GENERATED_DIR}"
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/pktserv"
  "${CMAKE_SOURCE_DIR}/server"
)
target_compile_options(pktserv PRIVATE -Wall)

if(HAVE_LIBRESOLV)
  target_link_libraries(pktserv PRIVATE resolv)
endif()

if(HAVE_SSL)
  target_link_libraries(pktserv PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# ---- icbd server executable ----
add_executable(icbd
  server/access.c
  server/dispatch.c
  server/globals.c
  server/groups.c
  server/icbdb.c
  server/icbutil.c
  server/ipcf.c
  server/dbm_gdbm.c
  server/lookup.c
  server/main.c
  server/mdb.c
  server/msgs.c
  server/namelist.c
  server/s_admin.c
  server/s_auto.c
  server/s_beep.c
  server/s_group.c
  server/s_info.c
  server/s_motd.c
  server/s_news.c
  server/s_person.c
  server/s_shuttime.c
  server/s_stats.c
  server/s_user.c
  server/s_version.c
  server/s_who.c
  server/send.c
  server/strlist.c
  server/strutil.c
  server/unix.c
  server/users.c
  server/utf8.c
  server/wildmat.c
)
target_include_directories(icbd PRIVATE
  "${GENERATED_DIR}"
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/server"
  "${CMAKE_SOURCE_DIR}/pktserv"
)
target_compile_options(icbd PRIVATE -Wall)

target_link_libraries(icbd PRIVATE pktserv "${GDBM_LIB}")
if(HAVE_LIBRESOLV)
  target_link_libraries(icbd PRIVATE resolv)
endif()

set_target_properties(icbd PROPERTIES
  OUTPUT_NAME "icbd"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/server"
)

