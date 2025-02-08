# Krita Segmentation Tools

Plugin which adds selection tools to mask objects in your image with a single
click, or by drawing a bounding box.

<h2><img src="media/tool_segmentation_point.png"> Select Segment from Point</h2>

Click on things to select them!

https://github.com/Acly/krita-ai-tools/assets/6485914/71fe2bb4-9b00-4eab-b6b7-4e9aa50d2511

<h2><img src="media/tool_segmentation_rect.png"> Select Segment from Box</h2>

Draw a box around things to select them!

https://github.com/Acly/krita-ai-tools/assets/6485914/38d92925-3146-4489-a2ea-a1d3aa57c72c

### Precise mode

Select "Precise" in Tool options to get better quality masks. Depending on
hardware the operation can take several seconds. This model typically extracts
all foreground objects in the area, rather than one specific object that is
contained entirely in the box.


## Installation

The current version of the plugin is built for [Krita 5.2.9](https://krita.org/en/download/krita-desktop/).
Using it with other versions may lead to crashes.

You can download the latest version of the plugin from the [releases page](https://github.com/Acly/krita-ai-tools/releases).

### Windows

Download the plugin and unpack the ZIP archive into your Krita installation
folder. Then run Krita.

Hints:
* The default installation folder is `C:\Program Files\Krita (x64)`
* Copy/extract the contents of the ZIP directly, don't make a separate folder

### Linux

You can use [this script](scripts/install-krita-with-segmentation.sh) to
download and patch Krita with the plugin. It also creates a shortcut to run it.

To do it manually, get the Krita AppImage from the official source and extract
it. This should result in a folder `squashfs-root`. Download the plugin `tar.gz`
and extract it into that folder. Then run Krita. See the commands below for some
environment variables that are needed to run outside the image.

```sh
./krita-5.2.9-x86_64.appimage --appimage-extract
tar -xf krita_segmentation_plugin-linux-x64-1.1.0.tar.gz -C squashfs-root/
APPDIR=/squashfs-root APPIMAGE=1 ./squashfs-root/AppRun
```

#### Arch Linux
Arch users can use the `PKGBUILD` on [the AUR](https://aur.archlinux.org/packages/krita-ai-tools), or with `yay` or `paru`:

```sh
yay -S krita-ai-tools
# or
paru -S krita-ai-tools
```

#### GPU support

Disclaimer: untested, and maybe not worth the hassle.

To use the GPU backend on Linux you need:
* An NVIDIA GPU for CUDA support
* [CUDA Toolkit 12.x](https://developer.nvidia.com/cuda-downloads) installed
* [cuDNN 9.x](https://developer.nvidia.com/cudnn) installed
* CUDA Toolkit and cuDNN libraries must be in `LD_LIBRARY_PATH`

### Alternative Models

For your own experiments only, not officially supported.

#### Precise mode

[List of alternative model downloads](https://github.com/ZhengPeng7/BiRefNet/releases/tag/v1)

To use a different model, find the model folder and replace one or more of the
models there (_recommended to make a backup!_) 
* Models are located in `share/krita/ai_models/segmentation`.
* Models must be `.onnx` files.

Files to add or replace:
* `birefnet_cpu.onnx` - preferred when using CPU backend
* `birefnet_gpu.onnx` - preferred when using GPU backend
* `birefnet_hr_cpu.onnx` - preferred when using high resolutions (~2K)
* `birefnet_hr_gpu.onnx` - preferred when using high resolutions (~2K)

If a sepcific model doesn't exist, the other ones are used as fallback. So you
only really need one of them.

> [!TIP]
> On older GPUs it might make sense to delete the `birefnet_gpu.onnx` file for better performance.


## Building

The plugin has to be built as part of Krita, see [Building Krita from Source](https://docs.krita.org/en/untranslatable_pages/building_krita.html#).

After GIT checkout, clone this repository into the plugins folder:
```sh
cd krita/plugins
git clone https://github.com/Acly/krita-ai-tools.git
```

Next modify the `CMakeLists.txt` in the same folder (`krita/plugins`) by
appending the following line:
```cmake
add_subdirectory( krita-ai-tools )
```

Now build and install Krita as usual according to official instructions, and the
plugin will be built alongside.

## Technology

* Object detection: [Segment Anything Model](https://segment-anything.com/), [MobileSAM](https://github.com/ChaoningZhang/MobileSAM)
* Dichotomous segmentation: [BiRefNet](https://github.com/ZhengPeng7/BiRefNet)
* Inference implementation: [dlimgedit](https://github.com/Acly/dlimgedit)
