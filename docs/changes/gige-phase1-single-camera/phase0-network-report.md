# Phase 0 网络探针记录

日期：2026-10-25（Asia/Shanghai）

## 环境

- Windows / Winsock，WSA 2.2。
- WLAN 3：`192.168.163.183/22`，接口索引 `17`。
- Loopback：`127.0.0.2`，接口索引 `1`。
- 探针：`.build/windows-debug/phase0_probe.exe`，通过 `WSARecvMsg` 接收 `IP_PKTINFO`，`WSASendMsg` 指定源 IPv4。

## 结果

| 绑定地址 | 收到的源地址 | pktinfo 目的地址 | 接口索引 | 负载 | 结果 |
|---|---|---|---:|---:|---|
| `127.0.0.2:39570` | `127.0.0.2:39571` | `127.0.0.2` | 1 | 8 bytes | 通过 |
| `192.168.163.183:39572` | `192.168.163.183:39573` | `192.168.163.183` | 17 | 8 bytes | 通过 |

## 限制

该探针验证本机 IPv4 上的 pktinfo 收发与源地址选择，不代表物理网卡外部广播/跨主机 discovery 已验证，也不代表 MVS/Aravis 客户端兼容。后续仍须由合成客户端端到端测试和用户环境客户端验收。

## 复现

```powershell
cmake --build --preset windows-debug --target phase0_probe
.build\windows-debug\phase0_probe.exe 192.168.163.183
```

运行日志记录了实际本地时间、接口索引和收发地址；本机结果以终端输出为准。
