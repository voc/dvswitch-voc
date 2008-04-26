CPPFLAGS := -I../src
CXXFLAGS := -g

all : mixer ring_buffer pic_in_pic

mixer pic_in_pic : CPPFLAGS += $(shell pkg-config --cflags libavcodec)
mixer pic_in_pic : LDFLAGS += $(shell pkg-config --libs libavcodec)

mixer : LDFLAGS += -lboost_thread -lpthread -lrt

mixer : mixer.o $(addprefix ../src/.objs/,mixer.o frame_timer.o dif.o frame_pool.o auto_codec.o frame.o os_error.o video_effect.o)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

ring_buffer : ring_buffer.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

pic_in_pic : pic_in_pic.o ../src/.objs/video_effect.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean :
	rm -f mixer ring_buffer pic_in_pic *.o 

.PHONY : all clean
