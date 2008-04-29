file(GLOB image_files *.png)
install(FILES ${image_files}
        DESTINATION ${sharedir}/dvswitch)

