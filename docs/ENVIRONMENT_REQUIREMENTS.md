# ENVIRONMENT_REQUIREMENTS.md — Yuki_1.0

## Target Platform

| Property | Value |
|---|---|
| Operating System | Windows 10 / Windows 11 |
| Architecture | x64 |
| Compiler | MSVC (Microsoft Visual C++) |
| Build System | CMake |
| C++ Standard | C++17 |
| External Libraries | None (pure stdlib) |

---

## Required Tools

All three tools must be present and accessible from the command line before building.

### 1. MSVC — Microsoft C++ Compiler

- Installed via **Visual Studio 2019** or **Visual Studio 2022**
- Required workload: **"Desktop development with C++"**
- Must be invoked from the **x64 Native Tools Command Prompt for VS**  
  (or a terminal where `vcvarsall.bat amd64` has been run)

Verify:
```
cl
```
Expected output includes: `Microsoft (R) C/C++ Optimizing Compiler` and a version number.

### 2. CMake

- Minimum version: **3.20**
- Installed via Visual Studio installer or from https://cmake.org/download/
- Must be on the system `PATH`

Verify:
```
cmake --version
```
Expected output: `cmake version 3.20.x` or higher.

### 3. Git

- Any recent version
- Must be on the system `PATH`

Verify:
```
git --version
```
Expected output: `git version 2.x.x.windows.x`

---

## Build Environment Note

> **Always build from an x64 MSVC environment.**

Opening a standard `cmd.exe` or PowerShell without loading MSVC variables will cause
`cmake --build .` to fail with generator or compiler errors.

The correct environment is:
- **Start Menu → Visual Studio 20xx → x64 Native Tools Command Prompt for VS 20xx**
- Or run `vcvarsall.bat amd64` in your terminal before invoking cmake.

---

## Build Commands

Run these from the project root (`d:\Yuki_1.0\`):

```bat
mkdir build
cd build
cmake ..
cmake --build .
```

The output executable will be at:
```
build\Debug\yuki.exe
```

To run it:
```bat
build\Debug\yuki.exe
```

---

## Practical Environment Section

> This section is updated automatically by `scripts/collect_setup_info.bat`.
> Run that script and its output will be appended here.

<!--COLLECT_SETUP_INFO_OUTPUT_START-->
(Run scripts\collect_setup_info.bat to populate this section.)
<!--COLLECT_SETUP_INFO_OUTPUT_END-->
