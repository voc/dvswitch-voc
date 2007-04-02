CPPFLAGS := -I../src

mixer : LDFLAGS += -lboost_thread -lpthread -lrt

mixer : mixer.o ../src/.objs/mixer.o ../src/.objs/frame_timer.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

ring_buffer : ring_buffer.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

clean :
	rm -f mixer ring_buffer *.o 

.PHONY : all clean
