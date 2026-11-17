# Host-made pictures through the game's own 2D draw: the texture bank

Read-only research, 2026-11-17. PAL disc (GEDP01). Nothing was built or run.

Tags: **[read]** read in `build/ed-db/disasm.txt`, `generated/main.dol` or the named file. **[disc]** read from
`dump/GEDP01.iso` with `tools/ed/disc.py` (a scratch script parsed `UBookPak.bin`; hashes are `xxhash.xxh64(base level,
seed 0)`, the method that gives image 27 = `0x7cd7b88669459bbb` and image 31 = `0x8680aee4cb0d0a97`, matching
`04-options-subpages.md` and `runtime/host/tome.cpp:39`). **[inferred]** not verified.

`r13 = 0x80625420`, `r2 = 0x80628460`. SDK names come from `generated/sdk_symbols.inc`.

---

## 1. Summary

- **A "bank" is the Dolphin SDK's `TEXPalette`**: the TPL file image in memory, with its offsets turned into absolute
  pointers in place. It is not a `GXTexObj` array. A `GXTexObj` is built on the stack, every time a texture is loaded, from
  the bank's `TEXHeader` (`0x801569F8` = the SDK's `TEXGetGXTexObjFromPalette`, which calls `GXInitTexObj` and
  `GXInitTexObjLOD`). [read]
- **The image number is a plain index into the bank's descriptor array**, with no bounds check (`0x801569E8`). [read]
- **Building a bank from the host is small**: a 12-byte header, 8 bytes a descriptor, 36 bytes a `TEXHeader`, then the
  CMPR data (32-byte aligned). Write pointers directly, set "unpacked" = 1, and never call the unpack function on it.
- **The host renderer keys textures by guest address + XXH3 of the bytes.** An override is asked by GX format and size,
  then by Dolphin's name hash (XXH64 of the base level), only when a texture is decoded (a cache miss). It may return
  RGBA8 of any size. A unique-size, unique-hash CMPR stub in scratch can be claimed and replaced by a host picture at
  any resolution, as `tome.cpp` does for image 31. [read]
- **From the Options frame hook the calls work.** hle.c's current `tome_stops_frame` already calls `0x801117F4`,
  `0x8019C9C8` and `0x8019CC10` from the intercept on `0x80101980`. But **the backdrop leaves the book-frame bank
  `r13-24320` bound, not UBookPak**, and a textured quad also needs the texcoord array set with `0x8019D178`. [read]
- **The game's 2D textures are stored upside down.** The full-quad texcoords at `0x80254D10` put t = 1 at the quad's top,
  so a host picture (stub and override) must be stored upside down too. [read]
- **No safe free slot in UBookPak was proved.** The images no constant in the code points at are runes, portraits, the
  "Load" and "Prologue" plaques and the like, which the game reaches through tables. Replacing one by hash changes it
  everywhere it is drawn. A host bank is the clean route. [read + disc; the "unused" list is inferred]

---

## 2. Question 1: what a bank is in memory

### 2.1 How UBookPak's TPL is loaded and prepared

| Step | Address | What it does |
|---|---|---|
| File read done | `0x801108C0` (callback) | Closes the file (`0x80207994`, DVDClose by its place [inferred]), then calls `0x80110844` at `0x801108EC`. [read] |
| Pak relocate | `0x80110854`: `0x801DCDD8(r13-24300)` | Pak format: `+0` count, `+4` magic `0x6B5`, then 8 bytes an item from `+8` (offset, size). Each offset gets the pak base added, making it a pointer. An item whose `+4` is `0xFB90` is a nested list and is relocated the same way. [read: `0x801DCDD8`..`0x801DCE8C`] |
| Item 0 | `0x80110860`: `0x801DCED0(pak, 0)` -> `r13-24316` (`0x8061F524`) | `0x801DCED0` checks the magic and the count, then returns `*(pak + 8 + 8*i)`. Items 1, 2 and 3 go to `r13-24212`, `r13-26464` and `r13-26268`. [read] |
| TPL unpack | `0x801108AC`: `0x80156924(r13-24316)` | The game's copy of the SDK's `UnpackTexPalette` (`0x80220A58`, which also checks the magic `0x0020AF30`). See 2.2. [read] |
| Again | `0x80116D6C` | The widescreen toggle reloads the Tome art and runs `0x80110844` again (`04-options-subpages.md` §6). **`r13-24316` can change, so read it each frame.** [read] |

On the disc, `UBookPak.bin` has 4 items. Item 0 is at `+0x40`, 1,295,264 bytes, magic `0x0020AF30`, 95 images, table at
`+0xC`. [disc]

### 2.2 The unpack: the TPL becomes a `TEXPalette` in place

`0x80156924(pal)` [read: `0x80156924`..`0x801569E4`]:

```
pal->descriptorArray (+8) += pal                       ; always
for i < pal->numDescriptors (+4):
    d = &descriptorArray[i]                             ; 8 bytes each
    if d->textureHeader:  d->textureHeader += pal
        if header->unpacked (+35) == 0: header->data (+8) += pal; header->unpacked = 1
    if d->CLUTHeader:     d->CLUTHeader += pal
        if clut->unpacked (+2) == 0:   clut->data (+8) += pal; clut->unpacked = 1
```

