# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(SRCS main.c rgb_led.c wifi_app.c send_email.c ldr_gl5537.c display.c font.c sntp.c mqtt.c
                       INCLUDE_DIRS "."
                       
                       
                       )


