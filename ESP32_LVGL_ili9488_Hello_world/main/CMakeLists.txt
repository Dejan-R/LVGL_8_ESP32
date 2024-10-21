file(GLOB srcs
    "*.c"
    "LVGL/*.c"
)

set(inc
    "."
    "LVGL"
)

set(components
    )


idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS ${inc}
                    REQUIRES ${components})

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE=1)
