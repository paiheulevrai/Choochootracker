# ChooChooTracker 用户手册
> 这是从当前英文手册自动生成的 `docs/USER_MANUAL.md` 翻译版本。如与英文内容有差异，以英文文本为准。
>
> **约定**：应用界面为英文。请保留屏幕、菜单、字段和按钮名称，以及 FX 代码、模块和引擎名称、文件扩展名均使用英文。

ChooChooTracker 是一款适用于掌机的 8 轨音乐追踪器。

它遵循经典 LSDj 的工作流程和导航系统，提供多种声音设计选项：

- 复古芯片和掌机模拟
- Soundfont 支持
- Mutable Instruments Braids、Plaits 和 Plaits-Alt 振荡器，带多模式滤波器
- PCM 采样、单周期波形和波表振荡器，带多模式滤波器
- aChChid，多功能基于 Open303 的酸性合成器
- Bogie，本机 12 模式鼓合成器，带多模式滤波器
- MonkSynth，人声合成器，带十声部齐奏和立体声延迟
- MME，双振荡器多调制合成器，带多模式滤波器
- Sintered，本机六模式实验数字鼓合成器

## 1. 安装与文件

### ArkOS / PortMaster

通过 PortMaster 安装 `choochootracker.zip`，或将解压后的包复制到掌机的 `ports` 文件夹。通过 Ports 菜单启动 **ChooChooTracker**。

### Miyoo Mini / Mini Plus (Onion OS)

解压 MiyooPorts 压缩包到 SD 卡根目录，使其 `Roms/PORTS/` 文件夹与 Onion 系统的文件夹合并。刷新收藏后从 Ports 菜单启动 **ChooChooTracker**。替换已安装版本前，请备份应用的 `projects/` 文件夹。

项目的加载时间可能因歌曲中使用的乐器类型而异。如果启动或加载项目异常缓慢，请说明每次所需时间，并附上主机型号和 Onion OS 版本。

### Raspberry Pi OS 64 位

解压 Raspberry Pi AArch64 压缩包，安装 SDL2 运行库（`sudo apt install libsdl2-2.0-0`），然后在解压目录中运行 `./chipnomad.sh`。请将内置乐器和其他资源放在可执行文件旁边。

### Windows

运行 `choochootracker.exe`，并将 `SDL2.dll` 和 `libwinpthread-1.dll` 放在同目录。

Windows 支持键盘输入（详情见 Jazz 模式部分）或手柄。

### Web

在线版本地址：https://choochootracker.vercel.app/

可使用键盘、移动设备上的屏幕控件或游戏手柄控制。

保存的设置、采样、主题、项目等存储在浏览器本地。

使用 **Download .cct** 导出项目。

注意：由于 iOS 限制，iPhone 上的 Web 版本可能无法使用 MIDI。

### 目录配置

| 文件夹 | 内容 |
|---|---|
| `AY_Wavetables` | `.aywave` AY 芯片波表 |
| `fonts` | `.cnfont` ChipNomad 字体文件 |
| `instruments` | `.cni` 乐器预设，也包含 FM 音色库和 Soundfonts |
| `pitch-tables` | `.csv` 备用调音表 |
| `projects` | `.cct` 曲目和项目文件 |
| `samples` | `.wav` 采样引擎音频文件 |
| `SR_wavetables` | BYOWTBL 引擎使用的 Serum 格式波表 |
| `themes` | `.cth` ChipNomad 主题 |
| `title` | 标题画面素材 |
| `waveforms` | `.wav` 2xSCWF 引擎单周期波形 |

## 2. 操控

ChooChooTracker 最适合使用配备方向键、2 个模拟摇杆和 7 个按钮的手柄。

| 逻辑控制 | 掌机默认 | Windows 默认 |
|---|---|---|
| **[LEFT/RIGHT/UP/DOWN]** | 方向键（D-pad） | 方向键 |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Space |
| **SELECT** | Select | Shift |
| 调制轴 | 左右摇杆 | 手柄摇杆 |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

映射可在 **Settings > Controls > Key mapping** 中更改，Android 上连接外部手柄时同样适用。在 PortMaster 上，保存映射请点击 **Done**，然后先释放按键，再继续操作设置。

### 导航

- 使用 **[UP/DOWN/LEFT/RIGHT]** 移动光标。
- 按住 **SELECT + [方向]** 在屏幕间切换。
- 按住 **OPT + [方向]** 执行特定屏幕导航。
- 选中数值时，使用 **EDIT + [LEFT/RIGHT]** 进行微调，**EDIT + [UP/DOWN]** 做大幅调整。
- 轻按 **EDIT** 进入数值输入或激活命令。

### 选择与剪贴板

- **SELECT + OPT** 进入选择模式，循环切换实用的选择区间。
- 使用 **[方向]** 扩展选择范围。
- 单击并释放 **OPT** 复制选中内容并退出选择模式。
- **OPT + EDIT** 剪切内容。
- 非选择模式下，按住 **SELECT** 并按 **EDIT** 粘贴。
- **EDIT + [方向]** 同时编辑选中单元。

### 播放

- **PLAY** 从当前光标处开始播放。再按一次停止。
- 播放时可编辑音符、乐器和链，变更将在下一个序列节拍生效。
- **SELECT + PLAY** 在非 Song 屏幕启动所有轨道。
- **EDIT + PLAY** 从乐器池预览乐器。

### 全局合成采样（Global Bounce）

选中 Song、Chain 或 Phrase 的行和列范围后，双击 **EDIT** 打开 BOUNCE TO SAMPLE 界面。合成采样包含所有选中轨道：其首条链条较晚开始的轨道会静默等待，并在首链条对应位置加入，确保文件包含全部轨道链条。

界面包含：

- **文件名**：使用字符键盘编辑。打开时默认显示未使用的序号（`001`、`002` 等），对应导出文件夹中尚不存在的 `.wav` 文件名。可自由修改，文件名直接使用。若文件已存在，会自动添加 `_001`、`_002` 等后缀，避免覆盖。载入或新建项目时序号重置。
- **采样率** 和 **位深度**：与导出界面相同选项。
- **包含至采样名**：三个复选框，勾选后合成文件名会添加对应标签（默认全不勾）：
  - **[BPM]** — 当前项目节拍速度，如 `[120]`
  - **[Key]** — 项目主音调及音阶，如 `[Cmaj]`、`[F#min Pent]`
  - **[Bars:Beats:16ths]** — 合成采样长度：一词组为一小节（16 个 16 分音符），一拍为 4 个 16 分音符。完整词组显示小节数，如 `[2]`；部分词组显示 `[0:2]`（两拍）或 `[0:1:3]`（一拍加三 16 分音符）。多轨合成以最长轨道长度为准。若选择在拍中间结束，则长度减少一个 16 分音符：结束行为切割点，音符关闭点位于上一 16 分音符末尾。
  - 标签按顺序 `[BPM][Key][Length]` 添加于文件名前。
- **Start**：渲染选区成 WAV 文件至导出目录（见 Project 屏幕的 *Export location*）
- **Cancel**：取消并返回

渲染时按 **OPT** 可取消。完成或取消后返回原始屏幕。

## 3. 曲目结构与屏幕映射

按住 **SELECT + [方向]** 在屏幕间切换。

主导航行为：

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

中央行：

- **M**：Mixer（分支到 Reverb、Delay）
- **S**：Song（分支到 Project、Settings）
- **C**：Chain
- **P**：Phrase（分支到 Groove）
- **I**：Instrument（分支到 Modulation 和 Instrument Pool）
- **T**：Table（分支到 AY Wavetable 编辑器）

顶层行：

- **R**：Reverb
- **P**：Project
- **G**：Groove
- **M**：Modulation
- **I**：Insert FX

底层行：

- **D**：Delay
- **S**：Settings
- **P**：Instrument Pool
- **W**：AY Wavetable 编辑器

ChooChooTracker 曲目结构示意：

```text
Song -> Chain -> Phrase 行 -> 音符 + 乐器 + FX
```

一首曲目有 8 个轨道。每轨包含一系列链条，每个链条包含一系列词组。词组为 16 步音符序列。每个音符包含音高、乐器、力度及最多 3 个轨道 FX。乐器即产生声音的对象。

## 4. Song、Chain 和 Phrase

### Song

Song 界面包含 8 列，对应 8 个音轨。每个单元格指代一个链条。曲目最多 256 行（`00-FF`）。

轨道从左到右排列，链条纵向排列。每轨独立，链条可包含 1 到 16 个词组，轨道可错开同步。请自行设计链条避免走样，或根据需求允许漂移。

追踪器社区通常保留一个链条专用于空词组，常用 `00` 或 `FE`。

你可以通过“孤岛”方式保存多个子曲段：由空行分隔的段落。

#### 现场模式（Live Mode）

双击 **OPT** 进入或退出现场模式，Song 标题显示为 `LIVE`。现场模式下，**PLAY** 会从轨道停止状态播放选中链条；若轨道已播放，将该链条加入至下一个链条边界队列；连按两次 **PLAY** 可于下一词组边界紧急切换。选中空 Song 单元时排队停止。`+` 表示排队链条，`!` 为紧急链条，`-` 为排队停止。

单行多列选中时可统一排队或停止对应轨道。多行选中被拒绝。曲目范围循环时现场队列会清空。现场模式为演出模式，项目不保存。

#### 控制

- **OPT + [UP/DOWN]**：上/下跳 16 个位置
- 选中范围后，**SHIFT + EDIT**：浅层克隆链条，词组保持不变
- 选中范围后，连击两次 **SHIFT + EDIT**：深度克隆链条和词组
- 选中范围后，**EDIT + [UP/DOWN]**：移动选区上下位置
- 连按 3 次 **OPT**：添加或取消高亮，方便视觉管理
- **OPT + SHIFT**：静音当前轨道或选中列（先释放 **OPT** 保持静音状态）
- **OPT + PLAY**：独奏当前轨道或选中列（先释放 **OPT** 保持独奏状态）
- **OPT + [LEFT/RIGHT]**：独奏当前轨道左侧或右侧所有轨道
- 选中范围后，双击 **EDIT**：打开 BOUNCE TO SAMPLE 界面（见 Song 中的合成采样）

停止播放以及更改静音或独奏会使用 3 ms 的短暂输出修正以减少爆音。这也包括在播放期间取消静音和移除独奏。

### Chain

Chain 为一组有序的 16 步词组序列，可设置可选转调。最多含 16 个词组，同一词组可多次出现。第二列用于设定转调半音数。

带星号（`*`）表示该链条在项目内被复用。可在 Song 界面克隆链条。

#### 控制

- **OPT + [LEFT/RIGHT]**：轨道间切换
- **OPT + [UP/DOWN]**：同轨道内链条切换
- 选中范围后，**SHIFT + EDIT**：克隆词组
- 选中范围后，双击 **EDIT**：打开当前轨道选中链条行的合成采样界面（见 Song 中合成采样）

### Phrase

Phrase 是传统步进序列器的轨道模式。

一个词组共有 16 步，每步一行。每行包含音符、乐器、`00` 到 `7F` 的音量，以及 FX 列。音符采用追踪器记谱法，如 `C-4`（音符+八度）。

停止音符可插入 `NOTE OFF`，使用杀音 FX，或播放无乐器的新音符。

多词组链条时，Phrase 界面是连续模式：超出行 `F` 向下或 `0` 向上翻页时打开链内前后词组。

带星号（`*`）表示该词组在项目内被复用。可在 Chain 界面克隆词组。

FX 选择器展示常用命令及乐器支持命令，避免展示冗长的全局引擎专用列表。ChooChooTracker 支持的 FX 多于 ChipNomad。

### 控制- **OPT + EDIT** 在空白音符上：插入 `NOTE OFF`
- **EDIT + [UP/DOWN]** 在 FX 名称列上：打开 FX 选择界面
- **OPT + [LEFT/RIGHT]**：在轨道间切换
- **OPT + [UP/DOWN]**：在当前 Chain 的短句间切换
- 在乐器列选择一个范围后，使用 **SHIFT + EDIT**：克隆乐器
- 选择一个范围后，使用 **EDIT + [UP/DOWN]**：旋转短句行
- 选择一个范围后，双击 **EDIT**：打开当前轨道选中短句行的 BOUNCE TO SAMPLE 界面（详见 Song 下的 Bounce）

在此界面，有选区时，音符栏可填充节奏模式：

- **OPT + LEFT** 用固定的16步模式填充选区的音符格，每次按下切换到下一个模式。最后一个模式后清除音符，循环开始。模式对齐绝对短句行，部分选区只接收其内的步数。
- **OPT + RIGHT** 用随机生成的3-8个音符（7和8较少出现）填充选区音符，按住 **OPT** 可无限循环。
- **OPT + UP** 变异选区内已有音符：每按一次随机选择1-3个音符，将其在一个八度内随机移位（小幅度移位更常见）。第8次按下恢复开始时选区内的音符，然后循环重新开始。
- **OPT + UP** 仅选中音量列（无音符/乐器/FX格）时，随机化音量而非变异：第一次应用 Minimal 模式（每个音量在+-20内变动），第二次 Medium（+-50），第三次 Random（完全随机音量），第四次恢复起始音量。松开 **OPT** 重置循环为 Minimal。无音量的行保持不变。
- **OPT + DOWN** 将选区内切片采样器乐器的音符展开成半音阶连续运行：每个乐器的第一个音符保持不变，后续音符每个升半音，将重叠音符堆变为连续切片触发。多个切片乐器独立展开，非切片乐器音符不变，且不强制贴合工程音阶。音符展开后最后写入的音符成为下一次输入的起点，手动继续运行可拾取下一个切片。仅包含切片乐器时，每次按下都会展开；若存在非切片音符，第一次按下展开，之后切换为随机琶音。
- **OPT + DOWN** 在无切片乐器音符的选区中，作为随机琶音器，基于已有音符生成和弦。根据选区内音符数从 CRD 调色板选取和弦 —— 1-2音符选用三音和弦，3音符及以上可选任意和弦。根音随机落在选区的音域内（音阶关闭或轨道排除时为任意半音），按播放时CRD同样按音阶量化。每次按下重新随机分布和弦音符：随机、从低到高、从高到低、往返，第5次恢复原音符，下一次选择新和弦。状态栏显示当前和弦名，琶音活动时不增加或删除音符，松开 **OPT** 重置流程。
- 每次填充操作均先清除选区音符，再填充，使用最后选定的乐器；松开 **OPT** 重置循环至初始步骤。

### Key jazz（仅桌面端）

