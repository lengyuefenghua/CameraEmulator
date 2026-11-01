## ADDED Requirements

### Requirement: 隧道内兼容 XML 获取
应用 MUST 在 Wintun peer 内以相机端 IPv4 提供兼容 GenICam XML 的 HTTP 响应，并完成最小 TCP 的连接握手、数据确认、响应关闭和超时处理；内容仅由已支持 RegisterMap 功能生成。

#### Scenario: MVS 读取相机 XML
- 当 MVS 根据发现应答中的 XML URL 发起 HTTP 请求时，它获得与当前相机端地址一致的兼容 XML。
