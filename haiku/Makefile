# Hedgewars for Haiku, without SDL. Run on Haiku from the top of the tree:
#   make -f haiku/Makefile            -> haiku/build/Hedgewars/{hwengine,lib/}
# Needs: llvm19_clang mesa_devel glu_devel ffmpeg6_devel rust_bin (cargo); fpc only for make compare
#
# On VitruvianOS (a BeOS-like OS based on Linux) the same tree builds against its kits,
# found through `pkg-config vos` (a link-time SDK's env.sh, haiku/vos/make-sdk.sh).
#
# The engine is built from haiku/engine-c, C generated from hedgewars/*.pas by
# upstream's pas2c (haiku/tools/gen-engine-c.sh, run on a system with GHC).

B        := haiku/build
APP      := $(B)/Hedgewars
LIB      := $(APP)/lib
OBJ      := $(B)/obj
DATADIR  ?= $(CURDIR)/share/hedgewars
CXX      ?= g++
CC       ?= gcc
HEADERS  := /boot/system/develop/headers

ifeq ($(shell pkg-config --exists vos 2>/dev/null && echo yes),yes)
# VitruvianOS: its kits on Linux; sockets are libc's, and the engine finds lib/ through
# $$ORIGIN (Haiku's runtime_loader looks in the app's lib/ by itself)
VOS_INC      := $(shell pkg-config --variable=includedir vos)
VOS_CFLAGS   := $(shell pkg-config --cflags vos) -I$(VOS_INC)/private/interface -I$(VOS_INC)/private/shared
VOS_LDFLAGS  := $(shell pkg-config --libs vos) -Wl,-rpath-link=$(shell pkg-config --variable=libdir vos)
HWHAIKU_LIBS := $(VOS_LDFLAGS) -ltranslation -lgame -lopengl -lmedia2 -lGL -lpthread
LAUNCH_LIBS  := $(VOS_LDFLAGS) -ltranslation -ltracker -lcrypto -lcurl -lz -lpthread
PHYSFS_BEOS  :=
RPATH_LIB    := -Wl,-rpath,'$$ORIGIN'
RPATH_APP    := -Wl,-rpath,'$$ORIGIN/lib'
MIMESET      := true
else
HWHAIKU_LIBS := -lbe -lgame -lGL -lmedia -ltranslation -ldevice -lnetwork
LAUNCH_LIBS  := -lbe -lnetwork -ltranslation -ltracker -lcolumnlistview -lcrypto -lcurl -lz
VOS_CFLAGS   := -I$(HEADERS)/os/opengl -I$(HEADERS)/private/interface
PHYSFS_BEOS  := misc/libphysfs/platform_beos.cpp
MIMESET      := mimeset -f
endif

