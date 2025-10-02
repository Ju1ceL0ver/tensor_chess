
"""Setup script for building the tensor_chess extension."""

from __future__ import annotations

from pathlib import Path

from setuptools import Extension, find_packages, setup

ROOT = Path(__file__).resolve().parent
C_SRC_DIR = ROOT / "non_python" / "c_src"
README = ROOT / "README.md"

extension = Extension(
    "tensor_chess._tensor_chess",
    sources=[
        "non_python/c_src/c_chess_module.c",
        "non_python/c_src/chess.c",
    ],
    include_dirs=["non_python/c_src"],
    extra_compile_args=["-O3", "-std=c11", "-Wall", "-Wextra", "-Wpedantic"],
)

setup(
    name="tensor-chess",
    version="0.1.1",
    description="High-performance chess move generation with tensor export.",
    long_description=README.read_text(encoding="utf-8") if README.exists() else "",
    long_description_content_type="text/markdown",
    author="tensor-chess developers",
    url="https://example.com/tensor-chess",
    packages=find_packages("src"),
    package_dir={"": "src"},
    ext_modules=[extension],
    python_requires=">=3.9",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: C",
        "Operating System :: OS Independent",
        "Topic :: Games/Entertainment :: Board Games",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
    ],
    license="MIT",
    license_files=["LICENSE"],
    include_package_data=True,
    zip_safe=False,
)
