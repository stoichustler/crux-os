set(CURRENT_DIR ${PROJECT_DIR}/utility/picolibc)

target_sources(${PROJECT_NAME}
	PRIVATE
	# {{BEGIN_TARGET_SOURCES}}
	${CURRENT_DIR}/picolibc_hook.c
	# {{BEGIN_TARGET_SOURCES}}
)

target_include_directories(${PROJECT_NAME} 
    PUBLIC 
    ${CURRENT_DIR}/aarch64/include
)

