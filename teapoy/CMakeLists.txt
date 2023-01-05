# cmake -D release=1 -D CMAKE_INSTALL_PREFIX=~/.local ..

#定义工程名及版本
project(teapoy2)
set(version 2.0.23)
set(soversion 2.0)


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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -fPIC")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -g -Wall -fPIC -Bsymbolic")

if(release)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O4 -Wall -DNDEBUG ")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -O4 -Wall ")
	message("Build as release")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -fstack-protector -DDEBUG -D_DEBUG ")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -g -Wall -fstack-protector ")
	message("Build as debug")
endif()

#定义安装目录
set(CMAKE_INSTALL_PREFIX $ENV{HOME}/.local)

#搜索源代码
aux_source_directory(src src_cpps)

#搜索头文件
file(GLOB src_hs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.h)
include_directories(src)

if(UNIX)
	aux_source_directory(src/sni src_cpps)
	include_directories(src/sni)
endif()
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

list(APPEND LIBS_DEPENDS -ldl)

## 搜索可选的外部依赖
if(UNIX)
	pkg_check_modules(OPENSSL openssl)
	pkg_check_modules(LIBMILK libmilk)
	pkg_check_modules(CAVEDB cavedb)
	pkg_check_modules(LEVELDB leveldb>1.19)
	pkg_check_modules(LIBCURL libcurl)
	pkg_check_modules(PCRE libpcre REQUIRED)
	pkg_check_modules(ZLIB zlib)
	pkg_check_modules(NGHTTP2 libnghttp2)
	pkg_check_modules(JS38 mozjs-38)
	#pkg_check_modules(LIBCMARK libcmark)
	if(NOT JS38_FOUND)
		pkg_check_modules(JS17 mozjs-17.0 REQUIRED)
	endif()
	pkg_check_modules(TINYXML2 tinyxml2)

	pkg_check_modules(MYSQL mysqlclient)
	check_include_file_cxx(jpeglib.h Z_HAVE_LIBJPEG)
	check_include_file_cxx(mongo/version.h Z_HAVE_MONGODB)
	check_include_file_cxx(mongo/client/connpool.h Z_HAVE_LIBMONGO)
endif()

if(OPENSSL_FOUND)
	list(APPEND BUILD_DEPENDS openssl-devel)
	list(APPEND RUN_DEPENDS openssl)
	list(APPEND LIBS_DEPENDS ${OPENSSL_LDFLAGS})
	list(APPEND INCS_DEPENDS ${OPENSSL_CFLAGS})
	list(APPEND CONFIG_MACROS OPENSSL_FOUND)
endif()

if(LIBMILK_FOUND)
	list(APPEND BUILD_DEPENDS libmilk-devel)
	list(APPEND RUN_DEPENDS libmilk)
	list(APPEND LIBS_DEPENDS ${LIBMILK_LDFLAGS})
	list(APPEND INCS_DEPENDS ${LIBMILK_CFLAGS})
	list(APPEND CONFIG_MACROS LIBMILK_FOUND)
endif()

if(CAVEDB_FOUND)
	list(APPEND BUILD_DEPENDS cavedb-devel)
	list(APPEND RUN_DEPENDS cavedb)
	list(APPEND LIBS_DEPENDS ${CAVEDB_LDFLAGS})
	list(APPEND INCS_DEPENDS ${CAVEDB_CFLAGS})
	list(APPEND CONFIG_MACROS CAVEDB_FOUND)
else()
	list(REMOVE_ITEM src_cpps src/sni/TeapoyDBServer.cpp)
	list(REMOVE_ITEM src_cpps src/functional_impl_cavedb.cpp)
endif()

if(LEVELDB_FOUND)
	list(APPEND BUILD_DEPENDS leveldb-devel)
	list(APPEND RUN_DEPENDS leveldb)
	list(APPEND LIBS_DEPENDS ${LEVELDB_LDFLAGS})
	list(APPEND INCS_DEPENDS ${LEVELDB_CFLAGS})
	list(APPEND CONFIG_MACROS LEVELDB_FOUND)
