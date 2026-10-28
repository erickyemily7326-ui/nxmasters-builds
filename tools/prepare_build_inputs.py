import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import zipfile

CORE_HASHES = {
    "4248": "6f4473845483fa2e7be9d1e8aeac17cd12561d8153d51201037826b9c1b5ffde",
    "3668": "4e378da548984dc0208ff2240aae003942ae9c5a6eec3a36c838c65ac6a37a63",
}

def write_member(archive, member, destination):
    path = PurePosixPath(member.filename)
    if path.is_absolute() or ".." in path.parts or "\\" in member.filename:
        raise ValueError(f"Unsafe archive member: {member.filename}")
    if member.file_size > 128 * 1024 * 1024:
        raise ValueError(f"Oversized archive member: {member.filename}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    with archive.open(member) as source, temporary.open("wb") as target:
        while chunk := source.read(1024 * 1024):
            target.write(chunk)
    os.replace(temporary, destination)

def extract_core(apk, version, root):
    with zipfile.ZipFile(apk) as archive:
        core = archive.read("lib/arm64-v8a/libemucore.so")
        if hashlib.sha256(core).hexdigest() != CORE_HASHES[version]:
            raise ValueError(f"{apk} does not contain the validated {version} native GS core")
        destination = root / f"NetherSX2-v2.2n-{version}"
        for member in archive.infolist():
            if not member.is_dir() and (member.filename.startswith("assets/") or
                                        member.filename == "lib/arm64-v8a/libemucore.so"):
                write_member(archive, member, destination / member.filename)
        print(f"Extracted and verified core {version}: {destination}")

def extract_sdk(path, root):
    prefix = "opt/devkitpro/portlibs/switch/"
    with zipfile.ZipFile(path) as archive:
        for member in archive.infolist():
            if not member.is_dir() and member.filename.startswith(prefix):
                relative = member.filename[len(prefix):]
                if relative.startswith(("include/", "lib/", "share/")):
                    write_member(archive, member, root / relative)
    for required in ("include/EGL/egl.h", "include/vulkan/vulkan_core.h", "lib/libEGL.a", "lib/libvulkan.a"):
        if not (root / required).is_file():
            raise ValueError(f"SDK is missing {required}")
    print(f"Extracted Mesa SDK: {root}")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk-4248", type=Path)
    parser.add_argument("--apk-3668", type=Path)
    parser.add_argument("--cores-dir", type=Path, default=Path("out/cores"))
    parser.add_argument("--mesa-sdk", type=Path)
    parser.add_argument("--sdk-dir", type=Path, default=Path("out/mesa-sdk"))
    args = parser.parse_args()
    if bool(args.apk_4248) != bool(args.apk_3668):
        parser.error("Provide both APK paths")
    if args.apk_4248:
        extract_core(args.apk_4248, "4248", args.cores_dir)
        extract_core(args.apk_3668, "3668", args.cores_dir)
    if args.mesa_sdk:
        extract_sdk(args.mesa_sdk, args.sdk_dir)

if __name__ == "__main__":
    main()
