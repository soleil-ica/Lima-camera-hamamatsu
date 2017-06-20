############################################################################
# This file is part of LImA, a Library for Image Acquisition
#
# Copyright (C) : 2009-2017
# European Synchrotron Radiation Facility
# BP 220, Grenoble 38043
# FRANCE
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################
#CMakeLists.txt for Hamamatsu camera

#HAVE TO BE UNDER WINDOWS
IF(WIN32)
	#SOURCEs
	SET(hamamatsu_srcs src/HamamatsuInterface.cpp src/HamamatsuCamera.cpp
		src/HamamatsuBinCtrlObj.cpp src/HamamatsuDCAMSDKHelper.cpp
		src/HamamatsuDetInfoCtrlObj.cpp src/HamamatsuRoiCtrlObj.cpp
		src/HamamatsuSyncCtrlObj.cpp)
	
	#no VERSION FILE
	#FILE(STRINGS "VERSION" photonic_ver)
	ADD_LIBRARY(limahamamatsu SHARED ${hamamatsu_srcs})
	
	#INCLUDES
	TARGET_INCLUDE_DIRECTORIES(limahamamatsu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
	TARGET_INCLUDE_DIRECTORIES(limahamamatsu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/include")
	TARGET_INCLUDE_DIRECTORIES(limahamamatsu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/include/legacy")

	TARGET_COMPILE_DEFINITIONS(limahamamatsu PRIVATE LIBPHAMAMATSU_EXPORTS)
	
	#LIBRARIES
	FIND_LIBRARY(HAMAMATSU_LIB_DCAMAPI dcamapi "{CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/bin")
	IF("${HAMAMATSU_LIB_DCAMAPI}" STREQUAL "HAMAMATSU_LIB_DCAMAPI-NOTFOUND")
	  MESSAGE(FATAL_ERROR "hamamatsu : dcamapi.lib not found, you can search it in cmake-gui or try to install it if it isn't here.")
	ENDIF()
	FIND_LIBRARY(HAMAMATSU_LIB_DCIMGAPI dcimgapi "{CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/bin")
	IF("${HAMAMATSU_LIB_DCIMGAPI}" STREQUAL "HAMAMATSU_LIB_DCIMGAPI-NOTFOUND")
	  MESSAGE(FATAL_ERROR "hamamatsu : dcimgapi.lib not found, you can search it in cmake-gui or try to install it if it isn't here.")
	ENDIF()
	
	TARGET_LINK_LIBRARIES(limahamamatsu limacore)
	TARGET_LINK_LIBRARIES(limahamamatsu ${HAMAMATSU_LIB_DCAMAPI})
	TARGET_LINK_LIBRARIES(limahamamatsu ${HAMAMATSU_LIB_DCAMAPI})

	#define version here.
	set(MYLIB_VERSION_MAJOR 1)
	set(MYLIB_VERSION_MINOR 0)
	set(MYLIB_VERSION_PATCH 4)
	set(MYLIB_VERSION_STRING ${MYLIB_VERSION_MAJOR}.${MYLIB_VERSION_MINOR}.${MYLIB_VERSION_PATCH})
	SET_TARGET_PROPERTIES(limahamamatsu PROPERTIES VERSION ${MYLIB_VERSION_STRING} SOVERSION ${MYLIB_VERSION_MAJOR})
	MESSAGE("limahamamatsu version defined in CMakeLists.txt. Create a VERSION file or modify lib version directly CMakeLists.txt.")
    SET_TARGET_PROPERTIES(limahamamatsu PROPERTIES PREFIX "lib")
    INSTALL(TARGETS limahamamatsu DESTINATION lib)

	IF(COMPILE_SIP)
		SET(NAME "hamamatsu")
		include(CameraRunSIP)
		INSTALL(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Hamamatsu")
	ENDIF()
ELSE()
	MESSAGE(FATAL_ERROR "This camera has to be compiled on Windows")
ENDIF()