**Hazard:** the descriptor array and header pointers are relocated whatever the flag says, so a second run on the same
bank corrupts it. A host bank must never be passed to `0x80156924`. [read]

**The layouts** (the SDK's `texPalette.h` structures; the offsets are confirmed by the readers below) [read]:

| Structure | Offset | Field | Read by |
|---|---|---|---|
| `TEXPalette` (12) | +0 | version `0x0020AF30` | `0x80220A70` (SDK unpack only) |
| | +4 | number of descriptors (u32) | `0x801569D4` |
| | +8 | descriptor array pointer | `0x801569E8` |
| `TEXDescriptor` (8) | +0 | `TEXHeader*` | `0x80156A1C` |
| | +4 | `CLUTHeader*`, 0 if none | `0x801E2C4C` (non-zero: the CI path) |
| `TEXHeader` (36, stride `0x24` in the file) | +0 | height (u16) | `0x80156A48` -> `GXInitTexObj` r6 |
| | +2 | width (u16) | `0x80156A44` -> r5 |
| | +4 | format (u32; 14 = CMPR) | `0x80156A4C` -> r7 |
| | +8 | data pointer | `0x80156A3C` -> r4 |
| | +12, +16 | wrap s, wrap t | -> r8, r9 |
| | +20, +24 | min filter, mag filter | -> `GXInitTexObjLOD` r4, r5 |
| | +28 | LOD bias (f32) | -> f3 |
| | +32 | edge LOD (u8) | -> r7 |
| | +33, +34 | min LOD, max LOD (u8); mipmap = (min != max) | `0x80156A20`..`0x80156A38` |
| | +35 | unpacked (u8) | `0x8015695C` |

### 2.3 Binding: `0x801E1B74(bank)`

```
old = *(r13-22132)              ; 0x8061FDAC, the bound bank
if bank != old: *(r13-22132) = bank; 0x801E1898()
return old
```

[read: `0x801E1B74`..`0x801E1BB0`]. **It returns the previous bank**, so a caller can put it back.

`0x801E1898` does no texture work. It clears the material system's shadow of GX state to -1: per-slot "last image
number" shorts at `0x80601898` (11 of them), "last texture height" words at `0x806018B8`, the TEV/texgen shadows
(`0x806019B8`.., `0x80602AF8`..), and `r13-22076`, `r13-22068`, `r13-21952`, `r13-21996`, `r13-22000` (base
`0x805FF6C0` = `0x80600000 - 2368`). [read: `0x801E1898`..`0x801E1B70`] So binding a new bank forces the next material to
reload its textures.

### 2.4 Material and texture load: `0x8019C7C8` -> `0x801E21FC` -> `0x801E2700`

- `0x8019C7C8(&rgba, field, image, flags)` copies the 32-byte template at `0x8023B060` to the stack. It stores `image`
  as short `field` (unless field == -1), `flags` at `+24` and `*rgba` at `+28`. Then it calls
  `0x801E21FC(block, 0, 703, 0)`. [read: `0x8019C7C8`..`0x8019C86C`]
- **The template** [read: DOL]: `ffff x11` (shorts 0..10: the image per texture role, all "none"), `+22` 0, `+24` 0,
  `+28` `0xFFFFFFFF`.
- `0x8019C870(&rgba, img0, img5, flags)` is the variant that sets shorts 0 and 5 together (the Journal's pairs,
  `0x80108A6C`..). [read]
- `0x801E21FC` runs `0x801E2700` once per texture role (role 10 first, then the list at `0x8025B9D8`). [read:
  `0x801E2318`, `0x801E2338`..`0x801E2444`]
- In `0x801E2700`, for a role whose short is not -1 [read: `0x801E2954`..`0x801E2CF4`]:
  1. If the material's flags (`+24`) have the role's bit, the short is an animation and goes through `0x801EA8C8`.
     Otherwise it is the image number as it is. The pages' flags `0x80000000` have no role bits.
  2. **The shadow check:** if the number equals `0x80601898[role]`, nothing is reloaded (`0x801E2A68`, counted at
     `r13-21996`).
  3. Otherwise: `TEXGet(*(r13-22132), n)` (`0x801569E8` at `0x801E2C48`). With a CLUT: `0x80156AC8` (`GXInitTexObjCI` +
     `GXInitTlutObj`) and `GXLoadTlut`. Without: **`0x801569F8(bank, &obj, n)` -> `GXInitTexObj` (`0x8021C91C`),
     `GXInitTexObjLOD` (`0x8021CBD8`)**, then `GXLoadTexObj(&obj, map)` (`0x8021CFA4` at `0x801E2CCC`).
     `GXGetTexObjHeight` is noted in `0x806018B8[role]`.
  4. Role 9 uses other palettes (`r13-21812`/`r13-21808`, the fog ramp). Role 3 uses fixed objects at `0x80602...`.
     A null material uses the caller's palette array with image 0 (`0x801E2C94`).

**Answer to question 1:** the bank is the TPL file image, relocated in place into the SDK's `TEXPalette` (a pointer to the
descriptor array, then pointers to headers and data). The GX texture object is made from the header at load time; nothing
is pre-built. The bank word is only `r13-22132`. The other readers of it, `0x800ED160`, `0x800EDDE4`, `0x801DFFAC` and
`0x801E3A94`, all use it as the same `TEXPalette*` through `0x801569F8`. [read]

