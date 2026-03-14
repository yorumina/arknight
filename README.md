# Arknight Demo

此專案使用 `PTSD` 框架，包含一個可玩的明日方舟核心玩法 demo（地圖載入、路線、波次、部署、DP/LP）。

## 如何執行

1. 下載專案（含 submodule）
```bash
git clone --recurse-submodules <your-repo-url>
cd Arknight
```
如果你已經 clone 過，請補抓 submodule：
```bash
git submodule update --init --recursive
```

2. 準備環境
- CMake 3.16+
- 可用 C++17 編譯器（MSVC / Clang / GCC）

3. 建置 Demo
```bash
cmake -S PTSD -B PTSD/build -DCMAKE_BUILD_TYPE=Debug
cmake --build PTSD/build --target Example
```

4. 執行 Demo
- Windows:
```powershell
.\PTSD\build\Example.exe
```
- Linux/macOS:
```bash
./PTSD/build/Example
```

## Demo 操作
- `滑鼠左鍵`：部署幹員
- `1`：選 Vanguard（只能放地面）
- `2`：選 Sniper（只能放高台）
- `SPACE`：開始關卡
- `R`：重開 demo
- `ESC`：離開

## 關卡來源
Demo 會優先載入下列關卡：
1. `tools/ark_builder/levels/test.json`
2. `tools/ark_builder/levels/tutorial_1.json`

可用 `ArknightBuilder` 編輯與驗證關卡：
```bash
cmake --build PTSD/build --target ArknightBuilder
PTSD/build/ArknightBuilder validate tools/ark_builder/levels/test.json
```
