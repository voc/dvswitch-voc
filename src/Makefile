CFLAGS := -std=c99 -Wall -Wextra -O2
CXXFLAGS := -std=c++98 -Wall -Wextra -O2
CPPFLAGS := -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_POSIX_C_SOURCE

csources := dvsource-file.c dvsource-firewire.c config.c socket.c
common_objects := .objs/config.o .objs/socket.o

all : .objs/dvsource-file .objs/dvsource-firewire

install :
	mkdir -p -m755 $(DESTDIR)/usr/bin
	install .objs/dvsource-file .objs/dvsource-firewire \
		$(DESTDIR)/usr/bin

clean :
	rm -rf .objs

.objs/dvsource-file.% : CFLAGS += $(shell pkg-config --cflags libdv)
.objs/dvsource-file : LDFLAGS += $(shell pkg-config --libs libdv) -lrt

.objs/dvsource-file : .objs/dvsource-file.o $(common_objects)

.objs/dvsource-firewire : .objs/dvsource-firewire.o $(common_objects)

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
