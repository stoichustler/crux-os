set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm64)

# 交叉工具链前缀
set(CMAKE_C_COMPILER   aarch64-none-elf-gcc)
set(CMAKE_ASM_COMPILER aarch64-none-elf-gcc)
set(CMAKE_AR           aarch64-none-elf-ar)
set(CMAKE_RANLIB       aarch64-none-elf-ranlib)

# 关闭共享库，强制静态
set(BUILD_SHARED_LIBS OFF)

# 让 CMake 跳过系统路径
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
