package main

import (
	"bufio"
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/wailsapp/wails/v2/pkg/runtime"
)

type Device struct {
	Id   string `json:"id"`
	Desc string `json:"desc"`
	Mac  string `json:"mac"`
	Ip   string `json:"ip"`
}

type Config struct {
	Device         string `json:"device"`
	Username       string `json:"username"`
	Password       string `json:"password"`
	AutoConnect    bool   `json:"autoConnect"`
	MinimizeToTray bool   `json:"minimizeToTray"`
	AutoStart      bool   `json:"autoStart"`
}

type App struct {
	ctx    context.Context
	cmd    *exec.Cmd
	stdin  io.WriteCloser
	mu     sync.Mutex
	exeDir string
}

var appInstance *App

func NewApp() *App {
	appInstance = &App{}
	return appInstance
}

func (a *App) startup(ctx context.Context) {
	a.ctx = ctx
	exe, _ := os.Executable()
	a.exeDir = filepath.Dir(exe)
	runtime.WindowShow(ctx)
}

func (a *App) shutdown(ctx context.Context) {
	a.Disconnect()
}

// ── Devices ─────────────────────────────────────────────────────

func (a *App) GetDevices() []Device {
	zzzPath := filepath.Join(a.exeDir, "zzz.exe")
	if _, err := os.Stat(zzzPath); os.IsNotExist(err) {
		zzzPath = filepath.Join(a.exeDir, "zzz")
	}

	cmd := exec.Command(zzzPath, "list")
	cmd.Dir = a.exeDir
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	output, err := cmd.Output()
	if err != nil {
		runtime.LogErrorf(a.ctx, "GetDevices: %v", err)
		return nil
	}

	var devices []Device
	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		parts := strings.Split(line, "\t")
		d := Device{}
		if len(parts) > 0 {
			d.Id = parts[0]
		}
		if len(parts) > 1 {
			d.Desc = parts[1]
		}
		if len(parts) > 2 {
			d.Mac = parts[2]
		}
		if len(parts) > 3 {
			d.Ip = parts[3]
		}
		devices = append(devices, d)
	}
	return devices
}

// ── Auth ─────────────────────────────────────────────────────────

func (a *App) Connect(device, username, password string) error {
	a.Disconnect()

	dir := a.configDir()
	os.MkdirAll(dir, 0755)
	cfg := fmt.Sprintf("[auth]\nusername = %s\npassword = %s\ndevice = %s\n", username, password, device)
	os.WriteFile(filepath.Join(dir, "config.ini"), []byte(cfg), 0644)

	zzzPath := filepath.Join(a.exeDir, "zzz.exe")
	if _, err := os.Stat(zzzPath); os.IsNotExist(err) {
		zzzPath = filepath.Join(a.exeDir, "zzz")
	}

	a.cmd = exec.Command(zzzPath, "run", device, "--config", filepath.Join(dir, "config.ini"))
	a.cmd.Dir = a.exeDir
	a.cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}

	var err error
	a.stdin, err = a.cmd.StdinPipe()
	if err != nil {
		return fmt.Errorf("stdin pipe: %w", err)
	}

	stdout, err := a.cmd.StdoutPipe()
	if err != nil {
		return fmt.Errorf("stdout pipe: %w", err)
	}

	stderr, err := a.cmd.StderrPipe()
	if err != nil {
		return fmt.Errorf("stderr pipe: %w", err)
	}

	if err := a.cmd.Start(); err != nil {
		return fmt.Errorf("start: %w", err)
	}

	go a.streamOutput(stdout)
	go a.streamOutput(stderr)

	go func() {
		err := a.cmd.Wait()
		a.mu.Lock()
		a.cmd = nil
		a.stdin = nil
		a.mu.Unlock()
		if err != nil {
			runtime.EventsEmit(a.ctx, "auth-exited", map[string]interface{}{
				"error": err.Error(),
			})
		} else {
			runtime.EventsEmit(a.ctx, "auth-exited", map[string]interface{}{})
		}
	}()

	runtime.EventsEmit(a.ctx, "auth-state", "authing")
	return nil
}

func (a *App) Disconnect() {
	a.mu.Lock()
	defer a.mu.Unlock()

	if a.stdin != nil {
		io.WriteString(a.stdin, "quit\n")
		a.stdin.Close()
	}
	if a.cmd != nil {
		done := make(chan struct{})
		go func() {
			a.cmd.Wait()
			close(done)
		}()
		select {
		case <-done:
		case <-time.After(500 * time.Millisecond):
			a.cmd.Process.Kill()
			<-done
		}
		a.cmd = nil
	}
	a.stdin = nil
	runtime.EventsEmit(a.ctx, "auth-state", "offline")
}

