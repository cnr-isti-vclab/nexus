# Merge Notes: Master → Version3

**Date**: January 9, 2026  
**Branch**: merge-master-to-version3  
**Strategy**: Selective manual integration of master fixes into version3 architecture

---

## Overview

This merge combines two significantly diverged branches:
- **version3**: 77 commits with major v5.0 architectural changes (GLTF, materials, Draco)
- **master**: 202 commits with bug fixes, optimizations, and incremental improvements

**Key Decision**: Kept version3's architecture while manually porting critical bug fixes and improvements from master's 202 commits.

---

## Version3 Branch Changes (New Architecture)

### Major New Features in Version3:
1. **GLTF Support** (`src/nxsbuild/gltfloader.cpp`, `gltfloader.h`, `gltf.cpp`)
   - Full GLTF/GLB file format support
   - GLTF material conversion to Nexus materials
   
2. **Material System** (`src/common/material.cpp`, `material.h`)
   - New Material class with PBR properties
   - Support for metallic, roughness, normal maps, occlusion
   - BuildMaterial class for build-time material processing
   
3. **Material Builder** (`src/nxsbuild/buildmaterial.cpp`, `buildmaterial.h`)
   - Material unification and texture mapping
   - Atlas offset management
   - Material compaction
   
4. **Draco Compression** (`src/draco/` submodule)
   - Google Draco compression library integration
   - Added as git submodule
   
5. **Node Texture Creator** (`src/nxsbuild/nodetexcreator.cpp`, `nodetexcreator.h`)
   - Advanced texture atlas creation for nodes
   - Material-aware texture packing
   
6. **DAG Support** (`src/common/dag.cpp`, `dag.h`)
   - Directed Acyclic Graph structure for hierarchy
   
7. **Configuration System** (`src/common/config.h`)
   - Build configuration management
   
8. **JSON Support** (`src/common/json.hpp`, `simplejson.hpp`)
   - JSON parsing for GLTF and configuration
   
9. **Version Update**
   - Nexus file format version bumped to 3 (was 2)
   - NEXUS_VERSION file set to 5.0
   
10. **Build System Updates**
    - C++14 requirement (was C++11)
    - GitHub Actions workflows for Linux, MacOS, Windows
    - GLEW integration for nxsview

---

## Master Branch Changes (202 Commits)

### Critical Fixes Ported:

#### **Priority 1: 64-bit File Support & Multithreading**

**Commits**: 4f3ec03, 5caeff8, 2a6dde6, a8eab5c, a6d9deb

**Files Modified**:
- `src/common/nexusfile.h` (14 changes)
- `src/common/qtnexusfile.h` (11 changes)
- `src/common/qtnexusfile.cpp` (15 changes)
- `src/common/nexusdata.cpp` (2 changes)
- `src/nxsbuild/nexusbuilder.cpp` (12 changes)

**Changes**:
1. **64-bit file operations**:
   - Changed `int read(char*, unsigned int)` → `long long int read(char*, size_t)`
   - Changed `int write(char*, unsigned int)` → `long long int write(char*, size_t)`
   - Changed `int size()` → `size_t size()`
   - Changed `void* map(unsigned int, unsigned int)` → `void* map(size_t, size_t)`
   - Changed `bool seek(unsigned int)` → `bool seek(size_t)`
   - **Reason**: Support files larger than 4GB (unsigned int max = 4,294,967,295 bytes)

2. **Virtual destructor**:
   - Added `virtual ~NexusFile() {}` to base class
   - **Reason**: Proper cleanup in polymorphic classes

3. **Multithreading setup**:
   - Changed `#include <QThread>` → `#include <QThreadPool>` and `<QRunnable>`
   - Added `#include <QDir>`
   - **Reason**: Better parallel processing infrastructure

4. **File padding function**:
   ```cpp
   static qint64 pad(qint64 s) {
       const qint64 padding = NEXUS_PADDING;
       qint64 m = (s-1) & ~(padding -1);
       return m + padding;
   }
   ```
   - **Reason**: Proper alignment for 64-bit file operations

