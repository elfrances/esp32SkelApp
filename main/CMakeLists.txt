set(srcs app.c
         ble.c
         led.c
         main.c
         mlog.c
         nvram.c
         wifi.c)


set(include_dirs ".")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}")
