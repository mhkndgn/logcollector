bin_PROGRAMS = logcollector

logcollector_SOURCES = logcollector.c common.h net.c cfg.c cfg.h config-handlers.c config-handlers.h ring_c11_mem.h ring.c ring.h dynamic-string.c dynamic-string.h hash.c hash.h hmap.c hmap.h json.c json.h shash.c shash.h timeval.h unicode.h util.c util.h custom_functions.c custom_functions.h

logcollector_CFLAGS  = -pthread
logcollector_LDADD   = -lpthread

logcollector_LDADD  += @PCRE_LIBS@
logcollector_CFLAGS += @PCRE_CFLAGS@

logcollector_LDADD  += -luuid
