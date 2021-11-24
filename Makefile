RELEASE = -O3
LOGFILE = -DFILE_LOG="debug.log"
DEBUG = -g -DMEMORY_CHECK -DLOG_OPERATION -DLOG_MEMORY -DLOG_LOCKS
COMMON_OPTIONS = -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LIBS = -lrt -lpthread
BASE_SET = pipelines.c config.c codec.c index.c cpages.c filebuf.c fileparse.c utils.c locks.c keyhead.c allocator.c rc_singularity.c
BASE_INCLUDE = defines.h logbin.h pipelines.h config.h codec.h index.h cpages.h filebuf.h fileparse.h utils.h locks.h keyhead.h allocator.h 

.PHONY: all test autotests

all: debug release autotests

deb_lib:
	gcc $(DEBUG) $(COMMON_OPTIONS) $(BASE_SET) -shared -fPIC -o lib/librc_singularity_deb.so $(LIBS)

rel_lib:
	gcc $(RELEASE) $(COMMON_OPTIONS) $(BASE_SET) -shared -fPIC -o lib/librc_singularity.so $(LIBS)

debug: deb_lib
	gcc $(DEBUG) handler_config.c sing_handler.c -o debug/sing_handler -Llib $(LIBS) -lrc_singularity_deb -Wl,-rpath='/usr/local/nic/rc-singularity/lib/'

release: rel_lib
	gcc $(RELEASE) handler_config.c sing_handler.c -o bin/sing_handler -Llib $(LIBS) -lrc_singularity -Wl,-rpath='/usr/local/nic/rc-singularity/lib/'

memory_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/common.c tests/memory.c $(BASE_SET) -o tests/memory $(LIBS)
keyheads_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/common.c tests/keyheads.c $(BASE_SET) -o tests/keyheads $(LIBS)
index_ops_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/common.c tests/index_ops.c $(BASE_SET) -o tests/index_ops $(LIBS)
codec_ops_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/codec_ops.c $(BASE_SET) -o tests/codec_ops $(LIBS)
locks_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/locks.c $(BASE_SET) -o tests/locks $(LIBS)
filebuf_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/filebuffers.c $(BASE_SET) -o tests/filebuffers $(LIBS)
fileparse_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/fileparse.c $(BASE_SET) -o tests/fileparse $(LIBS)
api_read_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/common.c tests/api_read_calls.c $(BASE_SET) -o tests/api_read_calls $(LIBS)
api_write_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/common.c tests/api_write_calls.c $(BASE_SET) -o tests/api_write_calls $(LIBS)
api_file_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/common.c tests/api_file_calls.c $(BASE_SET) -o tests/api_file_calls $(LIBS)
api_other_tests:
	gcc $(DEBUG) $(COMMON_OPTIONS) tests/common.c tests/api_other_calls.c $(BASE_SET) -o tests/api_other_calls $(LIBS)

autotests: memory_tests keyheads_tests index_ops_tests codec_ops_tests locks_tests filebuf_tests fileparse_tests api_read_tests api_write_tests api_file_tests api_other_tests
	
test:
	./tests/memory
	./tests/keyheads
	./tests/index_ops
	./tests/codec_ops
	./tests/locks
	./tests/filebuffers
	./tests/fileparse
	./tests/api_read_calls
	./tests/api_write_calls
	./tests/api_file_calls
	./tests/api_other_calls

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
	rm -f tests/memory tests/keyheads tests/index_ops tests/codec_ops tests/locks tests/api_read_calls tests/api_write_calls tests/api_file_calls tests/api_other_calls
