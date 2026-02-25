.PHONY: all clean

all: release

clean:
	rm -rf build build_debug

release:
	qt-cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build --parallel

debug:
	export NINJA_STATUS="[$$(printf '\033[96m')%f/%t %p$$(printf '\033[0m') | %es] "; \
	qt-cmake -G Ninja -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_COLOR_DIAGNOSTICS=ON -DGTM_TIME_COMPILES=ON; \
	cmake --build build_debug --parallel
