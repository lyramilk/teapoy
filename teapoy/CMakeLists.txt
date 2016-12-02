#定义工程名及版本
project(teapoy)
set(version 1.0.2)
set(soversion 1.0)


cmake_minimum_required (VERSION 2.8.11) 
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)
set(CMAKE_USE_RELATIVE_PATHS ON)
set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
set(CMAKE_BUILE_TYPE DEBUG) 


#是否开启内存泄露测试
set(MEMLEAKTEST 0)

#导入一此需要用到的工具包
include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(FindPkgConfig)
include(UsePkgConfig)
enable_testing()

#定制编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -g -Wall")

if(release)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O4 -DNDEBUG -Wall ")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -O4 -Wall ")
	message("Build as release")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -fstack-protector ")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -g -Wall -fstack-protector ")
	message("Build as debug")
endif()

#定义安装目录
if(NOT install_dir)
	set(install_dir ${CMAKE_CURRENT_SOURCE_DIR}/output)
endif()

#搜索源代码
aux_source_directory(src src_cpps)

#搜索头文件
file(GLOB src_hs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.h)
include_directories(src)

if(UNIX)
	aux_source_directory(src/sni src_cpps)
	include_directories(src/sni)
endif()

## 搜索可选的外部依赖
if(UNIX)
	pkg_check_modules(LIBMILK libmilk)
	pkg_check_modules(LEVELDB leveldb)
	pkg_check_modules(LIBCURL libcurl)
	pkg_check_modules(PCRE libpcre)
	pkg_check_modules(FFMPEG libavcodec)
	pkg_check_modules(FFMPEG_AVDEVICE libavdevice)
	pkg_check_modules(FFMPEG_AVFILTER libavfilter)
	pkg_check_modules(FFMPEG_AVFORMAT libavformat)
	pkg_check_modules(FFMPEG_AVUTIL libavutil)
	pkg_check_modules(FFMPEG_SWRESAMPLE libswresample)
	pkg_check_modules(FFMPEG_SWSCALE libswscale)
	pkg_search_module(JS17 mozjs-17.0)

	check_include_file_cxx(mysql/mysql.h Z_HAVE_MYSQLDEVEL)
	check_include_file_cxx(jpeglib.h Z_HAVE_LIBJPEG)
endif()

#if(NOT Z_HAVE_FILEDEVEL)
#	message(FATAL_ERROR "require rpm package file-devel")
#endif()

if(NOT Z_HAVE_MYSQLDEVEL)
	message(FATAL_ERROR "require rpm package mysql-devel")
endif()

if(NOT PCRE_FOUND)
	message(FATAL_ERROR "require rpm package pcre-devel")
endif()

if(LIBMILK_FOUND)
	list(APPEND BUILD_DEPENDS libmilk-devel)
	list(APPEND RUN_DEPENDS libmilk)
	list(APPEND LIBS_DEPENDS ${LIBMILK_LDFLAGS})
	list(APPEND INCS_DEPENDS ${LIBMILK_CFLAGS})
	list(APPEND CONFIG_MACROS LIBMILK_FOUND)
endif()

if(LEVELDB_FOUND)
	list(APPEND BUILD_DEPENDS leveldb-devel)
	list(APPEND RUN_DEPENDS leveldb)
	list(APPEND LIBS_DEPENDS ${LEVELDB_LDFLAGS})
	list(APPEND INCS_DEPENDS ${LEVELDB_CFLAGS})
	list(APPEND CONFIG_MACROS LEVELDB_FOUND)
endif()

if(LIBCURL_FOUND)
	list(APPEND BUILD_DEPENDS libcurl-devel)
	list(APPEND RUN_DEPENDS libcurl)
	list(APPEND LIBS_DEPENDS ${LIBCURL_LDFLAGS})
	list(APPEND INCS_DEPENDS ${LIBCURL_CFLAGS})
	list(APPEND CONFIG_MACROS LIBCURL_FOUND)
endif()

if(JS17_FOUND)
	list(APPEND BUILD_DEPENDS mozjs17)
	list(APPEND RUN_DEPENDS mozjs17-devel)
	list(APPEND LIBS_DEPENDS ${JS17_LDFLAGS})
	list(APPEND INCS_DEPENDS ${JS17_CFLAGS})
	list(APPEND CONFIG_MACROS JS17_FOUND)
endif()

if(Z_HAVE_MYSQLDEVEL)
	list(APPEND BUILD_DEPENDS mysql-devel)
	list(APPEND RUN_DEPENDS mysql-libs)
	list(APPEND LIBS_DEPENDS -L/usr/lib64/mysql -lmysqlclient)
	list(APPEND CONFIG_MACROS Z_HAVE_MYSQLDEVEL)
endif()

