#####
# A CMake build file for the ImageAlter service, a service that can
# manipulate images.
# 
# Copyright 2010, Yahoo!
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
# 
#  3. Neither the name of Yahoo! nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#####

cmake_minimum_required(VERSION 2.6)
IF (APPLE) 
  SET (CMAKE_OSX_DEPLOYMENT_TARGET "10.4"
       CACHE STRING "Compile for tiger deployment" FORCE)
ENDIF (APPLE)

INCLUDE(BuildConfigs.cmake)

SET (serviceName FileAccess)

PROJECT(${serviceName}Service)

SET (outputDir "${CMAKE_CURRENT_BINARY_DIR}/${serviceName}")

# default to a debug build.  This gets us symbols by default under
# the cmake makefile generator
IF (NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE "Release")
ENDIF ()

SET(ServiceTools ${CMAKE_CURRENT_SOURCE_DIR}/../external/bp-service-tools)

INCLUDE_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}/../external/dist/include"
  "${ServiceTools}/CppTools/external/bpsdk/include"
  "${ServiceTools}/CppTools/src"
)

LINK_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}/../external/dist/lib"
  "${CMAKE_CURRENT_SOURCE_DIR}/../external/dist/lib/${CMAKE_BUILD_TYPE}"
)

SET(EXT_SRCS
  ${ServiceTools}/CppTools/src/bptypeutil.cpp
  ${ServiceTools}/CppTools/src/bpurlutil.cpp
  ${ServiceTools}/CppTools/src/bpservicedescription.cpp
  ${ServiceTools}/CppTools/src/bpserviceversion.cpp
)

SET(SRCS service.cpp littleuuid.cpp FileServer.cpp util/fileutil.cpp)
SET(HDRS service.h littleuuid.h ResourceLimit.h FileServer.h util/bpsync.hh util/fileutil.hh)

# add required OS libs here
SET(OSLIBS)

IF (WIN32)
   # Visual Studio does some autolink magic with boost, no need
   # to specify library
   SET (OS_SRCS util/bpsync_Windows.cpp)
   SET (OS_LIBS Winmm Ws2_32 mswsock rpcrt4 psapi)
ELSE()
   SET(BOOST_LIBS "boost_filesystem" "boost_system")
   SET (OS_SRCS util/bpsync_UNIX.cpp)

   IF (APPLE)
       # need carbon headers and library
       FIND_LIBRARY(CARBON_LIBRARY Carbon)
       MARK_AS_ADVANCED(CARBON_LIBRARY)
       SET(OS_LIBS ${CARBON_LIBRARY})
   ENDIF()
ENDIF ()

ADD_LIBRARY(${serviceName} MODULE ${EXT_SRCS} ${SRCS} ${OS_SRCS} ${HDRS})


TARGET_LINK_LIBRARIES(${serviceName} mongoose_s bpfile_s ${BOOST_LIBS} ${OS_LIBS})

# create service directory
FILE(MAKE_DIRECTORY ${outputDir})

# copy in manifest
GET_TARGET_PROPERTY(loc ${serviceName} LOCATION)
GET_FILENAME_COMPONENT(ServiceLibrary "${loc}" NAME)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
               "${outputDir}/manifest.json")  
ADD_CUSTOM_COMMAND(TARGET ${serviceName} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different
                           \"${loc}\" \"${outputDir}\")

IF (APPLE)
  ADD_CUSTOM_COMMAND(TARGET ${serviceName} POST_BUILD
                     COMMAND strip -x \"${outputDir}/${ServiceLibrary}\")
ENDIF (APPLE)

# Pre-build step, build our externals
#
ADD_CUSTOM_TARGET(
    ${serviceName}Externals ALL
    COMMAND ruby build.rb
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../external"
    COMMENT Building externals...)
ADD_DEPENDENCIES( ${serviceName} ${serviceName}Externals )