else()
	list(REMOVE_ITEM src_cpps src/sni/leveldb.cpp)
	list(REMOVE_ITEM src_cpps src/sni/TeapoyDBServer.cpp)
endif()

if(LIBCURL_FOUND)
	list(APPEND BUILD_DEPENDS libcurl-devel)
	list(APPEND RUN_DEPENDS libcurl)
	list(APPEND LIBS_DEPENDS ${LIBCURL_LDFLAGS})
	list(APPEND INCS_DEPENDS ${LIBCURL_CFLAGS})
	list(APPEND CONFIG_MACROS LIBCURL_FOUND)
else()
	list(REMOVE_ITEM src_cpps src/sni/curl.cpp)
endif()

if(JS38_FOUND)
	list(APPEND BUILD_DEPENDS mozjs38-devel)
	list(APPEND RUN_DEPENDS mozjs38)
	list(APPEND LIBS_DEPENDS ${JS38_LDFLAGS})
	list(APPEND INCS_DEPENDS ${JS38_CFLAGS})
	list(APPEND CONFIG_MACROS JS38_FOUND)
	list(REMOVE_ITEM src_cpps src/js_extend17.cpp)
elseif(JS17_FOUND)
	list(APPEND BUILD_DEPENDS mozjs17-devel)
	list(APPEND RUN_DEPENDS mozjs17)
	list(APPEND LIBS_DEPENDS ${JS17_LDFLAGS})
	list(APPEND INCS_DEPENDS ${JS17_CFLAGS})
	list(APPEND CONFIG_MACROS JS17_FOUND)
	list(REMOVE_ITEM src_cpps src/js_extend38.cpp)
else()
	list(REMOVE_ITEM src_cpps src/js_extend17.cpp)
	list(REMOVE_ITEM src_cpps src/js_extend38.cpp)
endif()

if(TINYXML2_FOUND)
	list(APPEND BUILD_DEPENDS tinyxml2-devel)
	list(APPEND RUN_DEPENDS tinyxml2)
	list(APPEND LIBS_DEPENDS ${TINYXML2_LDFLAGS})
	list(APPEND INCS_DEPENDS ${TINYXML2_CFLAGS})
	list(APPEND CONFIG_MACROS TINYXML2_FOUND)
else()
	list(REMOVE_ITEM src_cpps src/sni/xml.cpp)
endif()

if(MYSQL_FOUND)
	list(APPEND BUILD_DEPENDS libmysqlclient-dev)
	list(APPEND RUN_DEPENDS libmysqlclient)
	list(APPEND LIBS_DEPENDS ${MYSQL_LDFLAGS})
	list(APPEND CONFIG_MACROS MYSQL_FOUND)
else()
	list(REMOVE_ITEM src_cpps src/sni/mysql.cpp)
endif()

if(PCRE_FOUND)
	list(APPEND BUILD_DEPENDS pcre-devel)
	list(APPEND RUN_DEPENDS pcre)
	list(APPEND LIBS_DEPENDS ${PCRE_LDFLAGS})
	list(APPEND INCS_DEPENDS ${PCRE_CFLAGS})
	list(APPEND CONFIG_MACROS PCRE_FOUND)
endif()

if(ZLIB_FOUND)
	list(APPEND BUILD_DEPENDS zlib-devel)
	list(APPEND RUN_DEPENDS zlib)
	list(APPEND LIBS_DEPENDS ${ZLIB_LDFLAGS})
	list(APPEND INCS_DEPENDS ${ZLIB_CFLAGS})
	list(APPEND CONFIG_MACROS ZLIB_FOUND)
endif()