HWHAIKU_SRCS := $(wildcard haiku/hwhaiku/*.cpp)
LUA_SRCS     := $(wildcard misc/liblua/*.c)
PHYS_SRCS    := $(addprefix misc/libphyslayer/,physfscompat.c physfsrwops.c physfslualoader.c hwpacksmounter.c)
# the bundled PhysFS 2.1: the engine builds paths like "//x.lua", which 3.x rejects
PHYSFS_SRCS  := $(addprefix misc/libphysfs/,physfs.c physfs_byteorder.c physfs_unicode.c \
	platform_posix.c platform_unix.c) $(wildcard misc/libphysfs/archiver_*.c)
ENGINE_SRCS  := $(wildcard hedgewars/*.pas) hedgewars/SDLh.pas
# the C engine and upstream's stand-in for Free Pascal's runtime; the runtime
# overloads C functions (__attribute__((overloadable))), which needs clang
ENGINE_C     := $(wildcard haiku/engine-c/*.c) $(wildcard project_files/hwc/rtl/*.c)
ENGINE_OBJS  := $(patsubst %.c,$(OBJ)/engine/%.o,$(notdir $(ENGINE_C)))
ENGINE_CC    ?= clang
ENGINE_LIBS  := $(LIB)/libhwhaiku.so $(LIB)/liblua.so $(LIB)/libphyslayer.so \
	$(LIB)/libhwengine_future.so $(LIB)/libphysfs.so $(LIB)/libavwrapper.so

all: $(APP)/hwengine $(APP)/Hedgewars

$(LIB)/libhwhaiku.so: $(HWHAIKU_SRCS) haiku/hwhaiku/hwhaiku.h
	@mkdir -p $(LIB)
	$(CXX) -O2 -g -std=c++17 -Wall -fPIC -shared -fvisibility=hidden \
		$(VOS_CFLAGS) -o $@ $(HWHAIKU_SRCS) $(HWHAIKU_LIBS) $(RPATH_LIB)

# upstream's video encoder, on the system's ffmpeg (Haiku's media plugin uses it too)
$(LIB)/libavwrapper.so: hedgewars/avwrapper/avwrapper.c
	@mkdir -p $(LIB)
	$(CC) -O2 -fPIC -shared -w -o $@ $< -lavcodec -lavformat -lavutil -lswresample

# LuaPas links both -llua and libhwlua; one soname keeps it to one copy
$(LIB)/liblua.so: $(LUA_SRCS)
	@mkdir -p $(LIB)
	$(CC) -O2 -fPIC -shared -DLUA_USE_POSIX -Wl,-soname,libhwlua.so -o $(LIB)/libhwlua.so $(LUA_SRCS) -lm
	ln -sf libhwlua.so $@

$(LIB)/libphysfs.so: $(PHYSFS_SRCS) $(PHYSFS_BEOS)
	@mkdir -p $(OBJ)/physfs $(LIB)
ifneq ($(PHYSFS_BEOS),)
	$(CXX) -O2 -fPIC -fpermissive -w -c -o $(OBJ)/physfs/platform_beos.o $(PHYSFS_BEOS)
	$(CC) -O2 -fPIC -fsigned-char -shared -DPHYSFS_SUPPORTS_7Z=0 -Wl,-soname,libphysfs.so \
		-o $@ $(PHYSFS_SRCS) $(OBJ)/physfs/platform_beos.o -lbe -lstdc++
else
	$(CC) -O2 -fPIC -fsigned-char -shared -DPHYSFS_SUPPORTS_7Z=0 -Wl,-soname,libphysfs.so \
		-o $@ $(PHYSFS_SRCS) -lpthread
endif

$(LIB)/libphyslayer.so: $(PHYS_SRCS) $(LIB)/liblua.so $(LIB)/libhwhaiku.so $(LIB)/libphysfs.so
	$(CC) -O2 -fPIC -shared -Ihaiku/include -Imisc/liblua -Imisc/libphysfs -o $@ $(PHYS_SRCS) \
		-L$(LIB) -llua -lhwhaiku -lphysfs $(RPATH_LIB)

$(LIB)/libhwengine_future.so: $(shell find rust -name '*.rs' -o -name Cargo.toml)
	@mkdir -p $(LIB)
	cd rust/lib-hwengine-future && cargo build --release
	cp rust/target/release/libhwengine_future.so $@

$(OBJ)/config.inc: hedgewars/config.inc.in haiku/Makefile
	@mkdir -p $(OBJ)
	sed -e 's|$${HEDGEWARS_PROTO_VER}|60|' -e 's|$${HEDGEWARS_VERSION}|1.1.0-dev|' \
		-e 's|$${HEDGEWARS_REVISION}|haiku|' -e 's|$${HEDGEWARS_HASH}|haiku|' \
		-e 's|$${HEDGEWARS_FULL_DATADIR}|$(DATADIR)|' \
		-e 's|$${FONTS_DIRS_ARRAY}|array [0..1] of PChar = (nil, nil);|' $< > $@

vpath %.c haiku/engine-c project_files/hwc/rtl
# -w: the generated code is warning-heavy by nature
$(OBJ)/engine/%.o: %.c $(wildcard haiku/engine-c/*.h project_files/hwc/rtl/*.h)
	@mkdir -p $(OBJ)/engine
	$(ENGINE_CC) -c -O2 -std=gnu99 -w -DPAS2C -DGL_GLEXT_PROTOTYPES -Ihaiku/engine-c \
		-Iproject_files/hwc/rtl -Imisc/libphysfs -Imisc/libphyslayer -Imisc/liblua -o $@ $<

$(APP)/hwengine: $(ENGINE_OBJS) $(ENGINE_LIBS)
	$(ENGINE_CC) -o $@ $(ENGINE_OBJS) -L$(LIB) -lhwhaiku -lhwlua -lphysfs -lphyslayer \
		-lhwengine_future -lavwrapper -lGL -lm $(RPATH_APP)

# the Pascal engine, built with Free Pascal, only to compare the two (make compare)
$(B)/pascal/hwengine: $(ENGINE_SRCS) $(OBJ)/config.inc $(ENGINE_LIBS)
	@mkdir -p $(B)/pascal/obj
	fpc -O2 -Si -Xs -Cs2000000 -B -fPIC -dSKIP_RESOURCES -dLUA_INTERNAL \
		-Fi$(OBJ) -Fuhedgewars -FU$(B)/pascal/obj -Fl$(LIB) \
		-o$(CURDIR)/$@ hedgewars/hwengine.pas

$(APP)/Hedgewars: $(wildcard haiku/launcher/*.cpp haiku/launcher/*.h haiku/launcher/*.rdef) frontend-qt6/weapons.h \
		haiku/hwhaiku/rawkeys.h
	@mkdir -p $(OBJ)
	$(CXX) -O2 -g -std=c++17 -Wall -Ifrontend-qt6 $(VOS_CFLAGS) -DHW_DATADIR='"$(DATADIR)"' \
		-o $@ haiku/launcher/*.cpp $(LAUNCH_LIBS)
	rc -I . -o $(OBJ)/Hedgewars.rsrc haiku/launcher/*.rdef	# -I: where import finds files
	xres -o $@ $(OBJ)/Hedgewars.rsrc
	$(MIMESET) $@

# host tests (surface code, stats parsing, INI files, drawn maps) run anywhere with a C++ compiler
ifneq ($(shell uname -s),Haiku)
TEST_FLAGS := -fsanitize=address,undefined	# not available on Haiku
endif
test:
	@mkdir -p $(OBJ)
	$(CXX) -std=c++17 -Wall -g $(TEST_FLAGS) -Ihaiku/hwhaiku \
		-o $(OBJ)/surface_test haiku/tests/surface_test.cpp haiku/hwhaiku/surface.cpp
	$(OBJ)/surface_test
	$(CXX) -std=c++17 -Wall -g $(TEST_FLAGS) -Ihaiku/launcher \
		-o $(OBJ)/stats_test haiku/tests/stats_test.cpp haiku/launcher/StatsData.cpp
	$(OBJ)/stats_test
	$(CXX) -std=c++17 -Wall -g $(TEST_FLAGS) -Ihaiku/launcher -Ifrontend-qt6 \
		-o $(OBJ)/net_test haiku/tests/net_test.cpp haiku/launcher/NetProto.cpp \
		haiku/launcher/SchemeData.cpp haiku/launcher/DrawnMap.cpp -lz
	$(OBJ)/net_test
	$(CXX) -std=c++17 -Wall -g $(TEST_FLAGS) -Ihaiku/launcher \
		-o $(OBJ)/content_test haiku/tests/content_test.cpp haiku/launcher/ContentData.cpp
	$(OBJ)/content_test
	$(CXX) -std=c++17 -Wall -g $(TEST_FLAGS) -Ihaiku/launcher -o $(OBJ)/ini_test haiku/tests/ini_test.cpp \
		haiku/launcher/Ini.cpp haiku/launcher/Binds.cpp haiku/launcher/DrawnMap.cpp -lz
	$(OBJ)/ini_test
	$(CXX) -std=c++17 -Wall -g $(TEST_FLAGS) -Ihaiku/launcher -Ifrontend-qt6 \
		-o $(OBJ)/scheme_test haiku/tests/scheme_test.cpp haiku/launcher/SchemeData.cpp
	$(OBJ)/scheme_test
	$(CXX) -std=c++17 -Wall -g $(TEST_FLAGS) -Ihaiku/launcher \
		-o $(OBJ)/datafs_test haiku/tests/datafs_test.cpp haiku/launcher/DataFS.cpp -lz
	$(OBJ)/datafs_test
ifeq ($(shell uname -s),Haiku)
	$(CXX) -std=c++17 -Wall -Ihaiku/launcher -o $(OBJ)/progress_test haiku/tests/progress_test.cpp \
		haiku/launcher/Game.cpp haiku/launcher/Settings.cpp haiku/launcher/BindEditor.cpp \
		haiku/launcher/Binds.cpp haiku/launcher/Ini.cpp haiku/launcher/DataFS.cpp -lbe -lnetwork -lz
	$(OBJ)/progress_test
endif

# the C engine must play exactly like the Pascal one (or online games go out of sync)
compare: $(APP)/hwengine $(B)/pascal/hwengine
	cp $(B)/pascal/hwengine $(APP)/hwengine-pascal	# beside lib/, where it finds its libraries
	sh haiku/tests/compare-engines.sh $(APP)/hwengine $(APP)/hwengine-pascal; s=$$?; \
		rm -f $(APP)/hwengine-pascal; exit $$s

clean:
	rm -rf $(B)

.PHONY: all test compare clean
