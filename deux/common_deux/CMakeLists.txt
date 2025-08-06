#############################################
#         _______  __  __
#        /__\_  _\/  \/ _\
#       /  \ / / / / /\ \
#       \/\/ \/  \__/\__/ DEUX 2025
#
#############################################

target_sources(${PROJECT_NAME}
    PRIVATE
    # {{BEGIN_TARGET_SOURCES}}
    ${CMAKE_CURRENT_LIST_DIR}/deux_libc_hooks.c
    ${CMAKE_CURRENT_LIST_DIR}/deux_threadx.c
    # {{END_TARGET_SOURCES}}
)
