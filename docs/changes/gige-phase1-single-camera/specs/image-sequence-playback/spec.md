## ADDED Requirements

### Requirement: 使用绝对图像源路径
虚拟相机配置的 imagePath MUST 是绝对路径，并可指向一个 BMP 文件或一个包含 BMP 的目录。

#### Scenario: 使用相对图像路径
- 当配置提供相对 imagePath 时，该档案不可启动并显示路径不合规原因。

### Requirement: 发送单图像源
应用 MUST 在 imagePath 指向有效 BMP 文件时持续发送该图像，并按相机的 frameRate 生成图像帧。

#### Scenario: 采集单张 BMP
- 当用户启动图像文件档案并在 MVS 开始采集时，MVS 连续收到由该 BMP 生成的图像帧。

### Requirement: 循环目录图像源
应用 MUST 在 imagePath 指向目录时按文件名排序循环其中 BMP，并按相机的 frameRate 切换发送图像。

#### Scenario: 采集 BMP 目录
- 当用户启动包含多个 BMP 的目录档案并在 MVS 开始采集时，MVS 按排序顺序循环收到目录图像帧。

### Requirement: 处理运行期图像失败
应用 MUST 跳过运行期无法读取或解码的目录 BMP；一个完整循环没有可用图像时，应用 MUST 停止采集并显示失败原因。

#### Scenario: 目录图片全部失效
- 当正在采集的目录中已无可解码 BMP 时，采集停止且诊断显示图像源失败原因。
