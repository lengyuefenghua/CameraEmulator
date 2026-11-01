# Tasks

> 设计基线：见 `change.md` 的协作卡与流程契约。

## 1. Tests

- [x] 1.1 在 `tests/unit/test_ipv4_udp_packet.cpp` 为 IPv4/UDP 解封装与封装添加 RED 测试，后果：错误报文被当作 GVCP/GVSP、校验和或端点错误导致 MVS 无图；seam：`Ipv4UdpPacketCodec` 的纯字节输入/输出。
- [x] 1.2 运行 `cmake --build --preset windows-debug --target camera_tests && .build\windows-debug\camera_tests.exe "[Ipv4UdpPacket]" --success`，确认 RED 因编解码器不存在而失败。
- [x] 1.3 在 `tests/integration/test_wintun_peer_loopback.cpp` 增加可跳过的 Windows 管理员集成测试，后果：Wintun peer 生命周期泄漏或 MVS 主机端与相机端报文无法互通；seam：`WintunPeer::start/receive/send/stop`。测试仅在管理员及 `wintun.dll` 可加载时执行，否则明确 skip。
- [x] 1.4 运行 `cmake --build --preset windows-debug --target camera_tests && .build\windows-debug\camera_tests.exe "[WintunPeer]" --success`，确认 RED 因 peer 尚未实现而失败或跳过前置环境检查。

## 2. Implementation

- [x] 2.1 新增 `src/protocol/ipv4_udp_packet.hpp/.cpp`，实现协作卡的 IPv4/UDP 字节边界：解析、校验、封装和最小 TCP 识别；拒绝 IPv6/分片/损坏报文。日志点：无；纯编解码器通过错误返回表达失败。关键注释说明网络字节序与校验和不变量。（blocked by 1.2）
- [x] 2.2 将编解码器接入 `CMakeLists.txt` 与 `tests/unit/test_ipv4_udp_packet.cpp`，运行 `cmake --build --preset windows-debug --target camera_tests && .build\windows-debug\camera_tests.exe "[Ipv4UdpPacket]" --success` 确认 GREEN。（blocked by 2.1）
- [x] 2.3 新增 `src/infra/win32/wintun_peer.hpp/.cpp`，动态加载 EXE 同级 `wintun.dll`、创建唯一命名适配器、用 IP Helper 配置主机端 IPv4 与仅指向相机端的 /32 路由、启动 Wintun 会话并提供收发 IPv4 入口；停止时按反序删除路由、地址、会话和本次创建的适配器。日志点：创建/地址/路由/会话/销毁 INFO，失败 ERROR，非目标或损坏隧道报文限流 WARN，含相机标识、适配器名和系统错误。关键注释说明 Wintun L3 方向和资源所有权。（blocked by 2.2）
- [x] 2.4 将 `wintun.dll` 部署规则、`WintunPeer` 和管理员条件集成测试接入 `CMakeLists.txt`，运行 `cmake --build --preset windows-debug --target camera_tests && .build\windows-debug\camera_tests.exe "[WintunPeer]" --success` 确认 GREEN 或受控 skip。（blocked by 1.4、2.3）
- [x] 2.5 修改 `src/engine/camera_instance.hpp/.cpp` 与 `src/engine/stream_controller.hpp/.cpp`，把协作卡上的 GVCP/GVSP 出入站从直连 Winsock 改为 `WintunPeer` IPv4/UDP；保留 `[有]` GVCP/GVSP 编解码与 RegisterMap。日志点：协议处理 ERROR 含相机端/主机端/协议错误，非目标报文 WARN 限流。（blocked by 2.4）
- [x] 2.6 在 `tests/unit/test_tunnel_tcp.cpp` 为纯 TCP 会话状态机添加 RED/GREEN 测试，后果：错误握手/确认/关闭使 MVS 无法读取 XML；seam：`TunnelHttpServer::receive/send_due` 的 TCP 段输入/IPv4 段输出。新增 `src/infra/tunnel_http_server.hpp/.cpp`，实现 SYN/SYN-ACK/ACK、HTTP GET、响应确认、FIN/ACK 与超时重传，并修改 `src/engine/camera_instance.hpp/.cpp` 接线。日志点：XML TCP 会话失败 ERROR，非支持请求 WARN。（blocked by 2.5）
- [x] 2.7 修改 `src/engine/engine_facade.hpp/.cpp`、`src/app/qt/main_window.hpp/.cpp` 和打包脚本：启动入口传递 peer 生命周期状态，GUI 显示适配器名、主机端/相机端地址和创建/销毁错误，EXE 请求管理员权限且部署 `wintun.dll`。日志点：切换/权限/启动/停止 INFO 或 ERROR，含配置与适配器标识；无 GUI 自动化，用户目视验收。（blocked by 2.5、2.6）
- [ ] 2.8 运行 `cmake --build --preset windows-debug --target camera_emulator camera_tests && ctest --preset windows-debug --output-on-failure`，确认自动测试通过。（blocked by 2.2、2.4、2.7）
- [ ] 2.9 [人工] 以管理员运行打包版：启动一份有效 JSON，确认自动创建专属适配器和不同的主机端/相机端 IP；在 MVS 发现、读取 XML、连接、连续取图；停止后确认适配器消失；切换另一份配置并重复。不做 GUI 自动化或截图。（blocked by 2.8）

## 3. Review

- [ ] 3.1 加载 `review-change` 审查本变更 diff（Spec、契约、Standards 三轴），修复 Critical/Important。
- [ ] 3.2 运行最终验证：`pwsh -NoProfile -File tools\build.ps1 package`、`ctest --preset windows-debug --output-on-failure` 和 `pwsh -NoProfile -File tools\build.ps1 xml-test`。

## 4. Self-review

- [ ] 4.1 自审：逐条对照需求卡验收标准、协作卡节点、流程契约、`specs/**/spec.md` 与本任务清单，确认完整实现、无遗漏；有缺口先修复再重审。

## 5. Commit

- [ ] 5.1 [人工] 人工验收通过且用户明确许可后，仅提交本变更相关文件：`git add -- <相关文件> && git commit -m "feat(wintun-peer): 接入专属虚拟相机网络端点"`；不 push。
