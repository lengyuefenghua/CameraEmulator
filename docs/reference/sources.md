# 参考语料

本项目只将公开实现作为协议/节点契约参考，不复制其生产实现代码。源文件在本地下载到 `.reference/`（gitignored）。

## Aravis

- 项目：[Aravis](https://github.com/AravisProject/aravis)，固定 commit `482547a3d56e3aa8d22d7ffed33c9f1588bfe17a`。
- 许可证：主库 LGPL-2.1-or-later；每个文件的 SPDX/头部为最终依据。GenApi XSD 自带 EMVA GenICam license notice；下载副本不随本项目分发。
- `src/arv-fake-camera.xml`：相机功能节点与 feature-register 映射参考。
- `src/arv-gev-bootstrap.xml`：标准 GVCP bootstrap feature 和 register 映射参考。
- `src/GenApiSchema_Version_1_1.xsd`：输出 XML 的结构校验 schema。
- `src/arvgvcp.c`、`src/arvgvcpprivate.h`：GVCP header、命令/ACK 编码参考。
- `src/arvgvsp.c`、`src/arvgvspprivate.h`：GVSP header 与 leader/data/trailer 布局参考。
- `src/arvgvfakecamera.c`、`src/arvfakecamera.c`、`tests/fakegv.c`：假相机寄存器语义、发现与取流测试参考。
- `tests/data/genicam.xml`：GenApi 节点解析测试样本；不作为相机功能节点清单。

获取命令：`pwsh -File tools/fetch-reference.ps1`。

## 交叉核对

- [Aravis Wireshark dissectors](https://github.com/AravisProject/aravis-misc/tree/main/wireshark-dissectors)：协议报文独立解码参考；不复制源码。
- [viva-genicam / viva-fake-gige](https://github.com/VitalyVorobyev/viva-genicam/tree/main/crates/viva-fake-gige)：独立 Rust 模拟器实现，用于核对行为边界；非兼容性认证来源。
- 真实 MVS XML：当前未提供、开发机未安装 MVS。若后续提供，放入 `.reference/vendor/` 并在变更记录中写明来源与许可证。
