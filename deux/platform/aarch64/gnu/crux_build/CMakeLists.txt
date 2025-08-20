set(APP_NAME deux)
set(DIS_NAME deux.lst)
set(EXE_NAME ${APP_NAME}.elf)
set(LDS ${CMAKE_CURRENT_LIST_DIR}/src/crux_threadx.ld)
add_executable(${EXE_NAME})
target_link_libraries(${EXE_NAME} ${PROJECT_NAME})
target_link_options(${EXE_NAME} PRIVATE -T ${LDS})

target_sources(${EXE_NAME} PRIVATE
	# {{BEGIN_TARGET_SOURCES}}
	${CMAKE_CURRENT_LIST_DIR}/src/crux_threadx.c
	${CMAKE_CURRENT_LIST_DIR}/src/GICv3_gicd.c
	${CMAKE_CURRENT_LIST_DIR}/src/GICv3_gicr.c
	${CMAKE_CURRENT_LIST_DIR}/src/sp804_timer.c
	${CMAKE_CURRENT_LIST_DIR}/src/timer_interrupts.c
	${CMAKE_CURRENT_LIST_DIR}/src/MP_Mutexes.S
	${CMAKE_CURRENT_LIST_DIR}/src/startup.S
	${CMAKE_CURRENT_LIST_DIR}/src/v8_aarch64.S
	${CMAKE_CURRENT_LIST_DIR}/src/v8_utils.S
	${CMAKE_CURRENT_LIST_DIR}/src/vectors.S
	${CMAKE_CURRENT_LIST_DIR}/src/pecoff.S
	# {{END_TARGET_SOURCES}} 
)

message(STATUS "Find libc from ${CMAKE_SOURCE_DIR}/utility/picolibc")
target_link_libraries(${EXE_NAME} ${CMAKE_SOURCE_DIR}/utility/picolibc/aarch64/libc.a)
target_link_libraries(${EXE_NAME} ${PROJECT_NAME})

add_custom_command(
	TARGET ${EXE_NAME}
	POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} ${EXE_NAME} -O binary ${APP_NAME}
)

add_custom_command(
	TARGET ${EXE_NAME}
	POST_BUILD
	COMMAND ${CMAKE_OBJDUMP} -d -S ${EXE_NAME} > ${DIS_NAME}
)
