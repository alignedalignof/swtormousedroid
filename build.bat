SET APP=swtormousedroid.exe
SET DLL=smd.dll
SET MH=src\minhook-master\src
SET CFLAGS= -march=i686 -m32 -O3 -s

del %APP% %DLL%
gcc %CFLAGS% src\main.c src\smd.c src\log.c -lwinmm -lpsapi -lkernel32 -o %APP%
gcc %CFLAGS% -shared -fno-exceptions src\dll.c src\log.c %MH%\buffer.c %MH%\trampoline.c %MH%\hook.c %MH%\hde\hde32.c src\d3d.cpp src\uiscan\uiscan.a -static-libgcc -lkernel32 -ld3d9 -lD3dx9 -o %DLL%
rcedit %APP% --set-icon rc/mouse.ico