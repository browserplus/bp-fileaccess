#####
# A CMake build file for the FileAccess service, a service that can
# serve user selected files over localhost HTTP to a Browser environment
# 
# Copyright 2009, Yahoo!
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
# 
# Original Author: Lloyd Hilaiel
#####

SET (serviceName FileAccess)

PROJECT(${serviceName}Service)

SET (outputDir "${CMAKE_CURRENT_BINARY_DIR}/${serviceName}")

# default to a debug build.  This gets us symbols by default under
# the cmake makefile generator
IF (NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE "Debug")
ENDIF ()

INCLUDE_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}/../external/mongoose/mongoose_built/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../external/bpsdk/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../external/bp-service-tools/DataMapping/src"
)

LINK_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}/../external/mongoose/mongoose_built/lib"
)

SET(EXT_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../external/bp-service-tools/DataMapping/src/bptypeutil.cpp
)

SET(SRCS service.cpp FileServer.cpp )
SET(HDRS FileServer.h )

IF (WIN32)
  SET(OSLIBS)
ELSE ()
  SET(OSLIBS pthread dl)
ENDIF ()

ADD_LIBRARY(${serviceName} MODULE ${EXT_SRCS} ${SRCS} ${HDRS})

TARGET_LINK_LIBRARIES(${serviceName} mongoose ${OSLIBS})

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
