if(FS_EXT2)
	add_sources(ext2fs.cpp)
endif()

if(FS_CRAMFS)
	add_sources(cramfs.cpp)
endif()

add_subdirectory(fat)
add_subdirectory(ankhfs)
