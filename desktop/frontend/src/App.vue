<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed, nextTick } from 'vue'
import {
  GetDevices, Connect, Disconnect, GetConfig, SaveConfig, SetAutoStart
} from '../wailsjs/go/main/App'
import { EventsOn, EventsOff, WindowHide, WindowMinimise } from '../wailsjs/runtime/runtime'

type State = 'offline' | 'authing' | 'connected' | 'failed'
interface Device { id: string; desc: string; mac: string; ip: string }

const state = ref<State>('offline')
const devices = ref<Device[]>([])
const selectedDevice = ref('')
const username = ref('')
const password = ref('')
const suffix = ref('@cucc')
const ISP_SUFFIXES = ['@cucc', '@cmcc', '@ctcc']
const logLines = ref<string[]>([])
const logVisible = ref(true)
const showSettings = ref(false)
const copied = ref(false)
const autoConnect = ref(false)
const autoStart = ref(false)
const lang = ref<'zh'|'en'>('zh')

const t = computed(() => ({
  zh: {
    state: {offline:'离线',authing:'认证中',connected:'已连接',failed:'失败'},
    connect:'连接', retry:'重试', stop:'停止',
    nic:'网卡', user:'用户名', pwd:'密码',
    autoConn:'启动时自动连接', autoStart:'开机自启',
    clearLog:'清除日志', hideLog:'隐藏日志', showLog:'显示日志',
    langLabel:'语言', copyLog:'复制日志', copied:'已复制',
  },
  en: {
    state: {offline:'Offline',authing:'Authing',connected:'Connected',failed:'Failed'},
    connect:'Connect', retry:'Retry', stop:'Stop',
    nic:'NIC', user:'Username', pwd:'Password',
    autoConn:'Auto-connect on startup', autoStart:'Start with Windows',
    clearLog:'Clear log', hideLog:'Hide log', showLog:'Show log',
    langLabel:'Language', copyLog:'Copy log', copied:'Copied',
  }
}[lang.value]))

const ipDisplay = computed(() => {
  const d = devices.value.find(d => d.id === selectedDevice.value)
  return d?.ip || ''
})
const connectLabel = computed(() => state.value === 'failed' ? t.value.retry : t.value.connect)
const stateLabel = computed(() => t.value.state[state.value])
const canConnect = computed(() => state.value === 'offline' || state.value === 'failed')
const canDisconnect = computed(() => state.value === 'connected' || state.value === 'authing')

function onState(s: string) { state.value = s as State }
function onOutput(data: any) {
  logLines.value.push(data.line)
  if (logLines.value.length > 200) logLines.value.shift()
  nextTick(() => { const el = document.getElementById('log-container'); if (el) el.scrollTop = el.scrollHeight })
}
function onExited(data?: any) {
  if (data?.error) logLines.value.push(`\u26A0 ${data.error}`)
}

async function loadDevices() {
  devices.value = await GetDevices()
  const cfg = await GetConfig()
  if (cfg.username) {
    for (const s of ISP_SUFFIXES) {
      if (cfg.username.endsWith(s)) {
        suffix.value = s
        username.value = cfg.username.slice(0, -s.length)
        break
      }
    }
    if (!username.value) username.value = cfg.username
  } else {
    username.value = ''
  }
  password.value = cfg.password || ''
  autoConnect.value = cfg.autoConnect
  autoStart.value = cfg.autoStart
  if (cfg.device) selectedDevice.value = cfg.device
  if (!selectedDevice.value && devices.value.length) selectedDevice.value = devices.value[0].id
}

async function doConnect() {
  const user = username.value ? (username.value + suffix.value) : ''
  if (!selectedDevice.value || !user) {
    logLines.value.push('\u26A0 Device and username required')
    return
  }
  logLines.value.push('=== Connecting ===')
  logVisible.value = true
  state.value = 'authing'
  try {
    await Connect(selectedDevice.value, user, password.value)
  } catch (e: any) {
    logLines.value.push(`\u26A0 ${e}`)
    state.value = 'failed'
  }
}

