# Toolchain file for cmake to crosscompile DDS Router for Raspberrypi (rpi)
# sreejith.naarakathil@gmail.com

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)
set(CMAKE_CROSSCOMPILING 1)

# Same location as the install prefix passed to cmake command,
# Eg: cmake -DCMAKE_INSTALL_PREFIX=/home/sreejithn/projects/DDS-Router/install
# Useful when dependency libraries needs to be built and installed independently using cmake.
# This is the case when cmake is complaining about a target library path 
# that it can't resolve to an absolute path in the sysroot (created in host machine using rsync) folder.
set(CMAKE_FIND_ROOT_PATH /home/sreejithn/projects/DDS-Router/install)
set(CMAKE_IMPORT_LIBRARY_PREFIX /home/sreejithn/projects/rpi/rootfs)
set(IMPORT_PREFIX /home/sreejithn/projects/rpi/rootfs)

set(TOOLCHAIN_INST_PATH /usr/bin)
set(CMAKE_C_COMPILER ${TOOLCHAIN_INST_PATH}/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_INST_PATH}/arm-linux-gnueabihf-g++)

# The location of the sysroot folder created using a rsync of a real rpi rootfs
# Useful when a dependency library do have support for cmake build. Eg: OpenSSL
# In this case, install openssl on the rpi using apt and then rsync the sysroot folder in the host machine
set(CMAKE_SYSROOT /home/sreejithn/projects/rpi/rootfs)

# https://github.com/eProsima/Fast-DDS/issues/1262
set(CMAKE_CXX_FLAGS "-latomic -lpthread")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# This assumes that pthread will be available on the target system
# (this emulates that the return of the TRY_RUN is a return code "0"
set(THREADS_PTHREAD_ARG "0" CACHE STRING "Result from TRY_RUN" FORCE)