在 Phrase 屏幕按 **Esc** 切换 key jazz，一种 QWERTY 钢琴式音符输入模式（类似 [m8c](https://github.com/laamaa/m8c)）：

- `Z S X D C V G B H N J M` 和 `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` 以物理键位顺序跨两三个八度发音，AZERTY、QWERTY 和 QWERTZ 布局相同。
- 每次按键在当前行写入音符，光标向下一行移动。
- `[` 和 `]` 改变八度。
- **Shift + 方向键** 选择行列范围，单独方向键取消选择。
- **Ctrl+C/X** 复制或剪切所选列，或无选择时剪切当前行音符/乐器/音量；**Ctrl+V** 在光标处粘贴。
- **Delete** 删除整行（所有列），上方短句内容上移填补，光标停留当前行。
- **Backspace** 删除当前列下光标所在元素（或选中列内元素），下方内容上移，光标上移一行，类似文本编辑器行为。
- **Insert** 插入空白行，短句内容下移。
- **Ctrl+S** 保存工程，key jazz 外无效。
- 再次按 Esc 关闭 key jazz。

开启时，key jazz 完全接管 Phrase 屏幕的音符键（包括共享键位的 Edit/Opt/Motion 快捷），关闭后可恢复使用。

key jazz 还可独立在以下界面用 Esc 切换：

- **Song** 和 **Chain**：直接输入链或短句的十六进制索引（`0-9`、`A-F`），替代上下翻页。输入的第一位替换当前单元格内容，继续输入数字则追加（如先输“3”，再输“F”变成“3F”）。Song 中 **Shift+方向键** 选范围，**Delete** 删行并上移，**Backspace** 删列并上移，**Insert** 插入行，**Ctrl+C/X/V** 复制/剪切/粘贴所选行或当前行。该模式接管 Shift，故 Shift+Up/Right 不再跳转 Chain/Project，按 Esc 恢复。
- **所有含文本输入框的界面**：可直接用键盘输入，替代屏幕虚拟键盘，包括 Project 文件名、标题、作者名，乐器名，颜色主题名，音高表名，bounce 文件名及命名对话框（输入名、创建文件夹）。Backspace 正常删除，Shift 输入大写字母，Esc 一次切换全部界面，关闭时虚拟键盘仍可正常使用。
- **Instrument**：打开时键盘按键播放当前轨道乐器，试听用，无实际音符录入，布局同 Phrase 页面，最后按键持续发音，松键停止。八度键切换八度，起始八度为乐器首个音符八度。Name 字段则输入文本，播放时该轨道播放状态时无声。

## 5. 乐器

所有乐器具备一些共通参数：

- 乐器类型
- 名称，最多15字符
- 默认表格速度，以 ticks/行计
- 音量
- 声像
- 是否开启移调

每个乐器都有一个默认表格，编号与 `00-7F` 范围内的一致。可以将它们用作辅助表，但将辅助表放在 `80-FE` 范围避免冲突和混淆。表格是 ChooChooTracker 主要的音色设计工具之一。

### 控制

- **OPT + [LEFT/RIGHT]**：切换乐器
- **EDIT + PLAY**：试听乐器
- **SHIFT + OPT**：复制乐器
- **SHIFT + EDIT**：粘贴乐器

### SOUNDFONT（实验性）

将原始 SF2/SF3 银行、带样本的 SFZ/EXS 乐器或 ZIP 档复制到 `instruments/USER/soundfont`。在 Instrument 页选择 **SOUNDFONT**。扫描行显示 **Not scanned**、**Scanning...** 或 **Scanned**，并带有 **Rescan** 操作。扫描读取元数据，不会将每个预设的样本加载到内存。

**Preset** 行显示当前激活的预设。点击并松开 EDIT 打开浏览器：左侧显示合集，右侧显示预设。每个合集显示扁平列表，包括银行文件或 ZIP 档内的预设。支持 WAV/AIFF 文件与其映射乐器绑定；仅含单个采样的合集将样本列为预设。显示标签去除下载 ID 和档案哈希，保留音符、BPM 和录音带编号。EDIT 选中预设；EDIT+PLAY 试听选中内容。

按住 EDIT，按 Preset 的左右键可按浏览器顺序逐个切换，继续进入下一个合集，边界循环。快速切换预备最新请求的选择。当前预设可持续播放，替换动作在后台准备。加载失败时，选择仍为请求的预设，乐器保持静音，Instrument 底部显示 **Error:** 和错误原因；切换从该预设继续。导入或更改库文件后请重新扫描。预设状态固定颜色显示于 Preset 下方。加载错误不会在 SONG 或其它页面弹消息。

**Native keys** 显示选中预设的原始触发音域，C4 对应 MIDI 60。**(gaps)** 表明音域内有部分音高无映射区。此为所有力度层的联合，并非所有力度或效果都覆盖全部显示音符。只有释放采样不扩展音域。调音和音高 FX 不影响此显示，也不扩展映射区。

Preset 右侧的 **Edit** 打开实验性 SoundFont 编辑器：调整调音（−2400至+2400分音），截止频率（20–20000 Hz），共振（0–40 dB），包络的延迟/起音/保持/衰减/释放时间（0–20000 毫秒），持续（0–100%），音量，声像，和 **Bend step**（每 PBN 单位弯音量，1–100分音）。新乐器默认弯音步长1分音。旧文件显示 **Stock**，保留其先前 PBN 比例（线性音高模式为25分音，周期模式为1）；精细弯音请设为1分音。Clear 恢复为1分音弯音。仅影响 SoundFont PBN，FPT 保持固定调音。音量和声像值为首页展示的乐器参数。音色与包络默认继承 **Bank**，即各层原始值。明确设置会覆盖该参数。Clear 恢复 **Bank**，调音归零分音，满音量，声像居中。EDIT+左右实现微调，EDIT+上下快速调整。OPT 返回 Instrument，EDIT+PLAY 在歌曲音符或默认音符上试听。仅编辑播放控制，不改银行的单样本映射、键/力度层区或循环点。

调音、截止频率与共振实时生效。起音、延迟和保持在音符启动时设定。衰减、持续和释放可动态更新音符播放；释放始于 NOTE OFF，而非仅空行。衰减在持续100%时无音。释放时间不会延长采样实际音频。显式释放覆盖使单次样本响应 NOTE OFF。Bank 设置时保持其原有单次行为。预设的键和力度范围仍为权威，范围外音符无声。FPT 可调整已选样本范围外的音高但不改映射。编辑存于乐器与工程文件，不修改源银行；旧歌未覆盖保持原银行设置。一个带有未知采样器特定循环类型的 WAV 文件可以播放其音频但不使用该循环。Instrument 警告会标明被忽略的循环类型；源文件保持不变。一个正向 WAV 循环结束比音频多一采样时会被接受为采样边界内的独占结束，附带警告；PCM 和显式 SFZ/EXS 循环映射不变。其他无效循环范围和根音会报告具体错误。一个错误计入奇数大小 PCM 帧布局的零 RIFF 对齐字节会被忽略并附带警告；其他部分 PCM 帧仍视为错误。标准的交替/反向 WAV 循环仍不支持。

**SoundFont FX** 在上下文 Lane FX 选择器中暴露八个控制。下列值均采用 tracker 十六进制表示。FX 仅影响其轨道，不改变乐器保存的控制。每个新音符从乐器设置开始，然后应用本行及后续行上的命令。

| FX | 控制 | 取值 |
| --- | --- | --- |
| FPT | 音高 | `00` = −24 半音，`18` = 零，`30` = +24；精确半音步进 |
| FCF | 截止频率 | `00`–`FE`: 20 Hz–20 kHz，按对数比例 |
| FRS | 共振 | `00`–`FE`: 0–40 dB |
| FAT | 包络启动 | `00`–`FE`: 0–20 秒，短时间精度更细 |
| FHD | 保持 | `00`–`FE`: 衰减前的保持时间 0–20 秒，在音符行设置 |
| FSU | 持续 | `00`–`FE`: 持续保持级别 0–100% |
| FDC | 衰减 | `00`–`FE`: 0–20 秒，短时间精度更细 |
| FRL | 释放 | `00`–`FE`: 0–20 秒，短时间精度更细 |

对于 FCF/FRS/FAT/FHD/FDC/FSU/FRL，`FF` 返回到 Edit 设定（含 **Bank**）。Attack 和 Hold 放在音符行。Decay、Sustain 和 Release 可改变活跃音符；使用 OFF 可听其释放。音量和声像使用通用控制；传统 FVL/FPN 命令仍加载播放但不再提供。Pitch FX 改变调音而不改键区。现有音序器音高/微调命令、MOD 和轨道效果依然可用。工程保存稳定的 bank/preset 引用和 FX 命令；采样库保持外部。

所有银行均为 USER 银行。大型乐器可能超过默认 64 MiB PCM 缓存，且不支持的源参数会产生警告。详见[SoundBanks 指南](soundbanks/README.md)了解平台路径、浏览器导入、缓存配置、格式覆盖及当前验证限制。

#### SoundFont 兼容性：银行、预设与调制

一个 SF2 或 SF3 **银行文件** 可含多个 **预设**，及其采样。一个库**集合**是浏览器中用于分组这些文件的文件夹。它们分属不同层级：即使同一文件中其他预设正常，某预设也可能表现不受支持。

**预设调制**是指从输入如力度、键位、调制轮或压力到声音参数的编程链接。链接可属于预设，也可属于多个预设共享的底层乐器层。不一定是作用于银行所有声音的设定。

| 预设特性 | 当前实现情况 |
| --- | --- |
| 采样/键位/力度映射，根音高与调音 | 已实现；保留作者设定范围。力度区与普通 tracker 力度继续有效。 |
| 采样偏移、支持循环、增益/声像及独占组 | 在解析器与音符限制内实现。 |
| 静态低通截止频率/共振及音量包络 | 由 tracker 渲染器实现；不保证与其他 SoundFont 播放器完全响应一致。 |
| 自定义预设/乐器调制路径（`pmod` / `imod`） | 未实现。作者控制器到参数映射、量和响应曲线被忽略。 |
| SoundFont 调制/颤音 LFO 与调制包络路由 | 未实现；可能缺失音高、滤波或音量的编程变化。 |
| 键控音量包络保持/衰减，固定键/固定力度发生器 | 未实现。 |
| SoundFont 合唱/混响发送 | 未实现。Tracker 效果可手动设置，但不自动还原预设设定。 |

旧消息 **“Custom bank modulation ignored”** 描述文件层检查：解析器发现银行中某处自定义调制记录。它**不**意味着选中预设使用了它们，或整个银行不可用。此通用限制于此处说明，避免于每个预设下重复。详细解析诊断保留这些记录；其它加载错误及兼容性警告视情况仍显示。引擎也不声称完全支持 SoundFont 默认调制器行为。Tracker PBN、颤音、Edit 与 Lane FX 是独立控制，非缺失预设编程的自动转换。

选库时，优先以采样映射、循环和静态包络为主。依赖控制器发音、动画滤波器或编程 LFO 的声音需用完整 SoundFont 播放器进一步比较。**加载并播放**不等于**完全还原预设**。比较不同音符和力度，以及持续音和音符 OFF。将 SF2 转为 SF3（或反向）并不自动移除调制需求。转换须将映射与行为转为引擎支持的特性；渲染效果为采样仅保留最终结果，不含交互控制响应。选择或转换银行前请参阅[格式覆盖与限制](soundbanks/README.md#format-coverage)及[验证说明](soundbanks/validation.md)。这些限制描述当前实验性实现，不是所有 SoundFont 播放器的固有限制。

#### 浏览 SoundFont 合集

在 SAMPLE → SOUNDFONT 中，**Preset** 打开你的合集。**Right** 或 **EDIT/A** 直接进入文件夹。在末级合集中，Right 进入其预设；再按一次 Right 会跳过 15 项而不加载。EDIT/A 加载高亮的预设。Left 返回合集，再返回上级文件夹；OPT 同样后退。浏览时 PLAY 开始/停止歌曲。

文件夹图标和声音图标用于区分条目。合集使用面板的整个高度。所选预设保持在八个轨道预览之下；长名称在选择停留一秒后滚动，移动时重置。乐器页面上的 EDIT+Left/Right 遵循相同的嵌套文件夹顺序，使每个合集的声音保持在一起。EDIT+PLAY 试听。

第三行报告 Reading preset、Finding samples、Unzipping、Checking samples 或 Loading samples。内嵌名称为空或为 `(null)` 的 EXS 乐器使用其文件名。源音色库和音频不会改变。修改资料库文件后请使用 **Rescan**。

有关受支持的采样器行为和采样格式，请参阅[格式覆盖与限制](soundbanks/README.md#format-coverage)。

### AY Classic、AY Plus 和 AY Sample

它们为原始 ChipNomad AY/YM 引擎。AY Classic 提供硬件风格音调、噪声和包络控制。AY Plus 增加软件振荡器与更丰富的调制。AY Sample 通过 AY 风格音量级别播放采样，与 Sampler 引擎不同。

#### AY Classic

这是 ChipNomad `v1.0.0` 的遗留乐器类型，更新的 AY Plus 乐器功能更全。

AY Classic 乐器参数：

- Mixer：音调开关、噪声开关、包络形状（`0-F`）
- 音量：软件生成的 ADSR 包络
- 自动包络周期：开关，速度范围 `1:1` 到 `F:F`

Settings 中的 **AY Quality** 仅影响 AY/YM 渲染，不改变 Braids、Plaits 或 Sampler 质量。**采样抖动**则用于 AY Sample 量化。

详见[ChipNomad AY-3-8910 文档](https://chipnomad.org/chips/ay-3-8910/)了解芯片详情。

#### AY Plus

AY Plus 将经典合成器概念应用于 AY-3-8910/YM2149F 声音设计。可将 AY 视为有 5 个振荡器的合成器：

- 3 个方波音调振荡器
- 1 个噪声振荡器
- 1 个幅度包络发生器，也可做振荡器用

在此模型下，每乐器有 3 个硬件振荡器：音调、噪声和包络。AY Plus 添加第 4 个软件振荡器。更新 AY 寄存器速度快于项目音轨节奏，扩展声音设计可能。由于 AY 工作原理，振荡器用环形调制混合。尝试不同音高偏移、调谐差和振荡器组合，发现新声音。

每个音调振荡器可独立调节音高和微调偏移。

软件振荡器类型及参数：

- Pulse - 可控脉冲宽度方波
  - FM 深度 - 简易 FM 深度
  - 脉冲宽度 - 全范围 `00-FF`，步数在 Project 界面设置
  - 脉冲低值 - 控制成对脉冲中低值，高值固定为 `15`
- Sync Tone - 音调振荡器硬同步效果
  - FM 深度 - 简易 FM 深度
- Sync Env - 包络振荡器硬同步效果
  - FM 深度 - 简易 FM 深度
  - 脉冲宽度 - 包络对中两个形状的持续比例
  - 包络对 - 可切换的两种包络形状
- Wavetable - 32 步 wavetable，编辑于 Wavetable 界面
  - FM 深度 - 简易 FM 深度
  - Wavetable 索引
- Tone FM - 音调振荡器的简易 FM 效果
  - FM 深度 - 音调 FM 深度
- Env FM - 包络振荡器的简易 FM 效果
  - FM 深度 - 包络 FM 深度

Pulse 和 Wavetable 软件振荡器不能与包络振荡器同时使用。包络振荡器自动关闭。

简易 FM 快速切换两个周期值，一个较低一个较高。类似把方波当作 FM 调制器。Pulse、Sync Tone、Sync Env 和 Wavetable 采用 FM 载波与调制的 `1:1` 比率。Tone FM 和 Env FM 可通过软件振荡器音高偏移模拟其它比率。

#### AY Sample

支持加载 `8-bit`、`16-bit`、`24-bit` 和 `32-bit` 单声道或立体声 WAV。播放时转换为 `8-bit` 并截断至 16 KB，此限制与 ZX Spectrum RAM 页大小相匹配。

AY 通过快速写入音量寄存器播放采样。输出为单极性，采样需“提升到零”以获得更干净的播放，消除采样尾端点击并改善低声部尾音。

AY 有 `4-bit` DAC 分辨率和非线性音量刻度，因此播放时将 `8-bit` 采样转为 `4-bit`。Settings 中的 Sample dithering 可提升感知位深。复古平台如 ZX Spectrum 和 Atari ST 不适合使用抖动，真实硬件或追求杂音质感时建议关闭。

采样表现类似另一个 AY Plus 软件振荡器，但额外参数使其更易作为独立乐器处理。

采样参数：

- 采样率 - 以原速播放采样对应 `C-4`
- 采样起点
- 采样长度
- 采样循环起点
- 采样音高偏移
- 采样微调偏移

音调与噪声振荡器部分与 AY Plus 相同。因 AY 工作机理，所有振荡器混合用环形调制。

### Braids

Braids 提供 47 种合成模式。其主要控制为：

- **Model**：振荡器或合成算法
- **Timbre** 和 **Color**：依模型不同的宏参数
- **Filter**：额外 LP/BP/HP 滤波器，斜率 `12` 或 `24 dB`，截止与共振
- **ADSR**：启动、衰减、保持与释放

点按 **Model** 可从分类模型列表中选择。

| 分类 | 模型 |
|---|---|
| 模拟 | `00 CSAW`、`01 MORPH`、`02 SAW-SQUARE`、`03 SINE-TRI`、`04 BUZZ`、`05 SQUARE-SUB`、`06 SAW-SUB`、`07 SQUARE-SYNC`、`08 SAW-SYNC` |
| 多振荡器 | `09 TRIPLE-SAW`、`10 TRIPLE-SQR`、`11 TRIPLE-TRI`、`12 TRIPLE-SINE`、`13 TRIPLE-RING`、`14 SAW-SWARM`、`15 SAW-COMB`、`16 TOY` |
| 滤波/音色 | `17 FILTER-LP`、`18 FILTER-PEAK`、`19 FILTER-BP`、`20 FILTER-HP`、`21 VOSIM`、`22 VOWEL`、`23 VOWEL-FOF`、`24 HARMONICS` |
| FM/混沌 | `25 FM`、`26 FEEDBACK-FM`、`27 CHAOTIC-FM` |
| 物理建模 | `28 PLUCKED`、`29 BOWED`、`30 BLOWN`、`31 FLUTED`、`32 STRUCK-BELL`、`33 STRUCK-DRUM` |
| 鼓声 | `34 KICK`、`35 CYMBAL`、`36 SNARE` |
| Wavetable | `37 WAVETABLES`、`38 WAVE-MAP`、`39 WAVE-LINE`、`40 WAVE-PARA` |
| 噪声/颗粒 | `41 FILTER-NOISE`、`42 TWIN-PEAKS`、`43 CLOCK-NOISE`、`44 GRAN-CLOUD`、`45 PARTICLE`、`46 DIGI-MOD` |

#### 参数

Timbre 和 Color 取决于所选模型。请参见 Mutable Instruments 原版 Braids 手册了解完整模型列表和细节，或依耳感演奏。

#### VCF 与 VCA

Braids 使用标准的 ChooChooTracker 多模滤波器和 ADSR 包络。

#### 设置

全局 Braids 设置位于应用设置中：

- **BITS**：降低输出分辨率
- **DRFT**：增加振荡器音高不稳定性
- **SIGN**：还原原版 Braids 波形缺陷算法

### aChChid**aChChid** 是基于 Open303 的单音酸贝斯引擎。`Square` 和 `Saw` 使用其原生的 TB-303 振荡器、滤波器、包络和重音行为。`Braids` 仅替换振荡器，随后继续通过相同的 303 滤波器和放大路径。它暴露了 Model、Timbre、Color 以及额外的波形整形器。波形整形器从柔和饱和逐渐过渡到波形折叠。

截止频率和衰退使用对数编辑：**EDIT + LEFT/RIGHT** 移动一步音乐控制步长，**EDIT + UP/DOWN** 移动十六步。截止频率范围为 200 Hz 到 20 kHz；衰退时间范围为 200 ms 到 2 s。

音符音量列中的 `F` 触发重音。`ASL` 从前一个音高滑向该音符而不重新触发 303 包络；自动保持前一个门控，`ASL 00` 产生 `60 ms` 滑音。`ATY` 使其音符在当前步长全程保持开启。无 `ASL` 的音符总是重新触发。

调制目标包括衰退和重音，以及 Braids 波形模式中的 Timbre 和 Color。

### Bogie

**Bogie** 是原生单次击打鼓合成器。Tracker 音符设定音高，tracker/乐器音量设定响度；每次触发都有独立衰退，因此无 ADSR 页。拥有 12 个模型：`Kick`、`Snare`、`Hat`、`Clap`、`Tom`、`Rim`、`FM`、`Noise`、`Cowbell`、`Cymbal`、`Shaker` 和 `Clave`。

六个宏参数为 **Decay**、**Tone**、**Sweep**、**Noise**、**FM** 和 **Drive**。标签会根据模型适应（例如 Kick 的 Noise 为 `Click`，Cowbell 的 FM 是金属交叉调制）。每个宏在所有模型中有效，但其音乐作用随模型变化：Sweep 可改变音高、爆发间距或金属扩散；FM 可添加交叉调制、金属层或类环谐波。各范围下半部分适合传统鼓声，上半部分故意开放更宽泛的音色形态和更合成的效果。

每个模型都可以使用共享的 LP/BP/HP 多模式滤波器，具有 Clean、Classic、Aggro 或 Acid 音色，`12`/`24 dB` 倾斜，截止和共振。Bogie 支持所有活跃宏，加上 Volume、Pitch、Cutoff 和 Resonance 作为调制及运动录制目标。用短衰退配合 Cowbell 可获得尖锐 phonk 攻击声；增加 FM 则产生更硬、更金属感的铃声。

在调制屏幕，按 **EDIT** 选择一个源后可选族（Envelopes、LFO 或 Sticks），之后选择具体源。**Left** 和 **Right** 键仍直接循环选择源。

### Sintered

**Sintered** 是原生单次数字打击合成器。音符设定基频；六个模型为 `Knot`、`Shard`、`Burst`、`Comb`、`Logic` 和 `Melt`。每个模型拥有 **Decay**、**Mod** 和 **Motion**，以及三个模型专属控制：Knot 的 Ratio/Spread/Fold，Shard 的 Ratio/Feedback/Bite，Burst 的 Noise/Color/Feedback，Comb 的 Time/Damping/Regen，Logic 的 Rate/Pattern/Crush，Melt 的 Ratio/Chaos/Drive。**Mod** 总是内部耦合深度。

Motion 是内部宏包络：值低于 `80` 使用 AD 形状，`80` 禁用，超过 `80` 使用衰退形状。越远离中心，包络越快。目标固定分配，确保 Sintered 在 tracker 模式中表现可预测：Knot 动画 Mod/Fold；Shard 动画 Feedback/Bite；Burst 动画 Noise/Color/Feedback；Comb 动画 Damping/Regen；Logic 动画 Rate/Crush；Melt 动画 Chaos/Drive。所有模型在音符触发时重置状态，内部反馈界限固定，确保击打可重复。

### MonkSynth

MonkSynth 是人声合成器。在 Instrument 页面中可调节元音、嗓音特性、气息、滑音、颤音、齐奏和延迟。EDIT 打开所有控件和补丁名称。

在 VOICE 控件上选择音色库和预设。ALL 包含 FACTORY 和 USER；EDIT+PLAY 试听高亮的预设，EDIT 则选中它。将自己的音色保存在 `instruments/USER/monk/`。已保存的乐器和歌曲会保留其 MonkSynth 设置。

数值为十六进制 `00..FF`，Voices 除外（`01..0A`，一到十个声部）。Attack、decay 和 release 覆盖 0..5 秒；detune 为 0..50 音分，声部展开为 0..0.5。Volume、Pitch 和六个主要嗓音控件可用作调制目标和动作录制目标。

十六个控件集中在 **MonkSynth FX** 中：`MWV`（元音）、`MIN`（嗓音）、`MAM`（气息）、`MFL`（滑音）、`MFB`（颤音深度）、`MSH`（颤音速度）、`MUV`（声部）、`MDT`（detune）、`MVS`（展开）、`MDM`（延迟混合）、`MDR`（延迟速度）、`MLV`（电平）以及 `EAT`/`EDC`/`ESU`/`ERL`（ADSR）。这些含义在 MONK 为当前乐器时生效；MME 命令的现有含义不变。共享的 Trigger FX 组保留 TDC/TCL，且没有重复的包络项。

### MME

**MME**（多调制引擎）是一个两振荡器合成器，专注于振荡器间交互。模型包括 `Ring`、`Fold`、`Cross`、`VPM`、`Sync`、`Logic` 和 `Vocode`。两个振荡器均为内部，并跟随 tracker 音符；`Interval` 设定它们间的关系。

六个宏参数为 **Waves**、**Interval**、**Amount**、**Flow**、**Feedback** 和 **Shaper**。Waves 在音乐的 A/B 波形对间插值。Amount 控制模型核心交互，Flow 改变方向或算法变体，Feedback 有界但末端刻意极端。Shaper 是共享最终阶段，从干净经过饱和到波形折叠。

MME 使用共享 LP/BP/HP 多模式滤波器和 ADSR。六个宏、Cutoff 和 Resonance 均可用作调制和运动录制。长按 **EDIT** 选 Model 可打开模型弹窗。

### MIDI Out

**MIDI Out**（桌面版、PortMaster 和支持 Web MIDI 的浏览器）驱动外部 MIDI 设备而非合成音频：触发时发送真实 MIDI Note On 到乐器的 **Channel**（`1-16`），音符结束时发送相应 Note Off。音量转为速度，和弦轨的每个声音都有单独的 Note On/Off。ChooChooTracker 本身无音频输出—需先于[设置](#13-settings)中选择 MIDI 输出设备。详见 [MIDI](#15-midi)，含 MIDI 键盘的声音预览。

**Program** 和 **Bank high/low**（CC0/CC32）为可选功能——关闭时显示为 `--`，`Clear` 可切换字段关闭并记忆其值。设置后只有在与该通道上次发送不同时，在下一音符前发送（非每个音符都发送，避免触发接收设备自身包络/音色）。

**MC1 number**-**MC4 number** 设定四个 MC1-MC4 行 FX 发送的 CC 号（`00-7F`）—详见 [MIDI Out FX](#midi-out-fx)。同样为可选；未设的槽位会使其对应 FX 无效。

### 经典架构引擎

此类引擎采用 VCO → VCF → VCA 架构。

提供若干 VCO 类型：

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

每个乐器将其 VCO 输入多模式滤波器。

滤波器有可切换 `12 dB` 或 `24 dB` 斜率，可做低通（LP）、带通（BP）或高通（HP）滤波。

滤波器音色选择：

- **Clean**：干净数字感
- **Classic**：灵感来自经典美式模拟合成器
- **Aggro**：灵感来自扬声日本模拟合成器
- **Acid**：Aggro 的两倍激进

Classic、Aggro 和 Acid 可产生强烈共振。建议先低开慢调。

滤波器输出进 VCA，由 ADSR 包络控制。包络形状可在指数、线性和对数曲线间变形。

#### Plaits 和 Plaits-Alt

**Plaits** 提供 24 个引擎，选择弹窗按如下分类：

| 分类 | 引擎 |
|---|---|
| 模拟 / 波形 | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| 数字 | `07 CHIPTUNE`, `15 SPEECH` |
| 纹理 / 噪声 | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| 物理 | `19 STRING`, `20 MODAL` |
| 鼓 | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** 是原版 Plaits 的另类模型集合。大多数更奇异或更实验。

| 分类 | 引擎 |
|---|---|
| 颗粒 / 微观 | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| 相位 / 谐波 | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| 原声 / 物理 | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| 复音 / 和声 | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| 数字 / 怪异 | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

参数遵循 Mutable 设计：

- **Harmonic** 控制谐波关系、平衡或引擎内部模型选择。
- **Timbre** 通常在暗/稀疏与亮/密集频谱间变化。
- **Morph** 探索另一维度的音色。
- **Main/Aux** 混合主输出与引擎的备用输出。

具体含义视引擎而定。例如和弦引擎用于和弦类型、转位和波形；物理模型用于材质、激励和衰减；鼓引擎用于音调、特性和衰减。详见 Plaits 用户手册。

对 `CHORD`、`DIATONIC CHORD` 和 `WT DIATONIC CHORD`，Harmonic 显示所选和弦名称（如 `MIN`、`MAJ`、`MIN7`、`MAJ7` 等）而非数值。Braids 的 `WAVE PARA` 也相似命名其 Color 音色。

点击 **Engine** 可从分类列表选择引擎。

**Env Mode** 有两种路由：

- `TRIG` 模拟模块连接 TRIG、LEVEL 不接时的行为
- `VCA` 持续打开 LEVEL 并在声音后接 tracker ADSR

#### Sampler

这款干净的 Sample 引擎播放加载到 RAM 的单声道或立体声音频 PCM 样本。- 点击 **Sample** 加载未压缩的 `8-bit`、`16-bit` 或 `24-bit` PCM WAV。浏览器中按 **PLAY** 试听高亮的文件。
- 加载样本后，样本名旁会出现 **EDIT**。点击它打开 Sample Edit 界面。按 **OPT** 或 **SELECT + [LEFT]** 返回乐器界面。
- Sample Edit 界面显示文件名（若内存中样本与磁盘文件不同则有 `*` 标记）、格式信息（采样率、声道）、纵向波形图及 **Region**、**Select**、**Slice**、**Process** 和 **File** 栏位。
- 编辑仅存在于内存，直到通过保存流程写入。工程仅存样本文件路径，下次加载工程时从磁盘重新加载 WAV，因此未执行 **Save**/**Save As** 即离开，所有处理操作都会丢失（`*` 标记为警告）。加载不同样本也会丢弃未保存的编辑并将 Region 重置为全样本（Start `00`，End `FF`）。
- **Region** 设定播放的起止标记，同样可在 Sampler 乐器界面调整。它们设为归一化的播放范围（`00-FF`）；若起点在终点之后，样本反向播放。按 **EDIT + [LEFT/RIGHT]** 细调标记（步进为 `01`），按住 **EDIT** 同时缩放标记周围的波形；按 **EDIT + [UP/DOWN]** 粗调（步进16），并恢复显示全样本。按 **EDIT + OPT** 将标记重置为默认值（Start `00`，End `FF`）。
- 波形视图缩放显示以编辑标记为中心的一秒音频窗口，自动平移以保持标记可见；若样本长度不足窗口大小，则显示1:1全视图。缩放为暂态效果，按住 **EDIT** 时启用，松开恢复全样本视图，粗调步进也恢复全视图。每次进入界面均重置为全样本视图。
- **Select** 定义绝对帧的处理选择区域，独立于播放的 Start/End 标记。按 **EDIT + [LEFT/RIGHT]** 移动选择把柄20帧并在按住 EDIT 时缩放查看；按 **EDIT + [UP/DOWN]** 跳转 `frameCount/64` 帧（最小16）并恢复全样本视图。点击 **EDIT (tap)** 可将当前 Start 或 End 标记位置复制到把柄。按 **EDIT + OPT** 清空该把柄的选择。选择为空时两把柄显示 `-`；把柄互换时自动调换它们位置。选择状态仅会话有效，不随工程保存，进入时默认填充播放区域（使用默认标记时为整个样本）。
- **Slice** 只有一行，含四个数值单元格 —— **Mode**、**Count**、**Slice** 和 **Frame**，随乐器保存。关闭时播放 Start/End 窗口。Phrase 音符会从 **C-0** 开始按半音选择切片，超出最后一个切片时循环回第一个（音符 N 触发切片 `N % count`）。切片音符不做音高转调或音阶量化，但 **CRD** 会保持选定切片，按和弦转调。切片起点用两像素宽的短线标示（深橙色，当前切片颜色更亮），光标下的切片区域波形有黑色背景条（选区色不会覆盖此条，保持纯黑，易于区分）。
  - **Mode** 循环切换 `OFF` / `EQUAL` / `AUTO` / `LAZY`（按 EDIT + 左右或点击切换；EDIT + OPT 关闭切片）。切换模式会初始化切片点：**EQUAL** 均分 Start/End 区间为 **Count** 个部分；**AUTO** 基于瞬态检测放置切片点，最多到 **Count** 个；**LAZY** 初始仅有单一切片覆盖整个区间。切回 LAZY 会保存当前 EQUAL/AUTO 设置（数量和范围），切回 EQUAL 或 AUTO 会完全恢复，无需确认且无数据丢失。Slice 与 Stretch 互斥：启用其中一项会禁用另一个，且切片单元在 Stretch 模式时为暗淡状态。
  - **Count** 显示切片总数（`01`..`64`）。按 **EDIT + [LEFT/RIGHT]** 变换步进1（最小1，最大64），按 **EDIT + [UP/DOWN]** 循环选择2、4、8、16、32、64 的幂次计数（循环切换）。每次更改都会重新计算划分：**EQUAL** 重新均分，**AUTO** 按新目标重新检测（检测灵敏度与数量成正比，切片越多阈值越灵敏）。模式为 OFF 时显示 `-`。LAZY 时计数显示为亮色（计算人工切片数），但仅作显示用，游标跳过该单元 —— 向右从 **Mode** 或向左从 **Slice** 进入时直接定位在 **Slice**，从 **Select** 行的 **END** 标记下移也落在这里，触摸该单元无效（手动切片分割点无须重新计算）。 **EDIT + OPT** 与普通 **EDIT (tap)** 在此无效：关闭切片功能操作在 **Mode**，删除切片操作在 **Slice**。
  - **Slice** 是切片浏览器，显示当前选中切片（1 索引，范围 `01`..`Count`）。按 **EDIT + [LEFT/RIGHT]** 单步切换切片，按 **EDIT + [UP/DOWN]** 跳跃四个切片（均有限制，无循环）；波形上显示当前切片的高亮标记和黑色带状背景，缩放视图中心跟随切片起点。按 **EDIT + OPT** 删除当前切片（会与前一个切片合并；第一个切片不可删除，删除最后剩余切片会关闭切片功能 —— OFF 状态切片范围保存，再次选择模式时恢复）。点击 **EDIT (tap)** 播放当前切片一次（任意切片模式均有效；松开按键或离开界面时停止）。模式为 OFF 时显示 `-`。
  - **Frame** 显示当前选中切片起始帧，十六进制。按 **EDIT + [LEFT/RIGHT]** 微调10帧（按住 EDIT 同时缩放查看），按 **EDIT + [UP/DOWN]** 微调100帧并恢复全视图。标记始终限制在本身切片内，不能越过前后切片起点。按 **EDIT + OPT** 删除该切片。
  - 按 **B** 从 Slice 行退出回乐器界面，界面其他位置同理；按 **SHIFT + LEFT** 可在任何位置退出。
  - **LAZY 工作流程**：选中 LAZY 模式时，点击 **PLAY** 启动一次性全样本播放 —— 从头播放一次，按样本原始音高和速度停止（波形上有绿色两像素宽标记指示播放位置）；再次点击 **PLAY** 可提前停止。播放期间，每点击一次 **EDIT** 都会在播放当前位置插入切片（无论光标位置）。距离已有切片小于 50 毫秒会拒绝，提示 `Too close to slice`。重复播放样本会保留已有切片，并按顺序在每个点击位置添加切片，**Count** 和 **Slice** 浏览器实时更新。对手动切片的浏览和编辑规则与其他模式相同：**EDIT + 方向键** 在切片间切换，**Frame** 编辑选中起点，**EDIT + OPT** 删除当前切片。播放滴定时 Frame 单元变暗。**SHIFT + PLAY** 在 LAZY 不会被截获：按惯例开始 Phrase 播放。LAZY 模式的切片在 sequencer 中表现和 EQUAL、AUTO 一致：Phrase 音符从 **C-0** 起半音映射手动切片，越界时循环。
  - **快捷提示**：光标停留在 Slice 行单元格且没有其它信息时，状态栏会显示该单元的提示，光标离开后消失。**Mode** 显示对应模式说明（模式名称已在单元格显示）：`Divides sample in equal parts`、`Divides sample based on transients`、`Press Play and add slices with EDIT`。切换模式时提示文本即时切换，OFF 清除提示。**Count** 显示 `Adjust the number of slices`，**Slice** 显示 `Browse slices`，**Frame** 显示 `Adjust slice start`。任何操作消息会覆盖提示，直到过期。
- **Process** 选择破坏性编辑操作：**Crop**、**Normalize**、**Delete**、**Silence**、**Fade In**、**Fade Out** 或 **Reverse**（按 EDIT + 左右切换，点击循环，EDIT + OPT 取消选择）。同一行的 **GO** 执行选中操作，若无选区则作用于全样本。**Crop** 保留选择区域；**Normalize** 缩放选择区域至峰值满量程（两声道共享同一增益，保持立体声像）；**Delete** 删除选择区域并拼接尾部（拒绝删除整个样本）；**Silence** 将选择区域置零；**Fade In**/**Fade Out** 线性淡入/淡出选择区域；**Reverse** 反向播放选择区域（帧内左右声道同步交换，长度和标记不变）。Crop 和 Delete 需要有效选择；空选择时显示 `Select region first`。每次操作会短暂停止音频，有一级撤销，样本标记为改动状态（内存中）。磁盘文件直到保存操作时才更新，离开界面会丢弃撤销。
- **UNDO**（与 GO 同行）恢复至最近操作前状态。操作前为暗色，操作后激活。多次点击在操作前后状态间切换，退出界面时撤销状态清空。
- **File** 处理保存流程。乐器保存加载 WAV 文件的完整路径；保存流程写入该文件或指向新文件，乐器名称不变。
  - File 字段循环显示 **Save** 和 **Save As**（按 EDIT + 左右或点击切换）；同排 **GO** 执行当前动作。
  - **Save** 覆盖加载的 WAV 文件。无切片样本先询问 `Overwrite <name>?`。有切片样本先弹出 **SAVE SLICES** 对话框，询问切片点保存位置：**SAVE TO SAMPLE** 将切片点写入 WAV 的 cue chunk（跨平台，DAW 中显示为标记）并保存在工程中；**SAVE TO PROJECT** 写普通 WAV，切片点仅保存在工程文件；**CANCEL** 取消。对话框含“本工程中不再提示”勾选，选中后将保存决策写入 .cct 文件（条目为 `- Sample save choice:`），仅可通过编辑工程文件更改。无切片样本跳过对话框。无有效路径的样本时，Save 按钮暗灰且跳过导航。保存失败时保持脏标记并显示错误。
  - **Save As** 将当前样本另存为新文件：先输入文件名（默认填当前名去掉扩展名），再在浏览器中选择文件夹。保存路径为 `<folder>/<name>.wav`，乐器指向新文件，文件夹记为默认样本目录。无样本数据时按钮暗灰且跳过导航，保证新加载样本能保存路径。路径长度超过255字符会被拒绝。
  - 文件名前的 `*` 标记表示内存样本与磁盘文件不同（任何处理操作设置该标记，Save 与 Save As 清除）。标记为仅会话状态，不随工程保存，界面复入时清除。若多个乐器引用同一 WAV 文件，保存任一样本会覆盖所有引用该文件的乐器。
- Sample 乐器界面中，按 **EDIT + [LEFT/RIGHT]** 可加载同文件夹内的上一个或下一个 WAV。
- **Pitch** 以半音调整音高范围为 `-48` 到 `+48`。
- **Loop** 选择关闭、循环或乒乓循环模式。
- **Speed** 控制颗粒时间伸缩，范围为 `0%` 到 `500%`（`100%` 为正常速度）。不支持的 WAV 格式会显示错误。导入前请将不常见的文件转换为 `PCM8`、`PCM16` 或 `PCM24` WAV 格式。

##### 音频输入

录制麦克风或 USB 音频接口之前，请先打开 **Settings → Audio → Audio Input**。列表显示设备上当前可用的输入。如果已启用的接口被拔出，捕获会中断，音频管理器会重新启动，使应用继续运行。如果当时正在录音，请按 **STOP** 结束被中断的录音。重新连接后，请选择 **[Refresh]** 并确认所需输入已启用，再重新录音；仅重新连接并不能可靠地重新启用它。**[List]** 会重新读取当前输入列表而不重启音频。

勾选输入的 **ARM** 复选框，使其可供 Sampler 录音器使用；其小电平表显示输入电平。一次只启用一个接口：此版本只会打开第一个已启用的接口，即使勾选了多个。你的选择按设备名称记忆，因此不可用的接口保持未启用，而不会被其他输入取代。按 **OPT** 返回 Audio 设置。

##### 录制音频输入

在 Sample 乐器界面中，**REC**（已加载采样时为 **EDIT/REC**）打开采样编辑器，将录音和编辑整合在同一界面。请先在 **Settings → Audio → Audio Input** 界面启用输入（见[音频输入](#音频输入)）。编辑器上方几行控制录音：

- **Input** 循环选择录音来源：`T1`..`T8` 录制八条 tracker 轨道，之后是已启用的麦克风或 USB 接口。要录制外部来源，请继续越过 `T8`，直到出现设备名称。`-` 表示未明确选择来源；此时 **REC** 使用第一个已启用的输入并显示其名称。按住 **EDIT** 并按 **LEFT**/**RIGHT** 后退/前进，或按 **EDIT+OPT** 回到 `-`。点击 Input 会直接前进。
- **Input** 旁的电平表显示已启用输入的实时电平（仅外部来源）。**MON** 切换输入监听：已启用的输入以 -6 dB 送至输出以减少反馈。该设置保存在设置文件中。
- **REC** 开始录音。外部来源立即录音；轨道来源显示 **ARMED**，播放开始时（按 **PLAY**）才开始录音。**STOP** 结束录音：WAV 写入项目的导出文件夹（`recNNN.wav`），加载到当前 Sampler 乐器，Region 恢复为整个采样。录音进行中按 **OPT** 退出界面，同样会结束录音。
- **PLAY**（录音后显示）试听该录音；预览播放时变为 **STOP**。旁边的 **CLEAR** 删除 WAV 文件并从乐器卸载采样；删除后 **REC** 回到原位。
- 波形在录音期间显示实时输入，结束后显示完整波形。状态行在录音期间显示文件名，之后显示帧数；外部录音在未丢失任何音频帧时还会显示 `drop 0`。录制的采样完全可编辑：循环区域、选择、切片、处理和保存与任何已加载文件相同。

录音以所选音频采样率写为 16 位立体声 WAV。轨道来源捕获轨道乐器渲染的内容（经过 FX 之后）；外部来源直接捕获设备输入。

#### 2xSCWF

2xSCWF 是一个双单周期波形振荡器。每个振荡器加载一个包含恰好 1 个周期的单声道 WAV。两个振荡器均向前读取，且在周期末尾循环。

混合并交叉淡化 A 和 B。

微调为细微且指数式，从合奏模式至 `+200 ct`，然后以半音步进从 `+3 st` 到 `+24 st`。

2xSCWF 将每个文件视为一个单周期表，每个振荡器周期精确读取一次。这样可以混合并微调两个单周期波形。

内置的 `waveforms/AKWF/` 文件夹包含 Adventure Kid 库中的波形。如果想扩展收藏，网络上有更多可用资源。

#### BYOWTBL

BYOWTBL 是一款兼容 Serum 表的双波表振荡器。每个振荡器加载一个包含连续单周期帧的单声道 WAV。`Pos A` 和 `Pos B` 独立扫描各自表格，从 `00` 到 `FF`。引擎在每帧内部及相邻帧之间进行线性插值。`Mix` 混合交叉淡化两个振荡器，`Detune` 偏移振荡器 B。

### FM 乐器系列

#### 原生 OPLL 与 VRC7 乐器

Instrument Type 选择器中有 FM 组，包括 **OPLL / MSX** (YM2413) 和 **VRC7** (DS1001)。它们均提供 **73 个预设程序**，在滚动的 Bank / Preset 列表中显示：原版 15 个音色，emu2413 的 YM2413/VRC7/YMF281B 表中 40 个额外独特色调，以及 18 个 ChooChoo 编写的双算符程序。每个引擎中精确重复的音调字节会被省略。额外音色使用芯片的可编程音色槽，并未扩展物理 ROM。程序零标识自定义音色。按住 EDIT + 左/右切换银行过滤器中的上一个或下一个预设。Fine ct 调整从 -100 到 +100 分的微调。

在程序列表中，EDIT + PLAY 可试听高亮音色，松开停止。EDIT 确认，OPT 取消。浏览及试听不改变乐器、波表或歌曲。选中后，常规 Instrument 页面试听手势依旧有效。原生芯片包络提供攻击/释放；可选 Amp env 添加软件 ADSR（详见下文原生芯片控制）。无完整的算符编辑器。

#### AdLib / OPL2 与 OPL3

FM 还包括 AdLib / OPL2 (YM3812) 与 OPL3 (YMF262)。银行过滤工厂列表；预设弹出类别组及“全部”视图。EDIT + PLAY 试听，EDIT 选择，OPT 取消，EDIT + 左/右 在预设中加载上一个/下一个匹配条目。Fine ct 局部微调。Mode 显示 2 算符、4 算符或双声道。OPL2 隐藏不兼容的 OPL3 补丁；OPL3 可播放共享的双算符集合。

银行和预设弹出标题标明当前引擎。切换引擎或乐器槽时，银行过滤重置为 **所有银行**，包括在兼容的 AdLib 和 OPL3 引擎间切换。在同一槽返回弹出菜单会保持所选过滤。取消 DX7 银行导入恢复之前的过滤。

预设确认会将按钮松开时的编辑发布于下一 UI/音频周期。播放歌曲时，无需额外按键或重启传输即可使用该补丁。输出仍遵循配置的音频缓冲以及引擎包络/重触发行为。

工厂库存含 The Fat Man 2-op、The Fat Man 4-op 和 DMXOPL3 的 697 个源条目，具有 589 个规范化唯一补丁标识。别名保留源名称和归属。保留源码算符电平、LFO 深度标记、音符偏移、固定打击乐音高及双声道调音。Tracker 音量使用软件振幅，但不重现原 MIDI 播放器的音量曲线或速度偏移策略。源释放时长估计未切断持续音符。部分音效攻击缓慢：按住 Seashore，勿指望短点击激发。

选中预设不会更改槽的波表或轨道上的插入效果。

#### 世嘉 PSG 和 Game Boy 原生乐器

CHIP 包含世嘉 PSG、GB Pulse 和 GB Noise。针对世嘉 PSG，**Bass range: Extended** 保持低音清晰区分；选择 **Chip** 则为原有限制范围，最低音趋同。固定频率噪声保持原始音高；频谱谱噪声随选定低音范围调整。

GB 乐器使用 DMG 脉冲/噪声寄存器；无 Game Boy 波形通道。其预设列表分别包含 **24、24 和 28** 个。新增内容涵盖连接/周期世嘉噪声、GB 音高扫频、短打击乐、金属噪声、无人声响和上升音。这些为原创程序，非游戏声音提取。

页面提供原生模式/占空比/宽度、包络/扫频/噪声控制（视情况而定），以及软件振幅 ADSR。

#### DX7 FM

选择 **Type → FM → DX7 FM**；DX7 是第七个条目，位于 Arcade / YM2151 之后。DX7 FM 使用六算符。其页面有银行、预设和 Fine ct，配合通用乐器设置。预设浏览使用银行/类别列表。按住 EDIT+PLAY 试听高亮声音，松开停止。单按 EDIT 松开确认，OPT 取消。加载声音时会完整复制补丁，不更改 Tracker 表或轨道插入效果。

如需持久个人库，请将 `.syx` 文件放于 **`instruments/banks/dx7/`** 中，与现有乐器库并列，支持子文件夹。打开 **DX7 → Bank**：每个文件显示为命名银行，供预设浏览。添加或移除文件后重新打开 Bank，无需转换或加载乐器。标准 DX7/TX7 原厂银行包含 **32 个音色**。含有四条银行消息的文件展示为四个编号银行（共 128 个音色）。单音色文件同样被接受。所选补丁随乐器保存，移除源银行不会改变歌曲。

**Load Instrument** 也支持原始 DX7/TX7 `.syx` 单音色及 32 音色银行。选中补丁使用，取消保持当前乐器。持久库请用上述银行文件夹，保存歌曲或 `.cni` 以保留编辑补丁。不支持的 Yamaha 格式会被拒绝。

DX7 限制歌曲内最多 **16 个活动音符**，包括和弦音和释放尾音。达到限制时先释放尾音，再压制静音持音，保证新触发可播放。播放歌曲时不支持预设试听。

#### FM 算符编辑器

Genesis FM、Arcade FM、OPL2、OPL3、OPLL 和 VRC7 乐器在 Fine ct 右侧有 **EDIT** 按钮，打开补丁参数表：

- Genesis FM / Arcade FM
- OPL2 / OPL3：在 OPL3 上可在此更改模式（2op、4op、双声道）。
- OPLL / VRC7：编辑 ROM 预设会转为“Custom”。

使用左右键和常用编辑键更改数值；乐器持续播放以便监听修改。状态栏显示光标下参数说明。按 OPT 或 左+Shift 返回乐器界面。DX7 与 SID 无算符编辑器。

Genesis FM (YM2612) 和 Arcade FM (YM2151) 使用相同的银行/预设浏览器、试听和微调控件。它们的原生包络控制释放，变更 Tracker 音量或音高不会触发包络重启。

#### FM 工厂和用户预设

原生芯片乐器用 **Bank:** 选择 ALL、内置集合或 USER。**Preset:** 从选择中选定音色。按住 EDIT 并在任意行按左或右键循环选择；短按 EDIT 打开选择器。

内置集合为 `instruments/FACTORY/` 下的 ZIP 包。USER 文件放于 `instruments/USER/<engine>/` 目录：`dx7`、`opll`、`vrc7`、`opl2`、`opl3`、`genesis`、`arcade`、`sid`、`sega`、`gb-pulse` 或 `gb-noise`。可直接浏览文件夹或放置兼容预设 ZIP，ZIP 包保持原样。所有这些文件夹支持兼容的 `.cni` 文件；CNI 是 ChooChoo/ChipNomad 的跨平台乐器格式，不是芯片厂商标准。

USER 浏览器还可读取以下源格式，包括 ZIP 内：

| USER 文件夹 | 附加格式 |
| --- | --- |
| `dx7` | Yamaha DX7/TX7 `.syx` 单音色及 32 音色银行 |
| `genesis` | `.tfi`，DefleMask `.dmp`，Furnace `.fui` |
| `arcade` | VOPM `.opm` 银行，DefleMask `.dmp`，Furnace `.fui` |
| `opl2`、`opl3` | `.wopl`，`.opli`，`.woplx`，`.sbi`，Furnace `.fui` |
| `opll`、`vrc7` | Furnace `.fui` 定制补丁及固定 ROM 选择 |
| `sega`、`gb-pulse`、`gb-noise` | Furnace `.fui`，DefleMask `.dmp` |
| `sid` | GoatTracker 2 `.ins` (GTI5)，Furnace `.fui` |

打开银行即载入兼容音色。不支持的文件或功能会报告原因；混合银行中不兼容音色被排除，且源编号保持稳定。加载操作事务性：拒绝预设时当前乐器保持不变。源预设替换乐器的 Tracker 表为空表，DX7 SysEx 保留现有表。CNI 恢复其保存的表。加载的补丁归歌曲所有，移动或删除源包后仍可工作。

WOPL 银行可包含未标明空白的未用插槽。浏览器省略无名称且所有激活算符完全抑制且无攻击的插槽。无名称可演奏声音仍作为 `Program N [Bbank]` 或 `Drum N [Bbank]` 可用，使用源零基程序和银行编号。无名称单个 OPLI 乐器使用其文件名。保留命名静音补丁。

Furnace、DefleMask 和 GoatTracker 文件作为乐器导入，而非完整歌曲。动画源宏、采样和特定歌曲播放设置可能无法迁移。请将 Game Boy 文件放在相应的 Pulse 或 Noise 文件夹。

USER 预设循环跨文件夹、文件、银行及 ZIP 包，双向循环，端点回绕。重新打开 USER 浏览器时，保持当前选择。ALL 包含 USER 音色归类于未分类引擎；USER 保持独立顶层选项。世嘉 PSG 和 Game Boy 使用扁平 ALL/Factory 列表及分层 USER 浏览器，无未分类面板。老旧 DX7 文件在 `instruments/banks/dx7/` 仍可在 USER 中访问。

引擎仅显示其兼容的内置集合。工厂预设为所选引擎汇总的 OPLL/VRC7 音色集；DX7 汇总 ChooChoo 和 YSE 原创。下载的命名合集独立保留。Fat Man 2-op 列于 OPL2，Fat Man 4-op 列于 OPL3。兼容的 OPL2 CNI 文件可在 OPL3 USER 文件夹使用。

#### 原生芯片控制

原生乐器波形预览在控件重绘时刷新。FM 乐器可覆盖其可选 Amp env 曲线；SID 保持波形预览但无 FM 专属覆盖，浏览银行和预设时亦然。

所有七个 FM 引擎均提供 **Bright**、**Feedback** 和 **Amp env**。Bright 范围 -63 到 +63，零值保持补丁不变；它改变调制算符电平，保持载波电平。其可听效果取决于算法。Feedback 默认为 **Preset**，或用 0–7 覆盖原生反馈。这些控制不会重写已保存的原生算符字节。Amp 包络默认设为 **Bypass（旁路）**。选择 **ADSR** 可添加包络的攻击（attack）、衰减（decay）、保持（sustain）、释放（release）和形状（shape）以塑造原生音色。A/D/R 采用共享的 0–5 秒二次曲线范围；保持区间从静音到满音量。原生包络仍然生效，因此此包络无法将声音延长超出其原生释放时间。选用预设时会保留该插槽的 FM amp 和音色控制。

下表中所列的 Phrase FX 同时作为支持的调制目标和动作录制目标。FX 栏的数值为十六进制。设置应用于播放时而不更改已保存的音色。原生 GB 扫频和包络控制在下一音符触发时锁存；占空比/宽度和噪声频率可实时变化。

| Engines | FX | 控制说明 |
|---|---|---|
| Native FM | `OL1`–`OL6` | 绝对算符输出电平：OPLL/VRC7 调制器和 OPL `00–3F`；OPLL/VRC7 载波 `00–0F`；Genesis/Arcade `00–7F`；DX7 `00–63`（0–99）。数值越大输出越强。只显示支持的算符。 |
| OPLL/VRC7/OPL2/OPL3 | `OAR`、`ODR`、`ORR`、`OSL` | 算符 1 的攻击、衰减、释放与保持衰减，`00–0F`。 |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | 算符 1 频率倍频器。 |
| Genesis/Arcade | `LFR` | 原生 LFO 速率：Genesis `00–07`，Arcade `00–FF`。 |
| Arcade | `LAD`、`LPD 00–7F` | 分别为 LFO 振幅深度和音高深度。 |
| Genesis/Arcade | `LAS`、`LEN` | 原生振幅灵敏度 `00–03` 及 LFO 启用开关 `00–01`。 |
| Genesis/Arcade | `LPS 00–07` | 原生音高灵敏度。 |
| SID | `SAT`、`SDE`、`SSU`、`SRL 00–0F` | 原生攻击、衰减、保持、释放。时间值向 `0F` 递增；保持值向满音量递增。 |
| SID | `SPR 01–10` | Silent partner 频率，1× 到 16×；影响环调和同步。 |
| 全部原生 FM | `FBK 00–07` | 绝对原生反馈值，由音色初始化。 |
| 全部启用 Amp 包络的原生 FM；Sega/GB | `EAT`、`EDC`、`ESU`、`ERL`、`ESH 00–FF` | 攻击、衰减、保持、释放、形状。不会启用旁路的 FM amp。 |
| Sega PSG | `CMD 00–02`、`CNR 00–03` | 纯音 / 白噪声 / 周期噪声；三种固定噪声速率或基于音调的速率。 |
| GB 脉冲 | `CMD 00–03` | 原生脉冲占空比。 |
| GB 噪声 | `CMD 00–01`、`CND 00–07`、`CNS 00–0D` | 噪声宽度、时钟除数和移位。 |
| GB 脉冲 | `CSP 00–07`、`CSS 00–07`、`CSD 00–01` | 扫频周期、移位、下降方向。 |
| GB 脉冲 / 噪声 | `CEI 00–0F`、`CEP 00–07`、`CED 00–01` | 原生包络初始电平、周期、上升方向。 |

原生 FX 选择使用 phrase 行中 `I` 列的音色，若 `I` 为空，则向上查找激活音色。FX 弹出窗口显示简短控制说明、当前预设值及有效指令范围于一个紧凑模块。选择不同的原生效果时，将从该值开始；重新打开同一效果将保留编辑过的值。多行选择会分别解析每行的音色。表格使用其对应的音色上下文。原生数值编辑会限制在合法端点，包括占空比、噪声、扫频、ADSR、反馈及算符电平。数值均为十六进制，例如 DX7 最大值 `63` 等同十进制 99。

#### SID 音色与 Phrase FX

选择 **CHIP → SID**。Bank 提供 **ChooChoo SID Originals** 和 **SIDkit Effects**。像管理其他原生芯片预设那样浏览、试听和保存它们。

音色页提供波形/脉冲宽度、滤波器模式/截止频率/共振以及原生 ADSR 控制。预设名旁的 **EDIT** 打开 **SID EDIT**。波形和脉冲宽度有独立标签；滤波器有模式行及截止和共振；ADSR 标签与数值并排。点击字段直接选中对应控制。保留现有参数范围与细调/粗调行为。

SID EDIT 继承 FM 编辑器的紧凑行结构、主题色彩、十六进制数值及 EDIT+方向快捷键。按 OPT 或左 Shift 返回；EDIT+PLAY 试听音色。页面直接编辑当前存储的补丁，修改后与音色及工程一同保存。

SID EDIT 包括波形、环调/同步、ADSR、脉冲宽度、滤波器、动作、琶音、音高扫频和颤音。状态栏显示各控件与其范围。环调和同步各占用四个可用 SID 声音的两个，因此激活时 SID 和弦可能需要更少音符。

| FX | 范围 | 作用 |
| --- | --- | --- |
| `SCP` | `00–FF` | 脉冲宽度基准，按原生 12 位宽度缩放；配方脉冲动作依然生效。 |
| `SCT` | `00–FF` | 截止频率基准，按原生 11 位截止缩放；配方滤波动作依然生效。 |
| `SRN` | `00–0F` | 原生共振度。 |
| `SWV` | `00–07` | 波形：三角、锯齿、三角+锯齿、脉冲、三角+脉冲、锯齿+脉冲、三角+锯齿+脉冲、噪声。 |
| `SFI` | `00–07` | 滤波模式位，1 低通，2 带通，4 高通；0 旁路。 |
| `SMR` | `00–FF` | 配方宏时钟，1–200 Hz 缩放。 |
| `SRG` | `00–01` | 三角环调与 silent partner。 |
| `SSY` | `00–01` | 振荡器同步与 silent partner。 |

脉冲宽度和截止支持 `SLE`。离散波形、开关、共振和 FM 反馈保持其有效原生步进；增大字节值并不会产生更多硬件状态。`FBK` 直接选择反馈 `00–07`。算符电平使用以上引擎特定范围。亮度和主控调节保留全字节范围。

#### SID 和原生芯片出厂及 USER 预设

原生芯片音色使用 **Bank:** 选择 ALL、包含集合或 USER。**Preset:** 在选中范围内选择音色。按住 EDIT 并用左右键切换；短按 EDIT 打开选择器。

包含集为 `instruments/FACTORY/` 目录下的 ZIP 压缩包。USER 文件放在 `instruments/USER/<engine>/`：包含 `dx7`、`opll`、`vrc7`、`opl2`、`opl3`、`genesis`、`arcade`、`sid`、`sega`、`gb-pulse` 或 `gb-noise`。可直接浏览这些文件夹，或放入 ZIP 包含对应 `.cni` 预设。DX7 支持 `.syx` 单音色及 32 音色库，包括 ZIP 内部文件。ZIP 文件在磁盘上保持完整。暂不支持直接加载 WOPL/WOPLX；请先离线转换成兼容的 CNI 预设。

USER 预设循环包括文件、文件夹、库和 ZIP 包，双向循环，末尾回绕。重新打开 USER 浏览器会保持当前选择。ALL 包含 USER 声音，分组引擎下为 Unsorted 分类；USER 始终为独立顶级选项。Sega PSG 和 GB 使用扁平的 ALL/Factory 列表及层级的 USER 浏览器，无 Unsorted 面板。旧版 DX7 文件保留在 `instruments/banks/dx7/` 下，也可在 USER 中访问。

引擎仅显示兼容的包含集合。Factory Presets 合并所选引擎的 OPLL/VRC7 音色集；DX7 合并 ChooChoo 和 YSE 原创。下载的命名集合独立显示。Fat Man 2-op 列于 OPL2，Fat Man 4-op 列于 OPL3。兼容的 OPL2 CNI 文件仍可用于 OPL3 USER 文件夹。

## 6. 调制与动作录制

每个音色最多有 4 个调制槽。音频音色可目标 **Instrument Pan** 或 **Track Pan**；两者均以 `00` 表示左，`80` 表示中央，`FF` 表示右。

可用的调制类型：

- ADSR – 经典攻击-衰减-保持-释放包络
- AHD – 攻击-保持-衰减包络
- LFO – 多种波形的低频振荡器
- syncLFO（原 SLFO）– 带每单位 tick 设置和周期乘数的 LFO
- slowLFO – 带比例周期控件的长周期，起点紧接在普通 LFO 范围之外
- FLFO – 高频 LFO，可达千赫兹以上；FLFO 占用较多 CPU 资源
- SLIN – 线性操纵杆控制
- SRAT – 速率基操纵杆控制
- Sequence – 将 tracker 序列读作调制信号：**PITCH** 是最终排序后的音高，**VOL** 是最终排序后的音量，**TRANCE GATE** 是独立的节奏调制器。Motif、Rate 和 Length 选择门限图案；Trigger 选择自由运行、逐音符重新触发，或在 phrase 或 chain 边界重置。关闭的门限输出零，开启的输出半电平，Accents 在活动门限中按欧几里得方式分配满电平门限。它不使用也不更改任何轨道插入。

可用调制目标取决于音色类型。

当 ADSR 或 AHD 包络目标为音量时，自动成为该乐器的音量包络。LFO 目标为音量时，则作为当前输出音量的偏移。

调制量可正可负。范围依目标而定。绝大多数参数全范围 ≤127 时，调制量定义绝对范围。音高等较宽范围参数使用缩放量。

Sequence 的 PITCH 和 VOL 来源带有 **Mode**。**Normal** 使 tracker 数值保持可听，同时将其用作调制。**Bypass** 使其仅作为来源：PITCH 在上一个音高（首个音符时为 C-4）上重新触发，VOL 保持上一个可听电平。Trance Gate 本来就只是来源。若任何活动的 Sequence 插槽将 PITCH 或 VOL 设为 bypass，则 bypass 优先。为避免反馈，PITCH 和 VOL 不能指向 Pitch 或 Volume。

所有持续时间参数（攻击、保持、衰减、周期）均以音符时钟步长（ticks）计。

**syncLFO** 即原来的 SLFO：设置其 ticks 和乘数，可获得与速度同步的运动。现有的 SLFO 设置和歌曲保持相同的时序。

需要比普通 LFO 更长的扫频时，请使用 **slowLFO**。其 Period 控件范围为 256 到 131,072 ticks。所有基于 tick 的 LFO 都跟随歌曲速度，并共用相同的波形、波表选择和触发模式。选择 Free 可让扫频在音符之间持续；Retrig、Phrase 和 Chain 会在各自边界重新启动它。

LFO 波形：

- Tri – 双极三角波
- Sin – 双极正弦波
- UniTri – 单极三角波
- UniSin – 单极正弦波
- RampDn – 线性下降斜坡（锯齿波）
- RampUp – 线性上升斜坡（锯齿波）
- ExpDn – 指数下降斜坡（指数锯齿波）
- ExpUp – 指数上升斜坡（指数锯齿波）
- Square – 方波
- Random – 采样保持随机波
- Wavetable – 读取工程里的 32 步 AY 波表。值 `00` 到 `0F` 映射 `-100%` 到 `+100%`，无插值。

LFO 触发类型：

- Free – 仅在音色变更时重启 LFO
- Retrig – 每个音符重启 LFO
- Phrase – 进入新 phrase 时重启 LFO、syncLFO 和 slowLFO
- Chain – 进入新 chain 时重启 LFO、syncLFO 和 slowLFO
- Hold – 运行 1 周期后停止并保持最后值
- Once – 运行 1 周期后停止并回归零点

### 动作录制

- **STICK LIVE**（默认 `L1`）应用操纵杆调制，但不改变 phrase。设置 > Stick live mode 中，`HOLD`（默认）按住时启用；`TOGGLE` 按一次启用， 再按一次禁用；`FREE` 永远启用。释放按钮时，`TOGGLE` 状态保持启用。
- 播放时按住 **MOTION RECORD** (`L2`) 应用操纵杆调制并以绝对 FX 值录制更改目标。
- 按住 **MOTION ERASE** (`R2`) 删除当前行中匹配的目标 FX。

右下角的 `~` 表示 Stick live 正在运行。录制（`*`，溢出时为 `!`）和擦除（`x`）图标优先显示且始终瞬时显示，且两者均在按住时启用实时操纵杆处理；松开后保留已设置的实时切换状态。

模式保存在应用设置中，但实时切换默认每次启动时关闭。切换模式会清除实时切换状态：`HOLD` 跟随当前按住的 Stick live 按钮，`TOGGLE` 等待新按键，`FREE` 即刻生效。

动作录制将轨道 FX 写入当前播放的 phrase。优先更新匹配的 FX，然后从右向左使用空槽。不会覆盖不同的 FX。若所有 3 个槽满，则该步不录制动作。右下角出现 `!` 表示目标数超过 3 个 FX 槽容量。

动作录制支持 Braids、Plaits、Sampler、2xSCWF、BYOWTBL 和 Bogie 目的地，包含激活的 Bogie 宏和滤波器控制（视适用情况）。

## 7. 轨道插入效果

八个轨道中每个都有两个串联槽，**TF1 → TF2**。在 Instrument 中，使用 Shift+上 进入 MOD，再 Shift+上 进入 **F: Insert FX**。Shift+下 返回 MOD。OPT+左/右 切换轨道（1–8），不改变音色选择器。模块标题打开已存在的预设选择器；旁路保存模块、已配置数值与自动化。参数为字节值：左栏 1–4，右栏 5–8。上下文文本显示完整名称及解码数值。旧工程加载时两个槽均为关闭。

导航显示在音色屏幕始终保持完整的 F–M–I–P 列可见，包括 Insert FX 和音色池，并高亮当前屏幕。| 模块 | 参数顺序 |
| --- | --- |
| Work Compressor | Threshold, Attack, Release, Makeup, Ratio, Detector source, Detector filter, Mix |
| Airwindows Distortion | Input, Mode, Output, Mix |
| Airwindows StereoDoubler | Detune, Mix |
| TAPESCAM | Input, Drive, Color, Wobble, Tone, Output |
| OTT (Rui-727) | Depth, Time, Upward, Downward, Input, Output |
| Chorus | Rate, Depth, Tone, Mix, Feedback |
| Flanger | Rate, Depth, Feedback, Mix |
| Phaser | Rate, Depth, Feedback, Mix, Stages |
| Rotary | Speed, Depth, Drive, Mix |
| Saturation | Drive, Tone, Level, Mix |
| Bitcrusher | Bit depth, Rate reduction, Tone, Mix |
| Destruction | Mode, Amount, Tone, Mix |
| FORMANT | Mode, Vowel, First formant, Second formant, Resonance, Mix |
| Trance Gate | Pattern, Rate, Length, Gate, Phase, Mode, Smooth, Mix |
| Beads (Mutable Instruments) | Time, Size, Density, Pitch, Scatter, Feedback, Shape/Freeze, Mix |

连续控制使用 00–FF 范围，包括有效的零和最大值 FF，FORMANT VOW 除外（00–7F）。双极控件在 80 处有精确的中性点。失真模式为 00 Density，01 Drive，02 Spiral，03 Mojo，04 Dyno。压缩器比率 00–07 分别为 1.5、2、3、4、6、8、16、20:1；Compressor Detect 00–02 监听自身的立体声输入、左声道或右声道；03–0A 则监听轨道 1–8。检测器滤波器 80 为中性，低于它为低通，高于它为高通。轨道来源取自其音量、声像和 Tilt EQ 之后、插入之前；静音或缺失的来源贡献静音。任意插槽均可包含任意模块；重复模块相互独立。Chorus 和 Phaser 起始时有明显但受控的调制。减小 Mix、Depth 或 Feedback 以获得近乎透明的效果；高 Depth、Feedback 和 Mix 设置则刻意制造极端效果。Flanger 和 Rotary 使用较柔和的调制默认值。Rotary Speed 使用完整 **00-FF** 范围：**00 = 0.01 Hz**（每 100 秒转一圈），**FF = 25 Hz**（前 Chaos 最大值）。指数分布提供对低速 LFO 运动的更细腻控制。新加入的 Rotary 默认值为 **8F**，大约 0.8 Hz。该方式替代了旧的四个离散速度值；现有 Rotary 设置及 F11/F21 速度自动化需根据新刻度重新调整（旧的 Slow/Fast/Hyper/Chaos 大致对应 **8F/CB/E7/FF**）。其他参数保持不变。Bitcrusher 位深范围为 4 至 16 位，速率衰减范围为 1x 至 32x。Destruction 模式有 Fold、Clip 和 Crush。Saturation 和失真包括输出电平或混合控件，用以平衡未处理信号。

Trance Gate 是一个节拍同步的幅度序列器，适用于 Pads、Leads 和 Stabs。Pattern 取值 00–3F 选择命名的音乐动机；00–1F 是核心律动，20–3F 是实验性变体。Rate 提供直音符、三连音和附点值，从 1/2 至 1/32 不等。Length 将独立旋转的 16 步动机截断为 1–16 步。每个轨道按键触发都会重启序列；松开按键则让序列在释放阶段继续运行。Forward、Reverse 和 Ping-Pong 定义遍历方向；Chaos 每四次循环进行一次可重复的、保持密度的变异。作为主插入效果时，跟随播放头时间而不重触发按键。Gate 设定每个开启步的宽度，Smooth 平滑边缘同时保持防止点击的最小渐变，Mix 在原音和闸音间交叉渐变。

**Beads** 使用原始 Mutable Instruments 颗粒 DSP 作为立体声插入。可在轨道插入插槽中选择 Beads。其八个控件也支持现有 F11–F18 / F21–F28 命令和插入 MOD 目标，无需新增 FX ID 或工程格式。颗粒持续运行，无需额外事件。

- **Time:** 录音缓冲区的四秒内位置。
- **Size:** 00 为长反向颗粒，55 附近为最短颗粒，随后逐步变成长正向颗粒。FF 在其过渡后进入原始延迟模式。
- **Density:** 中心以下为规律颗粒，中心以上为随机时序；中心死区阻止新颗粒生成。远离中心则增加颗粒速率。
- **Pitch:** −24 至 +24 半音；80 为完全齐唱。
- **Scatter:** 00 为无参数随机性。增加此值会施加 Beads 内部统一的 Time 随机性（全幅）以及 Size 随机性（半幅），对每个颗粒独立执行。Pitch 和 Shape 保持设定值。
- **Feedback:** 将处理后的颗粒反馈回录音路径。
- **Shape/Freeze:** 00–7F 为录制，80–FF 为冻结。在任一区间内，低 7 位控制同一包络形状，从矩形到衰减再到钟形及反向。加减 80 可切换冻结状态而不改变形状：40 和 C0 共享相同包络。数值提示显示包络位置和录制/冻结状态。按住 EDIT + 上/下可十六进制增减 10；左/右增减 1。此复合控件为离散模式，不会因渐变穿越冻结位；整体字节调制可故意跨越两状态。
- **Mix:** 00 为完全干声，FF 为完全处理。变化平滑。

冻结功能保持当前效果捕捉的音频；更换模块或重置 Beads 会清除冻结内容。轨道混响发送保持可用。

### 让底鼓抽吸混音或其他轨道

要得到抽吸感的母线，请将底鼓放在轨道 1，在 MASTER FX 中选择 Compressor，将 Detect 设为 03（Track 1），Ratio 设为 04（6:1），Attack 接近 60（约 1.6 ms），Release 接近 C0（约 115 ms），Makeup 设为 00，Mix 设为 FF。降低 Threshold，直到其他声音在每次底鼓时下沉，然后调整 Release，使它们在下一次底鼓前恢复。底鼓的直达声保留自己的轨道插入，但绕过两个母线插入插槽；它的混响和延迟返回仍会抽吸。

若只想压低贝斯、pad 或合成器，请将 Compressor 放在该轨道上，并用 Detect 选择底鼓轨道。源底鼓仍然可听，其音量不会被该轨道的压缩器改变。合并在同一轨道上的底鼓和贝斯无法用这种路由分开。

### FORMANT

在 TF1 或 TF2 上选择 **FORMANT**，可将声音塑造成类似元音的音色。最适合持续且谐波丰富的来源，例如锯齿波。

将 MODE 设为 `00` VOW，用 **VOW** 从 A（`00`）扫到 U（`7F`），途经 E、I 和 O。将 MODE 设为 `01` CUSTOM 可自行调节 **F1** 和 **F2**。**RES** 收窄或加宽元音特性；**MIX** 将效果与原始信号混合。使用 MOD → INSERT FX 来移动活动控件。MODE 不是 MOD 目标。

### 插入轨道 FX

插入命令使用 **F11–F18** 对应 TF1，**F21–F28** 对应 TF2。滚轮的 FX 选择及 Phrase/Table 弹出菜单仅显示所选轨道已配置插入使用的参数。弹出菜单按 **TF1: 效果名称** 和 **TF2: 效果名称** 分组；OFF 插槽隐藏。旁路的插入效果仍可编辑。Phrase 和 Table FX 弹出界面描述依据所选轨道插槽显示：显示模块、参数名称、解码范围端点及已保存的基础值。若模块更改导致某命令不再使用，原有歌曲命令会被保留；打开选择器时默认跳至可用控件。

编辑数值时，下方提示显示解码数值及单位。任意 Phrase 或 Table FX 列可指向任一插槽；Phrase 网格保持三列。绝对运行时数值会跨音符、乐器、片段、链和普通循环持续有效。全新播放或硬停止会重置这些值。F 页编辑基础值仅清除此参数的覆盖。MOD 作用于有效基础/自动化值，未保存偏移。选新模块会恢复默认并清除此插槽覆盖。旁路保留现有自动化，但处理暂停且缓慢淡出。

**地址指轨道插槽/参数位置，非命名效果。** 在另一轨重复使用的 Phrase 会采用该轨模块。模块更换可能改变命令含义，但命令不会重写。OFF/未用地址为安全无操作，且在保存 Phrase/Table 数据时保持不变。

乐器 MOD 选择器新增插入 FX 类别。各乐器源控制其所播放轨道上的插入效果，包括合成和弦信号。不同轨道上的相同乐器影响独立链。现有源定时、有符号量、组合规则、粘滞模式和动作录制均适用。离散控件量化并限制在有效选项内。无新增轨道专属调制库。

建议先添加若干插入，注意过载或爆裂声，对不助歌曲的效果进行旁路。驱动和增益设定可能导致混音削波，调整时请关注 Mixer 削波指示。

## 8. 表格

表格是 Tracker 中的核心音色设计工具。以 Vortex Tracker 术语，它结合了乐器与装饰符，但功能远超于此。若熟悉 LSDj 或 NerdSEQ，应已有相似概念。

Pitch 列接受相对（`~`）或绝对（`=`）半音值。Volume 列范围为 `00`-`0F`，施加于所有引擎包络之上，可用作门控效果：交替 `0F` 与 `00`，再用 `HOP 00` 循环。4 个 FX 轨道大体对应短语中的轨道，尽管部分 FX 在表格中表现不同。

在最后一行表格放置 `TIC` FX 可设置该列速度，覆盖乐器默认表格速度。各 FX 列可设不同滴答速度。Pitch 和 Volume 列跟随第 1 个 FX 列速度。

16 行表格对长条型 Vortex Tracker 乐器而言看似短小，但 `HOP` FX 可实现条件循环，如“重复这些行 5 次”，及嵌套循环。配合循环及独立 FX 列速度，16 行可发挥很大作用。

可在短语和表格之间复制粘贴轨道 FX ;)

### 重新触发模式

表格界面顶部的 `Retrig` 字段选择已激活表格何时重启。默认且兼容模式是 `Inst`：含乐器的短语行选中该乐器默认表格并从第 `00` 行启动。`Phrase` 在进入短语时重启激活表格，`Chain` 在进入链时触发，`Free` 则一直运行直到轨道停止或显式命令替换。

结构模式不会向时间回溯。如果轨道无活跃乐器表格，首个引入乐器的短语行会在该行精确启动表格，即使该行是短语的第 4、8 或 F 行。后续乐器号不会替换活跃的 `Phrase`、`Chain` 或 `Free` 表格。这样表格可用作类 LFO 自动化，且不会无声模拟先前短语步。

`TBL`、`TBX` 和 `RET` 始终为显式动作：无论模式如何，都会立即选择或重启。短语或链边界处重启发生在新短语行读取之前，所以该行上的 `TBL` 或 `TBX` 具最终决定权。表格第 `00` 行 FX 因此正常再运行；累积 FX 保持常规叠加。

表格 `00-7F` 保留给默认乐器表格。表格 `80-FE` 供由 `TBX` 启动的辅助表格使用。低位范围亦能用作辅助，但这样做易导致冲突和混淆。

### 控件

- **EDIT + [UP/DOWN]** 于 FX 名称列：打开 FX 选择界面
- **OPT + [方向键]**：切换表格

## 9. 乐器池与 AY Wavetable

### 乐器池

乐器池显示工程中所有乐器，并允许重新排序。播放时显示当前激活的乐器。

#### 控件

- **EDIT**：编辑乐器并跳转至乐器界面
- **SHIFT + OPT**：复制乐器
- **SHIFT + EDIT**：粘贴乐器
- **EDIT + [UP/DOWN]**：调整乐器顺序
- **EDIT + PLAY**：预览乐器

### AY Wavetable

Wavetable 是 AY Plus 软件振荡器类型。工程可包含最多 256 个波表，每个 32 步。所有波表乐器共享同一组波形，编辑界面即在此。

AY 音量非线性，Standard 视图波形绘制根据实际输出电平调整。双击 **OPT** 切换 Standard 视图与持久化的 **LFO 视图**，该快捷方式同 Song/Live 模式：LFO 视图线性绘制原始 0–15 步，7 与 8 之间为调制零点。

界面有两行逻辑区。第一行为 **Load** 和 **Save** 按钮，第二行为波表编辑器。

#### 控件

- **OPT + [LEFT/RIGHT]**：切换波表列表
- **OPT + [UP/DOWN]**：以 16 波为步长切换列表
- **SHIFT + OPT**：复制波表
- **SHIFT + EDIT**：粘贴波表
- **OPT + PLAY**：切换 Standard 和 LFO 视图

## 10. Tracker FX

轨道 FX 强制对单步的序列器或声音引擎值进行控制。其他 groovebox 通常称之为“参数锁定”。每个短语行有 3 个 FX 槽。### 序列器 FX

| FX | 值 | 详细行为 |
|---|---|---|
| `ARP` | `XY` | 对基音进行琶音，`+X` 步和 `+Y` 步。`37` 产生小三和弦模式。 |
| `ARC` | `XY` | `X` 选择琶音方向或范围模式；`Y` 是速度（以 tick 为单位）。 |
| `PVB` | `XY` | 音高颤音：`X` 是速度，`Y` 是深度。在线性模式下，深度以 `10-cent` 步进计算。 |
| `PBN` | 带符号的 `XX` | 每个短语/表格行增加 `XX` 音高单位；`FF` 表示 `-1`。使用 `00` 停止。 |
| `PSL` | `XX` ticks | 在 `XX` ticks 内从前一个音高滑音到新音符。 |
| `PIT` | 带符号的 `XX` | 音高表步长的累计相对偏移。 |
| `FIN` | 带符号的 `XX` | 线性音高下以 cents 为单位的累计细微偏移，否则为周期单位。 |
| `PRD` | 带符号的 `XX` | 累计相对振荡器周期偏移。 |
| `VOL` | 带符号的 `XX` | 累计相对音量偏移（`FF` 表示 -1，不是全音量）。 |
| `VSL` | 带符号的 `XX` | 每个短语/表格行增加 `XX` 音量，`00` 停止。 |
| `PAN` | `00-FF` | 绝对物件声像：左，中心 (`80`)，右。 |
| `TPN` | `00-FF` | 绝对混音轨道声像：左，中心 (`80`)，右。 |
| `RET` | `XY` | 每 `Y` ticks 重触发一次；`X` 应用音量变化。`Y=0` 停止重触发。 |
| `DEL` | `XX` ticks | 推迟 note-on。延迟超过当前 groove 步长则跳过音符。 |
| `OFF` | `XX` ticks | 经过 `XX` ticks 发送 note-off 并进入 ADSR 释放阶段。 |
| `KIL` | `XX` ticks | 经过 `XX` ticks 硬中断声部，不运行 ADSR 释放。 |
| `TIC` | `XX` ticks | 设置每行的表格 tick 数。表中修改该 FX 列速度。 |
| `TBL` | `00-FE`，`FF` 关闭 | 立即替换并重启乐器表格；`FF` 停止。 |
| `TBX` | `00-FE`，`FF` 关闭 | 立即启动或替换辅助表格；`FF` 停止。 |
| `THO` | 行 `XX` | 跳转所有乐器表格列到行 `XX`。 |
| `TXH` | 行 `XX` | 跳转所有辅助表格列到行 `XX`；表格内不使用。 |
| `GRV` | groove `XX` | 为当前轨道选择 groove。 |
| `GGR` | groove `XX` | 为所有轨道选择 groove。 |
| `HOP` | `XY` | 跳转到第 `Y` 行，重复 `X` 次；`X=0` 无限循环。表格 HOP 仅影响自身列。 |
| `SNG` | 带符号的 `XX` | 在歌曲中移动播放位置 `XX` 行。 |
| `PRO` | `00-64` | 绝对触发概率，`0%` 到 `100%`。 |
| `MOD` | `AB` | 在 `B` 轮循环的第 `A` 轮触发。 |
| `RNN` | `MX` | 播放随机音符。`M` 选择模式：`0` 始终随机，`1` 每次 PLAY 随机，`2` 选定后保持直到数值改变。`X` 选择范围：`0` 任意音符，`1` +3 半音，`2` +6 半音，`3` +12 半音，`4` -3 半音，`5` -6 半音，`6` -12 半音，`7` +/-3 半音，`8` +/-6 半音，`9` +/-12 半音。当音阶引擎应用于该轨道时，随机选择遵循工程音阶。 |
| `SPD` | 带符号的 `XX` | 选择持续的每轨时钟比例；详见速度表。 |
| `SLE` | `00-FF` ticks | 持续轨道滑音时间，用于持续引擎 FX。`00` 立即。停止播放时重置为 `00`。 |

### 音频 FX 发送

| FX | 值 | 详细行为 |
|---|---|---|
| `RSN` | `00-FF` | 设置当前轨道混响发送，直到下一次音符触发。 |
| `DSN` | `00-FF` | 设置当前轨道延迟发送，直到下一次音符触发。 |

### ADSR / 触发 FX

| FX | 值 | 详细行为 |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | 覆盖攻击，衰减，维持，释放或包络形状。 |
| `TDC` / `TCL` | `00-FF` | 覆盖 Plaits 和 Plaits-Alt 的触发模式衰减或色彩。 |

#### 条件

受到瑞典 "trig conditions" 启发，tracker 支持触发概率和模数条件。

| FX | 值 | 含义 |
|---|---|---|
| `PRO` | `00-64` | 绝对触发概率，从 `0%` 到 `100%` |
| `MOD` | `AB` | 在 `B` 轮循环中的第 `A` 次触发；例如 `12`、`22`、`14`、`34` |

`MOD` 计数器局限于轨道和短语。无效组合（如 `A` 大于 `B`）不会触发。

`MOD34` 在每 4 次循环的第 3 次触发。`MOD1F` 在每 16 次循环的第 1 次触发。

#### 播放速度

如同 NerdSEQ，ChooChooTracker 支持每轨独立播放速度。

`SPD` 以带符号字节读取。`00` 为正常速度，正值加速，负值减速。此设置保持生效，直到另一个 `SPD` 命令修改。

| 值 | 速度 |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

旧工程不支持带符号 `SPD` ，保持原有 `00-10` 映射，播放不变。

非常高的倍数可能超出 tick 调度器分辨率，未经硬件测试。

### 音阶与和弦 FX

#### 音阶自动化

`SCL XY` 改变全局播放音阶和根音，但不更改短语中的音符。`X` 选择音阶（`0` 全半音，`1` 大调，`2` 小调，至 `C` 自定义）；`Y` 选择根音（`0` C 到 `B` B）。该命令仅应用于短语 FX，不影响表格。若多个轨道在同一 tick 发送 `SCL`，编号最小轨道获胜。

`SCL` 属于量化器模式。启用 **Note Lock** 时，FX 列表不显示此命令，且忽略短语内已写的 `SCL`。

#### 和弦

`CRD XY` 将位于自身 Phrase 行上的音符转换为和弦。`Y` 选择和弦类型：`0` 大调，`1` 小调，`2` 减三和弦，`3` 增三和弦，`4` Sus2，`5` Sus4，`6` 五度和弦，`7` 大七和弦，`8` 小七和弦，`9` 属七和弦，`A` 小七降五和弦，`B` 减七和弦，`C` 添加九度，`D` 小调添加九度，`E` 大调九度，`F` 小调九度。`X` 选择 16 种转位：`0-3` 是上行闭合转位循环；`4-7` 加入 drop-2；`8-B` 加入 drop-3；`C-F` 同时加入 drop-2 和 drop-3。三和弦和五度和弦的转位会跨越一个额外八度，因此每个数值都独特；较大的数值音程更宽且不寻常。例如，`CRD 4Y` 是 drop-2 根音位，`CRD DY` 是两个 drop 在第一转位。音高表底部或顶部会限制声部在范围内。命令非持久，须每个和弦行写入。启用音阶时，每个生成的音符独立量化，适用于量化器和 Note Lock 模式。仅对软件引擎有效，AY 乐器忽略该命令且保持单音。

### 调制 FX

短语与表格 FX 可更改调制槽，而无需编辑乐器：

| FX 模式 | 含义 |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | 调制槽 1-4 的相对 Amount 偏移 |
| `M11`, `M12`, `M13`, `M14` | 调制槽 1 的相对 P1-P4 偏移 |
| `M21`, `M22`, `M23`, `M24` | 调制槽 2 的相对 P1-P4 偏移 |
| `M31`, `M32`, `M33`, `M34` | 调制槽 3 的相对 P1-P4 偏移 |
| `M41`, `M42`, `M43`, `M44` | 调制槽 4 的相对 P1-P4 偏移 |

值作为带符号 8 位相对变化解释（`01` 加 1，`FF` 减 1），重复命令累积。有效值限制在合法范围。

`P5` 不作为短语或表格 FX 暴露。它用作 `M1-M4 Wavetable` 调制目标，可让一个调制器扫描另一个 AYWavetable 索引。

| 调制类型 | P1 | P2 | P3 | P4 | 当形状为 Wavetable 时的 P5 |
|---|---|---|---|---|---|
| ADSR | 攻击 | 衰减 | 维持 | 释放 | 未使用 |
| AHD | 攻击 | 保持 | 衰减 | 未使用 | 未使用 |
| LFO | 形状 | 触发模式 | 周期 | 未使用 | AY 波表索引 |
| syncLFO | 形状 | 触发模式 | Ticks | 倍数 | AY 波表索引 |
| slowLFO | 形状 | 触发模式 | 周期步进（00–FF） | - | AY 波表索引 |
| FLFO | 形状 | 触发模式 | 频率（`1 Hz` 到 `20 kHz`） | 未使用 | AY 波表索引 |

### Braids FX

| FX | 值 | 含义 |
|---|---|---|
| `BMD` | `00-2E` | Braids 模型 |
| `BTM` | `00-FF` | 绝对归一化音色 |
| `BCL` | `00-FF` | 绝对归一化色彩 |
| `BCF` | `00-FF` | 指数滤波截止，`20 Hz` 到 `20 kHz` |
| `BRS` | `00-FF` | 指数滤波共振 |

### aChChid FX

| FX | 值 | 含义 |
|---|---|---|
| `ASL` | `00-FF` | 滑音到音符，不重触发。`00` 为 `60 ms`，值越大滑音时间越长。 |
| `ADC` | `00-FF` | 衰减覆盖：`200 ms` 到 `2 s`。 |
| `AAC` | `00-FF` | 强调量，从无到最大。 |
| `ATM` | `00-FF` | Braids 音色覆盖；仅在 Braids 波形模式有效。 |
| `ACL` | `00-FF` | Braids 色彩覆盖；仅在 Braids 波形模式有效。 |
| `ACF` | `00-FF` | 指数 303 滤波截止，`20 Hz` 到 `20 kHz`。 |
| `ARS` | `00-FF` | 303 滤波共振，从无到最大。 |
| `AEM` | `00-FF` | 303 滤波包络调制，从无到最大。 |

### Bogie FX

| FX | 值 | 含义 |
|---|---|---|
| `DMD` | `00-0B` | 选择 Bogie 模型，直到下一次触发。 |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | 覆盖衰减、色调、扫描、噪声、FM 或驱动，直到下次触发。所有六个宏对每个模型均可用。 |
| `DCF` | `00-FF` | 滤波截止，按对数映射。 |
| `DRS` | `00-FF` | 滤波共振。 |

### Sintered FX

| FX | 值 | 含义 |
|---|---|---|
| `SMD` | `00-05` | 选择 Sintered 模型，直到下一次触发。 |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | 覆盖衰减、Mod、前两个模型宏、运动或最终模型宏，到下一次触发为止。 |
| `SCF`, `SRS` | `00-FF` | 滤波截止和共振。 |

### MIDI Out FX

| FX | 值 | 含义 |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | 发送 MIDI CC，重映射为 `0-127`。每个插槽对应的 CC 编号由乐器设置（仪器界面中的 `MC1 number`-`MC4 number`），不由 FX 本身设置。未设置编号的插槽无效。 |

### Plaits FX

| FX | 值 | 含义 |
|---|---|---|
| `PMD` | `00-17` | Plaits 引擎 |
| `PHA` | `00-FF` | 绝对归一化谐波成分 |
| `PTM` | `00-FF` | 绝对归一化音色 |
| `PMO` | `00-FF` | 绝对归一化形态 |
| `PAX` | `00-FF` | 主/辅助混合：`00` 主声部，`FF` 辅声部 |
| `PCF` | `00-FF` | 指数截止，`20 Hz` 到 `20 kHz` |
| `PRS` | `00-FF` | 指数滤波共振 |

### 采样 FX

| FX | 值 | 含义 |
|---|---|---|
| `SPL` | `00-03` | 播放模式：`00` 正放，`01` 反放，`02` 循环，`03` 往返循环 |
| `SLI` | `00-FF` | 播放指定切片（`01` 为第一个切片），不受音符影响；`00` 保持正常音符映射。需乐器启用切片模式。 |
| `SPT` | 带符号的 `XX` | 采样转调，单位为半音 |
| `SST` | `00-FF` | 归一化播放起点 |
| `STA` | `00-FF` | `SST` 的别名（归一化播放起点） |
| `SEN` | `00-FF` | 归一化播放终点 |
| `SVL` | `00-FF` | 绝对采样音量 |
| `SCF` | `00-FF` | 指数截止，`20 Hz` 到 `20 kHz` |
| `SRS` | `00-FF` | 指数滤波共振 |
| `SSP` | `00-FF` | 采样速度，从 `0%` 到 `500%` 映射 |
| `RNS` | `M0` | 播放已切片采样的随机切片，而不是分配给该音符的切片。`M` 选择模式：`0` 始终随机，`1` 每次 PLAY 随机，`2` 选定后保持直到数值改变。需要乐器启用 slice 模式；低半字节未使用。 |

### AY 乐器共享 AY FX

| FX | 值 | 详细行为 |
|---|---|---|
| `AYM` | `XY` | `X` 是包络形状；`Y` 选择关闭/音调/噪声/音调+噪声（`0/1/2/3`）。 |
| `NOI` | 带符号的 `XX` | 累计相对噪声周期偏移。 |
| `NOA` | `00-1F`，`FF` | 绝对噪声周期；`FF` 赋予先前轨道噪声周期优先权。 |
| `ERT` | 任意 | 重新触发当前硬件包络形状。 |
| `EAU` | `XY` | 自动包络比例 `X:Y`；`X=0` 禁用。 |

### AY 经典专属 FX

| FX | 值 | 详细行为 |
|---|---|---|
| `EVB` | `XY` | 包络周期颤音：`X` 速度，`Y` 深度。 |
| `EBN` | 带符号的 `XX` | 每个短语/表格行增加 `XX` 到包络周期。 |
| `ESL` | `XX` ticks | 从前一个包络周期滑变到新值。 |
| `ENT` | 音符 `XX` | 设置包络周期为 UI 显示的音高表音符。 |
| `EPT` | 带符号的 `XX` | 累计相对包络周期偏移。 |
| `EPL` | 字节 `XX` | 设置包络周期低字节。 |
| `EPH` | 字节 `XX` | 设置包络周期高字节。 |

### AY Plus FX| FX | 数值 | 详细行为 |
|---|---|---|
| `TNN` | 音符 `XX` | 将音调振荡器设为音高表中的音符，忽略行音符。 |
| `TNP` / `TNF` | 有符号 `XX` | 累积的音调音高步进 / 微调偏移。 |
| `TRT` | 任意 | 在支持的情况下重触发音调振荡器相位。 |
| `ENN` | 音符 `XX` | 将包络振荡器设为音高表中的音符。 |
| `ENP` / `ENF` | 有符号 `XX` | 累积的包络音高步进 / 微调偏移。 |
| `SFT` | `00-07` | 软件类型：无，脉冲，Sync Tone，同步包络，波表，音调FM，包络FM，采样。 |
| `SFN` | 音符 `XX` | 将软件振荡器设为音高表中的音符。 |
| `SFP` / `SFF` | 有符号 `XX` | 累积的软件振荡器音高步进 / 微调偏移。 |
| `SRT` | 任意 | 重置软件振荡器相位。 |
| `SFM` | 有符号 `XX` | 累积的FM深度偏移。 |
| `PWM` | 有符号 `XX` | 累积的脉冲宽度偏移。 |
| `SPL` | 有符号 `XX` | 累积的脉冲低电平偏移。 |
| `SWT` | 有符号 `XX` | 累积的波表索引偏移。 |

### AY 采样 FX

AY 采样支持共享的 AY 命令以及上述的 `TNN`、`TNP`、`TNF`、`TRT`、`SFN`、`SFP` 和 `SFF`。

| FX | 数值 | 详细行为 |
|---|---|---|
| `SMS` | `00-FF` | 设置传统 AY 采样播放起始点为 `XX x 64` 源采样数。 |

引擎参数 FX 作用于对应的乐器类型，且在下一次音符触发时重置，除非另有说明。`SPD` 是故意保持的。

FX 选择器过滤为常用命令加上当前激活乐器支持的指令组。上下文帮助面板在编辑时提供数值细节。

## 11. 混音器、混响和延迟

混音器位于主屏幕最左侧。显示 CPU 负载和削波警告。**CLIP** 列中红色 `!` 标示某轨道的干信号使混音超出安全范围。

用它来平衡 8 条轨道，使用 Tilt EQ 调整每个轨道，并发送音频至混响或延迟：

| 字段 | 含义 |
|---|---|
| LVL | 引擎后轨道音量，`000-100` |
| REV | 发送至共享混响，`000-100` |
| DLY | 发送至共享乒乓延迟，`000-100` |
| PAN | 声音向左或向右偏移 |
| TLT | 单轨 Tilt EQ，`00-FF`；`80`为中性，低值偏重低频，高值偏重高频。低值会带来柔和过载感。 |
| M | `*` 静音此轨道 |
| S | `*` 独奏此轨道 |

混音器基于轨道而非乐器。如果轨道更换乐器，其音量、PAN、Tilt 和发送设置保留在轨道上。乐器本身在 Instrument 屏幕顶行也有独立的 `00-FF` **Vol** 和 **Pan**。乐器 PAN 先应用，然后是轨道 PAN。两者在中心位置保持原有立体声像，左或右偏移时逐渐削弱对侧声音。

### Auto Mix

Mixer 包含一个 **AUTO MIX** 按钮。它离线渲染 `6 秒`，平衡轨道音量，然后将 8 个八度频段与粉红噪声曲线比较，并轻轻压低使这些频段过载的轨道。它给出审慎的电平调整建议，保留一些峰值余量，并开始临时预览。**Apply** 保留建议的电平，**Cancel** 则恢复之前的 8 个电平。这是一个不错的起点，但你可能仍想微调结果。

### 混响

从混音器使用 **SELECT + [UP]** 打开混响设置屏幕。

- **算法**：选择 Clouds 或 Beads。每个算法拥有独立控制项。
- **返回**：混响湿信号音量。
- **Clouds**：含 **Time**、**Damping**，以及预混响的低通（LP）和高通（HP）滤波器。
- **Beads**：含 **Time**，原生反馈滤波 **Tone**，和预混响高通（HP）滤波器。

这是仅针对 Clouds 或 Beads 的发送混响部分，不是颗粒处理器。

### 乒乓延迟

从混音器使用 **SELECT + [DOWN]** 打开延迟，再用 **SELECT + [UP]** 返回。混响则相反：**SELECT + [UP]** 打开，**SELECT + [DOWN]** 返回混音器。

- **返回**：延迟湿信号音量。
- **至混响**：湿延迟信号发送入共享混响的比例，`0-100%`。
- **Ticks**：延迟时间，单位为跟踪器 ticks（默认 `9 ticks` = `1 拍`）。
- **反馈**：交叉反馈量，最大 `95%`。
- **滤波器**：延迟信号路径中的低通滤波器。

第一次重复跟随立体声输入，后续反馈在左右声道交叉。

## 12. 项目屏幕

项目屏幕包含 **加载**、**保存**、**新建**、**导出**、**管理** 和 **调音** 命令，以及文件名、标题和作者元数据。

### 导出位置

导出、分轨和混音输出写入每个项目文件夹，位置在应用预设的 samples 目录下：

- **默认**：`<app folder>/samples/Exports/<project name>/`。未命名项目时文件夹为 `current-project`。
- Web 版本默认为 `/user/samples/Exports/<project name>/`。
- 安卓版本 samples 文件夹位于应用工作区内：Play Store 版本为私有，直接 APK 版本在你选定的文件夹下 `samples`。

导出屏幕中的 **Folder** 行显示当前目标路径：

- 点击 **EDIT** 用文件夹浏览器选择自定义文件夹。选中路径不加项目子文件夹且会在设置中保存。
- **EDIT + OPT**（清除）恢复默认位置。

保存为新名时，默认导出文件夹随名更改，已导出文件随项目保留。自定义文件夹不受影响，自动保存亦不重命名。若目标文件夹已存在，则两个文件夹并存。若追踪器丢失文件夹（如重启后），保存项目时将现有 `current-project` 文件夹重命名。重命名后乐器继续工作：加载的路径会自动更新。

**加载**支持 `.mid`/`.midi` 文件，导入为新项目：音符按 MIDI 通道分组（一通道对应一轨，最多轨数限制），量化为每拍 4 行，置于默认 AY 乐器；MIDI 程序号无芯片调音对应，需要后续挑选真实乐器。仅使用文件首个速度（整体节拍，无分段速度变化）。导出屏幕的 **MIDI** 行执行逆向操作：将当前编排音符、音量及速度/律动写成标准 MIDI 文件（每轨道一 MIDI 通道）；[MIDI Out](#midi-out) 乐器及除音量和全局律动外的其他逐行 FX 无 MIDI 对应项，不做翻译。

**加载**支持 Dirtywave M8 歌曲文件（`.m8s`，固件 2.x-4.x），作为新项目导入。仅导入结构和音符（歌曲行，包含转调的链，带音符、力度和乐器序号的短语，note-off，速度和歌名）；音符保持真实音高（M8 八度命名比此处高两度：其 `C-6` 是这里的 `C-4`）。每个短语用到的乐器以 M8 乐器名导入。M8 **MACROSYN** 乐器转成 Braids 乐器（47 种波形对应 Braids 模型及编号；色调、色彩、滤波类型、截止频率、共振和声像得以继承，截止频率曲线近似）。**SAMPLER** 乐器变为 Sample 乐器：`.m8s` 只存储 M8 卡上的 WAV 路径，搜索路径为 `.m8s` 文件夹、父文件夹以及当前采样文件夹（`Samples` 和 `samples` 均支持）；找不到则保留 M8 路径且静音。导入采样自动提升 +24 半音，因 M8 播放采样的音高比此处低两八度。**FMSYNTH** 转为 Genesis FM 乐器（粗略拟合），**WAVSYNTH** 和 **HYPERSYNTH** 转为 aChChid。**MIDI OUT** 转为 MIDI Out 乐器（通道、Bank、程序及前四个 CC 编号）。其他类型（外部）转成默认 AY 乐器，后续补选真实音色。FX 和表格不导入。

**加载**支持 Little Sound Dj 歌曲文件（`.lsdsng`，单首歌文件）作为新项目导入。加载完整 LSDj 储存文件（`.sav`，含最多 32 首歌）时展开歌单（槽位、名称、版本、大小），可选单首导入（储存时编辑曲目不在列中）。仅导入结构和音符（歌曲行，含转调链，带音符和乐器序号的短语，速度和歌名）；所有音符默认最大音量，LSDj 无音量控制，四个 LSDj 通道 PU1、PU2、WAV、NOI 映射为轨道 1-4。脉冲乐器转为 GB Pulse（占空比、包络和声像保留，不包括脉冲扫频），噪声音色转为 GB Noise（包络、稳定度和声像），波形乐器转为 GB Pulse Hollow Lead 预设，套件转为默认 GB Noise。律动被导入，短语命令 `C`（和弦）、`D`（延迟）、`E`（脉冲和噪声音色包络，波形通道无）、`G`（律动）、`K`（终止）和 `V`（颤音）映射为 `ARP`、`DEL`、`CEI`/`CEP`/`CED`、`GGR`、`KIL` 和 `PVB`。其他命令 (`A`、`F`、`H`、`L`、`M`、`O`、`P`、`R`、`S`、`T`、`W`、`Z`)、表格、波形帧、套件采样和合成设置被忽略，因此依赖它们的歌曲音色会不同。速度转换为 6-tick 律动。

桌面和安卓版本导出屏幕的 **LSDJ** 行做逆向导出（有限制）：点击并选取现有 `.lsdsng` 作为 **模板**（记忆直到关闭程序，**EDIT + OPT** 可清除），其歌曲行、链、短语、速度及标题被取代，其他内容保持不变。轨道 1-4 映射回 PU1、PU2、WAV 和 NOI；GB Pulse 和 GB Noise 乐器写回为 LSDj 脉冲和噪音乐器，其他乐器写为默认脉冲乐器。FX、音符音量及 note-off 不导出，使用链号 `80` 以上或短语号 `FF` 以上的歌曲无法导出（LSDj 限 128 链和 255 短语）。导出文件尚未在 LSDj 自身测试。

桌面版本导出屏幕的 **M8S** 行也做有限逆向导出：点击并选取现有 `.m8s` 作为 **模板**（记忆直到关闭程序，**EDIT + OPT** 清除），歌曲、链、短语、速度和标题被替换，其余保持不变，Braids 乐器写回为 MacroSynth，Sample 乐器写回为 Sampler（路径 `/Samples/<文件名>`，需自行将 WAV 复制至 M8），前提是模板中有该类型乐器以便复制参数。FX 不导出，使用短语号 `FF` 以上的歌曲无法导出（M8 限 255 短语）。导出文件尚未在真实 M8 测试。详情见 [M8 songs](m8s-format.md)。

桌面版支持直接键盘输入文件名、标题和作者，避免使用虚拟键盘，详见 [Key jazz](#key-jazz-desktop-only)。

### 调音 / 量化

调音屏幕控制全局 12-TET 调音引擎。**Mode** 行选择两种模式：- **量化器**（默认）：短语输入保持半音阶模式；启用时，音符在送入声音引擎前立即向下取整，因此改变根音或音阶可以重新和声，而无需编辑短语。
- **音符锁定**：输入的音符会立即被吸附到音阶上最近的音符，只能写入音阶内的音符。新输入和降音会向下吸附；`EDIT + RIGHT` / `EDIT + UP` 可将音符提高到音阶上的下一个音高。此屏幕上输入始终按照工程音阶执行：更改根音或音阶会立即影响启用轨道的新音符，已有音符不受影响。此模式下不可用 `SCL` FX，且已写入的 `SCL` 会被忽略。纯音播放时跳过音符量化（输入已锁定）；和弦量化通过 `CRD` 仍然生效。

- **Clock source** 在 **Internal**（项目自身速度）、**Midi Clock**（音序器跟随传入的 MIDI 时钟）和 **Ableton Link**（音序器跟随本地网络上 Link 会话的速度）之间选择。使用 **Midi Clock** 时，**Tempo** 行变暗并显示测得的传入时钟速度；传输（play/stop）由发送方的 Start/Continue/Stop 消息控制，因此不需要应用内的 PLAY 控件。使用 **Ableton Link** 时，该行变为 **Link** 行：其中是加入/离开会话的开关，而不是速度字段（见下文）。时钟来源保存在 settings.txt 中，适用于所有项目。
- **Tempo** 以整数 BPM（`3-500`）设置项目速度。内部 tracker 仍以 Hz 保存其 tick rate（`tick rate = BPM x 0.4`），因此现有项目的加载和保存不变；带小数速度的项目会四舍五入显示，并在首次编辑速度时吸附到整数 BPM。粗调以 5 BPM 步进，细调以 1 BPM 步进；清除则恢复默认的 125 BPM。

ChooChooTracker 保存项目为 `.cct` 格式。这是一个专有格式，即使是人类也能轻松阅读。

在更改乐器类型或加载其他项目之前，请使用 **Save**。

### Ableton Link

Ableton Link 让 ChooChooTracker 跟随同一本地网络（Wi-Fi 或以太网）中其他支持 Link 的应用（Ableton Live、LinkHut 等）的速度。它只是接收端：tracker 将自己的速度调整到会话速度，绝不会向其他参与者发送速度或传输信息。

**会同步的内容：**

- 会话速度 - tracker 持续跟随 Link 会话速度（在 tracker 的 3-500 BPM 范围内）。
- 节拍相位 - 按 PLAY 会使播放对齐到会话的节拍网格：歌曲从下一小节开始（4 拍量化）。

**不会同步的内容：**

使用 Link 播放期间，ChooChooTracker 每个 4/4 小节检查一次共享节拍相位，并在需要时轻微调整自己的 tick rate。这样可补偿设备之间的细微时钟差异，而不会跳过或重复 tracker tick，也绝不会改变其他参与者的 Link 会话。

- 律动以及 `SPD` 和 `DELAY` 值不受影响 - 它们仍相对于被跟随的速度。
- song/chain/phrase 中的光标位置和混音器设置从不共享。

**使用方法：**

1. 打开 Project 界面，将 **Clock source** 设为 **Ableton Link**。下方一行变为 **Link** 行。
2. 在 **[Connect]** 上按 EDIT 加入会话。右侧的状态消息显示进度：连接时为 `Wait...`，无网络时为 `No net`，未找到会话时为 `No sess`，内部故障时为 `Err`。连接期间，按钮显示 **[Disconnect]**，状态大约每秒在 `connected` 和 `Peers: N`（N = 对等端数量）之间交替。
3. 等待 `connected`，然后按 PLAY - 播放将对齐到会话的下一小节开始。
4. 在 **[Disconnect]** 上按 EDIT（或将时钟来源改回 Internal）即可离开会话。

在 Link 模式下，速度由会话掌控：该行不显示 BPM 字段，会话速度会自动应用到项目（在 tracker 的 3-500 BPM 范围内）。

**各平台可用性：** 支持 PortMaster（主要目标）、Windows、Linux、macOS 和 Android。Web 版本和 Miyoo 版本不包含 Link。

**PortMaster 说明：** 在 PortMaster 设备上音频缓冲区较大（48 kHz 下 512 帧），因此节拍对齐是近似的，约在 10-15 ms 以内，而非采样级精确。速度同步本身不受影响。

## 13. 设置

设置保存应用级偏好，而不是当前项目。项目范围的控制项，如节奏、律动和混音器音量，保持在各自的页面。

### 控制

- **重复延迟** 和 **重复速度** 设置按住导航或编辑按钮后开始重复的时间和速度。按键响应过快时提高延迟；需要快速录入时降低它。
- **STICK LIVE 模式** 选择映射的 **STICK LIVE** 按钮的工作方式：`HOLD` 按住时激活，`TOGGLE` 按一次保持激活，`FREE` 始终激活。详情见 [Motion recording](#motion-recording)。
- **按键映射** 分配键盘或控制器按键。选中命令后按下新按键，点击 **Done** 保存映射。

### 音频

**Sample rate** 提供 `32 kHz` 或 `48 kHz`（默认）。更改立即生效并被保存。更改采样率会停止播放和采样预览，同时声音引擎和任何已启用的音频输入会以新采样率重新打开。如果设备拒绝新采样率，则保留之前的设置。`32 kHz` 减少处理量，并将可播放的最高频率限制为 16 kHz。CPU 负载指示器显示音频渲染的开销。

**缓冲区大小** 在响应速度和稳定性之间权衡。实时演奏需较小值；发现音频断裂或CPU负载持续高时请调大。可选值为 `32`、`64`、`128`、`256`、`512` 和 `1024` 样本。CPU负载显示有助于选择稳定设置。

### MIDI

在此选择输入输出设备，分配MIDI通道给乐器，并映射MIDI CC控制。详情见 [MIDI](#15-midi) – 连接键盘、控制混音器或发送音符给外接硬件。

### 合成器

- **AY 质量** 选择AY/YM乐器的品质等级。**AY Sample dithering** 可平滑AY采样播放；关闭时声音更直接、复古或适合原始硬件。
- **Braids BITS**、**DRFT** 和 **SIGN** 是所有Braids乐器共享的全局参数，用于设置分辨率、漂移和特性，之后可微调单个声音。详见 [Braids](#braids)。

### 混音器

- **混音音量** 为应用主输出音量。连接其他混音器或录音设备时请调低；不会改变歌曲轨道的混音。
- **倾斜轴心** 设定本项目中Mixer所有**Tilt**控件的中心频率，范围 `250 Hz` 到 `4000 Hz`，默认 `1 kHz`。

### 图形

- **编辑色彩主题** 更改界面配色。**加载字体** 用于加载 `.cnfont` 字体文件。
- **歌曲波形** 显示或隐藏歌曲页的混音波形，详见 [Song waveform](#song-waveform)。
- **轨道视觉效果** 为每个轨道选择 **详细**（乐器波形及叠加图形）或 **音频波形**（轨道的最终音频波形）。用 **全部详细** 或 **全部音频** 一键更改所有轨道。
- **波形帧率** 设置仪表和混音视觉的刷新频率，范围 `1` 到 `60` 帧每秒。降低可减少干扰或释放资源。
- **渲染器** 显示当前使用 `GPU` 还是 `Software` 渲染。

### 支持报告与退出

**支持报告** 生成小型诊断文件，用于Bug反馈。问题与某个项目有关时，请连同 `.cct` 文件一起提交。**退出 ChooChooTracker** 干净地关闭应用。

### 歌曲波形

图形 → 歌曲波形默认关闭。点击 EDIT 切换，或用 EDIT + 方向键切换，设置存为应用偏好。开启时，SONG页上方显示两行的最终混合单声道波形。其它页面保持原布局、间距和乐器预览不变。该偏好向前兼容旧版本。SONG触摸区域符合显示波形行。

## 14. 故障排查

### CPU

CPU负载取决于激活的引擎和效果。Plaits物理建模和Clouds混响比基础AY音色开销大。目标设备上运行时注意尤其是8个Plaits声部且两个发送开启时。CPU负载接近 `100%` 会导致音频断裂或丢失声音。

遇到爆音、崩溃或卡顿，使用 **Settings > Support report** 生成 `support-report.txt` 并附带报告。如某个项目触发问题，请携带该 `.cct`。

### 无声音

检查乐器编号、轨道静音/独奏状态、轨道音量、应用混音音量和乐器包络。采样文件需确保原始WAV文件存在于保存路径。

ArkOS 上，按 **MENU + L3** 切换系统静音。如无效，重启设备并重新连接音频接口。ArkOS 的“修复音频”选项也可能有效。

### 输入响应异常

调整设置中的重复延迟和速度。若单次按压触发两次，确认物理控制器只有一个映射到该方向，并报告具体屏幕和操作路径。

## 15. MIDI

设置 > **MIDI** 打开子菜单：**MIDI In** / **MIDI Out** 选择连接设备，端口可循环选择，头尾含 `OFF`；输入输出都需选定设备。

- **MIDI 键盘音色预览**：选定 MIDI 输入设备后，在任意屏幕上演奏键盘音符即可试听乐器，类似 [Instrument](#5-instruments) 页的屏幕快捷键 **EDIT + PLAY**。此为试听功能，不会插入音符。
- **通道映射**：打开16个MIDI通道列表；为每个通道分配乐器（默认 `OFF`）。此通道接收音符均预览对应乐器，无论 Instrument 页当前选中哪一个。如通道留 `OFF`，则回退至选中乐器，即原行为。映射保存到 settings.txt。
- **MIDI CC映射**：将控制器映射至乐器参数（含 **Volume**）或全局控制。全局栏显示 `TRK 1` 至 `TRK 8`，提供轨道的**静音**、**独奏**、**音量**、**混响发送**和**延迟发送**控制；**歌曲播放/停止**不设目标。新映射会忽略初始化时学到的CC值，以物理控制下一次动作生效，不支持播放中复位补丁。静音和独奏用 `0-63` 为关，`64-127` 为开。
- **MIDI 时钟同步**：选定 MIDI In 设备后，将 Project 界面的 **Clock source** 设为 **Midi Clock**，即可跟随外部音序器。tracker 测量传入时钟的速度（每四分音符 24 个脉冲）并显示在变暗的 **Tempo** 行中；**Start**、**Continue** 和 **Stop** 消息控制播放。时钟来源记忆于 settings.txt。
- **外接MIDI设备驱动**：给轨道指定 [MIDI Out](#midi-out) 乐器类型并设定通道后，该轨道发送的音符会发至选定MIDI输出设备，不再在 ChooChooTracker 内产生声音。

## 16. 制作与授权

ChooChooTracker 是 ChipNomad 的分支，维持其 MIT 许可方式。Braids、Plaits、Plaits-Alt、Clouds DSP、Warps 派生 MME DSP 及 stmlib 代码均源于 Mutable Instruments 开源项目，遵守其 MIT 许可。Plaits-Alt 来自 lylepmills/eurorack Plaits Lab 分支，保留原作者声明。aChChid 引擎基于 Robin Schmidt 的 Open303，版权2009年，MIT许可。MonkSynth 使用 JonET 的原生 DSP，MIT 许可；见 `LICENSE.monksynth.txt`。Bogie 和 Sintered 为原创本地实现。MIDI I/O 使用 Gary P. Scavone 的 RtMidi，MIT风格许可。详细授权见随包附带的许可证文件。Ableton Link 由 Ableton 提供特殊授权，不属于 MIT 许可，本项目授权不适用于您的分支或其他项目，仅限 ChooChooTracker。