package main

import (
	"bytes"
	"embed"
	"encoding/binary"
	"os"
	"path/filepath"
	"syscall"
	"time"
	"unsafe"

	"github.com/getlantern/systray"
	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
	wailsWindows "github.com/wailsapp/wails/v2/pkg/options/windows"
)

var (
	kernel32 = syscall.NewLazyDLL("kernel32.dll")
	user32   = syscall.NewLazyDLL("user32.dll")
	shell32  = syscall.NewLazyDLL("shell32.dll")

	procCreateMutex         = kernel32.NewProc("CreateMutexW")
	procLockFileEx          = kernel32.NewProc("LockFileEx")
	procFindWindow          = user32.NewProc("FindWindowW")
	procShowWindow          = user32.NewProc("ShowWindow")
	procSetForegroundWindow = user32.NewProc("SetForegroundWindow")
	procIsUserAnAdmin       = shell32.NewProc("IsUserAnAdmin")
	procShellExecuteW       = shell32.NewProc("ShellExecuteW")
)

//go:embed all:frontend/dist
var assets embed.FS

func main() {
	checkSingleInstance()
	elevateIfNeeded()

	app := NewApp()

	go systray.Run(onTrayReady, onTrayExit)

	err := wails.Run(&options.App{
		Title:     "GUI.for.zzz",
		Width:            360,
		Height:           600,
		MinWidth:         360,
		MinHeight:        380,
		Frameless:        true,
		BackgroundColour: &options.RGBA{R: 255, G: 255, B: 255, A: 255},
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		OnStartup:     app.startup,
		OnShutdown:    app.shutdown,
		OnBeforeClose: app.BeforeClose,
		Bind: []interface{}{
			app,
		},
		Windows: &wailsWindows.Options{
			WebviewIsTransparent: false,
			WindowIsTranslucent:  false,
		},
		Debug: options.Debug{
			OpenInspectorOnStartup: false,
		},
	})

	if err != nil {
		logError(err.Error())
	}
}

func onTrayReady() {
	systray.SetTitle("GUI.for.ZZZ")
	systray.SetTooltip("GUI.for.ZZZ - 802.1X")
	systray.SetIcon(makeTrayIcon())

	mShow := systray.AddMenuItem("Show", "显示窗口")
	systray.AddSeparator()
	mQuit := systray.AddMenuItem("Exit", "退出")

	go func() {
		defer func() { recover() }()
		for range mShow.ClickedCh {
			showWindow()
		}
	}()
	go func() {
		defer func() { recover() }()
		for range mQuit.ClickedCh {
			trayQuit()
		}
	}()
}

func onTrayExit() {}

