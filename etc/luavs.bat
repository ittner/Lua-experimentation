rem script to build LuaJIT under "Visual Studio .NET Command Prompt".
rem do not run it from this directory, run it from the toplevel: etc\luavs.bat
rem it creates lua51.dll, lua51.lib and luajit.exe in src.

cd src
cl /MD /O2 /W3 /c /D_CRT_SECURE_NO_DEPRECATE /D_CRT_NONSTDC_NO_DEPRECATE /DLUA_BUILD_AS_DLL /I "..\dynasm" l*.c
del lua.obj luac.obj
link /DLL /out:lua51.dll l*.obj
cl /MD /O2 /W3 /c /D_CRT_SECURE_NO_DEPRECATE /D_CRT_NONSTDC_NO_DEPRECATE /DLUA_BUILD_AS_DLL lua.c
link /out:luajit.exe lua.obj lua51.lib
del *.obj
cd ..
