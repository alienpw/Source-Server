#
# gcc.rules.mk
#
# Should be included last, but before any rules overrides
#

######## Common targets

#all:

#.PHONY: kdocs

#kdocs:
#	-mkdir kdoc
#	-kdoc -d kdoc -n coldstore --strip-h-path *.hh

# the following format is not supported by FreeBSD.

.c.o:
	$(CC) -c $(DEFINES) $(INCLUDES) $(CFLAGS) $< -o $@

.cpp.o:
	$(CC) -c $(DEFINES) $(INCLUDES) $(CFLAGS) $< -o $@

.s.o:
	$(CC) -c $(DEFINES) $(INCLUDES) $(CFLAGS) $< -o $@

.cxx.o:
	$(CC) -c $(DEFINES) $(INCLUDES) $(CFLAGS) $< -o $@

%.o : %.cxx
	$(CC) -c $(DEFINES) $(INCLUDES) $(CFLAGS) $< -o $@

# rule for making executables
%: %.o
	$(LD) $(LDFLAGS) -rdynamic -o $@ $^ $(LDLIBS)

%.so :
	$(LD) $(LDFLAGS) -shared -o $@ $^ $(LDLIBS)

#%.a :
#	ar rsv $@ $^

$(AUTOGENS) : rpcalls.xml
	../rpc/rpcgen.pl rpcalls.xml

######## Less common targets

.PHONY: ftclean clean distclean
ftclean:
	rm -rf $(OBJS) $(EXES) $(CLEAN)

clean:
	rm -rf $(OBJS) $(EXES) $(CLEAN) $(DISTCLEAN) .depend

distclean:
	rm -rf $(OBJS) $(EXES) $(CLEAN) $(DISTCLEAN)

ifneq ($(wildcard .depend),)
include .depend
endif

depend: 
	-rm -rf .depend
	-($(foreach src,$(CXXSRC),$(CC) -MM -MT $(patsubst %.cxx, %.o, $(src)) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $(src) >> .depend;))
	-($(foreach src,$(CPPSRC),$(CC) -MM -MT $(patsubst %.cpp, %.o, $(src)) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $(src) >> .depend;))