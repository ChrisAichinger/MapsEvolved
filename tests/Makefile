# Object files for all .cpp files in OutdoorMapper (i.e. the code to test)
REQ_OBJS=$(odm_src_targets)

# Force compiler and linker output into $(build_path)
# Disable incremental linking to avoid .ilk files
LD_OUTPUT_OPTS=/INCREMENTAL:NO /PDB:"$(build_path)\\"         \
               /MANIFESTFILE:"$(build_path)\\$(@B).manifest"
CPP_OUTPUT_OPTS=/Fo"$(build_path)\\" /Fd"$(build_path)\\"

# Commandlines for compiler and linker
DEPENDENCIES=user32.lib gdi32.lib $(extra_lib_dependencies)
LDFLAGS=/SUBSYSTEM:CONSOLE /DEBUG $(LD_OUTPUT_OPTS) $(extra_library_dirs)

DEFINES=/D "WIN32" /D "STRICT" /D "_DEBUG" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE"
CPPFLAGS=/nologo /EHsc /MDd /Od /Oy- /W3 /WX /Zi /GS /I$(odm_path) \
	 $(DEFINES) $(CPP_OUTPUT_OPTS) $(extra_include_dirs)
	 

# Guard against direct usage of this makefile with nmake
!IFNDEF odm_path
!message Run ./test.sh <target> instead of nmake
!message ---------------------------------------
!message Since MSVC's nmake is very limited, we rely on test.sh to pre-set a
!message few variables. Thus this makefile can't work standalone and needs to
!message be invoked from test.sh.
!message - Usage: ./test.sh            - Build and run tests
!message -        ./test.sh clean all  - Clean first, then build and run tests
!message
!ERROR Run ./test.sh <target> instead of nmake
!ENDIF

# Rules

all: test

test: executables
	test_util.exe
	pause

show: ..\tests\test_util.cpp
	@echo Filename parts: %|dF %|dpfF %s
	@echo -
	@echo Macros: $(**) -- $(**R:test=TEST) -- basename: $(@B)
	@echo -
	@echo odm_src_targets: $(odm_src_targets)
	pause

clean:
	del *.exe
	del /Q $(build_path)
	-rmdir $(build_path)

executables: $(build_path) $(REQ_OBJS) test_util.exe

$(build_path):
	mkdir $(build_path)

.cpp.exe:
	$(CPP) $< $(CPPFLAGS) /link $(LDFLAGS) $(DEPENDENCIES) $(REQ_OBJS)

{$(odm_path)}.cpp.obj:
	$(CPP) -c $< $(CPPFLAGS)

# nmake.exe documentation cheat sheet
# Filename macros:
#   $@  Current targets full name
#   $$@ Current target's full name (path, base name, extension)
#       Valid only as a dependent in a dependency.
#   $*  Current target's path and base name minus file extension.
#   $** All dependents of the current target.
#   $?  All dependents with a later timestamp than the target.
#   $<  Dependent file with a later timestamp than the target.
#       Valid only in commands in inference rules.
#   Modifier:
#     D  Drive + Dir
#     B  Base name
#     F  Base name + extension
#     R  Drive + Dir + Base name
#   Modifier usage: $(@B) -> Base name of current target
#
# Macro substitution:
#   $(macro:needle=repl)
#   Expands macro and replaces all occurrences of 'needle' with 'repl'
#
# Filename parts syntax: Access to first dependent filename
#   %s             Full name
#   %|F            Full name
#   %|<modifier>F  Parts of the full name
#   Modifier:
#     d drive
#     P path
#     f base name
#     e extension
#   Modifiers can be combined: %|dPfF -> Drive+Path+Basename