func (a *App) streamOutput(r io.Reader) {
	scanner := bufio.NewScanner(r)
	for scanner.Scan() {
		line := scanner.Text()
		runtime.EventsEmit(a.ctx, "auth-output", map[string]interface{}{
			"line": line,
		})
		if strings.Contains(line, "auth success") || strings.Contains(line, "success") {
			runtime.EventsEmit(a.ctx, "auth-state", "connected")
		} else if strings.Contains(line, "auth failed") || strings.Contains(line, "failed") {
			runtime.EventsEmit(a.ctx, "auth-state", "failed")
		} else if strings.Contains(line, "authing") {
			runtime.EventsEmit(a.ctx, "auth-state", "authing")
		}
	}
}

// ── Config ───────────────────────────────────────────────────────

func (a *App) configDir() string {
	return filepath.Join(os.Getenv("APPDATA"), "zzz")
}

func (a *App) GetConfig() Config {
	c := Config{MinimizeToTray: true}
	dir := a.configDir()

	configPath := filepath.Join(dir, "config.ini")
	if data, err := os.ReadFile(configPath); err == nil {
		for _, line := range strings.Split(string(data), "\n") {
			line = strings.TrimSpace(line)
			if strings.HasPrefix(line, "username") {
				if i := strings.Index(line, "="); i >= 0 {
					c.Username = strings.TrimSpace(line[i+1:])
				}
			} else if strings.HasPrefix(line, "password") {
				if i := strings.Index(line, "="); i >= 0 {
					c.Password = strings.TrimSpace(line[i+1:])
				}
			} else if strings.HasPrefix(line, "device") {
				if i := strings.Index(line, "="); i >= 0 {
					c.Device = strings.TrimSpace(line[i+1:])
				}
			}
		}
	}

	settingsPath := filepath.Join(dir, "settings.ini")
	if data, err := os.ReadFile(settingsPath); err == nil {
		for _, line := range strings.Split(string(data), "\n") {
			line = strings.TrimSpace(line)
			if strings.Contains(line, "auto_connect") && strings.Contains(line, "=") {
				c.AutoConnect = strings.Contains(line, "1")
			} else if strings.Contains(line, "minimize_to_tray") && strings.Contains(line, "=") {
				c.MinimizeToTray = strings.Contains(line, "1")
			} else if strings.Contains(line, "auto_start") && strings.Contains(line, "=") {
				c.AutoStart = strings.Contains(line, "1")
			}
		}
	}
	return c
}

func (a *App) SaveConfig(c Config) error {
	dir := a.configDir()
	os.MkdirAll(dir, 0755)

	settings := fmt.Sprintf("[settings]\nauto_connect = %d\nminimize_to_tray = %d\nauto_start = %d\n",
		boolInt(c.AutoConnect), boolInt(c.MinimizeToTray), boolInt(c.AutoStart))
	return os.WriteFile(filepath.Join(dir, "settings.ini"), []byte(settings), 0644)
}

func boolInt(b bool) int {
	if b {
		return 1
	}
	return 0
}

// ── AutoStart ─────────────────────────────────────────────────────

func (a *App) SetAutoStart(enabled bool) error {
	exe, _ := os.Executable()
	if enabled {
		cmd := exec.Command("schtasks", "/create",
			"/tn", "zzz",
			"/tr", fmt.Sprintf(`cmd /c start "" "%s"`, exe),
			"/sc", "onlogon",
			"/delay", "0000:04",
			"/it", "/f")
		cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
		out, err := cmd.CombinedOutput()
		if err != nil {
			return fmt.Errorf("schtasks: %s: %w", string(out), err)
		}
		return nil
	}
	cmd := exec.Command("schtasks", "/delete", "/tn", "zzz", "/f")
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	cmd.Run()
	return nil
}

// ── Window Helpers ────────────────────────────────────────────────

func (a *App) BeforeClose(ctx context.Context) bool {
	// When user clicks native close (Alt+F4) or Quit(), always prevent.
	// The window is kept hidden; systray controls actual exit.
	runtime.WindowHide(ctx)
	return true // true = prevent quit (stay running)
}

// showWindow is called from tray menu
func showWindow() {
	if appInstance != nil && appInstance.ctx != nil {
		runtime.WindowShow(appInstance.ctx)
	}
}

// trayQuit is called from tray menu
func trayQuit() {
	if appInstance != nil {
		appInstance.Disconnect()
	}
	os.Exit(0)
}
