# Native settings renderer — staged, not installed

The renderer runs inside the original frontend's 2D frame after `418C50` and
before the reverse native sprite flush in `418470`. It does not create an OS
window. A validated 640x480 DirectDraw back surface is filled black, the old
POD sprite queue is replaced, and the original bitmap font draws the settings.
The font object is cloned into DLL-owned memory; original font state, surfaces,
font assets and text archives are never changed or released.

The original frontend uses logical coordinates of approximately 1024x820.
Native `41ADB0` and `41ADD0` map those positions to the 640x480 back surface;
the layout values are deliberately in that logical coordinate system. The
font sprite sizes remain native pixels.

`430ED0` takes decoded glyph bytes. The separate `431680` text lookup override
must instead return glyph bytes minus `0x1F`. Mixing those representations
produces incorrect menu labels. The Russian atlas uses its original custom
alphabet; `render_layout.h` therefore contains decoded atlas glyphs, not UTF-8.

Validation executed the actual Steam `430ED0` and `4316A0` instructions in
Unicorn with the localized TAB data. Only the final sprite-enqueue call was
replaced with a recording fixture. All 13 text lines emitted 114 native glyphs,
every source/destination rectangle was checked, and the thiscall `ret 12`
stack balance was verified. `native-menu-preview.png` uses those recorded
glyph rectangles and the actual localized bitmap atlas. It is an offline
preview, not an in-game screenshot. The layout fixture also checks all five
selected rows and normal/read-error/save-error variants.

The normal path, every setting, Cancel, and the read/save error wording were
reviewed in the bitmap-font preview. Corrected the footer's encoded Russian
spelling before acceptance. No launcher, provider, or implementation language
appears in the game UI.

The production Windows/x86 translation unit compiles with `-Wall -Wextra
-Werror`. In-game display, DirectDraw interoperability and physical user
acceptance remain pending. No game was launched and no installed file changed.

DirectDraw contract used: Microsoft `IDirectDrawSurface7::Blt` and `DDBLTFX`
documentation, specifically `DDBLT_COLORFILL | DDBLT_WAIT`, initialized
`dwSize`, and `dwFillColor=0`. The game's HogsFix surface cache also confirms
`52083C + 8` is the DirectDraw7 back buffer. The original native `BltFast`
font flush retains HogsFix's normal sprite batching.

The verified Options screen is rendered through the same native-font queue:
original FE17 title and FE21–23 labels are fetched through the native `431680`
thiscall lookup, then decoded with the exact +0x1F glyph representation. The
fourth Mouse row is drawn explicitly. This avoids depending on an unverified
legacy row-object cache or rebuilding its layout after count changes. A second
retail-font emulation verified all four rows, selected Mouse, 55 glyphs, and the
native GetText ret4 contract. Its offline preview uses the actual Russian
labels: Настройки, Звук, Управление, Авторы, Мышь. Other screens keep their
normal rendering and the first three Options actions still use their original
handlers.
