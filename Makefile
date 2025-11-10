.PHONY: all clean

all: release

clean:
	rm -rf build build_debug

release:
	qt-cmake -G Ninja -S . -B build
	cmake --build build

debug:
	qt-cmake -G Ninja -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug
	cmake --build build_debug --verbose
