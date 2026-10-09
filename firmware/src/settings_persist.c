/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared PER4 layout: each feature updates only its own fields, preserving
 * the other feature's saved preferences when either is built independently.
 * PER1/PER2 are upstream; PER3 added bold; PER4 added favorites.
 * palette: UI_PAL_TAG + index; an old id (below 20, earlier firmware) is migrated on import.
 * bold: no longer used (one font weight); kept as it was saved, unless it holds the HOLD setting (panel.c).
 * zoom: no longer used (the large readout); kept as it was saved, unless it holds the LEDS setting (panel.c). */
/* PER5 (fm1-chord): hcp, the four chord machine presets' own state (hui.c hc_preset_pack: the chord settings, the
 * key inversions and locks), opaque bytes here; a PER4 record (upstream Felucca) imports with them empty */
#define HC_PRESET_BYTES 256u
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
    uint8_t hcp[4][HC_PRESET_BYTES];
    uint8_t hcl[HC_PRESET_BYTES];                /* PER6: the live chord machine state, back after power-off (hui.c) */
} persist_t;
#define PERSIST_MAGIC 0x50455236u
#define PERSIST_MAGIC_V5 0x50455235u
#define PERSIST_MAGIC_V4 0x50455234u
static uint8_t hc_presets[4][HC_PRESET_BYTES];   /* the presets as last imported / saved (hui.c reads and writes) */
static uint8_t hc_live[HC_PRESET_BYTES];         /* the live state as last imported / saved (hui.c hui_live_poll) */

/* Normalize in place; 1 = current, 2 = migrated, 0 = invalid. */
static int settings_import(persist_t *p, int n)
{
    int current = n == (int)sizeof *p && p->magic == PERSIST_MAGIC;
    int old5 = n == (int)(sizeof *p - sizeof p->hcl) && p->magic == PERSIST_MAGIC_V5;
    int old4 = n == (int)(sizeof *p - sizeof p->hcl - sizeof p->hcp) && p->magic == PERSIST_MAGIC_V4;
    int old3 = n == (int)(sizeof *p - sizeof p->hcl - sizeof p->hcp - sizeof p->favorites) && p->magic == 0x50455233u;
    int old2 = n == (int)(sizeof *p - sizeof p->hcl - sizeof p->hcp - sizeof p->favorites - sizeof p->bold) && p->magic == 0x50455232u;
    int old1 = n == (int)(8u + sizeof(panel_t)) && p->magic == 0x50455231u;
    if (!(current || old5 || old4 || old3 || old2 || old1)) return 0;
    if (old1) {
        panel_t old;
        memcpy(&old, (uint8_t *)p + 8, sizeof old);
        p->panel = old;
        p->lowcut = p->zoom = 0;
    }
    if (old1 || old2) p->bold = 0;
    if (old1 || old2 || old3) memset(&p->favorites, 0, sizeof p->favorites);
    if (!(current || old5)) memset(p->hcp, 0, sizeof p->hcp);
    if (!current) memset(p->hcl, 0, sizeof p->hcl);
    memcpy(hc_presets, p->hcp, sizeof hc_presets);
    memcpy(hc_live, p->hcl, sizeof hc_live);
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(palette_from_stored(p->palette));
    settings.magic = SETTINGS_MAGIC;
    settings.palette = palette_from_stored(p->palette);
    settings.lowcut = p->lowcut;
    settings.zoom = p->zoom;
    settings_hold = (uint8_t)hold_from_stored(p->bold);
    settings_leds = (uint8_t)leds_from_stored(p->zoom);
#ifdef FELUCCA_FAVORITES
    memcpy(&favorites, &p->favorites, sizeof favorites);
    favorites.filter = favorites.filter == 1u;
#if defined(FM4_NPRESETS) && !FELUCCA_FM4
    {   /* DIGITAL's starred presets (engine 1, retired) -> the FM6 presets that cover them (fm4_convert.c) */
        uint32_t k;
        for (k = 0; k < FM4_NPRESETS; k++)
            if ((favorites.factory[ENGI_DIGITAL][0] >> k) & 1u)
                favorites.factory[ENGI_FM6][FM4_TO_FM6[k] / 8u] |= (uint8_t)(1u << (FM4_TO_FM6[k] % 8u));
        memset(favorites.factory[ENGI_DIGITAL], 0, sizeof favorites.factory[ENGI_DIGITAL]);
    }
#endif
#endif
    if (p->panel.magic == PANEL_MAGIC) panel = p->panel;
    return current ? 1 : 2;
}

/* Start with the last imported/saved object, including fields owned by a
 * feature absent from this build. No save-on-boot or extra flash writes. */
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom = leds_to_stored(settings.zoom, settings_leds);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
#ifdef FELUCCA_FAVORITES
    memcpy(&p->favorites, &favorites, sizeof favorites);
#endif
    memcpy(p->hcp, hc_presets, sizeof p->hcp);
    memcpy(p->hcl, hc_live, sizeof p->hcl);
}
