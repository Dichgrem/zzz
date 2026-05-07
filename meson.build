project('zzz', 'c',
  version : '0.1.1',
  default_options : [
    'buildtype=release',
    'c_std=gnu23'
  ]
)

if get_option('buildtype') == 'debug'
  add_project_arguments('-fsanitize=address', language : 'c')
  add_project_link_arguments('-fsanitize=address', language : 'c')
endif

npcap_sdk = get_option('npcap_sdk')

if host_machine.system() == 'windows'
  npcap_inc = npcap_sdk != '' ? include_directories(npcap_sdk / 'Include') : []
  npcap_arch = host_machine.cpu_family() == 'aarch64' ? 'ARM64' : 'x64'
  npcap_lib = npcap_sdk != '' ? (npcap_sdk / 'Lib' / npcap_arch) : ''

  pcap_lib = meson.get_compiler('c').find_library('wpcap',
    dirs : npcap_lib != '' ? npcap_lib : [],
    required : true)
  ws2_32_lib = meson.get_compiler('c').find_library('ws2_32', required : true)
  iphlpapi_lib = meson.get_compiler('c').find_library('iphlpapi', required : true)
  pcap_dep = declare_dependency(
    dependencies : [pcap_lib, ws2_32_lib, iphlpapi_lib],
    include_directories : npcap_inc,
  )
else
  pcap_dep = dependency('libpcap', required : true)
endif

inc = include_directories('include', 'src')

src_files = files(
  'src/auth.c',
  'src/crypto/aes.c',
  'src/crypto/aes-md5.c',
  'src/crypto/base64.c',
  'src/crypto/const.c',
  'src/crypto/crypto.c',
  'src/crypto/md5.c',
  'src/main.c',
  'src/packet/packet.c',
  'src/packet/send.c',
  'src/utils/config.c',
  'src/utils/device.c',
  'src/utils/ini.c',
  'src/utils/log.c'
)

executable('zzz',
  src_files,
  include_directories : inc,
  dependencies : pcap_dep,
  install : true
)