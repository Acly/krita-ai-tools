import ctypes
import os
import sys
from krita import Extension, Krita
from pathlib import Path


class VisionMLExtension(Extension):
    """Loader for Vision ML tools and filters.

    This is not actually a Python plugin, it just acts as a loader for the native libraries.
    This makes distribution and installation easier.
    """

    def __init__(self, parent):
        super().__init__(parent)

        if sys.platform in ["win32", "cygwin", "msys"]:
            ext = ".dll"
        elif sys.platform == "linux":
            ext = ".so"
        elif sys.platform == "darwin":
            ext = ".dylib"
        else:
            raise RuntimeError(f"Unsupported platform: {sys.platform}")

        lib_dir = Path(__file__).parent / "lib"
        lib_file = lib_dir / f"kritavisionml{ext}"

        executable_dir = Path(sys.executable).parent
        env_path = os.environ.get("PATH", "")
        os.environ["PATH"] = (
            f"{executable_dir}{os.pathsep}{lib_dir.resolve()}{os.pathsep}{env_path}"
        )
        try:
            lib = ctypes.CDLL(str(lib_file.resolve()))
            lib.load_vision_ml_plugin()

        except OSError as e:
            deps = ""
            for dependency in lib_dir.glob(f"*.{ext}"):
                try:
                    ctypes.CDLL(str(dependency.resolve()))
                except OSError:
                    deps += f"\nFailed to load dependency {dependency}"
            raise RuntimeError(
                f"Failed to load VisionML library from {lib_file}: {e}{deps}"
            )
        finally:
            os.environ["PATH"] = env_path

    def setup(self):
        pass

    def shutdown(self):
        pass

    def createActions(self, window):
        pass


Krita.instance().addExtension(VisionMLExtension(Krita.instance()))
