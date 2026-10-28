import argparse
from pathlib import Path
import shutil

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cores-dir", type=Path, required=True)
    args = parser.parse_args()
    workspace = Path(__file__).resolve().parent.parent
    generated = workspace / "launcher" / "romfs" / "res"
    generated.mkdir(parents=True, exist_ok=True)
    for version in ("4248", "3668"):
        source = args.cores_dir / f"NetherSX2-v2.2n-{version}" / "assets"
        if not (source / "GameIndex.yaml").is_file():
            raise ValueError(f"Missing core assets: {source}")
        target = generated / version

        if target.is_symlink() or target.resolve() != workspace / "launcher" / "romfs" / "res" / version:
            raise ValueError(f"Unsafe generated resource directory: {target}")
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(source, target, ignore=shutil.ignore_patterns("dexopt"))
        print(f"Bundled {version} resources")

if __name__ == "__main__":
    main()
