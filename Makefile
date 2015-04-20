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
OFILES = $(sources:.cpp=.$(OBJEXT)) __build/smartptr_proxy.$(OBJEXT)
HFILES = $(headers) src_sip/smartptr_proxy.h Makefile


all: $(TARGET) pymaplib/maplib_sip.pyd pymaplib/OutdoorMapper.$(SHLIBEXT) pymaplib/csv

$(OFILES): $(HFILES)

%.$(OBJEXT): %.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $(CPPDEBUG) $(CPPOUT)$@ $<

$(BUILDDIR)/%.$(OBJEXT): src_sip/%.cpp
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) $(CPPDEBUG) $(CPPOUT)$@ $<

pymaplib/maplib_sip.pyd: __build/maplib_sip.pyd
	$(CP) $< $@

pymaplib/OutdoorMapper.$(SHLIBEXT): $(ODMPATH)/OutdoorMapper.$(SHLIBEXT)
	$(CP) $(wildcard $(ODMPATH)/*.$(SHLIBEXT)) $(dir $@).

pymaplib/csv: $(ODMPATH)/csv
	$(CPR) $< $@

# We need this ugly contraption because
# - Windows limits command line length, so we must use the @commandfile
#   argument to $(LINK) to fit all our libs.
# - GNU Make's $(file ...) function doesn't seem to work on Windows.
# - dd count=0 is a substitute for echo, which on Windows calls the cmd.exe
#   echo, which tells prints "Echo OFF" into $@.linkargs.
#   Basically, we create an empty file or truncate it if it already exists.
# - Then we iterate over all our args and echo them into our file.
# - The final echo terminates the && chain.
$(TARGET): $(ODMPATH)/OutdoorMapper.$(LIBEXT) $(OFILES)
	dd count=0 > $@.linkargs
	$(foreach lib,$(OFILES) $(LIBS),echo $(lib) >> $@.linkargs && ) echo
	$(LINK) $(LDFLAGS) $(LDDEBUG) $(LDOUT)$(TARGET) @$@.linkargs
	$(RM) $@.linkargs
	$(MANIFEST)

clean:
	-$(RMRF) $(BUILDDIR)
	-$(RMRF) pymaplib/csv
	-$(RM) pymaplib/maplib_sip.pyd
	-$(RM) pymaplib/*.$(SHLIBEXT)
