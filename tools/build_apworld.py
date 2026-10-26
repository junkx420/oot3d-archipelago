"""Builds oot3d.apworld from a TriAevum checkout with the Archipelago work.

  python tools/build_apworld.py <TriAevum checkout> <output .apworld>

The archive holds the world's folder (tools/oot3d/archipelago/apworld/oot3d)
as "oot3d/", without its tests and caches, as Archipelago installs it from
custom_worlds.
"""
import pathlib
import sys
import zipfile

SKIPPED_DIRECTORIES = {"test", "__pycache__"}


def main() -> None:
    checkout, output = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    world = checkout / "tools/oot3d/archipelago/apworld/oot3d"
    if not (world / "archipelago.json").is_file():
        raise SystemExit(f"no apworld at {world}")
    output.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(world.rglob("*")):
            relative = path.relative_to(world)
            if path.is_dir() or SKIPPED_DIRECTORIES.intersection(relative.parts) or path.suffix == ".pyc":
                continue
            archive.write(path, pathlib.PurePosixPath("oot3d", *relative.parts))
            count += 1
    print(f"{output}: {count} files")


if __name__ == "__main__":
    main()