`GXInitTexObj` keeps only `(ptr >> 5) & 0x01FFFFFF` of the data address (`0x8021CA70`: `rlwinm r0, r27, 27, 7, 31`), so
**texture data must be 32-byte aligned**. [read]

---

## 3. Question 2: building a bank in guest scratch

### 3.1 Where

| Range | State |
|---|---|
| `SCRATCH_MENU 0x822F0000` | Taken by the uncommitted stops code: label words at `+64*i` (i < 4) and colours at `+0x400`/`+0x404` (`hle.c` `tome_stops_labels`, `tome_stops_frame`). [read] |
| `0x822F1000`..`0x822FFFFF` (60 KB) | **Proposed** `SCRATCH_PLAQUES`. It lies between the co-op Tome's memory end (`0x822D8020`) and `SPELL_COLOUR 0x82300000`, as the `SCRATCH_MENU` comment says. A grep finds no other `0x822F`/`0x822E` use. [read; free is inferred] |

- The guest RAM is 48 MB (`.deps/GXRuntime/src/core/cpu.c:19-28`, `ram_size += 24 MB`). The renderer's resolver accepts
  any address under `ram_size` (`.deps/GXRuntime/src/guest_memory.c:174-200`, `aurora_backend.cpp:944-985`). [read]
- `GXInitTexObj`'s 25-bit `ptr >> 5` covers 1 GB, so `0x822F1000` is fine. [read]

### 3.2 The bytes (all big-endian, all addresses absolute)

For N images at base `B` (32-aligned):

