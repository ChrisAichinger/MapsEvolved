BUILDDIR = __build
TARGET = $(BUILDDIR)/maplib_sip.pyd
SBF_FILE = $(BUILDDIR)/maps.sbf

ifeq (0,1)
# We need the advanced features of GNU Make.
# Guard against accidently trying to make with nmake on Windows.
!ERROR This Makefile requires GNU Make.
$(error This Makefile requires GNU Make.)
endif

ifeq ("$(wildcard $(SBF_FILE))","")
$(error SIP must be run before invoking make (use 'invoke build'))
endif
include $(SBF_FILE)

ifeq ($(OS),Windows_NT)
    include Makefile.win32
else
    $(error This platform is currently not supported)
endif

# Object files and headers. Add Makefile to headers to trigger a full rebuild
# if the Makefile is changed..
OFILES = $(sources:.cpp=.$(OBJEXT)) $(BUILDDIR)/smartptr_proxy.$(OBJEXT)
HFILES = $(headers) src_sip/smartptr_proxy.h Makefile


all: $(TARGET)

$(OFILES): $(HFILES)

%.$(OBJEXT): %.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $(CPPDEBUG) $(CPPOUT)$@ $<

$(BUILDDIR)/%.$(OBJEXT): src_sip/%.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $(CPPDEBUG) $(CPPOUT)$@ $<

# $(LINK) on Windows can't deal with long commandlines, this seems to work.
# Our make version doesn't support the $(file ..) function, unfortunately.
$(TARGET): $(PYMAPLIB_CPP_LIB) $(OFILES)
	echo $(OFILES) $(LIBS) > $@.linkargs
	$(LINK) $(LDFLAGS) $(LDDEBUG) $(LDOUT)$(TARGET) @$@.linkargs
	$(MANIFEST)

clean:
	-$(RMRF) $(BUILDDIR)
