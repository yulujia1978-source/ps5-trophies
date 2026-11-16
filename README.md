# PSN 奖杯 · PS5 Trophies

在折腾版 PS5 上读取本地奖杯，通过手机或电脑浏览器查看。无需登录 PSN，不修改或解锁奖杯。

作者：**snkwz** · 当前版本：**v0.2.3** · GPL-3.0-or-later

## 功能

- 按本地用户展示已获得过奖杯的 PS5 游戏，包括卸载后保留的奖杯记录。
- 查看游戏封面、奖杯名称与说明、等级、解锁状态、时间、分组及完成进度。
- 游戏搜索、奖杯筛选与排序、个人奖杯统计。
- 深蓝光效、PSN 浅色、简洁深色三种主题，浏览器记忆选择。
- 各页面均可直接回首页，顶部显示实际运行版本。
- 新版启动自动请求旧版退出；网页断开后自动重连。
- 对唯一且可验证的标准 Payload Manager 安装，删除对应 ELF 后自动退出。

## 使用

1. 获取 `releases/ps5-trophies-v0.2.3.elf`。
2. 在支持运行 payload、允许读取本机奖杯目录的 PS5 环境中，通过 Payload Manager 导入并启动。
3. 手机或电脑连接同一局域网，在浏览器打开：

   ```text
   http://你的PS5局域网IP:6082
   ```

4. 选择用户与游戏查看奖杯。点击刷新重新读取记录。

网页已嵌入 ELF，不需要额外复制网页文件。插件不会自动注册开机启动。

## 更新与停止

正常更新可直接导入并启动新版，插件会请求旧版正常退出，最多等待 15 秒，不强制杀进程。重复启动同版或降级不会打断当前服务。

第一次升级至带自动重连逻辑的版本后，手动刷新一次网页。之后运行版本或会话变化时，网页会自动刷新。

手动停止：创建空文件 `/data/ps5-trophies/STOP`。重启主机也会结束服务。

删除退出仅在插件启动时能够唯一验证安装位置、标准文件名、元数据与 ELF 身份时启用。无法确认的位置不启用此监测，但仍支持新版启动时自动接替。

## 支持范围与已知问题

- 当前支持 PS5 原生 Trophy2 记录；PS4 兼容游戏暂不支持。
- 昵称和图片取决于本地系统服务、文件与访问权限；缺失时显示用户 ID 或默认图标。
- 无法识别的解锁状态显示为未知，不把它当作未解锁，相关完成百分比暂不显示。
- 进度按奖杯点数计算，含追加内容时与正式版分组百分比可能不同。
- 不上传截图、录像，不进行云同步，也不提供 PSN 在线功能。
- 网页没有账户登录，供可信局域网使用。
- **曾收到与连发插件同时运行时连发失效、游戏或主机卡住的反馈，兼容性仍在排查。** v0.2.3 不代表已经修复这一问题。

用户已验证早期版本的主机读取、手机和电脑访问，以及奖杯游戏列表。v0.2.3 的升级交接、删除退出和重连已通过电脑端测试，仍需更多主机实测。

## 日志

| 文件 | 内容 |
|---|---|
| `/data/ps5-trophies/startup.txt` | 最近启动/退出结果；`watch=enabled` 表示启用删除监测 |
| `/data/ps5-trophies/users.log` | 用户服务返回码 |
| `/data/ps5-trophies/icons.log` | 游戏封面查找来源 |

反馈问题时请附运行版本、固件、加载环境、复现步骤和有关日志；分享前自行去除不希望公开的用户 ID。

## 本地测试与页面预览

需要主机 C 编译器、make、Python 3 和 Node.js 18+。

```sh
make test
python3 tests/fixtures.py build/fixture
./build/trophy-server "$PWD/build/fixture" 6082
```

打开 `http://127.0.0.1:6082/` 验证构造数据的读取，或 `http://127.0.0.1:6082/?demo=1` 查看界面演示。页面会标明测试数据。

`releases/网页演示.html` 可以直接在浏览器打开，无需 PS5；只演示界面，数据为虚构。

## 编译 PS5 ELF

需要 PS5 Payload SDK v0.42、支持该 SDK 的 Clang/LLD 工具链和 `llvm-config`。可使用已安装的 SDK，或下载固定版本：

```sh
bash tools/setup-sdk.sh
make LLVM_CONFIG=/absolute/path/to/llvm-config
```

已有 SDK 时：

```sh
make PS5_PAYLOAD_SDK=/absolute/path/to/ps5-payload-sdk LLVM_CONFIG=/absolute/path/to/llvm-config
```

脚本会校验 SDK 下载文件的 SHA-256。SDK 和编译工具不包含在仓库中。SDK 安装说明见 [官方仓库](https://github.com/ps5-payload-dev/sdk)。

产物为 `releases/ps5-trophies-v0.2.3.elf`。前端由 `tools/embed.py` 重新嵌入；仓库不需要保存生成的 `payload/assets.h`。

## 项目结构

- `payload/`：只读奖杯解析、用户昵称、图片读取、HTTP 服务与生命周期管理。
- `web/`：网页源码，无第三方前端运行时依赖。
- `tools/`：SDK 下载校验、网页嵌入与独立演示生成。
- `tests/`：自动测试和合成样本生成器，不包含用户主机奖杯数据。
- `releases/`：当前版本 ELF、校验值和离线演示。

## 来源与许可

文件格式参考 [PS5-Suite](https://github.com/manos555555/PS5-Suite/tree/9aebf59fa756dd2b7e86987debf1e72520a3c43d)，编译使用 [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk)。具体说明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 与 [LICENSE](LICENSE)。