async function doDisconnect() {
  await Disconnect()
  logLines.value.push('=== Disconnected ===')
}

async function saveSettings() {
  await SaveConfig({ device: selectedDevice.value, username: username.value ? username.value + suffix.value : '', password: password.value, autoConnect: autoConnect.value, minimizeToTray: true, autoStart: autoStart.value })
}

async function handleAutoConnect() { await nextTick(); await saveSettings() }

async function handleAutoStart() { await nextTick(); await saveSettings(); SetAutoStart(autoStart.value) }

function toggleLang() { lang.value = lang.value === 'zh' ? 'en' : 'zh' }

async function handleClose() {
  WindowHide()
}

function clearLog() { logLines.value = [] }

async function copyLog() {
  await navigator.clipboard.writeText(logLines.value.join('\n'))
  copied.value = true
  setTimeout(() => copied.value = false, 1500)
}

onMounted(async () => {
  EventsOn('auth-state', onState)
  EventsOn('auth-output', onOutput)
  EventsOn('auth-exited', onExited)
  await loadDevices()
  if (autoConnect.value) doConnect()
})

onUnmounted(() => {
  EventsOff('auth-state'); EventsOff('auth-output'); EventsOff('auth-exited')
})
</script>

<template>
  <div class="shell" @click="showSettings = false">
    <header class="titlebar">
      <div class="flex items-center">
        <span class="text-sm font-semibold tracking-tight" style="color:#1e293b">GUI.for.ZZZ</span>
      </div>
      <div class="flex items-center gap-1" style="--wails-draggable:no-drag;">
        <button class="btn-ctrl" title="Settings" style="font-size:14px;position:relative;padding-top:2px;"
          @click.stop="showSettings = !showSettings"
          onmouseover="this.style.background='rgba(148,163,184,0.15)';this.style.color='#475569'"
          onmouseout="this.style.background='none';this.style.color='#94a3b8'">⚙
          <div v-if="showSettings" class="settings-drop" @click.stop>
            <label class="settings-item">
              <span>{{ t.autoConn }}</span>
              <span class="toggle"><input type="checkbox" v-model="autoConnect" @change="handleAutoConnect" /><span class="slider" /></span>
            </label>
            <label class="settings-item">
              <span>{{ t.autoStart }}</span>
              <span class="toggle"><input type="checkbox" v-model="autoStart" @change="handleAutoStart" /><span class="slider" /></span>
            </label>
            <label class="settings-item" @click="toggleLang" style="cursor:pointer;">
              <span>{{ t.langLabel }}</span>
              <span style="color:#2563eb;font-weight:600;">{{ lang === 'zh' ? '中文' : 'English' }}</span>
            </label>
          </div>
        </button>
        <button @click="WindowMinimise()" class="btn-ctrl" title="Minimize"
          onmouseover="this.style.background='rgba(148,163,184,0.15)';this.style.color='#475569'"
          onmouseout="this.style.background='none';this.style.color='#94a3b8'">&minus;</button>
        <button @click="handleClose()" class="btn-ctrl btn-close" title="Close"
          onmouseover="this.style.background='rgba(239,68,68,0.12)';this.style.color='#ef4444'"
          onmouseout="this.style.background='none';this.style.color='#94a3b8'">&times;</button>
      </div>
    </header>

    <div class="body">
      <div class="card" style="margin-bottom:18px;">
        <div class="flex items-center justify-between">
          <div class="flex items-center gap-2">
            <span class="status-dot" :class="state" style="width:12px;height:12px;" />
            <span class="text-sm font-semibold capitalize" style="color:#1e293b">{{ stateLabel }}</span>
          </div>
          <div class="flex items-center gap-2 text-xs" style="color:#94a3b8">
            <span class="font-mono">{{ ipDisplay || '' }}</span>
          </div>
        </div>
      </div>

      <div class="field-group">
        <select v-model="selectedDevice" class="field combo" @change="saveSettings">
          <option value="">{{ t.nic }}</option>
          <option v-for="d in devices" :key="d.id" :value="d.id">{{ d.desc }}{{ d.mac && d.mac !== '00:00:00:00:00:00' ? '  [' + d.mac + ']' : '' }}</option>
        </select>
      </div>

      <div class="field-group">
        <div class="flex gap-1.5">
          <input v-model="username" type="text" class="field" :placeholder="t.user" @change="saveSettings" style="flex:1;" />
          <select v-model="suffix" class="field combo" @change="saveSettings" style="width:auto;min-width:110px;padding-right:28px;">
            <option value="@cucc">{{ lang === 'zh' ? '联通用户' : '@cucc' }}</option>
            <option value="@ctcc">{{ lang === 'zh' ? '电信用户' : '@ctcc' }}</option>
            <option value="@cmcc">{{ lang === 'zh' ? '移动用户' : '@cmcc' }}</option>
          </select>
        </div>
      </div>

      <div class="field-group">
        <input v-model="password" type="password" class="field" :placeholder="t.pwd" @change="saveSettings" />
      </div>

      <div class="flex gap-2 mb-4">
        <button class="btn btn-primary flex-1" :disabled="!canConnect" @click="doConnect">{{ connectLabel }}</button>
        <button class="btn btn-disconnect" :disabled="!canDisconnect" @click="doDisconnect">{{ t.stop }}</button>
      </div>

      <div v-if="logVisible" class="log-panel" style="margin-top:14px;">
        <div id="log-container" class="log-container">
          <div v-for="(line,i) in logLines" :key="i" class="whitespace-pre-wrap break-all">{{ line }}</div>
        </div>
      </div>

      <div v-if="logVisible" class="flex items-center justify-center gap-4" style="padding:6px 0 2px;">
        <button class="log-toggle" style="width:auto;"
          @click="clearLog"
          onmouseover="this.style.color='#ef4444'"
          onmouseout="this.style.color='#94a3b8'">{{ t.clearLog }}</button>
        <button class="log-toggle" style="width:auto;"
          @click="copyLog"
          onmouseover="this.style.color='#2563eb'"
          onmouseout="this.style.color='#94a3b8'">{{ copied ? t.copied : t.copyLog }}</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.shell { display: flex; flex-direction: column; width: 100%; height: 100%; background: #f1f5f9; overflow: hidden; }
.titlebar { display: flex; align-items: center; justify-content: space-between; height: 44px; padding: 0 16px; background: #f1f5f9; flex-shrink: 0; --wails-draggable: drag; }
.btn-ctrl { display: flex; align-items: center; justify-content: center; width: 32px; height: 32px; border: none; background: none; font-size: 15px; color: #94a3b8; cursor: pointer; border-radius: 6px; transition: all 0.15s; --wails-draggable: no-drag; }
.btn-close { font-size: 17px; }
.body { flex: 1; display: flex; flex-direction: column; overflow-y: auto; padding: 14px 16px 16px; }
.field-group { margin-bottom: 12px; }
.log-panel { border-radius: 10px; overflow: hidden; flex: 1; min-height: 130px; background: #fff; border: 1px solid rgba(148,163,184,0.12); margin-bottom: 6px; }
.log-container { height: 100%; overflow-y: auto; padding: 10px 12px; font-size: 11px; color: #64748b; font-family: 'SF Mono','Cascadia Code','Consolas',monospace; line-height: 1.65; user-select: text; }
.log-toggle { width: 100%; text-align: center; padding: 5px 0; font-size: 11px; font-weight: 500; letter-spacing: 0.05em; color: #94a3b8; background: none; border: none; cursor: pointer; transition: color 0.15s; }
.settings-drop { position: absolute; top: 36px; right: 0; background: #fff; border: 1px solid rgba(148,163,184,0.15); border-radius: 10px; padding: 8px; min-width: 220px; box-shadow: 0 4px 16px rgba(0,0,0,0.08); z-index: 50; }
.settings-item { display: flex; align-items: center; justify-content: space-between; padding: 8px 10px; border-radius: 6px; cursor: pointer; font-size: 13px; color:#475569; }
.settings-item:hover { background: #f1f5f9; }
</style>
