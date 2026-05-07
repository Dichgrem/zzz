# zzz — justfile
set windows-shell := ["cmd.exe", "/c"]
set dotenv-load := true

# ── Paths ───────────────────────────────────────────────────────

zig := if os() == "windows" { env_var_or_default("ZIG", "zig.exe") } else { "zig" }
npcap_sdk := env_var_or_default("NPCAP_SDK", "")
iscc := env_var_or_default("ISCC", "")

c_src := "src/main.c src/auth.c src/crypto/aes.c src/crypto/aes-md5.c src/crypto/base64.c src/crypto/const.c src/crypto/crypto.c src/crypto/md5.c src/packet/packet.c src/packet/send.c src/utils/config.c src/utils/device.c src/utils/ini.c src/utils/log.c"
c_flags := "-I include -I src"
c_libs := "-lwpcap -lPacket -lws2_32 -liphlpapi"
c_out := ".output/zzz.exe"

# ── Windows ─────────────────────────────────────────────────────

[windows]
build-cli-win:
    @if not exist .output mkdir .output
    {{zig}} cc -target x86_64-windows-gnu {{c_flags}} -I {{npcap_sdk}}/Include -L {{npcap_sdk}}/Lib/x64 -o {{c_out}} {{c_src}} {{c_libs}}
    @echo   --^> {{c_out}}

build-gui-win:
    cd desktop && wails build -ldflags=-H=windowsgui
    @copy /y desktop\build\bin\zzz-gui.exe .output\zzz-gui.exe >nul
    @echo   --^> .output\zzz-gui.exe

list-win: build-cli-win
    .output\zzz.exe list

package-win: build-cli-win build-gui-win
    @if "{{iscc}}"=="" echo ERROR: set ISCC in .env & exit /b 1
    {{iscc}} installer\setup.iss
    @echo   --^> .output\zzz-setup.exe

dev-win:
    cd desktop && wails dev

clean-win:
    @rmdir /s /q .output 2>nul
    @rmdir /s /q desktop\build 2>nul
    @rmdir /s /q desktop\frontend\node_modules 2>nul
    @rmdir /s /q desktop\frontend\dist 2>nul
    @echo cleaned

# ── Linux ───────────────────────────────────────────────────────

[unix]
build-cli:
    meson setup .output
    meson compile -C .output

clean:
    rm -rf .output/

# ── Shared ──────────────────────────────────────────────────────

fmt:
    @echo [1/3] go fmt...
    cd desktop && go fmt ./...
    @echo [2/3] go vet...
    cd desktop && go vet ./...
    @echo [3/3] vue-tsc typecheck...
    cd desktop\frontend && bunx vue-tsc --noEmit
    @echo OK

info:
    @just --list
