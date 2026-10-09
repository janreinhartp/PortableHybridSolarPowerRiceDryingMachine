idf_component_register(
    SRCS "ds1307.c"
    INCLUDE_DIRS "."
    REQUIRES driver i2c log
)
