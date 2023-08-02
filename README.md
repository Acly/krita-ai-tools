# Krita Segmentation Tools

Plugin which adds selection tools to mask objects in your image with a single click, or by drawing a bounding box.

<h2><img src="media/tool_segmentation_point.png"> Select Segment from Point</h2>

Click on things to select them!

<h2><img src="media/tool_segmentation_rect.png"> Select Segment from Box</h2>

Draw a box around things to select them!

## Limitations

Object detection works quite well in both photos and artwork. As a solution for masking the rough shape of distinct objects and regions it is usually much quicker than lasso tool, and more flexible than contiguous selection tool ("magic wand"). Computation is instantanious for medium resolution on recent hardware (especially GPU), for high resolutions there is a noticeable delay.

The generated masks are binary masks, and typically not pixel-perfect, especially for large resolutions (the mask is always an upscale from 1024x1024). It might be possible to improve with tiling, but workflows which require precise masking would still have to invest manual work.

## Installation

The current version of the plugin requires [Krita 5.2.0 (Beta1)](https://krita.org/en/item/first-beta-for-krita-5-2-0-released/).

### Windows

To install, simply unpack the ZIP archive into your Krita installation folder. If Krita is running you have to restart it.

### Linux

Currently no pre-built binaries available, but the plugin can be built from source.

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
