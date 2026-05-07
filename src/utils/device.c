#include "utils/device.h"
#include "packet/packet.h"
#include "utils/log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#define SNAP_LEN 65535

struct Device g_device;

void device_send_packet(pcap_t *handle, uint8_t *packet, size_t length) {
  size_t padded_length = length;

  if (length < ETHERNET_FRAME_MIN_SIZE - CRC_SIZE) {
    padded_length = ETHERNET_FRAME_MIN_SIZE - CRC_SIZE;
  }

  uint8_t *padded_packet = calloc(1, padded_length);
  if (!padded_packet) {
    log_error("memory allocation failed for padding", NULL);
    exit(EXIT_FAILURE);
  }

  memcpy(padded_packet, packet, length);

  if (pcap_sendpacket(handle, padded_packet, padded_length) != 0) {
    log_error(pcap_geterr(handle), NULL);
    free(padded_packet);
    exit(EXIT_FAILURE);
  }

  free(padded_packet);
}

void device_set_filter(const char *filter_str) {
  struct bpf_program filter;
  if (pcap_compile(g_device.handle, &filter, filter_str, 1, 0) != 0) {
    log_error(pcap_geterr(g_device.handle), NULL);
    exit(EXIT_FAILURE);
  }
  if (pcap_setfilter(g_device.handle, &filter) != 0) {
    log_error(pcap_geterr(g_device.handle), NULL);
    pcap_freecode(&filter);
    exit(EXIT_FAILURE);
  }
  pcap_freecode(&filter);
}

#ifdef _WIN32

static int get_mac_ip_by_name(const char *name, uint8_t *mac_out, char *ip_out,
                               size_t ip_size) {
  const char *guid_start = strchr(name, '{');
  if (!guid_start) {
    strncpy(ip_out, IP_DEFAULT, ip_size);
    return -1;
  }

  char guid[64];
  strncpy(guid, guid_start, sizeof(guid) - 1);
  guid[sizeof(guid) - 1] = '\0';

  ULONG buf_len = 15000;
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
  IP_ADAPTER_ADDRESSES *adapters =
      (IP_ADAPTER_ADDRESSES *)malloc(buf_len);
  if (!adapters)
    return -1;

  ULONG ret =
      GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
  if (ret == ERROR_BUFFER_OVERFLOW) {
    free(adapters);
    adapters = (IP_ADAPTER_ADDRESSES *)malloc(buf_len);
    if (!adapters)
      return -1;
    ret = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
  }

  int found = 0;
  if (ret == ERROR_SUCCESS) {
    for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter;
         adapter = adapter->Next) {
      if (strcmp(adapter->AdapterName, guid) == 0) {
        if (adapter->PhysicalAddressLength >= HARDWARE_ADDR_SIZE)
          memcpy(mac_out, adapter->PhysicalAddress, HARDWARE_ADDR_SIZE);

        if (adapter->FirstUnicastAddress) {
          struct sockaddr_in *addr =
              (struct sockaddr_in *)adapter->FirstUnicastAddress->Address.lpSockaddr;
          char ip_buf[INET_ADDRSTRLEN];
          if (inet_ntop(AF_INET, &addr->sin_addr, ip_buf, sizeof(ip_buf)))
            strncpy(ip_out, ip_buf, ip_size);
          else
            strncpy(ip_out, IP_DEFAULT, ip_size);
        } else {
          strncpy(ip_out, IP_DEFAULT, ip_size);
        }
        found = 1;
        break;
      }
    }
  }

  free(adapters);
  if (!found)
    strncpy(ip_out, IP_DEFAULT, ip_size);
  return found ? 0 : -1;
}

void device_set_addr(const char *interface_name) {
  get_mac_ip_by_name(interface_name, g_device.src_mac, g_device.ip_addr,
                     IP_ADDR_SIZE);
}

void device_init(const char *interface_name) {
  char err[PCAP_ERRBUF_SIZE];
  g_device.handle = pcap_open_live(interface_name, SNAP_LEN, 0, 250, err);
  if (!(g_device.handle)) {
    log_error(err, NULL);
    exit(EXIT_FAILURE);
  }

  device_set_filter("ether proto 0x888E");
  device_set_addr(interface_name);
}

