# KidDraw - 儿童电子画板

基于 C++ + SDL2 的儿童电子画板程序。使用自实现的 Bresenham 画线、中点画圆等图形学算法完成全部像素级渲染。

## 功能

| 功能 | 说明 |
|------|------|
| 🖌️ 自由绘制 | 按住鼠标拖动画线，记录轨迹 |
| 🎨 10 色调色板 | 红/橙/黄/绿/蓝/紫/粉/棕/黑/白 |
| 📏 3 档笔粗 | 小(S) / 中(M) / 大(L) |
| 🧹 橡皮擦 | 切换擦除模式，用白色覆盖 |
| ↩️ 撤销 | 撤销最近一笔 |
| 🗑️ 清除 | 一键清空画布 |
| 💾 保存 | 导出为 BMP 图片 |
| ⭐💚😊 印章 | 点击放置星形/爱心/笑脸 |

## 编译 & 运行

### Linux

```bash
# 安装依赖
sudo apt install libsdl2-dev cmake g++

# 构建
cmake -B build && cmake --build build

# 运行
./build/kid_draw
```

### macOS

```bash
brew install sdl2 cmake
cmake -B build && cmake --build build
./build/kid_draw
```

### Windows (Visual Studio)

```bash
# 1. 安装 vcpkg (如未安装)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat
vcpkg install sdl2:x64-windows

# 2. 生成 VS 工程
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg路径]/scripts/buildsystems/vcpkg.cmake

# 3. 打开 build/KidDraw.sln 编译运行
```

## 操作说明

- **选颜色**：点击工具栏上方彩色圆钮
- **调笔粗**：点击 S / M / L 三个圆
- **橡皮擦**：点击橡皮按钮切换模式
- **撤销**：点击 ↩ 按钮
- **清除画布**：点击 🗑 按钮
- **保存作品**：点击 💾 按钮，自动生成 BMP 文件
- **用印章**：先点 ⭐/💚/😊，再点画布放置

## 技术要点

- 全部图元（线、圆、按钮、图标）由 **Bresenham 和中点圆算法** 逐像素绘制
- 画布使用 SDL2 的纹理渲染目标实现双缓冲
- 鼠标轨迹通过圆点插值实现连续等粗笔触
- 印章图案使用参数方程 + 几何变换（缩放/平移）
- 撤销功能通过保存笔画历史 + 重建画布纹理实现

## 项目结构

```
kid-draw/
├── CMakeLists.txt      # 跨平台构建
├── README.md
└── src/
    └── main.cpp        # 完整源码（单文件）
```

## 提交物清单

- [ ] `src/main.cpp` — 源代码
- [ ] `README.md` — 说明文档
- [ ] 运行时截图 × 3（画板界面 / 绘制效果 / 印章效果）
