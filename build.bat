SET APP=swtormousedroid.exe
SET DLL=rc\tor.bin
SET MH=src\minhook-master\src
SET CFLAGS=-march=i686 -m32 -O3 -s

del %APP% %DLL%

gcc %CFLAGS% -shared -fno-exceptions src\dll.c src\log.c %MH%\buffer.c %MH%\trampoline.c %MH%\hook.c %MH%\hde\hde32.c src\d3d.cpp src\torui\torui.a -static-libgcc -lkernel32 -ld3d9 -lD3dx9 -o %DLL%
windres rc/rs.rs -O coff --target=pe-i386 -o smd.res
gcc %CFLAGS% src\main.c src\smd.c src\io.c src\gui.c src\cntx.c src\log.c smd.res -lmsimg32 -lgdi32 -lwinmm -lpsapi -lkernel32 -o %APP%
