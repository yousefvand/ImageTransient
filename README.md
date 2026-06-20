# Image Transient

A small Qt 6 desktop app for Arch Linux that creates a short MP4 transition video from two still images.

The app is Wayland-friendly because it does **not** record the screen. It uses Qt for the UI and FFmpeg for rendering.

## Features

- Select or drag-and-drop two images
- Export MP4/H.264 video
- Choose transition type
- Set first-image hold duration, transition duration, and second-image hold duration
- Set output resolution, FPS, CRF quality, and encoder preset
- Copy the generated FFmpeg command
- KDE menu `.desktop` entry included

## Supported input images

PNG, JPEG, WebP, BMP, TIFF and anything else your FFmpeg build can decode.

## Build on Arch Linux

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base ffmpeg
./build-arch.sh
./build/imagetransient
```

## Install on Arch Linux

```bash
./install.sh
```

Then launch **Image Transient** from the KDE application launcher, or run:

```bash
imagetransient
```

## How rendering works

The app builds an FFmpeg command using the `xfade` filter. Both input images are normalized to the selected output size, frame rate, pixel format, sample aspect ratio and time base before the transition is applied.

Example command shape:

```bash
ffmpeg -loop 1 -i image1.png -loop 1 -i image2.png \
  -filter_complex "[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30,format=rgba,trim=duration=4,setpts=PTS-STARTPTS[v0];[1:v]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30,format=rgba,trim=duration=4,setpts=PTS-STARTPTS[v1];[v0][v1]xfade=transition=fade:duration=2:offset=2,format=yuv420p[v]" \
  -map "[v]" -t 6 -an -c:v libx264 -preset medium -crf 18 -movflags +faststart output.mp4
```

## Notes

- If FFmpeg fails with an unsupported transition name, choose `fade` first, then test other transitions.
- Lower CRF means higher quality and bigger files. CRF 18 is a good high-quality default.
- 1920×1080 at 30 FPS is a good default for normal videos.
