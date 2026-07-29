@echo off
rem Stage the WebView2 loader beside tiko.exe.
rem
rem AfxNova resolves the five CreateCoreWebView2* entrypoints at RUNTIME with DyLibLoad
rem on the plain name "WebView2Loader.dll" -- there is no import lib and no link flag --
rem so the DLL must sit in tiko's PROJECT ROOT, which is where -x ..\tiko.exe puts the
rem executable. It is NOT a bin\ file like Scintilla/Lexilla.
rem
rem 64-bit, to match fbc64. If it is missing, the Help Center reports the WebView2
rem Runtime as absent -- the loader failure and a genuinely missing Edge runtime are the
rem same E_POINTER as far as the wrapper is concerned.

copy /Y C:\dev\AfxNova\examples\WebView2\WebView2Loader_64.dll WebView2Loader.dll
