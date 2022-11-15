# avif-format

An AV1 Image (AVIF) file format plug-in for Adobe® Photoshop®.

Single images can be loaded and saved using 8, 10 or 12 bits-per-channel, image sequences and animations are not supported.

HDR files that use the Rec. 2100 PQ and SMPTE 428-1 transfer functions can be loaded and edited as 32-bits-per-channel documents.
The Rec. 2100 HLG transfer function is not supported, these images will be loaded as SDR 16-bits-per-channel documents.
   
32-bits-per-channel documents can be saved as HDR AVIF files using the following transfer functions and bit-depths:   
* Rec. 2100 PQ using either 10-bits-per-channel or 12-bits-per-channel.   
* SMPTE 428-1 using 12-bits-per-channel.

32-bits-per-channel documents can also be saved as 10-bit or 12-bit SDR AVIF files using the `None, clip` transfer characteristic.

This plug-in uses [libheif](https://github.com/strukturag/libheif) with the [AOM](https://aomedia.googlesource.com/aom/) decoder and encoder.

The latest version can be downloaded from the [Releases](https://github.com/0xC0000054/avif-format/releases) tab.

### System Requirements

* Windows 7, 8, 10 or 11.
* A compatible 32-bit or 64-bit host application.

## Installation

1. Close your host application.
2. Place `Av1Image.8bi` in the folder that your host application searches for file format plug-ins.
3. Restart your host application.
4. The plug-in will now be available as the `AV1 Image` item in the Open and Save dialogs.

### Installation in Specific Hosts

#### Photoshop

Photoshop CC common install folder: `C:\Program Files\Common Files\Adobe\Plug-ins\CC`   
Version-specific install folder: `C:\Program Files\Adobe\Photoshop [version]\Plug-ins`

### Updating

Follow the installation instructions above and allow any existing files to be replaced.

## License

This project is licensed under the terms of the GNU Lesser General Public License version 3.0.   
See [License.md](License.md) for more information.

# Source code

## Prerequisites

* Visual Studio 2019
* The dependencies in the 3rd-party folder, see the [read-me](3rd-party/README.md) in that folder for more details.

## Building the plug-in

* Open the solution in the `vs` folder
* Update the post build events to copy the build output to the file formats folder of your host application
* Build the solution

```
 Adobe and Photoshop are either registered trademarks or trademarks of Adobe Systems Incorporated in the United States and/or other countries.
 Windows is a registered trademark of Microsoft Corporation in the United States and other countries.   
 All other trademarks are the property of their respective owners.
```