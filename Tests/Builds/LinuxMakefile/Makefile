# Automatically generated makefile, created by the Projucer
# Don't edit this file! Your changes will be overwritten when you re-save the Projucer project!

# build with "V=1" for verbose builds
ifeq ($(V), 1)
V_AT =
else
V_AT = @
endif

# (this disables dependency generation if multiple architectures are set)
DEPFLAGS := $(if $(word 2, $(TARGET_ARCH)), , -MMD)

ifndef PKG_CONFIG
  PKG_CONFIG=pkg-config
endif

ifndef STRIP
  STRIP=strip
endif

ifndef AR
  AR=ar
endif

ifndef CONFIG
  CONFIG=Debug
endif

JUCE_ARCH_LABEL := $(shell uname -m)

ifeq ($(CONFIG),Debug)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Debug
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DDEBUG=1" "-D_DEBUG=1" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=0.9.2" "-DJUCE_APP_VERSION_HEX=0x902" $(shell $(PKG_CONFIG) --cflags alsa) -pthread -I../../JuceLibraryCode -I../../../JuceLibraryCode/modules $(CPPFLAGS)
  JUCE_CPPFLAGS_CONSOLEAPP :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_CONSOLEAPP := RouteMIDITests

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -g -ggdb -O0 $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++17 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) $(shell $(PKG_CONFIG) --libs alsa) -fvisibility=hidden -latomic -lrt -ldl -lpthread $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_CONSOLEAPP) $(JUCE_OBJDIR)
endif

ifeq ($(CONFIG),Release)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Release
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DNDEBUG=1" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=0.9.2" "-DJUCE_APP_VERSION_HEX=0x902" $(shell $(PKG_CONFIG) --cflags alsa) -pthread -I../../JuceLibraryCode -I../../../JuceLibraryCode/modules $(CPPFLAGS)
  JUCE_CPPFLAGS_CONSOLEAPP :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_CONSOLEAPP := RouteMIDITests

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -O3 $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++17 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) $(shell $(PKG_CONFIG) --libs alsa) -fvisibility=hidden -latomic -lrt -ldl -lpthread $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_CONSOLEAPP) $(JUCE_OBJDIR)
endif

OBJECTS_CONSOLEAPP := \
  $(JUCE_OBJDIR)/ApplicationCommand_76f77315.o \
  $(JUCE_OBJDIR)/ApplicationState_c736e69b.o \
  $(JUCE_OBJDIR)/BitScaling_72e52674.o \
  $(JUCE_OBJDIR)/ChannelBucket_429cdbdd.o \
  $(JUCE_OBJDIR)/Conversion_b34d9fd0.o \
  $(JUCE_OBJDIR)/Latch_d69170d4.o \
  $(JUCE_OBJDIR)/Mono_cdd7cedd.o \
  $(JUCE_OBJDIR)/Mpe_31077172.o \
  $(JUCE_OBJDIR)/Route_503ad659.o \
  $(JUCE_OBJDIR)/ScriptMidiMessageClass_3119217.o \
  $(JUCE_OBJDIR)/ScriptOscClass_3ed124de.o \
  $(JUCE_OBJDIR)/ScriptUtilClass_da719b5b.o \
  $(JUCE_OBJDIR)/McpServer_7747aa0d.o \
  $(JUCE_OBJDIR)/Schema_68a49d5b.o \
  $(JUCE_OBJDIR)/TextMidi_ff6fa9e8.o \
  $(JUCE_OBJDIR)/TestRunner_6696edd7.o \
  $(JUCE_OBJDIR)/ScalingTests_c076264f.o \
  $(JUCE_OBJDIR)/TransformTests_ccb5580a.o \
  $(JUCE_OBJDIR)/FilterTests_ea1d09fe.o \
  $(JUCE_OBJDIR)/ConversionTests_5d376220.o \
  $(JUCE_OBJDIR)/ParsingTests_ed594946.o \
  $(JUCE_OBJDIR)/ScriptTests_bfe5520b.o \
  $(JUCE_OBJDIR)/MpeTests_c7653914.o \
  $(JUCE_OBJDIR)/ConnectionTests_32ca958.o \
  $(JUCE_OBJDIR)/include_juce_audio_basics_8a4e984a.o \
  $(JUCE_OBJDIR)/include_juce_audio_devices_63111d02.o \
  $(JUCE_OBJDIR)/include_juce_core_f26d17db.o \
  $(JUCE_OBJDIR)/include_juce_data_structures_7471b1e3.o \
  $(JUCE_OBJDIR)/include_juce_events_fd7d695.o \
  $(JUCE_OBJDIR)/include_juce_midi_ci_1fda4092.o \
  $(JUCE_OBJDIR)/include_juce_osc_f3df604d.o \

