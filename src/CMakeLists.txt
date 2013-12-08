include(Symlink)

add_definitions(${ALSA_CFLAGS_OTHER} ${GTKMM_CFLAGS_OTHER} ${LIBAVCODEC_CFLAGS_OTHER})
include_directories(${ALSA_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS} ${LIBAVCODEC_INCLUDE_DIRS})
link_directories(${ALSA_LIBRARY_DIRS} ${GTKMM_LIBRARY_DIRS} ${LIBAVCODEC_LIBRARY_DIRS})

add_definitions(-DSHAREDIR="\\"${sharedir}\\"")

set(common_sources config.c dif.c socket.c)

add_executable(dvsink-command-voc dvsink-command.c ${common_sources})

add_executable(dvsink-files-voc dvsink-files.c ${common_sources})

add_executable(dvsource-file-voc dvsource-file.c frame_timer.c ${common_sources})
target_link_libraries(dvsource-file-voc pthread rt)

add_executable(dvsource-dvgrab-voc dvsource-dvgrab.c ${common_sources})

add_executable(dvsource-alsa-voc dvsource-alsa.c dif_audio.c ${common_sources})
target_link_libraries(dvsource-alsa-voc m ${ALSA_LIBRARIES})

add_executable(dvswitch-voc dvswitch.cpp mixer.cpp frame_timer.c
  mixer_window.cpp dv_display_widget.cpp dv_selector_widget.cpp
  server.cpp auto_pipe.cpp os_error.cpp video_effect.c frame_pool.cpp
  frame.c auto_codec.cpp dif_audio.c vu_meter.cpp
  status_overlay.cpp ${common_sources})
target_link_libraries(dvswitch-voc m pthread rt X11 Xext Xv
  ${BOOST_THREAD_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES}
  ${GTKMM_LIBRARIES} ${LIBAVCODEC_LIBRARIES} ${LIBAVUTIL_LIBRARIES})

install(TARGETS dvsink-command-voc dvsink-files-voc dvsource-file-voc
		dvsource-dvgrab-voc dvsource-alsa-voc dvswitch-voc
        DESTINATION ${bindir})
install_symlink(dvsource-dvgrab-voc "${bindir}/dvsource-firewire-voc")
install_symlink(dvsource-dvgrab-voc "${bindir}/dvsource-v4l2-dv-voc")
