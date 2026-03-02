# Install

`gtm` is currently only tested on macOS (version 14) so YMMV


## Requirements

- CMake 3.16 or newer
- A C++23-capable compiler
- Qt6 with the `Widgets` and `Svg` modules
- Ninja

Optional:
- Python 3 if you want to enable the debug-only `GTM_TIME_COMPILES` build timing option


## macOS

#### Install Qt6
```sh
brew install qt
```
or, see alternatives [here](https://doc.qt.io/qt-6/macos-installation.html)

#### Install CMake
```sh
brew install cmake
```
or, download directly [here](https://cmake.org/download/)


#### Install Ninja
```sh
brew install ninja
```
I don't know alternatives

#### Use a C++ toolchain that supports C++23

`gtm` uses a [Makefile](Makefile) to wrap the build commands, so building it is easy:

Debug build:

```sh
make debug
```

Release build:

```sh
make
```


## Windows

Typical setup:
- Install Qt 6
- Install CMake
- Install Ninja or use another supported generator

#### Use a C++ toolchain that supports C++23

`gtm` uses a [Makefile](Makefile) to wrap the build commands, so building it is easy:

Debug build:

```sh
make debug
```

Release build:

```sh
make
```

## Linux

Typical setup:
- Install Qt 6 development packages with `Widgets` and `Svg`
- Install CMake
- Install Ninja

#### Use a C++ toolchain that supports C++23

`gtm` uses a [Makefile](Makefile) to wrap the build commands, so building it is easy:

Debug build:

```sh
make debug
```

Release build:

```sh
make
```
