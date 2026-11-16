# 第三方参考与许可

本项目按 GPL-3.0-or-later 发布，完整许可证见 LICENSE。

## PS5-Suite

- 作者：manos555555 及上游贡献者。
- 仓库：https://github.com/manos555555/PS5-Suite
- 参考版本：9aebf59fa756dd2b7e86987debf1e72520a3c43d。
- 上游许可：GPL-3.0。
- 参考文件：`payload/main.c` 的 Trophy Viewer 和 UCP 结构；`client-avalonia/MainWindow.Trophies.cs` 的 trophy JSON 与 T2PD 记录格式。
- 本项目重新实现了有边界检查的只读文件读取器、HTTP 接口与中文网页。解锁状态只使用匹配定义的状态行，不使用上游解锁/重锁功能，不采用位图猜测回退。
- 上游对格式的验证不等于本插件已经通过实机验证。

## PS5 Payload SDK

编译使用 ps5-payload-dev SDK v0.42，SDK 不包含在本项目源码包中。

- https://github.com/ps5-payload-dev/sdk
- SDK 各组件许可证以其自身文件为准。

## 页面参考

用户提供的三张 PSN 页面截图仅作为交互层级、卡片、进度和奖杯排列的设计参考；不将截图中的账号、游戏进度当作主机读取数据。奖杯图形为本项目 SVG，游戏与奖杯图片在实机运行时从本地奖杯包读取。

## 用户昵称读取

接口声明参考 PS5-Suite 的 UserService 声明与 PS5 Payload SDK browser 示例。新增 `profile.c` 使用独立实现的单线程队列、超时等待与昵称缓存。

## 游戏封面回退

本地化 `icon0_<语言>.png` 与 `npbind.dat` 绑定路径参考上述 PS5-Suite 的 Trophy Viewer 和游戏图片读取流程；本项目仅接受匹配到的本地 PNG，不扫描不相关的存储目录，不依赖网络封面服务。

## 自动接替

协作 STOP 和文件锁交接设计参考工作区 PS5 Turbo 的 `payload/upgrade.c`（GPL-3.0-or-later），本项目 `lifecycle.c` 实现奖杯插件专用目录、版本记录与安装文件消失监测，不复用连发插件的锁或停止标记。
