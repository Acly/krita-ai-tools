# Krita Segmentation Tools

Plugin which adds selection tools to mask objects in your image with a single click, or by drawing a bounding box.

<h2><img src="media/tool_segmentation_point.png"> Select Segment from Point</h2>

Click on things to select them!

https://github.com/Acly/krita-ai-tools/assets/6485914/71fe2bb4-9b00-4eab-b6b7-4e9aa50d2511

<h2><img src="media/tool_segmentation_rect.png"> Select Segment from Box</h2>

Draw a box around things to select them!

https://github.com/Acly/krita-ai-tools/assets/6485914/38d92925-3146-4489-a2ea-a1d3aa57c72c

## Limitations

Object detection works quite well in both photos and artwork. As a solution for masking the rough shape of distinct objects and regions it is usually much quicker than lasso tool, and more flexible than contiguous selection tool ("magic wand"). Computation is instantanious for medium resolution on recent hardware (especially GPU), for high resolutions there is a noticeable delay.

The generated masks are binary masks, and typically not pixel-perfect, especially for large resolutions (the mask is always an upscale from 1024x1024). It might be possible to improve with tiling, or a subsequent alpha-matte step.

## Installation

The current version of the plugin is built for [Krita 5.2.0 RC1](https://krita.org/en/item/krita-5-2-release-candidate-is-out/). Using it with other versions may lead to crashes.

You can download the latest version of the plugin from the [releases page](https://github.com/Acly/krita-ai-tools/releases).

### Windows

Download the plugin and unpack the ZIP archive into your Krita installation folder. Then run Krita.

### Linux

Easiest way is to download the AppImage, make it executable and run it. It contains the entirety of Krita, repackaged with the plugin, but is only available for some versions.

Alternatively get the Krita AppImage from the official source and extract it. This should result in a folder `squashfs-root`. Download the plugin `tar.gz` and extract it into that folder. Then run Krita. See the commands below for some environment variables that are needed to run outside the image.

```sh
./krita-5.2.0-rc1-x86_64.appimage --appimage-extract
tar -xf krita_segmentation_plugin-linux-x64-1.0.1.tar.gz -C squashfs-root/
APPDIR=/squashfs-root APPIMAGE=1 ./squashfs-root/AppRun
```

## Building

The plugin has to be built as part of Krita, see [Building Krita from Source](https://docs.krita.org/en/untranslatable_pages/building_krita.html#).

After GIT checkout, clone this repository into the plugins folder:
```sh
cd krita/plugins
git clone https://github.com/Acly/krita-ai-tools.git
```

Next modify the `CMakeLists.txt` in the same folder (`krita/plugins`) by appending the following line:
```cmake
add_subdirectory( krita-ai-tools )
```

Now build and install Krita as usual according to official instructions, and the plugin will be built alongside.

## Technology

Object detection uses the [Segment Anything Model](https://segment-anything.com/), a deep neural network developed by Meta AI. To get interactive performance on local hardware, an optimized version of the model ([MobileSAM](https://github.com/ChaoningZhang/MobileSAM)) is integrated using the [dlimgedit](https://github.com/Acly/dlimgedit) library.
