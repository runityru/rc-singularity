COMMON_OPTIONS = -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LIBS = -lrt -lpthread

BASE_SET = pipelines.o config.o codec.o index.o cpages.o filebuf.o fileparse.o utils.o locks.o keyhead.o allocator.o rc_singularity.o
HANDLER_SET = handler_config.o sing_handler.o

LOGFILE = -DFILE_LOG="debug.log"
DCFLAGS = -g -DMEMORY_CHECK -DLOG_OPERATION -DLOG_MEMORY -DLOG_LOCKS
DOBJDIR = obj/debug
DOBJLIBDIR = obj/debug_lib
DLIBOBJS := $(addprefix $(DOBJLIBDIR)/,$(BASE_SET))
DHNDLOBJS := $(addprefix $(DOBJDIR)/,$(HANDLER_SET))

RCFLAGS = -O3
ROBJDIR = obj/release
ROBJLIBDIR = obj/release_lib
RLIBOBJS := $(addprefix $(ROBJLIBDIR)/,$(BASE_SET))
RHNDLOBJS := $(addprefix $(ROBJDIR)/,$(HANDLER_SET))

TCOBJDIR = obj/ctests
TCOBJS := $(addprefix $(TCOBJDIR)/,$(BASE_SET))

TOBJDIR = obj/tests
TESTS_SET = memory keyheads index_ops codec_ops locks filebuffers fileparse api_read_calls api_write_calls api_file_calls api_other_calls

TESTS_LIST := $(TESTS_SET:%=tests/%)
TESTS_OBJS := $(TESTS_SET:%=$(TOBJDIR)/%.o)

TESTS_RUN := $(TESTS_SET:%=t_%)

.PHONY: all debug release autotests test install clean

all: debug release autotests

debug: debug/sing_handler

debug/sing_handler: lib/librc_singularity_deb.so $(DHNDLOBJS)
	gcc $(DHNDLOBJS) -o $@ -Llib $(LIBS) -lrc_singularity_deb -Wl,-rpath='/usr/local/nic/rc-singularity/lib/'

lib/librc_singularity_deb.so: $(DLIBOBJS)
	gcc $(DLIBOBJS) -shared -o $@ $(LIBS)

$(DOBJDIR)/%.o : %.c | $(DOBJDIR)
	gcc -c $(DCFLAGS) $(COMMON_OPTIONS) -MMD -MP $< -o $@
	
$(DOBJLIBDIR)/%.o : %.c | $(DOBJLIBDIR)
	gcc -c $(DCFLAGS) -fPIC $(COMMON_OPTIONS) -MMD -MP $< -o $@
	
-include $(DLIBOBJS:.o=.d)
-include $(DHNDLOBJS:.o=.d)
	
release: bin/sing_handler

bin/sing_handler: lib/librc_singularity.so $(RHNDLOBJS)
	gcc $(RHNDLOBJS) -o $@ -Llib $(LIBS) -lrc_singularity -Wl,-rpath='/usr/local/nic/rc-singularity/lib/'

lib/librc_singularity.so: $(RLIBOBJS)
	gcc $(RLIBOBJS) -shared -o $@ $(LIBS)

$(ROBJDIR)/%.o : %.c | $(ROBJDIR)
	gcc -c $(RCFLAGS) $(COMMON_OPTIONS) -MMD -MP $< -o $@
	
$(ROBJLIBDIR)/%.o : %.c | $(ROBJLIBDIR)
	gcc -c $(RCFLAGS) -fPIC $(COMMON_OPTIONS) -MMD -MP $< -o $@
	
-include $(RLIBOBJS:.o=.d)
-include $(RHNDLOBJS:.o=.d)
	
autotests: $(TESTS_LIST)

tests/memory: $(TOBJDIR)/common.o $(TOBJDIR)/memory.o $(TCOBJS) 
	gcc $(TOBJDIR)/common.o $(TOBJDIR)/memory.o $(TCOBJS) -o $@ $(LIBS)
	
tests/keyheads: $(TOBJDIR)/common.o  $(TOBJDIR)/keyheads.o $(TCOBJS) 
	gcc $(TOBJDIR)/common.o $(TOBJDIR)/keyheads.o $(TCOBJS) -o $@ $(LIBS)
	
