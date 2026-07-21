@echo off
cd /d "%~dp0"
set MINGW=C:\mingw64\bin
set GPP=%MINGW%\g++.exe
set GLAD=deps\glad
set IMGUI=deps\imgui\imgui-1.91.8
set GLFW=deps\glfw\glfw-3.4.bin.WIN64
set GLM=deps\glm\glm-1.0.1
set STB=deps\stb
set FLAGS=-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -mwindows -static-libgcc -static-libstdc++ -DNDEBUG
set INC=-I"%GLFW%\include" -I"%GLAD%\include" -I"%IMGUI%" -I"%GLM%" -I"%STB%"
set LIB=-L"%GLFW%\lib-mingw-w64" -lglfw3 -lopengl32 -lgdi32 -lshell32 -lcomdlg32

echo [1/6] ImGui...
"%GPP%" %FLAGS% -c "%IMGUI%\imgui.cpp" -o imgui.o
"%GPP%" %FLAGS% -c "%IMGUI%\imgui_draw.cpp" -o imgui_draw.o
"%GPP%" %FLAGS% -c "%IMGUI%\imgui_tables.cpp" -o imgui_tables.o
"%GPP%" %FLAGS% -c "%IMGUI%\imgui_widgets.cpp" -o imgui_widgets.o
"%GPP%" %FLAGS% -c "%IMGUI%\imgui_impl_glfw.cpp" -o imgui_impl_glfw.o
"%GPP%" %FLAGS% -c "%IMGUI%\imgui_impl_opengl3.cpp" -o imgui_impl_opengl3.o
if errorlevel 1 (echo FAIL ImGui & exit /b 1)

echo [2/6] GLAD...
"%GPP%" %FLAGS% -c "%GLAD%\src\gl.c" -o gl.o
if errorlevel 1 (echo FAIL GLAD & exit /b 1)

echo [3/6] OCP core...
"%GPP%" %FLAGS% %INC% -c src\mesh.cpp -o mesh.o
if errorlevel 1 (echo FAIL mesh & exit /b 1)
"%GPP%" %FLAGS% %INC% -c src\scene.cpp -o scene.o
if errorlevel 1 (echo FAIL scene & exit /b 1)
"%GPP%" %FLAGS% %INC% -c src\camera.cpp -o camera.o
if errorlevel 1 (echo FAIL camera & exit /b 1)
"%GPP%" %FLAGS% %INC% -c src\primitives.cpp -o primitives.o
if errorlevel 1 (echo FAIL primitives & exit /b 1)

echo [4/6] OCP renderer...
"%GPP%" %FLAGS% %INC% -c src\renderer.cpp -o renderer.o
if errorlevel 1 (echo FAIL renderer & exit /b 1)

echo [5/6] OCP AI + exporters + main...
"%GPP%" %FLAGS% %INC% -c src\ai_engine.cpp -o ai_engine.o
if errorlevel 1 (echo FAIL ai_engine & exit /b 1)
"%GPP%" %FLAGS% %INC% -c src\exporters.cpp -o exporters.o
if errorlevel 1 (echo FAIL exporters & exit /b 1)
"%GPP%" %FLAGS% %INC% -c src\main.cpp -o main.o
if errorlevel 1 (echo FAIL main & exit /b 1)

echo [6/6] Linking...
"%GPP%" %FLAGS% -o OCP.exe imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_impl_glfw.o imgui_impl_opengl3.o gl.o mesh.o scene.o camera.o primitives.o renderer.o ai_engine.o exporters.o main.o %LIB% -Wl,--allow-multiple-definition
if errorlevel 1 (echo FAIL link & exit /b 1)

echo.
echo BUILD OK
dir OCP.exe
del /q *.o 2>nul