5. **Uninitialized pointer fix**:
   - Changed `NexusData::NexusData(): nodes(0), patches(0)...`
   - To: `NexusData::NexusData(): file(nullptr), nodes(0), patches(0)...`
   - **Reason**: Prevent crashes from null pointer dereference

---

#### **Priority 2: Texture & Material Handling**

**Commits**: 877287e, 51d8275, 5a06bc1, c6cf873, 26c4f68, f7d96dd, 327defe

**Files Modified**:
- `src/nxsbuild/plyloader.cpp` (32 changes)
- `src/nxsbuild/objloader.cpp` (5 changes)
- `src/nxsbuild/nexusbuilder.cpp` (simplification threshold)

**Changes**:

1. **Encoding fix for international filenames**:
   - Changed `filename.toLatin1().data()` → `filename.toLocal8Bit().data()`
   - Changed `interpret_texture_name(..., buf2)` → `interpret_texture_name(..., buf2, 255)`
   - **Locations**: PLY file opening and texture name interpretation
   - **Reason**: Support filenames in non-Latin character sets (Chinese, Japanese, Cyrillic, etc.)

2. **OBJ relative index validation**:
   ```cpp
   face_[w] = rr[0] - 1;
   vtxt_[w] = rr[1] - 1;
   normal_[w] = rr[2] - 1;
   if(face_[w] < 0 || vtxt_[w] < 0 || normal_[w] < 0)
       throw QString("Relative indexes in OBJ are not supported");
   ```
   - **Reason**: OBJ files can use negative indices (relative to current position), which aren't supported

3. **Default material color**:
   ```cpp
   nx::BuildMaterial material;
   material.color[0] = material.color[1] = material.color[2] = 1.0f;
   material.color[3] = 1.0f;
   ```
   - **Reason**: Materials without Kd (diffuse color) should default to white, not black

4. **Simplification threshold improvement**:
   - Changed `stream->size()/(float)last_top_level_size > 0.7f` → `> 0.9f`
   - **Reason**: Better handling of heavily parametrized meshes

---

#### **Priority 3: File Format Support**

**Commits**: 180daf9

**Files Modified**:
- `src/nxsbuild/plyloader.cpp` (ushort indices)

**Changes**:

1. **Ushort PLY index support**:
   - Added property descriptors:
     - `PropDescriptor vindices[1]` (vertex_indices, int)
     - `PropDescriptor vindices_uint[1]` (vertex_indices, uint)
     - `PropDescriptor vindices_ushort[1]` (vertex_indices, ushort) ← NEW
     - `PropDescriptor vindex_ushort[1]` (vertex_index, ushort) ← NEW
   - Added to read calls:
     ```cpp
     pf.AddToRead(vindex[0]);
     pf.AddToRead(vindex_uint[0]);
     pf.AddToRead(vindex_ushort[0]);        // NEW
     pf.AddToRead(vindices[0]);
     pf.AddToRead(vindices_uint[0]);
     pf.AddToRead(vindices_ushort[0]);      // NEW
     ```
   - **Reason**: Support PLY files with unsigned short indices (common in smaller meshes)

---

## Changes NOT Ported (Intentionally Skipped)

### 1. **Deepzoom Support** (commit f4f31b0)
- **Reason**: Very recent (Jan 2026), requires extensive nexusfile.h changes that conflict with version3 architecture
- **Status**: Defer to future work

### 2. **Level Extraction Options** (commit bce36d2, nxsedit -l/-L)
- **Reason**: Feature addition, not a bug fix; can be added later if needed
- **Status**: Low priority

### 3. **STL Export** (commit 6823aad)
- **Reason**: New feature, not critical for merge
- **Status**: Can be cherry-picked later

### 4. **TS File Format Support** (commits 38dc81d, tsloader.cpp/h)
- **Reason**: Already present in master, brought in via merge
- **Status**: Automatic merge handled this

### 5. **Colormap Support** (commit baa5751, colormap.cpp/h)
- **Reason**: Already present in master, brought in via merge
- **Status**: Automatic merge handled this

### 6. **VCG Loader** (commit a3759ef, vcgloader.h)
- **Reason**: Already present in master, brought in via merge
- **Status**: Automatic merge handled this

---

## Merge Conflicts Resolution Strategy

