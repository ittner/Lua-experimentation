cd src
cl /O2 /W3 /c /DLUA_BUILD_AS_DLL /I "..\dynasm" l*.c
del lua.obj luac.obj
link /DLL /out:lua51.dll l*.obj
cl /O2 /W3 /c /DLUA_BUILD_AS_DLL lua.c
link /out:luajit.exe lua.obj lua51.lib
cd ..
