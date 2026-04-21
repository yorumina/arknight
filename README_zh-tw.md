# Arknight（明日方舟核心玩法 Demo）

此專案基於 `PTSD` 框架，實作可遊玩的明日方舟核心玩法原型（地圖、路線、波次、部署、DP/LP）。

## 環境需求

- CMake 3.16+
- 支援 C++17 的編譯器（MSVC / Clang / GCC）

## 建置與執行

1. 下載專案（含 submodule）
```bash
git clone --recurse-submodules <your-repo-url>
cd Arknight_Linux
```

2. 建置
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Arknight
```

3. 執行
- Linux/macOS
```bash
./build/Arknight
```
- Windows
```powershell
.\build\Arknight.exe
```

## 操作方式

- `滑鼠左鍵`：部署幹員
- `1`：選擇 Vanguard（僅地面）
- `2`：選擇 Sniper（僅高台）
- `SPACE`：開始波次
- `R`：重開
- `ESC`：離開

## 資料目錄

遊戲資料已統一放在 `data/`：

- `data/levels`
- `data/enemy`
- `data/operators`

Demo 會優先嘗試載入：

1. `data/levels/test.json`
2. `data/levels/tutorial_1.json`

## ArknightBuilder

建置：
```bash
cmake --build build --target ArknightBuilder
```

範例：
```bash
./build/ArknightBuilder validate tutorial_1.json
```

`ArknightBuilder` 現在會自動把關卡檔參數映射到 `data/levels/`。  
完整文件請參考：

- 英文：`docs/arknightbuilder/README.md`
- 繁中：`docs/arknightbuilder/README_zh-tw.md`
