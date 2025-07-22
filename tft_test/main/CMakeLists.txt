# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_sntp nvs_flash esp_netif
)
