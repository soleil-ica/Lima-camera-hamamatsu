###########################################################################
# This file is part of LImA, a Library for Image Acquisition
#
#  Copyright (C) : 2009-2017
#  European Synchrotron Radiation Facility
#  CS40220 38043 Grenoble Cedex 9 
#  FRANCE
# 
#  Contact: lima@esrf.fr
# 
#  This is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
# 
#  This software is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################

#HAVE TO BE UNDER WINDOWS
if(WIN32)
	set(NAME "hamamatsu")

	#SOURCEs
	set(${NAME}_srcs src/HamamatsuInterface.cpp src/HamamatsuCamera.cpp
		src/HamamatsuBinCtrlObj.cpp src/HamamatsuDCAMSDKHelper.cpp
		src/HamamatsuDetInfoCtrlObj.cpp src/HamamatsuRoiCtrlObj.cpp
		src/HamamatsuSyncCtrlObj.cpp)
	
	add_library(lima${NAME} SHARED ${${NAME}_srcs})
	
	#INCLUDES
	target_include_directories(lima${NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
	target_include_directories(lima${NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/include")
	target_include_directories(lima${NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/include/legacy")

	target_compile_definitions(lima${NAME} PRIVATE LIBPHAMAMATSU_EXPORTS)
	
	#LIBRARIES
	find_library(HAMAMATSU_DCAMAPI_LIB dcamapi "{CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/bin")
	if("${HAMAMATSU_DCAMAPI_LIB}" STREQUAL "HAMAMATSU_DCAMAPI_LIB-NOTFOUND")
	  message(FATAL_ERROR "${NAME} : dcamapi.lib not found, you can search it in cmake-gui or try to install it if it isn't here.")
	endif()
	find_library(HAMAMATSU_DCIMGAPI_LIB dcimgapi "{CMAKE_CURRENT_SOURCE_DIR}/sdk/msvc/bin")
	if("${HAMAMATSU_DCIMGAPI_LIB}" STREQUAL "HAMAMATSU_DCIMGAPI_LIB-NOTFOUND")
	  message(FATAL_ERROR "${NAME} : dcimgapi.lib not found, you can search it in cmake-gui or try to install it if it isn't here.")
	endif()
	
	target_link_libraries(lima${NAME} limacore)
	target_link_libraries(lima${NAME} ${HAMAMATSU_DCAMAPI_LIB})
	target_link_libraries(lima${NAME} ${HAMAMATSU_DCIMGAPI_LIB})

	limatools_set_library_soversion(lima${NAME} "VERSION")
    set_target_properties(lima${NAME} PROPERTIES PREFIX "lib")
    install(TARGETS lima${NAME} DESTINATION lib)

	#if(LIMA_ENABLE_PYTHON)
	#	limatools_run_sip_for_camera(${NAME})
	#	install(FILES python/__init__.py DESTINATION "${PYTHON_SITE_PACKAGES_DIR}/Lima/Hamamatsu")
	#endif()
else()
	message(FATAL_ERROR "This camera has to be compiled on Windows")
endif()
