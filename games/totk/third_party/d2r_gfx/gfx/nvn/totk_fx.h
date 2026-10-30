// Effect switches (Mac, TOTK; user 2026-10-30: "参考 ultracam 添加 … 着色器开关 都能单独开关"): the settings panel's effects
// page turns single picture effects off and on while the game runs. Game-specific and never needed to run the game;
// included by nvn_mtl.m after totk_ssao.h. Own implementation of UltraCam's idea (its shader toggles replace chosen shader
// programs; nothing of its code): an effect is the set of shaders whose draws (fragment shader key) or dispatches (compute
// shader key) are left out while it is off. The keys are TOTK 1.4.3's, found by leaving out each full-screen pass of a
// frame in turn and comparing screenshots (MacNativeGames-artifacts/claude/fx, "flag skipfs_<key>" / "skipcs_<key>"):
// only passes whose absence leaves a whole picture minus that effect are listed (a pass that feeds later ones, such as
// the deferred lighting tiles, would leave holes; one whose result the game reads back ends the game). Ambient occlusion
// is computed inside the shadow pre-pass with the shadows, so its switch is the AO patch's own (totk_ssao.h, c1 .w).
typedef struct { const char *id, *zh, *en; const char *fs[6], *cs[4]; } fx_effect;
static const fx_effect k_fx[] = {
    {"ao", "环境光遮蔽", "Ambient occlusion", {0}, {0}},   // totk_ssao.h
    {"grass", "草", "Grass", {"33099f8c133950f5"}, {0}},   // the field's grass blades (2026-10-30 survey, v143-grass)
    {"charlight", "角色补光", "Character light", {"61315f715cdf30ed"}, {0}},   // the extra light on Link
};
enum { FX_N = sizeof k_fx / sizeof *k_fx, FX_AO = 0 };
static _Atomic uint32_t g_fx_off;   // bit i: k_fx[i] is off

/* The effect a shader belongs to: k_fx index + 1, or -1 for none; worked out at its first draw while any effect is off and
   kept in the shader (NvShader.fx, 0 = not looked up yet). Keys are compared on their 16 hex digits (a variant's key
   carries a suffix after them). */
static int fx_of(NvShader *o, int stage) {
    int v = o->fx;
    if (!v) {
        v = -1;
        for (int i = 0; i < FX_N && v < 0; i++) {
            const char *const *keys = stage == 5 ? k_fx[i].cs : k_fx[i].fs; int n = stage == 5 ? 4 : 6;
            for (int j = 0; j < n && keys[j]; j++) if (!strncmp(o->key, keys[j], 16)) { v = i + 1; break; }
        }
        o->fx = v;
    }
    return v;
}
/* Whether to leave out this draw (stage 1, its fragment shader) or dispatch (stage 5, its compute shader). */
static inline int fx_drop(NvShader *o, int stage) {
    uint32_t off = atomic_load_explicit(&g_fx_off, memory_order_relaxed);
    if (!off || !o) return 0;
    int v = fx_of(o, stage);
    return v > 0 && (off >> (v - 1) & 1);
}

/* The settings panel (runtime/settings_panel.h): the effects in their order, their names, and the switch. AO without the
   AO patch (TOTK_SSAO=0) cannot be turned off: dm_gfx_fx_available says so. */
int dm_gfx_fx_count(void) { return FX_N; }
const char *dm_gfx_fx_id(int i) { return i >= 0 && i < FX_N ? k_fx[i].id : ""; }
const char *dm_gfx_fx_name(int i, int english) { return i >= 0 && i < FX_N ? (english ? k_fx[i].en : k_fx[i].zh) : ""; }
int dm_gfx_fx_available(int i) {
#ifdef GAME_BOTW
    (void)i; return 0;   // the keys are TOTK's
#else
    return i >= 0 && i < FX_N && (i != FX_AO || ssao_on());
#endif
}
int dm_gfx_fx_on(int i) { return i >= 0 && i < FX_N && !(atomic_load(&g_fx_off) >> i & 1); }
void dm_gfx_fx_set(int i, int on) {
    if (!dm_gfx_fx_available(i)) return;
    if (on) atomic_fetch_and(&g_fx_off, ~(1u << i)); else atomic_fetch_or(&g_fx_off, 1u << i);
    if (i == FX_AO) ssao_set_off(!on);
    MLOG("[fx] %s %s", k_fx[i].id, on ? "on" : "off");
}