if(NGHTTP2_FOUND)
	list(APPEND BUILD_DEPENDS libnghttp2-devel)
	list(APPEND RUN_DEPENDS libnghttp2)
	list(APPEND LIBS_DEPENDS ${NGHTTP2_LDFLAGS})
	list(APPEND INCS_DEPENDS ${NGHTTP2_CFLAGS})
	list(APPEND CONFIG_MACROS NGHTTP2_FOUND)
	list(REMOVE_ITEM src_cpps src/http_2_0_hpack_huffman.cpp)
	list(REMOVE_ITEM src_cpps src/http_2_0_hpack.cpp)
	list(REMOVE_ITEM src_cpps src/http_2_0_hpack.cpp)
else()
	list(REMOVE_ITEM src_cpps src/http_2_0_nghttp2.cpp)
	list(REMOVE_ITEM src_cpps src/http_2_0_nghttp2.h)
endif()

if(CAVEDB_FOUND)
	list(APPEND BUILD_DEPENDS cavedb-devel)
	list(APPEND RUN_DEPENDS cavedb)
	list(APPEND LIBS_DEPENDS ${CAVEDB_LDFLAGS})
	list(APPEND INCS_DEPENDS ${CAVEDB_CFLAGS})
	list(APPEND CONFIG_MACROS CAVEDB_FOUND)
endif()

if(Z_HAVE_LIBJPEG)
	list(APPEND BUILD_DEPENDS libjpeg-turbo-devel)
	list(APPEND RUN_DEPENDS libjpeg-turbo)
	list(APPEND LIBS_DEPENDS -ljpeg)
	list(APPEND CONFIG_MACROS Z_HAVE_LIBJPEG)
else()
	list(REMOVE_ITEM src_cpps src/sni/jpeg.cpp)
endif()

if(Z_HAVE_MONGODB)
	list(APPEND BUILD_DEPENDS mongo-cxx-driver-devel)
	list(APPEND RUN_DEPENDS mongo-cxx-driver)
	list(APPEND LIBS_DEPENDS -lmongoclient)
	list(APPEND CONFIG_MACROS Z_HAVE_MONGODB)
elseif(Z_HAVE_LIBMONGO)
	list(APPEND BUILD_DEPENDS libmongodb-devel)
	list(APPEND RUN_DEPENDS libmongodb)
	list(APPEND LIBS_DEPENDS -lmongoclient)
	list(APPEND CONFIG_MACROS Z_HAVE_LIBMONGO)
else()
	list(REMOVE_ITEM src_cpps src/sni/mongo.cpp)
endif()

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

#message(${CMAKE_LIBS_DEPENDS})

foreach(v ${CONFIG_MACROS})
	set(CMAKE_CONFIG_MACROS "${CMAKE_CONFIG_MACROS}#define ${v}\n")
endforeach()

#修改配置文件
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pkg/teapoy.spec.in ${CMAKE_CURRENT_SOURCE_DIR}/pkg/teapoy.spec)


#目标动态库
add_library(${PROJECT_NAME} SHARED ${src_cpps} ${src_hs})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${version} SOVERSION ${soversion})

#拷贝文件
file(COPY ${src_hs} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/)

#对libmilk的依赖
if(NOT ${LIBMILK_CFLAGS})
	add_custom_target(milk COMMAND make milk -j -C /data/src/libmilk/build/)
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
	#install(TARGETS ${PROJECT_NAME} DESTINATION ${install_dir})
	#定义翻译目标
	#if(Z_HAVE_GETTEXT)
	#	add_custom_target(${var2}_pot xgettext -d ${PROJECT_NAME} -s -j -o ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pot ${var})
	#endif()
endforeach()

#定义安装目标
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pkg/${PROJECT_NAME}.pc DESTINATION lib64/pkgconfig)
install(FILES ${src_hs} DESTINATION include/teapoy)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/config.h DESTINATION include/teapoy)
install(TARGETS ${PROJECT_NAME} DESTINATION lib64)
install(TARGETS teapoyd DESTINATION bin)

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