| At | Size | Value |
|---|---|---|
| `B+0` | 4 | `0x0020AF30` (only the SDK unpack checks it; write it anyway) |
| `B+4` | 4 | N |
| `B+8` | 4 | `B + 12`, the descriptor array |
| `B+12 + 8i` | 4 | `H_i`, the header pointer |
| `B+16 + 8i` | 4 | 0 (no CLUT, so not the CI path) |
| `H_i = B + 12 + 8N + 36i` (4-aligned) | 2 | height |
| `H_i+2` | 2 | width |
| `H_i+4` | 4 | 14 (`GX_TF_CMPR`) |
| `H_i+8` | 4 | `D_i`, the data (**32-aligned**) |
| `H_i+12`, `+16` | 4 + 4 | 0, 0 (`GX_CLAMP`, as UBookPak's own; images 23, 24 and 40 use 1, repeat) |
| `H_i+20`, `+24` | 4 + 4 | 1, 1 (`GX_LINEAR`) |
| `H_i+28` | 4 | `0x00000000` (bias 0.0f) |
| `H_i+32` | 1 | 0 (edge LOD) |
| `H_i+33`, `+34` | 1 + 1 | 0, 0 (no mipmaps) |
| `H_i+35` | 1 | **1 (unpacked)** |
| `D_i` | `ceil(w/8) * ceil(h/8) * 32` | CMPR data |

All of UBookPak's 95 headers have wrap 0,0 (except 23, 24 and 40), filters 1,1, bias 0, LOD 0-0 and unpacked 0 on the
disc. [disc]

**CMPR layout** [read: the disc decoder written for this, which drew the images right]: 8x8 tiles, row-major. Each tile
holds four 8-byte DXT1 sub-blocks (top-left, top-right, bottom-left, bottom-right). A sub-block is `c0` (u16 BE RGB565),
`c1`, then 4 bytes of 2-bit indices, the leftmost pixel in the top bits. With `c0 <= c1`, index 3 is transparent.

**A stub that shows nothing while the override is not ready:** fill every sub-block with
`00 00 kk kk FF FF FF FF`. Here `c0 = 0` and `c1 = kk kk` (any value >= 0 keeps transparent mode), and every index is 3,
so every pixel is transparent. Use `kk kk` to make each plaque's bytes, and so its hash, unique: for example a plaque id
in the first sub-block and a version number in the second. When `apply` returns 2 ("still being made"), the decoded stub
shows (`gxcore_draw.cpp:2995-3001`), so a transparent stub hides that frame.

**Orientation:** see 5.3. Row 0 of the stored picture is the bottom of what is seen.

### 3.3 Caches and when a change is seen

| Cache | Keyed by | What a host change needs |
|---|---|---|
| The game's slot shadow `0x80601898[role]` | image number only (per role), reset by a bank change (`0x801E1898`) | Binding the host bank (a new pointer) resets it, and so does binding back. **Changing bytes in place does not need a reload on the guest side**: the GPU state still points at the same address. [read] |
| The renderer's per-frame hash memo `g_frameTextureHashes` | (address, size, format, tlut) | Cleared at each EFB copy and frame end (`gxcore_draw.cpp:1730`, `961`). **An in-place change in the middle of a frame is seen from the next frame.** [read] |
| The renderer's texture cache `g_textureCache` | `TextureKey{address, tsize, format, w, h, (tlut), content_hash}`, where `content_hash` = XXH3 of the bytes, XOR'd with the override's generation when claimed (`gxcore_draw.cpp:2672-2737`, struct at 302) | New bytes make a new key. The old entry at the same address is evicted (`g_textureAddrKey`, `gxcore_draw.cpp:2878-2894`). **No explicit invalidation is needed.** |
| The override's answer | cached with that entry. `apply` runs only on a miss (or again while it returned 2) | To redraw the host picture with the guest bytes unchanged, change the claimer's `generation` (or the bytes' version number). |
| The game's TMEM/`GXInvalidateTexAll` | not modelled: the renderer reads guest RAM by address at draw time | Nothing. [inferred from the resolve path] |

**Two memories (player 2's copy Tome):** `tome_copy_swap` (`hle.c` ~16354) points `cpu->ram` at a whole copy, and the
renderer is told with `Mem1Turn` (`aurora_backend.cpp:2121-2124`, `2482-2489`). So:

- The bank must be present in **whichever memory is in** when it is drawn. Write it, or check its magic and a canary word
  and rewrite it, in the frame hook before binding.
- **Keep the bytes identical at the same address in both memories.** The per-frame hash memo is keyed by address, not by
  memory. Different bytes at one address in one frame would reuse the first half's hash and texture for the second, or
  thrash the cache between the two. A plaque that differs per player gets its own address or id. [read + inferred]
- The renderer reads the bytes when the frame is flushed (`Op::Present`), not at the draw call, so they must not change
  between the draw and the end of the frame. [read: `aurora_backend.cpp:2125-2129`; inferred for the timing]

---

## 4. Question 3: how the host renders guest textures and applies overrides

### 4.1 The path (`.deps/GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.cpp`, `resolve_texture_handle`, 2660-3010)

1. **EFB-copy shadows first** (`g_efbCopyTextures.find(address)`). A scratch address is never one of these.
2. `content_hash = XXH3_64bits(bytes, tsize)`, plus the TLUT for CI. This is the cache identity, memoised per frame.
3. **`claims(format, w, h)`.** If it accepts and `claims_hash` is set: `override_hash = XXH64(bytes, base_level_size, 0)`,
   which is **Dolphin's name hash** (the `tex1_WxH_<hash>_<fmt>` name). It covers the texels only, never the palette,
   and the base level rounded to whole blocks. Then `claimed = claims_hash(format, w, h, override_hash)`.
4. When claimed: `content_hash ^= (generation(format, w, h) + 1) * 0x9E3779B97F4A7C15`. The HD pack is skipped
   (`pack_generation` is 0 when claimed).
5. On a cache miss the texture is decoded to RGBA8 (`gxc::decode_texture`). If overridden,
   `ask_override(decoded, w, h, hash, &out, &ow, &oh)` calls `apply_hash` (the host's `apply_any`).
   - 1: `out` (malloc'd RGBA8 of **any size**) is uploaded as a 1-mip texture (`upload_replaced`, `new_static_texture_2d(w,
     h, 1, ...)`).
   - 2: the decoded original shows and it is asked again on later frames (`g_pendingOverride`).
   - 0: the original.
6. Cached under the key (3.3).

The API comment says the same (`.deps/GXRuntime/include/gxruntime/aurora_backend.h:149-174`). [read]

### 4.2 The host's override (`runtime/host/buttons.cpp:275-331`)

- `claims_any`: titlevideo, the prompt sheet (`claims`), fonts, `tome_claims` (CMPR 432x480), `art_claims` (glyph_art's
  sizes). [read: 279-283]
- `claims_hash_any`: titlevideo by hash. **Any claimed size that is neither tome's nor art's returns true with no hash
  check** (`if (!tome_claims(...) && !art_claims(...)) return true;`, 290-291). Tome and art sizes are claimed only for
  their hashes. [read: 287-294]
- `apply_any`: dispatch in the same order; **the default falls to `lightmaps_apply`** (308). [read: 296-309]
- `generation_any`: tome and art share `art_generation()`; the default is `lightmaps_generation()`. [read: 313-323]
- `buttons_init` registers them (`dol_aurora_set_texture_override`, 325-331). [read]

**tome.cpp's image 31** (`runtime/host/tome.cpp`): `tome_claims` = CMPR 432x480 (541-543). `tome_claims_hash` = one of
`kPages {0x8680aee4cb0d0a97, 0x7ba7ca0b431bba6a}` and the hand font loaded (545-547). `tome_apply` returns a 1728x1920
(`kScale` 4) RGBA8 picture built on a thread. It returns 2 until the build is ready, then 1 with a copy (549-572). The
picture is written **upside down** (`tome.cpp:62`, `110`, `448`). [read]

### 4.3 Would a host-made 256x64 CMPR be claimed and replaced at any resolution?

**Yes, once a claimer is added** [read; inferred until run]:

- Add `plaques_claims(format, w, h)` (CMPR and the plaque sizes) to `claims_any`.
- In `claims_hash_any`, return `plaques_claims_hash(hash)` for those sizes. Without that, any game texture of the same size
  is claimed too (290-291).
- In `apply_any`, put a branch **before** the `lightmaps_apply` fallback.
- In `generation_any`, a plaques generation, bumped when a plaque's picture changes (language, label, layout).

The upload is 1 mip with linear filtering (`GXInitTexObjLOD` from the header). A picture far larger than its screen size
aliases when shrunk. Size it to the screen: about the rectangle in 640x480 units times the render scale (tome.cpp uses
4x).

---

## 5. Question 4: drawing from the Options frame hook

### 5.1 It works there

hle.c's uncommitted `tome_stops_frame` (the intercept on `0x80101980`, installed with the check `0x9421FFD0`) already
guest-calls `0x801117F4(0, alpha)`, `0x8019C9C8` (twice per stop), `0x8019CC10` and the text calls, in both player 1's
Tome and player 2's copy Tome. That is the same material path (`0x8019C7C8` with image -1 at `0x8019CA18`). [read]
`hle_guest_call` takes up to 8 integer arguments plus f1 (`runtime/host/main.c:57-80`), which is enough for
`0x8019D084` (8).

### 5.2 State that must be set first

| What | Why | Evidence |
|---|---|---|
| **Bind the bank:** `old = 0x801E1B74(B)` | After `0x801117F4` the bound bank is **`r13-24320`, the book frame's**. Its last bind is at `0x80111A60`, for the medallion marker. It binds UBookPak only in the middle (`0x80111940`..`0x80111998`). | [read] |
| **Set the texcoord array:** `0x8019D178(0x80254D10)` | `0x8019D1A4` sends texcoord **indices** 0..3 (`0x8019D704`) and sets no array. `0x8019D178` is `GXSetArray(GX_VA_TEX0 = 13, ptr, 4)`. The backdrop left TEX0 on its own table (`0x8019D358`, `0x80252714`). The game does the same before its free quads (`0x80109704` Journal; `0x80114460` Load/Save through `0x8019D358(0x80254D10, …)`). | [read] |
| Vertex format | `0x8019D1A4` sets its own (`0x801E16D0(6)`, `GXBegin(0x80, 5, 4)`). | [read: `0x8019D1D0`..`0x8019D1E4`] |
| Projection | Set by the Tome frame before the page frame: `0x8019CFD4(5)` (`0x80111234`, step 4 in `03-tome-options.md` §4.1) is `C_MTXOrtho(t 0, b 480, l 0, r 640, n 1, f 30360)`, or l -80 / r 720 when `r13-24660 == 1` (widescreen), then `GXSetProjection`, identity `GXLoadPosMtxImm`. | [read: `0x8019CFD4`..`0x8019D080`; floats at `r2-20588`..`-20568`] |
| Colour | `*rgba` is the material colour. The page art uses `r13-27120` = `0xFFFFFFFF` [DOL `0x8061EA30`]. The tab art puts the page's pulse into the alpha byte (`0x801119E8`). | [read] |
| **Put the bank back:** `0x801E1B74(old)` | Later draws in the frame (the ring with no texture, then the text at `0x801DA574`, which binds its own font banks at `0x801D7C58`/`0x801D7E88`) don't need it, but `0x801DFFAC` and others read `r13-22132` as found. | [read] |

### 5.3 Texcoords and orientation

`0x80254D10` holds four (s, t) s16 pairs: `(0, 0x200) (0x200, 0x200) (0x200, 0) (0, 0)`, for the vertices
`(x0,y0) (x1,y0) (x1,y1) (x0,y1)`. 0x200 is 1.0 (9 fraction bits) [inferred from it being the whole image], and **the top
of the quad samples t = 1, the last stored row**. That is why the disc's images are upside down (seen in the decodes:
"Load", "Chapter", the portraits). A host stub and its override must be stored upside down. For a sub-rectangle of an
atlas, use `0x8019D084(texcoords, x0, y0, x1, y1, z, 0, 5)` with four such pairs in scratch, in 1/512 units, t flipped
the same way. [read: DOL bytes, `0x8019D084`..`0x8019D174`]

### 5.4 Z and order

- The ortho's near is 1 and far is 30360, so **z -1 is the front and -30360 the back**. The page picture (image 27) is at
  -30360. The wings, the tab art, the rings and the host's current plain frames are at -1. [read: `03-tome-options.md`
  §4.3 table; floats above]
- Whether the 2D material tests z was not decoded. The game draws everything in painter's order and its -1 draws land on
  top, so **draw the plaques at z -1, after the backdrop and before the gold ring**. Then the ring (also -1) and the text
  elements (drawn later by `0x801DA574`) sit on top under either rule (LEQUAL or ALWAYS). [inferred]
- The hook runs inside the Tome's frame, which the host already draws per player half (the ring and the plain frames
  land in the right half today). So the plaques follow the same split and the same 640x480 page layout, widescreen
  included (the page stays at x 144..575 there). [inferred from the existing draws]

---

## 6. Question 5: a free slot in UBookPak instead?

### 6.1 The images the code draws with UBookPak bound

From every `0x8019C7C8`/`0x8019C870` call, the nearest `0x801E1B74` before it and the constants or tables feeding r5
[read; tables resolved where noted]:

| Page / function | Images |
|---|---|
| Backdrop `0x801117F4` (per section: page, tab, lower art) | 0: 27, 32, 50; 2: 0, 1, 51; 3: 4, 7, 52; 4: 44, 46, 53 (section 1 binds nothing: book-frame bank) |
| Controller `0x80114BD8` | 31, 28, 41, 29, 42, 43 |
| Audio `0x80115950`; slider `0x801151F0(30, 18, 91, …)` | 27, 38, 40, 57, 39; 30, 18, 91 |
| Display `0x80116470` | 33, 37, 36, 35, 34, 21 (brightness picture); **95 and 96 for language 5 (Japanese) only** (`0x8011666C`), past the English bank's 95 images |
| Load/Save `0x8011440C` | 27, 22, 24, and 25, 26 from the pair at `r2-23792` (`0x0019001A`) |
| Card dialog `0x800B4840` | 74 (`0x800B542C`; its others come from its own bank at `0x80309124+16`) |
| Journal `0x801083F8`, `0x8010917C`, `0x801093BC` | 6, 45, 76-87 (pairs), 49, 60, 47 + n |
| Magick `0x8010D9AC`, `0x8010DFC4`, `0x8010E6E4`, `0x8010EBA8`, `0x8010EE08`, `0x8010F228` | 2, 3, 5, 6, 8, 9, 13, 14, 15, 16, 54, 55, 56, 88-90, 93, 94 |
| Map `0x80112594` | 0, 58 (the rest from its own bank `r13-24216`) |

**Options and its four sub-pages draw:** 18, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
40, 41, 42, 43, 50, 57, 74, 91.

### 6.2 Not found behind a constant (candidates, [inferred] unused)

| Image | Size | What it is (decoded) | Dolphin hash |
|---|---|---|---|
| 10, 11, 12 | 116x7 | a thin bar; **9-12 are identical** (one hash for all four; 9 is drawn by Magick) | `0x736f9cda785904fc` |
| 17 | 50x50 | a rune on parchment (the rune set: 16 is drawn) | `0xb4ff4f012b9e7278` |
| 19 | 48x46 | a soft white glow shape | `0xfad2c70ba848121b` |
| 20 | 40x40 | a skeletal hand (a pointer) | `0xfc69fbdd44af65ef` |
| 23 | 64x32 | the "Load" plaque (24 is "Save"), wrap repeat | `0xc21147fae9991fff` |
| 47, 48 | 81x38, 73x49 | "Chapter", "Prologue" (drawn as 47 + n, `0x801094E8`) | `0xba96141d93c3d542`, `0xda7339914ded6ddb` |
| 59 | 67x72 | a scroll with a rune | `0x596f7c89f900803b` |
| 61-73, 75 | 126x89 | character portraits | 61 `0x4dc0cc88542db31e` … (full list in the appendix) |
| 92 | 126x89 | a blank white soft-edged card | `0x99435315d168f899` |

Every one of these is game art that some page probably reaches through a table or an index the scan did not follow
(Journal's 47 + n, the portraits by chapter, the Load plaque). So none is proved unused.

### 6.3 Why a slot is the weaker route

- **A hash override is global:** every draw of that image (any page, both players, the title screen if it shares it)
  shows the host picture.
- **Only one language's pak is known** (English, item 0 of `UBookPak.bin`). The other languages' paks
  (`0x802522B4` names) have their own images and hashes, as for image 27/31 today.
- **The size is fixed** by the slot: a 126x89 picture stretched to any rectangle (the override can still be any
  resolution).
- **Four identical images share one hash** (9-12), so "one slot" can mean several.
- A host bank has none of these problems and costs about 100 bytes plus the stub data. **Use the host bank.**
- If a slot is still wanted: run the Tome on every page with `DOL_GXCORE_TEXTURE_DUMP=<dir>` (`gxcore_draw.cpp:2951-2980`
  writes `f0E_WxH_ADDR_content_dDOLPHINHASH.rgba` per decode) and pick an image whose hash never appears. Then claim
  that hash. Image 92 (a blank card the size of the portraits) is the natural first look.

---

## 7. Recipe for hle.c

Constants (names proposed):

```
#define PLAQUE_BANK      0x822F1000u   // SCRATCH_MENU + 0x1000; 32-aligned; up to 0x82300000
#define BANK_BIND        0x801E1B74u   // (bank) -> previous bank
#define MATERIAL_IMAGE   0x8019C7C8u   // (&rgba, field 0, image, 0x80000000)
#define QUAD_TEXCOORDS   0x8019D178u   // (array): GXSetArray(TEX0, array, 4)
#define QUAD_UV_FULL     0x80254D10u   // the game's whole-image texcoords (t flipped)
#define QUAD_DRAW        0x8019D1A4u   // (x0, y0, x1, y1, z, 0, 5)
#define QUAD_DRAW_UV     0x8019D084u   // (texcoords, x0, y0, x1, y1, z, 0, 5)
```

**Once (host start), pick each plaque's guest size**: CMPR, multiples of 8, sizes no game texture uses (for example
`248x40`, `184x40`), and a unique id per plaque.

**Each frame in `tome_stops_frame`** (after `0x801117F4(0, alpha)`, before the gold ring), in whichever memory is in:

1. **Make sure the bank is there.** If `mem_read32(PLAQUE_BANK) != 0x0020AF30` or the canary/version word differs,
   write it:
   - `B+0` `0x0020AF30`, `B+4` N, `B+8` `B+12`.
   - For each i: `B+12+8i` = `H_i`, `B+16+8i` = 0.
   - Each header at `H_i = B + 12 + 8N + 36i`: u16 h, u16 w, u32 14, u32 `D_i`, u32 0, u32 0, u32 1, u32 1, u32 0,
     u8 0, u8 0, u8 0, u8 1.
   - Each `D_i` 32-aligned (after the headers, round up): `ceil(w/8)*ceil(h/8)*4` sub-blocks of
     `00 00 kk kk FF FF FF FF`. `kk kk` = plaque id in sub-block 0, the version in sub-block 1, 0 elsewhere.
   - Write the same bytes in both memories (player 1's real memory and player 2's copy) and never change them in the
     middle of a frame.
2. **Colour:** `mem_write32(SCRATCH_MENU + 0x408, 0xFFFFFF00 | a)`, with `a` 255, or the pulse `r3` if a plaque should
   pulse like the tab art.
3. **Draw each plaque:**
   ```
   old = call(BANK_BIND, {PLAQUE_BANK})
   call(QUAD_TEXCOORDS, {QUAD_UV_FULL})
   for each plaque i:
       call(MATERIAL_IMAGE, {SCRATCH_MENU + 0x408, 0, i, 0x80000000})
       call(QUAD_DRAW, {x0, y0, x0 + w, y0 + h, (u32)-1, 0, 5})     // 640x480 units, z -1
   call(BANK_BIND, {old})
   ```
4. Then the gold ring (`0x8019CC10`) and the labels, as now. If the plaques carry their words, the text elements and the
   plain wash/rim frames are not needed for them.

**In buttons.cpp (a new `plaques.cpp`, as tome.cpp):**

- `plaques_claims(fmt, w, h)`: `fmt == 14` and (w, h) is one of the plaque sizes.
- `plaques_claims_hash(hash)`: the hash is one of the plaques' known XXH64s. The host computes them once from the stub
  bytes it writes (`XXH64(bytes, base, 0)`; `xxhash.h` is already used by `lightmaps.cpp:23`).
- `plaques_apply(rgba, w, h, hash, out...)`: find the plaque by hash and return a malloc'd RGBA8 copy of its picture,
  **stored upside down**, at about 4x its 640x480 rectangle. Return 2 while it is being drawn on a thread, as
  `tome_apply` does.
- `plaques_generation()`: a counter bumped whenever a plaque's picture changes (label, language, the chosen state if
  drawn as a picture).
- Wire them into `claims_any`, `claims_hash_any` (before the "neither tome nor art: true" line), `apply_any` (before
  `lightmaps_apply`) and `generation_any`.

**A selected/unselected look** can be two images per plaque (two ids, two hashes), or one image drawn with a different
colour alpha (step 2). Changing the picture itself goes through the generation, not the guest bytes.

**Tests (when built):** `DOL_GXCORE_TEXTURE_DUMP` should show one `f0E_248x40_822F...` decode per plaque, once, and no
re-decode per frame. Check that the plaques sit in both halves in co-op, in widescreen, and under the gold ring and the
words. Then run `ed test`.

---

## 8. Open questions

1. **Z test of the 2D material** (`0x801E21FC` with 703): not decoded. Painter's order at z -1 is assumed safe.
2. **Texcoord fixed point:** 0x200 = 1.0 (9 bits) is inferred from the full-quad table. `GXSetVtxAttrFmt` for format 5
   was not read. It only matters for `0x8019D084` sub-rectangles.
3. **Free scratch:** `0x822F1000`..`0x822FFFFF` is free by grep and the `SCRATCH_MENU` comment, not by a run with a watch.
4. **The copy Tome's memory:** how `tome_copy_step` refreshes the copy from the real memory (every 600 turns, per
   `TOME_COPY_EVERY`) was not read. Writing the bank in the frame hook, whichever memory is in, avoids depending on it.
5. **Render timing:** that the renderer reads the bytes at `Present` (not at the draw) is inferred from the `Mem1Turn` op
   being queued in the command stream.
6. **A free UBookPak slot** is not proved for any image. A texture dump over every page (and the title screen, the card
   dialog, the Journal's chapter pages) would settle it.
7. **Other languages:** a host bank is language-free. A slot or image 27 repaint is not.

---

## Appendix: UBookPak.bin item 0 (English), all 95 images [disc]

All CMPR (format 14) except 58 (format 1, I8). wrap 0,0 except 23, 24, 40 and 58 (1,1). Filters 1,1, LOD 0-0, bias 0.

| # | Size | Hash | # | Size | Hash |
|---|---|---|---|---|---|
| 0 | 432x480 | 09e293e32e372ede | 48 | 73x49 | da7339914ded6ddb |
| 1 | 133x164 | f7d871e3619321ac | 49 | 198x57 | 72b3c90acf3b44b5 |
| 2 | 90x30 | 41986c8697dd2ad7 | 50 | 133x160 | b6c6766899838f9f |
| 3 | 80x80 | b8623f296edc6f5f | 51 | 133x160 | 08b4ab52b432f946 |
| 4 | 432x480 | c7ce663af8d823a0 | 52 | 133x160 | 7ab0185c11e74ea3 |
| 5 | 268x72 | d3b676b1da901f62 | 53 | 133x160 | 1514e1a7beaf1cc8 |
| 6 | 50x50 | 510cf371c24be249 | 54 | 48x46 | df6b17537d495e07 |
| 7 | 133x164 | 55dcc67bfb257502 | 55 | 72x76 | 1e01f71343d270a4 |
| 8 | 74x75 | 4ce308911857840e | 56 | 75x46 | 5b2ecb5d1ba4b71a |
| 9-12 | 116x7 | 736f9cda785904fc | 57 | 69x26 | d9389a8337d2881f |
| 13 | 189x57 | 60847f49db56ef95 | 58 | 64x64 (I8) | 3c5178e7dab4668b |
| 14 | 64x896 | 3c944174e4b0dc33 | 59 | 67x72 | 596f7c89f900803b |
| 15 | 64x896 | ee9fa34f235b3761 | 60 | 73x49 | 2cb382d91dfb9a4d |
| 16 | 50x50 | 7b90073d0845167a | 61 | 126x89 | 4dc0cc88542db31e |
| 17 | 50x50 | b4ff4f012b9e7278 | 62 | 126x89 | 887860f403544c27 |
| 18 | 324x143 | 03406308bcc79b0f | 63 | 126x89 | f006e2a88f8fde8f |
| 19 | 48x46 | fad2c70ba848121b | 64 | 126x89 | 0242ba0d94b9393e |
| 20 | 40x40 | fc69fbdd44af65ef | 65 | 126x89 | 595be602b6959aae |
| 21 | 640x480 | 9e51063f6b37567a | 66 | 126x89 | 0d84faaaab3292f9 |
| 22 | 421x331 | 334f41197a692f84 | 67 | 126x89 | 8d9680c6aae268f5 |
| 23 | 64x32 | c21147fae9991fff | 68 | 126x89 | 896417736e2de49a |
| 24 | 64x32 | 98265b23f5c3a5a2 | 69 | 126x89 | ec2d5de777c06db0 |
| 25 | 203x31 | 6e983f69d30cfd57 | 70 | 126x89 | 7817090e47a33d08 |
| 26 | 204x25 | db099e6e21495860 | 71 | 126x89 | 0b5ae2fd8a7926d8 |
| 27 | 432x480 | 7cd7b88669459bbb | 72 | 126x89 | 7addfcba52e36b99 |
| 28 | 40x33 | 82c947603a0d5cca | 73 | 126x89 | 6a28b478e09cea0f |
| 29 | 47x48 | 685c3eef7c007675 | 74 | 126x89 | f1dd8c46b9d1eb48 |
| 30 | 175x84 | b5f77b87e228ea3f | 75 | 126x89 | 7ff99760a6e88b5b |
| 31 | 432x480 | 8680aee4cb0d0a97 | 76 | 29x51 | d17ece8360ed8f76 |
| 32 | 133x164 | 21e57b13f972c999 | 77 | 29x51 | 6aede21a6a525e02 |
| 33 | 354x149 | ffe9e9ee96ed4441 | 78 | 29x51 | 33107eb5657b6eb1 |
| 34 | 106x30 | b76f74ade4a8f002 | 79 | 29x51 | dfbfd63ce9790282 |
| 35 | 47x31 | f5e2ebaf4657e0fe | 80 | 29x47 | 5fe11aadd1e11977 |
| 36 | 51x27 | 9819c9e49ca9adb5 | 81 | 29x47 | 6c9554c538abb7e5 |
| 37 | 36x27 | c50324bb9211c247 | 82 | 29x47 | 0aacbf30fac687a0 |
| 38 | 324x106 | 9bb70494c500e0c3 | 83 | 29x47 | 9c24dcf2b6990c9e |
| 39 | 176x32 | be3d056ad28d1679 | 84 | 24x49 | e22d2a23dd9ee25b |
| 40 | 64x32 | f9ae32fb138dcdad | 85 | 24x49 | 40ef6ab84b59f320 |
| 41 | 40x33 | c8192a1f82ceb7a9 | 86 | 24x49 | d94132fd8b56a6ab |
| 42 | 47x48 | 432cfa0db442b523 | 87 | 24x49 | 9bf445c0014cf79f |
| 43 | 138x32 | a26a2d697edb2776 | 88 | 46x46 | 465e1dd7db36bbea |
| 44 | 432x480 | 54deddbf99c23af0 | 89 | 46x46 | d2106ab6494cac20 |
| 45 | 640x384 | 215a6eb6cea7ee14 | 90 | 46x46 | 081800b8fbf7c969 |
| 46 | 133x164 | eecc56d99bcfe908 | 91 | 162x72 | 9d40e157fd4dcfdc |
| 47 | 81x38 | ba96141d93c3d542 | 92 | 126x89 | 99435315d168f899 |
| | | | 93 | 72x31 | dbb433307b529e09 |
| | | | 94 | 73x29 | 851d5b4727bd3854 |
