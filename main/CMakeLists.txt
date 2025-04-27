set(srcs app.c
         ble.c
         led.c
         main.c
         mlog.c
         nvram.c
         ota.c
         wifi.c)


set(include_dirs ".")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       EMBED_TXTFILES ${project_dir}/server_certs/ota_cert.pem)