if(FFMPEG_FOUND)
	list(APPEND BUILD_DEPENDS ffmpeg)
	list(APPEND RUN_DEPENDS ffmpeg-devel)

	list(APPEND LIBS_DEPENDS ${FFMPEG_AVUTIL_LDFLAGS} )
	list(APPEND INCS_DEPENDS ${FFMPEG_AVUTIL_CFLAGS})

	list(APPEND LIBS_DEPENDS ${FFMPEG_SWSCALE_LDFLAGS} )
	list(APPEND INCS_DEPENDS ${FFMPEG_SWSCALE_CFLAGS})

	list(APPEND LIBS_DEPENDS ${FFMPEG_SWRESAMPLE_LDFLAGS} )
	list(APPEND INCS_DEPENDS ${FFMPEG_SWRESAMPLE_CFLAGS})

	list(APPEND LIBS_DEPENDS ${FFMPEG_LDFLAGS} )
	list(APPEND INCS_DEPENDS ${FFMPEG_CFLAGS})

	list(APPEND LIBS_DEPENDS ${FFMPEG_AVFORMAT_LDFLAGS} )
	list(APPEND INCS_DEPENDS ${FFMPEG_AVFORMAT_CFLAGS})

	list(APPEND LIBS_DEPENDS ${FFMPEG_AVFILTER_LDFLAGS} )
	list(APPEND INCS_DEPENDS ${FFMPEG_AVFILTER_CFLAGS})

	list(APPEND LIBS_DEPENDS ${FFMPEG_AVDEVICE_LDFLAGS} )
	list(APPEND INCS_DEPENDS ${FFMPEG_AVDEVICE_CFLAGS})

	list(APPEND CONFIG_MACROS FFMPEG_FOUND)
endif()

if(PCRE_FOUND)
	list(APPEND BUILD_DEPENDS pcre)
	list(APPEND RUN_DEPENDS pcre-devel)
	list(APPEND LIBS_DEPENDS ${PCRE_LDFLAGS})
	list(APPEND INCS_DEPENDS ${PCRE_CFLAGS})
	list(APPEND CONFIG_MACROS PCRE_FOUND)
endif()


#if(Z_HAVE_LIBJPEG)
	list(APPEND BUILD_DEPENDS libjpeg-turbo)
	list(APPEND RUN_DEPENDS libjpeg-turbo-devel)
	list(APPEND LIBS_DEPENDS -ljpeg)
	list(APPEND CONFIG_MACROS Z_HAVE_LIBJPEG)
#endif()



if(UNIX)
	set(LIBRARIES -lpthread)
else()
	message(FATAL_ERROR "teapoy is works on Linux only!!!")
endif()

#根据外部依赖的存在与定义一些宏
foreach(v ${BUILD_DEPENDS})
	set(CMAKE_BUILD_DEPENDS "${CMAKE_BUILD_DEPENDS} ${v}")
endforeach()

foreach(v ${RUN_DEPENDS})
	set(CMAKE_RUN_DEPENDS "${CMAKE_RUN_DEPENDS} ${v}")
endforeach()

foreach(v ${INCS_DEPENDS})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${v}")
endforeach()

set(CMAKE_LIBS_DEPENDS ${LIBS_DEPENDS})
message(${CMAKE_LIBS_DEPENDS})

foreach(v ${CONFIG_MACROS})
	set(CMAKE_CONFIG_MACROS "${CMAKE_CONFIG_MACROS}#define ${v}\n")
endforeach()

#修改配置文件
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/teapoy.spec.in ${CMAKE_CURRENT_SOURCE_DIR}/pkg/teapoy.spec)


#目标动态库
add_library(${PROJECT_NAME} SHARED ${src_cpps} ${src_hs})
#定义安装目标
install(TARGETS ${PROJECT_NAME} DESTINATION ${install_dir})

#对libmilk的依赖
if(NOT ${LIBMILK_CFLAGS})
	add_custom_target(milk COMMAND make -C ~/libmilk/build)
	add_dependencies(${PROJECT_NAME} milk)
endif()


#链接库
if(UNIX)
	 target_link_libraries(${PROJECT_NAME} -lpthread ${CMAKE_LIBS_DEPENDS})
else()
	message(FATAL_ERROR "teapoy is works on Linux only!!!")
endif()

#定义应用目标
file(GLOB src_app RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/app/*.cc)
foreach(var ${src_app})
	string(REGEX REPLACE "src/app/(.*).cc" "\\1" var2 ${var})
	add_executable(${var2} ${var})
	target_link_libraries(${var2} ${PROJECT_NAME})
	#target_link_libraries(${var2} -L/usr/lib64/mysql -lmysqlclient)
	install(TARGETS ${PROJECT_NAME} DESTINATION ${install_dir})
	#定义翻译目标
	#if(Z_HAVE_GETTEXT)
	#	add_custom_target(${var2}_pot xgettext -d ${PROJECT_NAME} -s -j -o ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pot ${var})
	#endif()
endforeach()


#单元测试，是否开启内存泄露测试
if(MEMLEAKTEST)
	set(unittest valgrind --tool=memcheck --leak-check=full)
else()
	set(unittest )
endif()

file(GLOB src_test RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} testsuite/*.cc)
foreach(var ${src_test})
	string(REGEX REPLACE "testsuite/(.*).cc" "\\1" var2 ${var})
	add_executable(${var2} ${var})
	target_link_libraries(${var2} ${PROJECT_NAME})
	add_test(NAME ${var2} COMMAND ${unittest} ./${var2})
endforeach()

