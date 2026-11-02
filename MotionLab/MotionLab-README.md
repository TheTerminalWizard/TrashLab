# MotionLab

Small C++17 tools that generate animated frames (PPM) which you can turn into GIF/MP4 with ffmpeg.
ابزارهای کوچک C++17 برای ساخت فریم‌های متحرک (PPM) که با ffmpeg به GIF یا MP4 تبدیل می‌شوند.

<a id="summary"></a>
## 📋 Summary | خلاصه

| Project | پروژه | One line | Links |
|---|---|---|---|
| **MotionLab_v1** | مشین‌لب نسخه ۱ | Text command → shape animation (48×48) | [📁 Code](./MotionLab_v1.cpp) · [🇬🇧 EN](#v1-en) · [🇮🇷 FA](#v1-fa) |
| **MotionLab_v2** | مشین‌لب نسخه ۲ | 10 effects + custom width/height | [📁 Code](./MotionLab_v2.cpp) · [🇬🇧 EN](#v2-en) · [🇮🇷 FA](#v2-fa) |
| **MotionLab_v3** | مشین‌لب نسخه ۳ | Any aspect ratio + hex colours + background | [📁 Code](./MotionLab_v3.cpp) · [🇬🇧 EN](#v3-en) · [🇮🇷 FA](#v3-fa) |

---

<a id="v1-en"></a>
### MotionLab_v1 (English)
The first version. You pass one quoted text command and it renders a fixed 48×48 animation at 10 fps.
- Shapes: `circle square triangle heart polygon N star N ellipse R rounded_square R spike N`
- Colours: black, white, red, green, blue, yellow. Give several colours to get a smooth gradient over time.
- Morph between two shapes with `to`.

```
clang++ -std=c++17 -O2 MotionLab_v1.cpp -o motionlab1
./motionlab1 "star 5 yellow red 3s"
./motionlab1 "heart red to circle blue 2s"
```
[📁 Code](./MotionLab_v1.cpp) · [🇮🇷 فارسی](#v1-fa) · [➡ Next: v2](#v2-en) · [⬆ Summary](#summary)

<a id="v1-fa"></a>
### MotionLab_v1 (فارسی)
نسخه‌ی اول. یک دستور متنیِ داخل کوتیشن می‌دهی و برنامه انیمیشن ۴۸×۴۸ با ۱۰ فریم در ثانیه می‌سازد.
- شکل‌ها: `circle square triangle heart polygon N star N ellipse R rounded_square R spike N`
- رنگ‌ها: black، white، red، green، blue، yellow. با دادن چند رنگ، رنگ شکل به‌تدریج بین آن‌ها تغییر می‌کند.
- با کلمه‌ی `to` می‌توان یک شکل را به شکل دیگر تبدیل کرد.

```
clang++ -std=c++17 -O2 MotionLab_v1.cpp -o motionlab1
./motionlab1 "star 5 yellow red 3s"
./motionlab1 "heart red to circle blue 2s"
```
[📁 کد](./MotionLab_v1.cpp) · [🇬🇧 English](#v1-en) · [➡ بعدی: v2](#v2-fa) · [⬆ خلاصه](#summary)

---

<a id="v2-en"></a>
### MotionLab_v2 (English)
Effect-based version with a configurable canvas size (`--width`, `--height`).
- Effects: `circle fade move scale rainbow explosion rotate fire shake pulse`
- Options are passed as `key=value` (e.g. `shape=heart color=red duration=3`).
- 10 named colours (adds orange, purple, cyan, magenta).

```
clang++ -std=c++17 -O2 MotionLab_v2.cpp -o motionlab2
./motionlab2 --width 64 --height 64 --effect rotate shape=star color=yellow
```
[📁 Code](./MotionLab_v2.cpp) · [🇮🇷 فارسی](#v2-fa) · [⬅ Prev: v1](#v1-en) · [➡ Next: v3](#v3-en) · [⬆ Summary](#summary)

<a id="v2-fa"></a>
### MotionLab_v2 (فارسی)
نسخه‌ی مبتنی بر افکت، با اندازه‌ی قابل تنظیم برای بوم (`--width` و `--height`).
- افکت‌ها: `circle fade move scale rainbow explosion rotate fire shake pulse`
- تنظیمات به شکل `key=value` داده می‌شوند (مثل `shape=heart color=red duration=3`).
- ۱۰ رنگ با نام (به‌علاوه‌ی orange، purple، cyan، magenta).

```
clang++ -std=c++17 -O2 MotionLab_v2.cpp -o motionlab2
./motionlab2 --width 64 --height 64 --effect rotate shape=star color=yellow
```
[📁 کد](./MotionLab_v2.cpp) · [🇬🇧 English](#v2-en) · [⬅ قبلی: v1](#v1-fa) · [➡ بعدی: v3](#v3-fa) · [⬆ خلاصه](#summary)

---

<a id="v3-en"></a>
### MotionLab_v3 (English)
Adds aspect ratios and flexible colours; shapes now scale to fit any canvas.
- Aspect ratio: `--ratio 9:16` (also `3:4`, `1:1`, `9:20`, `16:9`, `9x16`, `9/16`) with `--size N` for the long side. Sizes are made even so MP4 encoding works.
- Colours: names (`Red`, `blue`), hex (`#FFFF00`, `#0FD`, `#FF000080` with alpha) or `255,128,0`. Invalid colours give an error.
- `--bg <colour>` sets the background.
- Fixes: fire effect random-range bug; `param=` now works for polygon, star, spike, ellipse, rounded_square.

```
clang++ -std=c++17 -O2 MotionLab_v3.cpp -o motionlab3
./motionlab3 --ratio 9:16 --size 128 --bg "#101030" --effect rotate shape=star color="#FFD400"
```
Quote hex colours in the shell: `"#FF0000"`.

[📁 Code](./MotionLab_v3.cpp) · [🇮🇷 فارسی](#v3-fa) · [⬅ Prev: v2](#v2-en) · [⬆ Summary](#summary)

<a id="v3-fa"></a>
### MotionLab_v3 (فارسی)
نسخه‌ای با پشتیبانی از نسبت تصویر و رنگ انعطاف‌پذیر؛ شکل‌ها حالا با اندازه‌ی بوم تنظیم می‌شوند.
- نسبت تصویر: `--ratio 9:16` (همچنین `3:4`، `1:1`، `9:20`، `16:9`، `9x16`، `9/16`) و `--size N` برای ضلع بلندتر. ابعاد زوج می‌شوند تا ساخت MP4 مشکلی نداشته باشد.
- رنگ: نام (`Red`، `blue`)، هگز (`#FFFF00`، `#0FD`، و `#FF000080` با شفافیت) یا `255,128,0`. رنگ نامعتبر پیام خطا می‌دهد.
- `--bg <رنگ>` رنگ پس‌زمینه را مشخص می‌کند.
- اصلاحات: خطای بازه‌ی تصادفی در افکت fire؛ پارامتر `param=` برای polygon، star، spike، ellipse و rounded_square کار می‌کند.

```
clang++ -std=c++17 -O2 MotionLab_v3.cpp -o motionlab3
./motionlab3 --ratio 9:16 --size 128 --bg "#101030" --effect rotate shape=star color="#FFD400"
```
در ترمینال کد هگز را داخل کوتیشن بنویس: `"#FF0000"`.

[📁 کد](./MotionLab_v3.cpp) · [🇬🇧 English](#v3-en) · [⬅ قبلی: v2](#v2-fa) · [⬆ خلاصه](#summary)

---

## From frames to video | از فریم تا ویدیو

```
ffmpeg -framerate 10 -i video_output/frame%03d.ppm -pix_fmt yuv420p output.mp4
ffmpeg -framerate 10 -i video_output/frame%03d.ppm output.gif
```
