# NASInstall

Nintendo Switch NAS 游戏安装工具 —— 直接从 NAS/HTTP 服务器安装游戏。

## 功能特性

- 🌐 **HTTP 协议支持** - 浏览和安装 HTTP 服务器上的游戏文件
- 📁 **多格式支持** - NSP、NSZ、XCI、XCZ 格式
- 💾 **多服务器管理** - 保存多个服务器配置，快速切换
- 📊 **安装进度显示** - 实时显示下载速度、进度、剩余时间
- ⏸️ **断点续传** - 支持 HTTP Range 请求，中断后可恢复
- 🔍 **文件浏览** - 目录导航、文件搜索、格式筛选
- 🎮 **Joy-Con & 触屏支持** - 完整的主机操作体验

## 系统要求

- Nintendo Switch 运行 Atmosphere 自定义固件
- 固件版本 18.0.0+（建议 22.5.0 / Atmosphere 1.11.2）
- SD 卡（至少剩余空间大于要安装的游戏）
- 局域网连接（WiFi）

## 安装

1. 下载 `NASInstall.nro`
2. 复制到 SD 卡的 `switch/NASInstall/` 目录
3. 在 hbmenu 中启动

## 使用方法

### 添加 HTTP 服务器

1. 启动 NASInstall
2. 选择 "Add New Server"
3. 按 A 键输入服务器 URL（例如 `http://192.168.1.100:8080/games`）
4. 按 X 保存服务器

### 安装游戏

1. 从主菜单选择 "Browse HTTP Server" 或从 "Saved Servers" 选择服务器
2. 浏览文件列表，找到要安装的 NSP/NSZ 文件
3. 按 A 键开始安装
4. 等待安装完成

## 从源码编译

### Windows 环境（推荐）

#### 1. 安装 devkitPro

- 下载 [devkitPro Updater](https://github.com/devkitPro/installer/releases)
- 运行安装程序，勾选 **Switch** 开发组件
- 默认安装路径：`C:\devkitPro`

安装完成后，确保以下环境变量已设置：
```
DEVKITPRO=C:\devkitPro
DEVKITA64=C:\devkitPro\devkitA64
```

#### 2. 安装依赖库

打开 "devkitPro MSYS2" 终端，执行：

```bash
sudo dkp-pacman -S switch-dev switch-curl switch-zlib switch-mbedtls
```

或使用 pacman：
```bash
pacman -S --noconfirm switch-dev switch-curl switch-mbedtls
```

#### 3. 编译项目

在 "devkitPro MSYS2" 终端中：

```bash
cd /path/to/NASInstall
make -j$(nproc)
```

编译完成后，项目根目录会生成 `NASInstall.nro`。

### Linux 环境

```bash
# 安装 devkitPro
wget https://apt.devkitpro.org/install-devkitpro-pacman
chmod +x ./install-devkitpro-pacman
sudo ./install-devkitpro-pacman

# 安装 Switch 开发包
sudo dkp-pacman -S switch-dev switch-curl switch-mbedtls

# 设置环境变量
export DEVKITPRO=/opt/devkitpro
export DEVKITA64=$DEVKITPRO/devkitA64

# 编译
cd NASInstall
make -j$(nproc)
```

### 编译选项

```bash
# 清理编译产物
make clean

# 并行编译（加速）
make -j4

# 调试版本
make DEBUG=1
```

## 项目结构

```
NASInstall/
├── source/           # 源代码
│   ├── main.c        # 主程序入口 & 状态机
│   ├── ui.c          # 用户界面渲染
│   ├── http.c        # HTTP 客户端 & 文件列表解析
│   ├── install.c     # NSP 安装引擎
│   └── config.c      # 配置管理（服务器列表保存）
├── include/          # 头文件
│   ├── types.h       # 类型定义
│   ├── ui.h
│   ├── http.h
│   ├── install.h
│   └── config.h
├── data/             # 二进制数据文件
├── romfs/            # ROMFS 资源（运行时挂载）
├── Makefile          # 构建配置（libnx 标准模板）
└── README.md
```

## 技术栈

- **语言**: C17
- **UI**: libnx console（文本模式）
- **网络**: libcurl + mbedTLS
- **安装**: NCM (Content Manager) + ES (Ticket)
- **构建系统**: Make + devkitPro

## 路线图

### v0.1 (MVP)
- [x] HTTP 目录浏览
- [x] NSP 安装基础框架
- [x] 多服务器保存
- [x] 控制台 UI

### v0.2
- [ ] 完整的 NSP 流式安装
- [ ] 断点续传支持
- [ ] NSZ 格式支持
- [ ] 安装后验证

### v0.3
- [ ] SMB 协议支持
- [ ] 内网设备自动发现 (mDNS/SSDP)
- [ ] 游戏封面显示
- [ ] 图形化 UI

### v1.0
- [ ] XCI/XCZ 格式支持
- [ ] NFS 协议支持
- [ ] 批量安装
- [ ] 安装队列管理

## 许可证

GPLv3 - 详见 LICENSE 文件

## 致谢

- devkitPro 团队 - 提供工具链
- libnx 项目 - Switch 自制软件开发库
- DBI 项目 - 灵感来源
- switchbrew 社区 - 文档和参考
