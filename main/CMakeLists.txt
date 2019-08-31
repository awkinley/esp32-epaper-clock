set(COMPONENT_SRCS "main.c"
                   "DEV_Config.c"
                   "EPD_2in9.c"
                   "GUI_BMPfile.c"
                   "GUI_Paint.c"
                   "ImageData.c"
                   "font8.c"
                   "font23.c"
                   "font20.c"
                   "font16.c"
                   "font12.c"
                   "Anonymous24.c"
                   "Anonymous64.c")


set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_ADD_INCLUDEDIRS "Fonts/.")

set(COMPONENT_EMBED_FILES 100x100.bmp 2in9.bmp)

register_component()
