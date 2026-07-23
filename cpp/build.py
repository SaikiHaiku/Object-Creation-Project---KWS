import subprocess, os, sys

CPP = r"E:\Object Creation Project - KWS\cpp"
GPP = r"C:\mingw64_msvcrt\mingw64\bin\g++.exe"
DEPS = os.path.join(CPP, "deps")
GLAD = os.path.join(DEPS, "glad")
IMGUI = os.path.join(DEPS, "imgui", "imgui-1.91.8")
IMGUI_BB = os.path.join(IMGUI, "backends")
GLFW = os.path.join(DEPS, "glfw", "glfw-3.4.bin.WIN64")
GLM = os.path.join(DEPS, "glm", "glm-1.0.1")
STB = os.path.join(DEPS, "stb")
MINGW_BIN = r"C:\mingw64_msvcrt\mingw64\bin"

env = os.environ.copy()
env["PATH"] = MINGW_BIN + ";" + env.get("PATH", "")

FLAGS = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter",
         "-mwindows", "-static-libgcc", "-static-libstdc++", "-DNDEBUG"]
INC = [f"-I{GLFW}\\include", f"-I{GLAD}\\include", f"-I{IMGUI}", f"-I{IMGUI_BB}", f"-I{GLM}", f"-I{STB}"]
LIB = [f"-L{GLFW}\\lib-mingw-w64", "-lglfw3", "-lopengl32", "-lgdi32", "-lshell32", "-lcomdlg32"]

os.chdir(CPP)

def run(cmd, label):
    print(f"  {label}...", end=" ", flush=True)
    r = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=CPP)
    if r.returncode != 0:
        print(f"FAIL\n{r.stderr[:500]}")
        sys.exit(1)
    print("OK")

print("[1/3] ImGui")
for f in ["imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp"]:
    out = f.replace(".cpp", ".o")
    run([GPP] + FLAGS + ["-c", os.path.join(IMGUI, f), "-o", out], f)
for f in ["imgui_impl_glfw.cpp", "imgui_impl_opengl3.cpp"]:
    out = f.replace(".cpp", ".o")
    run([GPP] + FLAGS + INC + ["-c", os.path.join(IMGUI_BB, f), "-o", out], f)

print("[2/3] GLAD + OCP")
run([GPP] + FLAGS + INC + ["-c", os.path.join(GLAD, "src", "gl.c"), "-o", "gl.o"], "gl.c")
for src in ["mesh", "scene", "camera", "primitives", "renderer", "ai_engine", "exporters", "bmesh", "edit_tools", "sculpt_tools", "main"]:
    run([GPP] + FLAGS + INC + ["-c", f"src\\{src}.cpp", "-o", f"{src}.o"], f"{src}.cpp")

print("[3/3] Link")
objs = ["imgui.o", "imgui_draw.o", "imgui_tables.o", "imgui_widgets.o",
        "imgui_impl_glfw.o", "imgui_impl_opengl3.o", "gl.o",
        "mesh.o", "scene.o", "camera.o", "primitives.o", "renderer.o",
        "ai_engine.o", "exporters.o", "bmesh.o", "edit_tools.o", "sculpt_tools.o", "main.o"]
run([GPP] + FLAGS + ["-o", "OCP.exe"] + objs + LIB + ["-Wl,--allow-multiple-definition"], "link")

sz = os.path.getsize("OCP.exe")
print(f"\nBUILD OK - OCP.exe ({sz // (1024*1024)} MB)")
