CPPFLAGS := -I../src

mixer : LDFLAGS += -lboost_thread -lpthread -lrt

mixer : mixer.o ../src/.objs/mixer.o ../src/.objs/frame_timer.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

ring_buffer : ring_buffer.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

pic_in_pic : CPPFLAGS += $(shell pkg-config --cflags libavcodec)
pic_in_pic : LDFLAGS += $(shell pkg-config --libs libavcodec)

pic_in_pic : pic_in_pic.o ../src/.objs/video_effect.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean :
	rm -f mixer ring_buffer pic_in_pic *.o 

.PHONY : all clean