func makeTrayIcon() []byte {
	const w, h = 16, 16
	rowStride := ((w*32 + 31) / 32) * 4
	pixelDataSize := rowStride * h
	maskSize := ((w + 31) / 32) * 4 * h

	var buf bytes.Buffer

	// ── ICO header ──
	binary.Write(&buf, binary.LittleEndian, uint16(0)) // reserved
	binary.Write(&buf, binary.LittleEndian, uint16(1)) // type: ICO
	binary.Write(&buf, binary.LittleEndian, uint16(1)) // count

	// ── ICO entry ──
	bmpInfoSize := uint32(40)
	imageSize := bmpInfoSize + uint32(pixelDataSize) + uint32(maskSize)
	dataOffset := uint32(6 + 16) // header + entry

	buf.WriteByte(w)                                    // width
	buf.WriteByte(h)                                    // height (doubled for BMP)
	buf.WriteByte(0)                                    // palette
	buf.WriteByte(0)                                    // reserved
	binary.Write(&buf, binary.LittleEndian, uint16(1))  // planes
	binary.Write(&buf, binary.LittleEndian, uint16(32)) // bpp
	binary.Write(&buf, binary.LittleEndian, imageSize)  // size
	binary.Write(&buf, binary.LittleEndian, dataOffset) // offset

	// ── BMP Info header ──
	binary.Write(&buf, binary.LittleEndian, uint32(40))                     // biSize
	binary.Write(&buf, binary.LittleEndian, int32(w))                       // biWidth
	binary.Write(&buf, binary.LittleEndian, int32(h*2))                     // biHeight (2x for ICO transparency)
	binary.Write(&buf, binary.LittleEndian, uint16(1))                      // biPlanes
	binary.Write(&buf, binary.LittleEndian, uint16(32))                     // biBitCount
	binary.Write(&buf, binary.LittleEndian, uint32(0))                      // biCompression (BI_RGB)
	binary.Write(&buf, binary.LittleEndian, uint32(pixelDataSize+maskSize)) // biSizeImage
	binary.Write(&buf, binary.LittleEndian, int32(0))                       // biXPelsPerMeter
	binary.Write(&buf, binary.LittleEndian, int32(0))                       // biYPelsPerMeter
	binary.Write(&buf, binary.LittleEndian, uint32(0))                      // biClrUsed
	binary.Write(&buf, binary.LittleEndian, uint32(0))                      // biClrImportant

	// ── Pixel data (BGRA, bottom-up) ──
	for y := h - 1; y >= 0; y-- {
		row := make([]byte, rowStride)
		for x := 0; x < w; x++ {
			dx := float64(x) - 7.5
			dy := float64(y) - 7.5
			off := x * 4
			if dx*dx+dy*dy <= 36 {
				row[off] = 235   // B
				row[off+1] = 99  // G
				row[off+2] = 37  // R
				row[off+3] = 255 // A
			} else {
				row[off] = 0
				row[off+1] = 0
				row[off+2] = 0
				row[off+3] = 0
			}
		}
		buf.Write(row)
	}

	// ── AND mask (all 0 = opaque) ──
	for i := 0; i < maskSize; i++ {
		buf.WriteByte(0)
	}

	return buf.Bytes()
}

func checkSingleInstance() {
	lockPath := filepath.Join(os.Getenv("APPDATA"), "zzz", ".lock")
	os.MkdirAll(filepath.Dir(lockPath), 0755)
	f, err := os.OpenFile(lockPath, os.O_CREATE|os.O_RDWR, 0644)
	if err != nil {
		os.Exit(0)
	}
	// Try exclusive lock — fails if another instance holds it
	ol := new(syscall.Overlapped)
	r1, _, _ := procLockFileEx.Call(f.Fd(), 3, 0, 1, 0, uintptr(unsafe.Pointer(ol)))
	if r1 == 0 {
		f.Close()
		// Another instance exists — find and show it
		title, _ := syscall.UTF16PtrFromString("GUI.for.zzz")
		hwnd, _, _ := procFindWindow.Call(0, uintptr(unsafe.Pointer(title)))
		if hwnd != 0 {
			procShowWindow.Call(hwnd, 9) // SW_RESTORE
			procSetForegroundWindow.Call(hwnd)
		}
		os.Exit(0)
	}
	// Keep file open — lock released when process exits
}

func logStart() {
	writeLog("started")
}

func elevateIfNeeded() {
	// If already admin, proceed
	r, _, _ := procIsUserAnAdmin.Call()
	if r != 0 {
		return
	}
	// Not admin — restart with elevation
	exe, _ := os.Executable()
	verb, _ := syscall.UTF16PtrFromString("runas")
	file, _ := syscall.UTF16PtrFromString(exe)
	procShellExecuteW.Call(0, uintptr(unsafe.Pointer(verb)), uintptr(unsafe.Pointer(file)), 0, 0, 1)
	os.Exit(0)
}

func logError(msg string) {
	writeLog("ERROR: " + msg)
}

func writeLog(msg string) {
	dir := filepath.Join(os.Getenv("APPDATA"), "zzz")
	os.MkdirAll(dir, 0755)
	t := time.Now().Format("2006-01-02 15:04:05")
	f, err := os.OpenFile(filepath.Join(dir, "startup.log"), os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		return
	}
	defer f.Close()
	f.WriteString(t + " " + msg + "\n")
}
