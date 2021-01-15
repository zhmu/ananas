set(LIBSTDCXX_PATH /work/external/gcc/libstdc++-v3)

add_sources(
    ${LIBSTDCXX_PATH}/src/c++98/list.cc
    ${LIBSTDCXX_PATH}/src/c++98/list-aux.cc
    ${LIBSTDCXX_PATH}/src/c++98/list-aux-2.cc
    abi.cpp
    stdlib.cpp
    sup.cpp
)
