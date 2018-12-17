if(FS_EXT2)
	add_sources(ext2fs.cpp)
endif()

add_subdirectory(fat)
add_subdirectory(ankhfs)
