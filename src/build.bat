SET APP=..\swtormousedroid51.exe
SET DLL=rc\smd5.bin
SET MH=minhook-master\src
SET CFLAGS=-march=x86-64 -O3 -s -mwindows
SET CROSS=C:\x86_64-15.2.0-release-win32-seh-msvcrt-rt_v13-rev0\mingw64\bin\
SET GCC=%CROSS%gcc
SET G++=%CROSS%g++

del %APP% %DLL%
del *.o

%GCC% %CFLAGS% -shared -fno-exceptions dll.c log.c %MH%\buffer.c %MH%\trampoline.c %MH%\hook.c %MH%\hde\hde64.c d3d.cpp -static-libgcc -lkernel32 -ld3d9 -lD3dx9 -o %DLL%
%CROSS%windres rc/rs.rs -O coff --target=pe-x86-64 -o smd.res

%GCC% %CFLAGS% -c main.c smd.c io.c log.c
%G++% %CFLAGS% -std=c++2a -c gui.cpp cntx.cpp
%G++% %CFLAGS% *.o smd.res -static-libgcc -static-libstdc++ -lshlwapi -lmsimg32 -lgdi32 -lgdiplus -lwinmm -lpsapi -lkernel32 -o %APP%