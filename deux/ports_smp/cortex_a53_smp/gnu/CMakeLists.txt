target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_initialize_low_level.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_context_restore.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_context_save.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_fp_disable.c
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_fp_enable.c
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_interrupt_control.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_interrupt_disable.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_interrupt_restore.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_schedule.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_core_get.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_core_preempt.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_current_state_get.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_current_thread_get.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_initialize_wait.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_low_level_initialize.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_protect.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_time_get.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_smp_unprotect.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_stack_build.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_thread_system_return.S
    ${CMAKE_CURRENT_LIST_DIR}/src/tx_timer_interrupt.S
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/inc
)

set(APP_NAME deux)
set(EXE_NAME ${APP_NAME}.elf)
set(LDS ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/threadx.ld)
add_executable(${EXE_NAME})
target_link_libraries(${EXE_NAME} ${PROJECT_NAME})
target_link_options(${EXE_NAME} PRIVATE -T ${LDS})
target_sources(${EXE_NAME} PRIVATE
    # 
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/GICv3_gicd.c
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/GICv3_gicr.c
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/MP_Mutexes.S
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/sp804_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/startup.S
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/threadx.c
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/timer_interrupts.c
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/v8_aarch64.S
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/v8_utils.S
    ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/vectors.S
    # ${CMAKE_CURRENT_LIST_DIR}/crux_build/deux/pecoff.S
    # 
)
add_custom_command(
    TARGET ${EXE_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${EXE_NAME} -O binary ${APP_NAME}
)
