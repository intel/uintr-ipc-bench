###########################################################
## TARGETS
###########################################################

add_executable(uintrfd-uni uintrfd-uni.c)
add_executable(uintrfd-bi uintrfd-bi.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(uintrfd-uni ipc-bench-common)
target_link_libraries(uintrfd-bi ipc-bench-common)
