if(FS_FAT)
	add_sources(
		fatfs.cpp
		block.cpp
		dir.cpp
		inode.cpp
	)
endif()
