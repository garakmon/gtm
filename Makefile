.PHONY: all clean

all: release

clean:
	rm -rf build build_debug

release:
	qt-cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build --parallel

debug:
	export NINJA_STATUS="[%f/%t %p | %e s] "; \
	qt-cmake -G Ninja -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_COLOR_DIAGNOSTICS=ON; \
	cmake --build build_debug --parallel --verbose
