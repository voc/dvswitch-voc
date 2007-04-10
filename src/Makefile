prefix := /usr/local

bindir := $(prefix)/bin
sharedir := $(prefix)/share
docdir := $(sharedir)/doc
mandir := $(sharedir)/man

CFLAGS := -std=c99 -Wall -Wextra -O2 -g
CXXFLAGS := -std=c++98 -Wall -Wextra -O2 -g
CPPFLAGS := -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_POSIX_C_SOURCE

csources := dvsink-files.c dvsource-file.c dvsource-firewire.c config.c \
	socket.c frame_timer.c
cxxsources := dvswitch.cpp mixer.cpp mixer_window.cpp dv_display_widget.cpp \
	dv_selector_widget.cpp server.cpp
common_objects := .objs/config.o .objs/socket.o

all : .objs/dvsink-command .objs/dvsink-files .objs/dvsource-file \
	.objs/dvsource-firewire .objs/dvswitch

install :
	mkdir -p -m755 $(DESTDIR)$(bindir)
	install .objs/dvsink-command .objs/dvsink-files .objs/dvsource-file \
		.objs/dvsource-firewire .objs/dvswitch \
		$(DESTDIR)$(bindir)

clean :
	rm -rf .objs

.objs/% : CFLAGS += $(shell pkg-config --cflags libdv)
.objs/dvsink-files .objs/dvsource-file .objs/dvswitch : \
	LDFLAGS += $(shell pkg-config --libs libdv)

.objs/dvsource-file .objs/dvswitch : LDFLAGS += -lpthread -lrt

.objs/dvswitch.% .objs/mixer_window.% .objs/dv_display_widget.% \
.objs/dv_selector_widget.% .objs/server.% : \
	CXXFLAGS += $(shell pkg-config --cflags gtkmm-2.0)
.objs/dvswitch : LDFLAGS += $(shell pkg-config --libs gtkmm-2.0)

.objs/dvswitch : LDFLAGS += -lboost_thread

.objs/dvswitch : LDFLAGS += -lXv

.objs/dvsink-command : .objs/dvsink-command.o $(common_objects)

.objs/dvsink-files : .objs/dvsink-files.o $(common_objects)

.objs/dvsource-file : \
	.objs/dvsource-file.o .objs/frame_timer.o $(common_objects)

.objs/dvsource-firewire : .objs/dvsource-firewire.o $(common_objects)

.objs/dvswitch : .objs/dvswitch.o .objs/mixer.o .objs/frame_timer.o \
	.objs/mixer_window.o .objs/dv_display_widget.o \
        .objs/dv_selector_widget.o .objs/server.o $(common_objects)

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