tests/index_ops: $(TOBJDIR)/common.o  $(TOBJDIR)/index_ops.o $(TCOBJS) 
	gcc $(TOBJDIR)/common.o $(TOBJDIR)/index_ops.o $(TCOBJS) -o $@ $(LIBS)
	
tests/codec_ops:  $(TOBJDIR)/codec_ops.o $(TCOBJS) 
	gcc $(TOBJDIR)/codec_ops.o $(TCOBJS) -o $@ $(LIBS)
	
tests/locks:  $(TOBJDIR)/locks.o $(TCOBJS) 
	gcc $(TOBJDIR)/locks.o $(TCOBJS) -o $@ $(LIBS)
	
tests/filebuffers: $(TOBJDIR)/filebuffers.o $(TCOBJS) 
	gcc $(TOBJDIR)/filebuffers.o $(TCOBJS) -o $@ $(LIBS)
	
tests/fileparse: $(TOBJDIR)/fileparse.o $(TCOBJS) 
	gcc $(TOBJDIR)/fileparse.o $(TCOBJS) -o $@ $(LIBS)
	
tests/api_read_calls: $(TOBJDIR)/common.o $(TOBJDIR)/api_read_calls.o $(TCOBJS) 
	gcc $(TOBJDIR)/common.o $(TOBJDIR)/api_read_calls.o $(TCOBJS) -o $@ $(LIBS)
	
tests/api_write_calls: $(TOBJDIR)/common.o $(TOBJDIR)/api_write_calls.o $(TCOBJS) 
	gcc $(TOBJDIR)/common.o $(TOBJDIR)/api_write_calls.o $(TCOBJS) -o $@ $(LIBS)
	
tests/api_file_calls: $(TOBJDIR)/common.o $(TOBJDIR)/api_file_calls.o $(TCOBJS) 
	gcc $(TOBJDIR)/common.o $(TOBJDIR)/api_file_calls.o $(TCOBJS) -o $@ $(LIBS)
	
tests/api_other_calls: $(TOBJDIR)/common.o $(TOBJDIR)/api_other_calls.o $(TCOBJS) 
	gcc $(TOBJDIR)/common.o $(TOBJDIR)/api_other_calls.o $(TCOBJS) -o $@ $(LIBS)
	
$(TCOBJDIR)/%.o : %.c | $(TCOBJDIR)
	gcc -c $(DCFLAGS) $(COMMON_OPTIONS) -MMD -MP $< -o $@
	
$(TOBJDIR)/%.o : tests/%.c | $(TOBJDIR)
	gcc -c $(DCFLAGS) $(COMMON_OPTIONS) -MMD -MP $< -o $@

-include $(TCOBJS:.o=.d)
-include $(TESTS_OBJS:.o=.d)
-include $(TOBJDIR)/common.d

$(DOBJDIR) $(DOBJLIBDIR) $(ROBJDIR) $(ROBJLIBDIR) $(TCOBJDIR) $(TOBJDIR):
	mkdir -p $@

test:	$(TESTS_RUN)

t_%:
	$(patsubst t_%,./tests/%,$@)

install:
	mkdir -p /var/lib/rc-singularity
	mkdir -p /etc/rc-singularity
	mkdir -p /usr/local/nic/rc-singularity/lib
	mkdir -p /usr/local/nic/rc-singularity/bin
	mkdir -p /usr/local/nic/rc-singularity/debug
	mkdir -p /usr/local/nic/rc-singularity/include
	install -m755 lib/librc_singularity.so /usr/local/nic/rc-singularity/lib/librc_singularity.so
	install -m755 lib/librc_singularity_deb.so /usr/local/nic/rc-singularity/lib/librc_singularity_deb.so
	install -m755 bin/sing_handler /usr/local/nic/rc-singularity/bin/sing_handler
	install -m755 debug/sing_handler /usr/local/nic/rc-singularity/debug/sing_handler
	install -m644 rc_singularity.h /usr/local/nic/rc-singularity/include/rc_singularity.h
	install -m644 singularity.cnf /etc/rc-singularity/singularity.cnf
	chmod 777 /var/lib/rc-singularity

clean:
	rm -f bin/*
	rm -f debug/*
	rm -f lib/*
	rm -rf obj/*
	rm -f $(patsubst t_%,./tests/%,$(TESTS_RUN))
