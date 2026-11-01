## GigE Vision
基于以太网的工业相机发现、控制和图像流协议族。Avoid: 把 GVCP 与 GVSP 混称为一个通道。

## GVCP
GigE Vision Control Protocol，负责发现、寄存器访问、控制权与控制命令。Avoid: 把 GVCP ACK 等同于 UDP 可靠传输。

## GVSP
GigE Vision Stream Protocol，通过 UDP 数据报传输图像帧 leader、payload、trailer。Avoid: 将 sendto 成功视为客户端已收到。

## CCP
Control Channel Privilege，表示 GVCP 控制权状态。Avoid: 与采集状态混为一谈。

## RegisterMap
唯一寄存器/特征映射来源，驱动 GVCP 寄存器语义与 GenICam XML 投影。Avoid: 分别维护一份独立 XML 默认值表。

## GenICam XML
描述相机特征、寄存器地址与节点关系的 GenApi 文档。Avoid: 把任意格式良构的 XML 当成 GenApi 合规。

## XML URL
Discovery ACK 公布的相机 XML 获取地址，host 必须与相机 IP 相符。Avoid: 用 GUI 本机名替代相机 IP。

## 虚拟相机
软件创建的一台本地 GigE Vision 相机实例；每台实例拥有独立生命周期的虚拟网络适配器。Avoid: 将其理解为绑定用户现有 WLAN/WSL/VPN 适配器。

## 虚拟网络适配器
由软件在新建虚拟相机时自动创建、为该相机专用的本地网络接口；相机停止时立即销毁。实现组件为 Wintun.dll。Avoid: 手工预建或跨相机复用适配器。

## Wintun.dll
随产品部署、由程序经用户态 API 创建和删除 Wintun 适配器的组件；它提供三层 IP 数据包收发，不是由本程序自行编写或注入的网卡驱动。Avoid: 将它称为 WLAN、以太网帧驱动或要求用户预建网卡。

## 虚拟相机配置
EXE 同级 `virtual-cameras` 目录内的一个 schemaVersion=1 JSON 文件，描述一台可选择启动的相机及其图像源。Avoid: 为每台相机要求专属子目录、从导出厂商 XML 原样模拟私有功能。

## 兼容 GenICam XML
由 RegisterMap 根据实际支持的寄存器和特征生成、供 MVS 读取的 XML 描述。Avoid: 导入或分发第三方厂商 XML、让未实现的私有节点出现在客户端。

## 图像序列目录
虚拟相机配置的 imagePath 指向目录时，按文件名排序循环其中的 BMP，并以配置的 frameRate 切换图像。Avoid: 固定只读取首张图、递归扫描子目录、将相对路径作为配置格式的一部分。

## Wintun L3 peer
由 Wintun 适配器承载的本机用户态 IP 对端：Windows 主机端持有适配器 IPv4，相机端 IPv4 仅在隧道报文中存在，程序负责两端 IPv4/UDP 转发与协议处理。Avoid: 要求相机端 IP 先绑定到 Windows 主机接口、将 Wintun 当成二层以太网驱动。

## 主机端地址
分配给本次创建 Wintun 适配器、供 MVS 使用的本机 IPv4。Avoid: 把它写成相机 JSON 的相机 IP。

## 相机端地址
配置 JSON 中声明、只在 Wintun L3 peer 报文内使用的远端 IPv4；GVCP Discovery、XML URL 和 GVSP 源均使用该地址。Avoid: 让 Windows 对该地址直接 Winsock bind。
