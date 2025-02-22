// mdk sprite support
// Michael Moffitt 2018-2022

#ifndef MD_SPR_H
#define MD_SPR_H

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

#include "md/vdp.h"
#include "md/macro.h"

// Macros for defining sprite attribute and size data.
#define SPR_ATTR(_tile, _hf, _vf, _pal, _prio) VDP_ATTR(_tile, _hf, _vf, _pal, _prio)
#define SPR_SIZE(w, h) (((h-1) & 0x3) | (((w-1) & 0x3) << 2))

#define SPR_STATIC_OFFS 128

// Number of sprites in the sprite table, corresponding to the maximum supported.
#define SPR_MAX 80

// Operating modes for the sprite system.
typedef enum SprMode
{
	SPR_MODE_SIMPLE,  // The "md_spr_put" interface is used. This is the default.
	SPR_MODE_DIRECT,  // g_sprite_table is modified directly.
} SprMode;

// Struct representing a sprite table slot.
typedef struct SprSlot
{
	uint16_t ypos;
	uint8_t size;
	uint8_t link;  // Only 7 bits are valid; range is 0x00-0x7F.
	uint16_t attr;
	uint16_t xpos;
} SprSlot;

// Clears sprites and initializes sprite cache with link data.
// This may be called multiple times in order to change modes or clear sprites.
// In both SIMPLE and DIRECT modes, the link field is populated in a linear
// order, with sprite 0 pointing to sprite 1, sprite 1 pointing to sprite 2,
// and so on and so forth.
// In DIRECT mode, g_sprite_count is set to the maximum sprite number for the
// current video mode (H40 --> 80 sprites; H32 --> 64 sprites).
void md_spr_init(SprMode mode);

// SIMPLE mode interface
// ---------------------
// These routines allow for easy sprite placement without consideration for how
// sprites are ordered in the sprite table. Positions are offset so that (0, 0)
// is the top-left corner of the screen, and the sprite link value is set
// automatically so that sprites are drawn in order.
//
// Usage of SIMPLE mode:
//
// During init: This is called by megadrive_init(), but no harm in repeating it.
//     md_spr_init(SPR_MODE_SIMPLE);
//
// In game logic:
//     md_spr_put(10, 20, SPR_ATTR(0x1234, 0, 0, 0, 0), SPR_SIZE(2, 2));
//
// This places a 2x2 tile sprite at 10, 20, with tile index 0x1234, palette 0.
//
// Nothing more is needed to place sprites. The call to megadrive_finish() will
// handle termination and DMA transfer of the sprite list, as well as the
// preparation of the list for the next frame.

// Place a sprite using screen position coordinates.
static inline void md_spr_put(int16_t x, int16_t y, uint16_t attr, uint8_t size);

// Masks off any sprites on scanlines that span between y and the height.
static inline void md_spr_mask_line_full(int16_t y, uint8_t size);

// Masks off any sprites on scanlines that intersect two sprite positions.
static inline void md_spr_mask_line_comb(int16_t y1, uint8_t size1,
                                      int16_t y2, uint8_t size2);

// DIRECT mode interface
// ---------------------
// With DIRECT mode, g_sprite_table is modified directly and transferred to the
// VDP without any automatic intervention aside from the DMA itself.
// g_sprite_table may be modified during the frame, and objects can even hold
// pointers to SprSlot entries within. There are no restrictions placed on what
// you do with the data in that list, so you could even go so far as to have
// the position fields act as object data if you were so inclined.
//
// The link field is no longer automatically managed, but the call to
// md_spr_init(SPR_MODE_DIRECT) will configure the link fields so that all
// sprites are drawn in sequence. Therefore, you may either hide a sprite by
// placing it at a negative coordinate <= -32, or update the link fields
// manually to terminate the list (point back at 0). As there is no bandwidth
// advantage to the latter, I suggest doing the former instead.
//
// Usage of DIRECT mode:
//
// During init: This must be called as megadrive_init() defaults to SIMPLE mode.
//     md_spr_init(SPR_MODE_DIRECT);
//
// In game logic:
//     g_sprite_table[0].xpos = 10 + SPR_STATIC_OFFS;
//     g_sprite_table[0].ypos = 20 + SPR_STATIC_OFFS;
//     g_sprite_table[0].attr = SPR_ATTR(0x1234, 0, 0, 0, 0);
//     g_sprite_table[0].size = SPR_SIZE(2, 2);
//
// This places a 2x2 tile sprite at 10, 20, with tile index 0x1234, palette 0.
//
// Nothing more is needed at this point. megadrive_finish() will still schedule
// the DMA transfer, but it it will not touch the link fields for you like it
// does in SIMPLE mode.
//
// Optionally, g_sprite_count may be set to a value less than 80 (or 64 in H32)
// and you may terminate the sprite list prematurely, should you wish to save
// on DMA bandwidth by transferring less sprites.


// The sprite table cache. This is transferred to the VDP via DMA.
// It is used by both SIMPLE and DIRECT mode, but user modification is only
// recommended when using DIRECT mode.
extern SprSlot g_sprite_table[SPR_MAX];

// In SIMPLE mode, this is used by the md_spr_put() functions as a counter.
// In DIRECT mode, it controls how many sprite slots are transferred via DMA.
extern uint8_t g_sprite_count;

// SIMPLE mode static implementations ==========================================

static inline void md_spr_put(int16_t x, int16_t y, uint16_t attr, uint8_t size)
{
	if (g_sprite_count >= ARRAYSIZE(g_sprite_table)) return;
	if (x <= -32 || x >= 320) return;  // Avoid triggering line mask.
	SprSlot *spr = &g_sprite_table[g_sprite_count];
	spr->ypos = y + SPR_STATIC_OFFS;
	spr->size = size;
	spr->attr = attr;
	spr->xpos = x + SPR_STATIC_OFFS;
	g_sprite_count++;
}

static inline void md_spr_mask_line_full(int16_t y, uint8_t size)
{
	if (g_sprite_count >= ARRAYSIZE(g_sprite_table)) return;
	SprSlot *spr = &g_sprite_table[g_sprite_count];
	spr->ypos = y + SPR_STATIC_OFFS;
	spr->size = size;
	spr->xpos = 0;
	g_sprite_count++;
}

static inline void md_spr_mask_line_overlap(int16_t y1, uint8_t size1,
                                           int16_t y2, uint8_t size2)
{
	if (g_sprite_count >= ARRAYSIZE(g_sprite_table) - 1) return;
	SprSlot *spr = &g_sprite_table[g_sprite_count];
	spr->ypos = y1 + SPR_STATIC_OFFS;
	spr->size = size1;
	spr->xpos = 0;
	spr++;
	spr->ypos = y2 + SPR_STATIC_OFFS;
	spr->size = size2;
	spr->xpos = 1;
	g_sprite_count += 2;
}

// Internal functions (used by megadrive_finish() in SIMPLE mode)

// Prepares the sprite cache for a frame. Called by megadrive_finish().
void md_spr_start(void);

// Terminate the sprite list and schedule a DMA. Called by megadrive_finish().
void md_spr_finish(void);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // MD_SPR_H
