#!/usr/bin/env python3
"""Compile actual tag routing: DLAA output must copy back after evaluation."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/Stereo/EngineStereoDlss.cpp").read_text()
start = source.index("            const bool inputSized =")
end = source.index("            if (input) eyeTags", start)
classification = source[start:end]
assert "if (isOutput) after[nAfter++] = job;" in source
assert "if (r == sl::Result::eOk && nAfter)" in source
program = r"""
#include <cassert>
namespace sl { constexpr unsigned kBufferTypeScalingOutputColor = 4; }
struct Extent { unsigned width, height; };
struct Tag { unsigned type; };
int classify(unsigned type, Extent a, Extent in, Extent out) {
    struct { Tag tag[1]; } tags{{{type}}};
    unsigned i = 0;
""" + classification + r"""
    return isOutput ? 2 : input ? 1 : 0;
}
int main() {
    const Extent full{6288,3000}, low{2096,1000};
    // Same-sized input/color/depth/MV/mask stay inputs; only tag4 is output.
    for (unsigned t : {0u,1u,2u,3u,29u}) {
        assert(classify(t,full,full,full)==1);
        assert(classify(t,low,low,full)==1);
    }
    assert(classify(4,full,full,full)==2);
    assert(classify(4,full,low,full)==2);
    // Preserve existing output-sized auxiliary handling in upscaling mode.
    assert(classify(2,full,low,full)==2);
    assert(classify(5,{1,1},full,full)==0);
    assert(classify(5,{1,1},low,full)==0);
}
"""
program = "#include <initializer_list>\n" + program
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory)/"routing.cpp"
    exe = Path(directory)/"routing"
    cpp.write_text(program)
    subprocess.run(["g++", "-std=c++17", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS: DLAA output routes to copy-back; inputs/exposure/upscaling preserved")