.PHONY: clean all strip

all : $(JUCE_OUTDIR)/$(JUCE_TARGET_CONSOLEAPP)

$(JUCE_OUTDIR)/$(JUCE_TARGET_CONSOLEAPP) : $(OBJECTS_CONSOLEAPP) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES)
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo >&2 "pkg-config not installed. Please, install it."; exit 1; }
	@$(PKG_CONFIG) --print-errors alsa
	@echo Linking "RouteMIDITests - ConsoleApp"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_CONSOLEAPP) $(OBJECTS_CONSOLEAPP) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(RESOURCES) $(TARGET_ARCH)

$(JUCE_OBJDIR)/ApplicationCommand_76f77315.o: ../../../Source/ApplicationCommand.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ApplicationCommand.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ApplicationState_c736e69b.o: ../../../Source/ApplicationState.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ApplicationState.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/BitScaling_72e52674.o: ../../../Source/BitScaling.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling BitScaling.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ChannelBucket_429cdbdd.o: ../../../Source/ChannelBucket.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ChannelBucket.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Conversion_b34d9fd0.o: ../../../Source/Conversion.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Conversion.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Latch_d69170d4.o: ../../../Source/Latch.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Latch.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Mono_cdd7cedd.o: ../../../Source/Mono.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Mono.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Mpe_31077172.o: ../../../Source/Mpe.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Mpe.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Route_503ad659.o: ../../../Source/Route.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Route.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ScriptMidiMessageClass_3119217.o: ../../../Source/ScriptMidiMessageClass.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ScriptMidiMessageClass.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ScriptOscClass_3ed124de.o: ../../../Source/ScriptOscClass.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ScriptOscClass.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ScriptUtilClass_da719b5b.o: ../../../Source/ScriptUtilClass.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ScriptUtilClass.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/McpServer_7747aa0d.o: ../../../Source/McpServer.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling McpServer.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Schema_68a49d5b.o: ../../../Source/Schema.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Schema.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/TextMidi_ff6fa9e8.o: ../../../Source/TextMidi.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling TextMidi.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/TestRunner_6696edd7.o: ../../TestRunner.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling TestRunner.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ScalingTests_c076264f.o: ../../ScalingTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ScalingTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/TransformTests_ccb5580a.o: ../../TransformTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling TransformTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/FilterTests_ea1d09fe.o: ../../FilterTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling FilterTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ConversionTests_5d376220.o: ../../ConversionTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ConversionTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ParsingTests_ed594946.o: ../../ParsingTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ParsingTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ScriptTests_bfe5520b.o: ../../ScriptTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ScriptTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/MpeTests_c7653914.o: ../../MpeTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling MpeTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ConnectionTests_32ca958.o: ../../ConnectionTests.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ConnectionTests.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_basics_8a4e984a.o: ../../JuceLibraryCode/include_juce_audio_basics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_basics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_devices_63111d02.o: ../../JuceLibraryCode/include_juce_audio_devices.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_devices.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_core_f26d17db.o: ../../JuceLibraryCode/include_juce_core.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_core.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_data_structures_7471b1e3.o: ../../JuceLibraryCode/include_juce_data_structures.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_data_structures.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_events_fd7d695.o: ../../JuceLibraryCode/include_juce_events.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_events.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_midi_ci_1fda4092.o: ../../JuceLibraryCode/include_juce_midi_ci.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_midi_ci.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_osc_f3df604d.o: ../../JuceLibraryCode/include_juce_osc.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_osc.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_CONSOLEAPP) $(JUCE_CFLAGS_CONSOLEAPP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/execinfo.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link libexecinfo"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/execinfo.x -lexecinfo - >/dev/null 2>&1 && printf -- "-lexecinfo" > "$@" || touch "$@"

$(JUCE_OBJDIR)/cxxfs.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link stdc++fs"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/cxxfs.x -lstdc++fs - >/dev/null 2>&1 && printf -- "-lstdc++fs" > "$@" || touch "$@"

clean:
	@echo Cleaning RouteMIDITests
	$(V_AT)$(CLEANCMD)

strip:
	@echo Stripping RouteMIDITests
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_CONSOLEAPP)

-include $(OBJECTS_CONSOLEAPP:%.o=%.d)