### Automatic Resolutions:
- **JavaScript files**: Took master's versions (latest Three.js updates)
- **HTML demos**: Took master's versions
- **nexus3d directory**: Kept from master (new JavaScript module architecture)
- **Workflows**: Combined both (5 total: 3 from version3 + 2 from master)

### Manual Resolutions:
- **CMakeLists.txt**: 
  - Kept version3's C++14 requirement
  - Kept version3's Draco integration
  - Adopted master's vcglib/corto package finding logic
  - Adopted master's install/export rules

- **src/CMakeLists.txt**:
  - Combined source lists from both branches
  - Kept version3's GLTF sources
  - Kept master's colormap/tsloader/vcgloader sources
  - Kept version3's Draco linking

- **Core C++ files** (nexus.cpp, nexusdata.cpp, etc.):
  - Initially took version3's versions (for v5.0 architecture)
  - Then manually ported master's bug fixes on top

- **nxsbuild files**:
  - Kept version3's versions (GLTF and material system)
  - Then manually ported master's bug fixes on top

- **Submodule (src/corto)**:
  - Took master's version (more recent update)

---

## Testing Recommendations

### Critical Tests:
1. **Large file support**: Test with files >4GB
2. **International filenames**: Test PLY/OBJ with non-ASCII paths
3. **Ushort PLY**: Test PLY files with unsigned short indices
4. **OBJ materials**: Test OBJ files without Kd specification
5. **GLTF loading**: Ensure GLTF support still works
6. **Material system**: Verify material unification and texture mapping
7. **Draco compression**: Test Draco-compressed meshes

### Build Tests:
1. Verify C++14 compilation on all platforms
2. Test with system-provided vcglib/corto packages
3. Test with bundled vcglib/corto
4. Verify Draco submodule builds correctly

---

## File Change Summary

```
7 files changed, 60 insertions(+), 31 deletions(-)

src/common/nexusdata.cpp      |  2 +-
src/common/nexusfile.h        | 14 ++++++--------
src/common/qtnexusfile.cpp    | 15 +++++----------
src/common/qtnexusfile.h      | 11 +++++------
src/nxsbuild/nexusbuilder.cpp | 12 ++++++++++--
src/nxsbuild/objloader.cpp    |  5 +++++
src/nxsbuild/plyloader.cpp    | 32 ++++++++++++++++++++++++++++----
```

---

## Known Limitations

1. **Deepzoom not supported**: Recent master feature not included
2. **Some texture handling differences**: Version3's material system handles textures differently than master
3. **Multithreading not fully implemented**: Infrastructure added but Worker class implementation deferred
4. **TextureGroupData vs TextureData**: Version3 has different texture architecture, some master texture fixes may not apply directly

---

## Future Work

1. Consider porting deepzoom support when stable
2. Evaluate adding level extraction options (-l/-L) to nxsedit
3. Monitor for additional master commits and port as needed
4. Full multithreading implementation using QThreadPool
5. Performance testing and optimization

---

## References

- Version3 base commit: `7d5b387`
- Master base commit: `c368b00`
- Common ancestor: `f9c8b72` ("small changes in the corto worker")
- Master commits analyzed: 202 (71 with C++ changes)
- Version3 commits: 77
- Manual fixes ported: ~20 critical commits

---

## Verification Commands

```bash
# Check 64-bit support
grep -n "size_t" src/common/nexusfile.h

# Check encoding fix
grep -n "toLocal8Bit" src/nxsbuild/plyloader.cpp

# Check ushort support
grep -n "vindices_ushort" src/nxsbuild/plyloader.cpp

# Check relative index validation
grep -n "Relative indexes" src/nxsbuild/objloader.cpp

# Check default color
grep -n "material.color\[0\] = material.color\[1\]" src/nxsbuild/objloader.cpp

# Check file initialization
grep -n "file(nullptr)" src/common/nexusdata.cpp

# Check pad function
grep -n "static qint64 pad" src/nxsbuild/nexusbuilder.cpp

# Check multithreading includes
grep -n "QThreadPool" src/nxsbuild/nexusbuilder.cpp
```

---

**Merge Completed By**: GitHub Copilot (AI Assistant)  
**Review Required**: Human verification of critical path functionality
