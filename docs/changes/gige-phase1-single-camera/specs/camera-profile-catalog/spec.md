## ADDED Requirements

### Requirement: 枚举虚拟相机配置
应用 MUST 在启动和用户刷新时枚举 EXE 同级 `virtual-cameras` 目录中的全部 JSON 文件，并将每个文件作为一项相机档案显示。

#### Scenario: 配置目录包含多个 JSON
- 当目录包含多个 JSON 文件时，用户在相机列表中看到每个文件对应的一项档案。

### Requirement: 显示不可启动档案
应用 MUST 保留无效 JSON、相对 imagePath、不存在图像源或空 BMP 目录的档案，并显示该档案不可启动的具体原因。

#### Scenario: 配置图像路径无效
- 当 JSON 的 imagePath 不是可用绝对路径时，该档案显示为不可启动且用户能看到原因。

### Requirement: 单相机档案切换
应用 MUST 在启动所选档案前停止当前相机，并只运行所选的一台相机。

#### Scenario: 切换已运行相机
- 当用户启动另一项有效档案时，原相机停止且新相机成为唯一运行相机。

### Requirement: 提供兼容设备描述
应用 MUST 向 MVS 提供根据实际已支持功能生成的兼容 GenICam XML，且不得读取、复制或分发第三方厂商 XML。

#### Scenario: MVS 读取设备描述
- 当 MVS 请求相机 XML 时，获得仅包含当前程序支持功能的兼容描述。
