@echo off


set bindir=..\dunger\bin\
set prjdir=..\..\
set srcdir=%prjdir%src\

if not exist %bindir% mkdir %bindir%
pushd %bindir%

set buildRelease=
set buildEng=
set buildD3D11=
set buildGL=
set buildMain=
set buildGame=
set compiler=

set stackSize=0x800000

:parseNextArg
if "%1" neq "" (
    if "%1" == "release" (
        set buildRelease=1
        shift & goto parseNextArg
    ) 
    if "%1" == "gcc" (
        set compiler=gcc
        shift & goto parseNextArg
    )
    if "%1" == "all" (
        set buildEng=1
        set buildD3D11=1
        set buildMain=1
        set buildGame=1
        shift & goto parseNextArg
    )
    if "%1" == "eng" (
        set buildEng=1
        shift & goto parseNextArg
    )
    if "%1" == "main" (
        set buildMain=1
        shift & goto parseNextArg
    )
    if "%1" == "game" (
        set buildGame=1
        shift & goto parseNextArg
    )
    if "%1" == "d3d11" (
        set buildD3D11=1
        shift & goto parseNextArg
    )
    if "%1" == "gl" (
        set buildGL=1
        shift & goto parseNextArg
    )
    shift & goto parseNextArg
)
if "%compiler%" == "gcc" (
    goto gcc
)
goto msvc

:gcc
set cFlags=-mavx2 -std=gnu++2a -fconcepts -lwinmm -ld3dcompiler -ld3d11 -ldxgi

if "%buildRelease%" == "1" (
    set cFlags=%cFlags% -Ofast
) else (
    set cFlags=%cFlags% -Og
)

if "%buildEng%" == "1" (
    echo building eng...
    g++ %srcdir%eng.cpp -shared -o eng.dll -D"BUILD_ENG" %cFlags% -Wl,--out-implib,eng.a
)
if %errorlevel% neq 0 goto fail

if "%buildD3D11%" == "1" (
    echo building r_d3d11...
    g++ %srcdir%r_d3d11.cpp -shared -o r_d3d11.dll -D"BUILD_RENDERER" -D"BUILD_D3D11" %cFlags% -L. -l:eng.a
)
if %errorlevel% neq 0 goto fail

if "%buildGame%" == "1" (
    echo building game...
    g++ %prjdir%dunger\src\game.cpp -shared -o game.dll -D"BUILD_GAME" %cFlags% -L. -l:eng.a -Wl,--out-implib,game.a
)
if %errorlevel% neq 0 goto fail

if "%buildMain%" == "1" (
    echo building main...
    g++ %srcdir%main.cpp -o main.exe %cFlags% -L. -l:game.a -l:eng.a
)
if %errorlevel% neq 0 goto fail

goto success

:msvc
set cFlags=/nologo /link
set runtime=/MD

if "%buildRelease%" == "1" (
    set cFlags=%runtime% /O2 /GL /D"BUILD_DEBUG=0" %cFlags% /LTCG
) else (
    set cFlags=%runtime%d /Od /D"BUILD_DEBUG=1" %cFlags%
)

set deps=user32.lib d3d11.lib d3dcompiler.lib dxgi.lib winmm.lib opengl32.lib gdi32.lib shell32.lib d3dx11.lib
set cFlags=/favor:INTEL64 /Z7 /GF /FC /Oi /EHa /fp:fast /std:c++latest /Wall /D"ENABLE_PROFILER=1" /GR- /GS- /Gs0x1000000 /wd4711 /wd4710 /wd4464 %cFlags% /STACK:%stackSize%,%stackSize% /LIBPATH:"%prjdir%dep\Microsoft DirectX SDK\Lib\x64\" /incremental:no %deps%
set cFlags=/arch:AVX2 %cFlags%

if "%buildEng%" == "1" (
    cl %srcdir%eng.cpp /D"BUILD_ENG" /LD %cFlags% /out:eng.dll
)
if %errorlevel% neq 0 goto fail

if "%buildD3D11%" == "1" (
    cl %srcdir%r_d3d11.cpp /D"BUILD_RENDERER" /D"BUILD_D3D11" /LD %cFlags% /out:r_d3d11.dll eng.lib
)
if %errorlevel% neq 0 goto fail

if "%buildGL%" == "1" (
    cl %srcdir%r_gl.cpp /D"BUILD_RENDERER" /D"BUILD_GL" /LD %cFlags% /out:r_gl.dll eng.lib
)
if %errorlevel% neq 0 goto fail

if "%buildGame%" == "1" (
    cl %prjdir%dunger\src\game.cpp /D"BUILD_GAME" /LD %cFlags% /out:game.dll eng.lib
)
if %errorlevel% neq 0 goto fail

if "%buildMain%" == "1" (
    cl %srcdir%main.cpp %cFlags% /out:main.exe game.lib eng.lib
)
if %errorlevel% neq 0 goto fail

goto success

:fail
echo [31mCompilation failed[0m
goto end
:success
echo [32mCompilation succeeded[0m
:end
popd