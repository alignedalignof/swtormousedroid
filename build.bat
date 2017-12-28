del SWTORMOUSEDROID*.EXE SMD.DLL
SET MH=src\minhook-master\src
SET CFLAGS=-m32 -O3 -s
gcc %CFLAGS% src\main.c -lwinmm -lpsapi -lkernel32 -o SWTORMOUSEDROID.EXE
gcc %CFLAGS% -shared src\dll.c src\d3d.cpp %MH%\buffer.c %MH%\trampoline.c %MH%\hook.c %MH%\hde\hde32.c -lkernel32 -ld3d9 -lstdc++ -o SMD.DLL
