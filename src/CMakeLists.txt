include(Symlink)

add_definitions(${ALSA_CFLAGS_OTHER} ${GTKMM_CFLAGS_OTHER} ${LIBAVCODEC_CFLAGS_OTHER})
include_directories(${ALSA_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS} ${LIBAVCODEC_INCLUDE_DIRS})
link_directories(${ALSA_LIBRARY_DIRS} ${GTKMM_LIBRARY_DIRS} ${LIBAVCODEC_LIBRARY_DIRS})

add_definitions(-DSHAREDIR="\\"${sharedir}\\"")

set(common_sources config.c dif.c socket.c)

add_executable(dvswitch-voc dvswitch.cpp mixer.cpp frame_timer.c
  mixer_window.cpp dv_display_widget.cpp dv_selector_widget.cpp
  server.cpp auto_pipe.cpp os_error.cpp video_effect.c frame_pool.cpp
  frame.c auto_codec.cpp dif_audio.c vu_meter.cpp
  status_overlay.cpp ${common_sources})
target_link_libraries(dvswitch-voc m pthread rt X11 Xext Xv
  ${BOOST_THREAD_LIBRARIES} ${BOOST_SYSTEM_LIBRARIES}
  ${GTKMM_LIBRARIES} ${LIBAVCODEC_LIBRARIES} ${LIBAVUTIL_LIBRARIES})

install(TARGETS dvswitch-voc
        DESTINATION ${bindir})
