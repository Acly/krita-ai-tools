# Krita Vision Tools

[Installation](#installation) | [Release Download](https://github.com/Acly/krita-ai-tools/releases) | [Building](#building)

Plugin for painting application Krita which adds various tools and filters based on machine learning:
* Selection tools to mask objects in your image
* Background removal filter
* Alternative "Smart Patch" for filling/smoothing small areas

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

![precise-mode](https://github.com/user-attachments/assets/f48e9e4e-a009-4275-b6d3-03cf1359cc08)

## Background Removal

Filter which extracts colors belonging to foreground objects in a layer.
You can find it at Filters › Other › Background Removal...

![background-removal]()

## Installation

The current version of the plugin is built for [Krita 5.2.11](https://krita.org/en/download/krita-desktop/).
Using it with other versions may lead to crashes.

You can download the latest version of the plugin from the [releases page](https://github.com/Acly/krita-ai-tools/releases).
Currently **Windows** and **Linux** are supported.

### Plugin installation

Since version 2.0 the plugin can be installed as a Python extension.
In Krita, go to Tools › Scripts › Import Python Plugin from File...
and select the `.zip` file you [downloaded](https://github.com/Acly/krita-ai-tools/releases).

![plugin-installation]()

Accept and restart Krita. The plugin should now be active, and the tools appear
in the tool bar.

<details>
<summary>Krita Python Plugin manager</summary>
![krita-plugin-manager]()
</details>

> [!WARNING]
> If you have an older version (before 2.0) of the plugin installed, please remove it first.
> The easiest way is re-installing the latest version of Krita (you will keep your settings).

### Alternative Models

The Plugin comes with a default ML model for each task. There are alternative
models which can yield higher precision and better results, but usually at the
cost of running slower and higher memory requirements.

Model files have the `.gguf` file extension.

#### Background Removal

[Models Download](https://huggingface.co/Acly/BiRefNet-GGUF/tree/main)

You can find the location where to place models with the "Folder" button in the Background Removal Filter dialog.

![model-location]()


## Building

To build the plugin, it has to be part of the Krita source tree and build alongside.
Refer to [Building Krita from Source](https://docs.krita.org/en/untranslatable_pages/building_krita.html#) for setting up the environment.

After GIT checkout of the Krita sources, go to the root of the Krita repository,
and clone this repository into the plugins folder:
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

* Inference implementation: [vision.cpp](https://github.com/Acly/vision.cpp)
* Object detection: [Segment Anything Model](https://segment-anything.com/), [MobileSAM](https://github.com/ChaoningZhang/MobileSAM)
* Dichotomous segmentation: [BiRefNet](https://github.com/ZhengPeng7/BiRefNet)
* Inpainting: [MI-GAN](https://github.com/Picsart-AI-Research/MI-GAN)
