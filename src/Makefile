CFLAGS := -std=c99 -Wall -Wextra -O2
CXXFLAGS := -std=c++98 -Wall -Wextra -O2
CPPFLAGS := -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_POSIX_C_SOURCE

csources := dvsource-file.c dvsource-firewire.c config.c socket.c frame_timer.c
cxxsources := dvswitch.cpp mixer.cpp
common_objects := .objs/config.o .objs/socket.o

all : .objs/dvsource-file .objs/dvsource-firewire .objs/dvswitch

install :
	mkdir -p -m755 $(DESTDIR)/usr/bin
	install .objs/dvsource-file .objs/dvsource-firewire .objs/dvswitch \
		$(DESTDIR)/usr/bin

clean :
	rm -rf .objs

.objs/dvsource-file.% : CFLAGS += $(shell pkg-config --cflags libdv)
.objs/dvsource-file .objs/dvswitch : \
	LDFLAGS += $(shell pkg-config --libs libdv)

.objs/dvsource-file .objs/dvswitch : LDFLAGS += -lpthread -lrt

.objs/dvswitch.% .objs/dv_view_widget.% .objs/dv_selector.% : \
	CXXFLAGS += $(shell pkg-config --cflags gtkmm-2.0)
.objs/dvswitch : LDFLAGS += $(shell pkg-config --libs gtkmm-2.0)

.objs/dvswitch : LDFLAGS += -lboost_thread

.objs/dvsource-file : \
	.objs/dvsource-file.o .objs/frame_timer.o $(common_objects)

.objs/dvsource-firewire : .objs/dvsource-firewire.o $(common_objects)

.objs/dvswitch : \
	.objs/dvswitch.o .objs/mixer.o .objs/frame_timer.o $(common_objects)

.PHONY : all install clean

.objs/%.d : .objs/.created
	touch $@

.objs/%.o : %.cpp .objs/.created
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ -MD -MF .objs/$*.d -c $<

.objs/%.o : %.c .objs/.created
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -MD -MF .objs/$*.d -c $<

%/.created :
	mkdir -p $*
	touch $@

ifneq ($(MAKECMDGOALS),clean)
    include $(cxxsources:%.cpp=.objs/%.d) $(csources:%.c=.objs/%.d)
endif

.PRECIOUS : %/.created
