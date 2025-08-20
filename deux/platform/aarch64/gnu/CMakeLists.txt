set(CURRENT_DIR ${PROJECT_DIR}/platform/aarch64/gnu)
target_sources(${PROJECT_NAME}
    PRIVATE
    # {{BEGIN_TARGET_SOURCES}}
	${CURRENT_DIR}/src/tx_initialize_low_level.S
	${CURRENT_DIR}/src/tx_thread_context_restore.S
	${CURRENT_DIR}/src/tx_thread_context_save.S
	${CURRENT_DIR}/src/tx_thread_fp_disable.c
	${CURRENT_DIR}/src/tx_thread_fp_enable.c
	${CURRENT_DIR}/src/tx_thread_interrupt_control.S
	${CURRENT_DIR}/src/tx_thread_interrupt_disable.S
	${CURRENT_DIR}/src/tx_thread_interrupt_restore.S
	${CURRENT_DIR}/src/tx_thread_schedule.S
	${CURRENT_DIR}/src/tx_thread_smp_core_get.S
	${CURRENT_DIR}/src/tx_thread_smp_core_preempt.S
	${CURRENT_DIR}/src/tx_thread_smp_current_state_get.S
	${CURRENT_DIR}/src/tx_thread_smp_current_thread_get.S
	${CURRENT_DIR}/src/tx_thread_smp_initialize_wait.S
	${CURRENT_DIR}/src/tx_thread_smp_low_level_initialize.S
	${CURRENT_DIR}/src/tx_thread_smp_protect.S
	${CURRENT_DIR}/src/tx_thread_smp_time_get.S
	${CURRENT_DIR}/src/tx_thread_smp_unprotect.S
	${CURRENT_DIR}/src/tx_thread_stack_build.S
	${CURRENT_DIR}/src/tx_thread_system_return.S
	${CURRENT_DIR}/src/tx_timer_interrupt.S
    # {{END_TARGET_SOURCES}}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CURRENT_DIR}/inc
)

target_compile_definitions(${PROJECT_NAME} PUBLIC "-D_GNU_SOURCE -DTX_LINUX_DEBUG_ENABLE")

# Target directory
add_subdirectory(${CURRENT_DIR}/crux_build)