void device_list(void) {
  pcap_if_t *alldevs;
  char errbuf[PCAP_ERRBUF_SIZE];

  if (pcap_findalldevs(&alldevs, errbuf) == -1) {
    fprintf(stderr, "error: %s\n", errbuf);
    return;
  }

  ULONG buf_len = 15000;
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
  IP_ADAPTER_ADDRESSES *adapters =
      (IP_ADAPTER_ADDRESSES *)malloc(buf_len);
  if (adapters) {
    ULONG ret =
        GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
      free(adapters);
      adapters = (IP_ADAPTER_ADDRESSES *)malloc(buf_len);
      if (adapters) {
        ret = GetAdaptersAddresses(AF_INET, flags, NULL, adapters, &buf_len);
        if (ret != ERROR_SUCCESS) {
          free(adapters);
          adapters = NULL;
        }
      }
    } else if (ret != ERROR_SUCCESS) {
      free(adapters);
      adapters = NULL;
    }
  }

  for (pcap_if_t *dev = alldevs; dev; dev = dev->next) {
    char mac_str[32] = "00:00:00:00:00:00";
    char ip_str[IP_ADDR_SIZE + 1] = {0};
    const char *desc = dev->description ? dev->description : "";
    strncpy(ip_str, IP_DEFAULT, IP_ADDR_SIZE);

    const char *guid_start = strchr(dev->name, '{');
    if (guid_start && adapters) {
      char guid[64];
      strncpy(guid, guid_start, sizeof(guid) - 1);
      guid[sizeof(guid) - 1] = '\0';

      for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter;
           adapter = adapter->Next) {
        if (strcmp(adapter->AdapterName, guid) == 0) {
          if (adapter->PhysicalAddressLength >= HARDWARE_ADDR_SIZE) {
            snprintf(mac_str, sizeof(mac_str),
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     adapter->PhysicalAddress[0],
                     adapter->PhysicalAddress[1],
                     adapter->PhysicalAddress[2],
                     adapter->PhysicalAddress[3],
                     adapter->PhysicalAddress[4],
                     adapter->PhysicalAddress[5]);
          }
          if (adapter->FirstUnicastAddress) {
            struct sockaddr_in *addr =
                (struct sockaddr_in *)adapter->FirstUnicastAddress
                    ->Address.lpSockaddr;
            char ip_buf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr->sin_addr, ip_buf, sizeof(ip_buf)))
              strncpy(ip_str, ip_buf, IP_ADDR_SIZE);
          }
          if (adapter->FriendlyName) {
            char fname[256] = {0};
            WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1,
                                fname, sizeof(fname) - 1, NULL, NULL);
            if (fname[0])
              desc = fname;
          }
          break;
        }
      }
    }

    printf("%s\t%s\t%s\t%s\n", dev->name, desc, mac_str, ip_str);
  }

  free(adapters);
  pcap_freealldevs(alldevs);
}

#else

void device_set_addr(const char *interface_name) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    log_error("failed to create socket", NULL);
    exit(EXIT_FAILURE);
  }

  struct ifreq ifr;
  strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
  ifr.ifr_name[IFNAMSIZ - 1] = '\0';

  if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) != 0) {
    log_error("failed to get hardware address", NULL);
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
  memcpy(g_device.src_mac, mac, HARDWARE_ADDR_SIZE);

  if (ioctl(sockfd, SIOCGIFADDR, &ifr) != 0) {
    log_warn("failed to get ip address", NULL);
    strncpy(g_device.ip_addr, IP_DEFAULT, IP_ADDR_SIZE);
    close(sockfd);
    return;
  }

  struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
  char ip_buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &addr->sin_addr, ip_buf, sizeof(ip_buf)))
    strncpy(g_device.ip_addr, ip_buf, IP_ADDR_SIZE);
  else
    strncpy(g_device.ip_addr, IP_DEFAULT, IP_ADDR_SIZE);

  close(sockfd);
}

void device_init(const char *interface_name) {
  char err[PCAP_ERRBUF_SIZE];
  g_device.handle = pcap_open_live(interface_name, SNAP_LEN, 0, 250, err);
  if (!(g_device.handle)) {
    log_error(err, NULL);
    exit(EXIT_FAILURE);
  }

  device_set_filter("ether proto 0x888E");
  device_set_addr(interface_name);
}

void device_list(void) {
  pcap_if_t *alldevs;
  char errbuf[PCAP_ERRBUF_SIZE];

  if (pcap_findalldevs(&alldevs, errbuf) == -1) {
    fprintf(stderr, "error: %s\n", errbuf);
    return;
  }

  for (pcap_if_t *dev = alldevs; dev; dev = dev->next) {
    char mac_str[32] = "00:00:00:00:00:00";
    char ip_str[IP_ADDR_SIZE + 1] = {0};
    strncpy(ip_str, IP_DEFAULT, IP_ADDR_SIZE);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd >= 0) {
      struct ifreq ifr;
      strncpy(ifr.ifr_name, dev->name, IFNAMSIZ - 1);
      ifr.ifr_name[IFNAMSIZ - 1] = '\0';

      if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) {
        unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
        snprintf(mac_str, sizeof(mac_str),
                 "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
                 mac[2], mac[3], mac[4], mac[5]);
      }

      if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
        char ip_buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &addr->sin_addr, ip_buf, sizeof(ip_buf)))
          strncpy(ip_str, ip_buf, IP_ADDR_SIZE);
      }

      close(sockfd);
    }

    printf("%s\t%s\t%s\t%s\n", dev->name,
           dev->description ? dev->description : "", mac_str, ip_str);
  }

  pcap_freealldevs(alldevs);
}
#endif