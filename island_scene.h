/*
 * Island Scene: single-header Glide 3 demo.
 *
 * Include this file for the declarations. Define ISLAND_SCENE_IMPLEMENTATION
 * in one C translation unit to compile the renderer, and ISLAND_SCENE_MAIN
 * there to build the command-line demo. See README.island.md for build and
 * build and usage details.
 */
#ifndef ISLAND_SCENE_H
#define ISLAND_SCENE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owns the Glide context until it returns. Call once per process.
   Returns 0 on success, 1 on rendering failure, or 2 on usage error. */
int island_scene_run(int argc, char **argv);

#ifdef ISLAND_SCENE_COVERAGE
/* Regression runner services. Key events use lowercase ASCII, Space or Escape.
   Read scanout and save callbacks must return a synchronized frame number. */
typedef struct IslandScenePlatform {
  uint32_t window;
  void (*present)(void *context);
  int (*poll_key)(void *context, int *key, int *down);
  unsigned (*scanout)(void *context);
  int (*save_frame)(void *context, const char *path, unsigned *scanout);
  void *context;
} IslandScenePlatform;

int island_scene_run_with_platform(int argc, char **argv, const IslandScenePlatform *platform);
#endif

#ifdef __cplusplus
}
#endif
#endif /* ISLAND_SCENE_H */

#if defined(ISLAND_SCENE_IMPLEMENTATION) && !defined(ISLAND_SCENE_IMPLEMENTED)
#define ISLAND_SCENE_IMPLEMENTED

/*
** Animated seaside island and optional Glide 3 feature coverage.
**
** Original test program for the Rougarou harness.  Every texture and every
** vertex is generated in code from a fixed seed; the animation is a function
** of the frame number only. The optional performance HUD uses wall-clock
** time and should be off for deterministic captures.
**
** One source for both personalities.  Feature use is decided at run time
** from grGetString(GR_EXTENSION) / grGetProcAddress / grGet:
**   VSA-100 (h5 Glide: PIXEXT COMBINE TEXFMT ...): 32bpp colour with 24-bit Z
**     + 8-bit stencil, stencil shadows, combine extensions, ARGB8888/FXT1/DXTn
**     textures, 2048x2048 texture, render-to-texture reflection, T-buffer
**     writes in the full-scene AA mode (-a 2 / -a 4).
**   Avenger (h3 Glide): 16bpp, blended-decal shadows, classic combine units,
**     256x256 textures, procedural sparkle layer instead of render-to-texture.
**
** Scene: sky gradient dome, additive sun disc with point/line glow, scrolling
** ARGB4444/AP88 clouds, a wave mesh (vertex arrays: strips, fans, contiguous)
** with perspective-correct two-TMU texturing, Gouraud colour, fog table on the
** fog coordinate and alpha-blended foam, an island with trilinear (even/odd LOD
** split across TMU0/TMU1) and detail-textured sand, a mirror-wrapped reef ring
** carrying the largest texture the board allows, palm trunks (Gouraud fans)
** with P8/PALETTE6666 alpha-tested and NCC chroma-keyed leaf billboards,
** shadows, a shell gallery with one texture per format, spray points/lines,
** an anti-aliased sailboat, colour-masked and clipped passes, and an LFB HUD.
**
** VSA corpus elements (docs/h5_coverage.md section 7), each a thing in the picture:
**   sun glitter band on the water (per-TMU constant colour, LOD-fraction alpha, 2x/4x
**   combine output shift, per-channel colour masks, GR_FOG_ADD2), embossed wet sand
**   (signed X_MINUS_HALF detail), distant islands fogged in two passes (ADD2 + MULT2, on Q
**   while the rest fogs on the fog coordinate), the jetty's subtractive shadow (REVSUB), a
**   buoy drawn src - dst (SUB), sunglasses (SAME_COLOR factor), flare ghosts (separate alpha
**   blend), counted stencil shadows (INCR/DECR wrap, 4-bit masks) and the tide pool cut out
**   by an INVERT mask in stencil bit 7, shells from one atlas read by both TMUs under UMA,
**   a swell crest animated by partial-row downloads, a pinwheel with TMU1's own palette and
**   chroma range, roof faces through TMU1's own NCC table, the pool reflection rendered
**   with its own depth buffer, a stippled fishing net, gulls in clip space with a viewport
**   and per-primitive smoothing (one wing phase per T-buffer sample in the AA modes).
**
** Usage: scene_island [-n frames] [-r WxH] [-m mask] [-s seed] [-a 0|2|4|8] [-p 15|16|32]
**                     [-d] [-g] [-P] [-A|-A-only] [-q] [--detail|--showcase] [--interactive]
**   -n frames  frames to render (default 60)
**   -r WxH     512x384, 640x480 (default), 800x600, 1024x768, 1600x1200
**   -m mask    hex feature mask (64 bits); set bit k to disable feature k (see FEATURE list)
**   -s seed    procedural seed (default 1)
**   -a n       full-scene AA mode: open with a 2-, 4- or 8-sample pixel format (VSA); a
**              count the board cannot do is refused by Glide and the scene runs without AA
**   -p bpp     15 = ARGB1555, 16 = RGB565, 32 = ARGB8888 + Z24/S8 (default on VSA)
**   -d         T-buffer depth of field (AA modes)
**   -P         AA modes: run the 16bpp LFB probe after the scene (it is skipped otherwise)
**   -g         gallery mode: the plain per-format texture strip and visible LFB test patches
**   -A         check alpha-test colour/depth masking on a single-chip rasterizing backend
**   -A-only    run just the alpha/depth swatches, without the scene and API sweep
**   -q         quiet: no per-frame lines
**   --detail   denser terrain and palms, with higher-resolution procedural textures
**   --showcase  --detail at 1024x768 with board-selected AA and T-buffer depth of field
**               -r and -a override the preset's resolution and sample count
**   --interactive  live controls and help overlay. Runs until Q or Esc unless -n sets a limit.
**   --record DIR FPS  write each presented scene frame to DIR as a numbered RGB PPM
**   --cinematic  eased camera, fixed lighting and depth mode, with the test HUD hidden
**   --perf-hud  show wall-clock FPS and Glide FBI pixel input/output rates
**
** Self-test: prints the detected/exercised feature list in a stable order,
** "CHECK <name>: ok|FAIL" lines (these decide PASS/FAIL), "INFO <name>: ..."
** lines (results that need a rasterising backend, informational on the C card
** model), and a final "scene_island: ... -> PASS|FAIL" line; exit status 0 on
** PASS.
*/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <glide.h>
#include <glideutl.h>
#ifdef ISLAND_SCENE_COVERAGE
static const IslandScenePlatform *islandPlatform;
#endif

/* ------------------------------------------------------------------------------
** Extension constants (values from the h5 g3ext.h); defined here so the same
** source compiles against the h3 headers, where they do not exist.
*/
#ifndef GR_BUFFER_TEXTUREBUFFER_EXT
#define GR_BUFFER_TEXTUREBUFFER_EXT 0x6
#endif
#ifndef GR_STENCIL_MODE_EXT
#define GR_STENCIL_MODE_EXT 0x07
#endif
#ifndef GR_STENCILOP_KEEP
#define GR_STENCILOP_KEEP    0x00
#define GR_STENCILOP_ZERO    0x01
#define GR_STENCILOP_REPLACE 0x02
#define GR_STENCILOP_INCR_CLAMP 0x03
#endif
#ifndef GR_STENCILOP_DECR_CLAMP
#define GR_STENCILOP_DECR_CLAMP 0x04
#define GR_STENCILOP_INVERT     0x05
#define GR_STENCILOP_INCR_WRAP  0x06
#define GR_STENCILOP_DECR_WRAP  0x07
#endif
#ifndef GR_TEXTURE_UMA_EXT
#define GR_TEXTURE_UMA_EXT 0x06
#endif
#ifndef GR_BUFFER_TEXTUREAUXBUFFER_EXT
#define GR_BUFFER_TEXTUREAUXBUFFER_EXT 0x7
#endif
#ifndef GR_AA_ORDERED_POINTS_OGL          /* fxglide.h (not an application header) */
#define GR_AA_ORDERED_POINTS_OGL    0x00010001
#define GR_AA_ORDERED_LINES_OGL     0x00010002
#define GR_AA_ORDERED_TRIANGLES_OGL 0x00010003
#endif
#ifndef GR_FOG_MULT2
#define GR_FOG_MULT2 0x100
#define GR_FOG_ADD2  0x200
#endif
#ifndef GR_BLEND_SAME_COLOR_EXT
#define GR_BLEND_SAME_COLOR_EXT           0x08
#define GR_BLEND_ONE_MINUS_SAME_COLOR_EXT 0x09
#endif
#ifndef GR_FUNC_MODE_X_MINUS_HALF
#define GR_FUNC_MODE_NEGATIVE_X   0x03
#define GR_FUNC_MODE_X_MINUS_HALF 0x04
#endif
#ifndef GR_PIXFMT_RGB_565
#define GR_PIXFMT_RGB_565          0x0003
#define GR_PIXFMT_ARGB_8888        0x0005
#define GR_PIXFMT_AA_2_ARGB_8888   0x0008
#define GR_PIXFMT_AA_4_ARGB_8888   0x000b
#endif
#ifndef GR_LFBWRITEMODE_Z32
#define GR_LFBWRITEMODE_Z32 0x0008
#endif
#ifndef GR_CMBX_ZERO
#define GR_CMBX_ZERO                0x00
#define GR_CMBX_TEXTURE_ALPHA       0x01
#define GR_CMBX_ALOCAL              0x02
#define GR_CMBX_AOTHER              0x03
#define GR_CMBX_B                   0x04
#define GR_CMBX_CONSTANT_ALPHA      0x05
#define GR_CMBX_CONSTANT_COLOR      0x06
#define GR_CMBX_DETAIL_FACTOR       0x07
#define GR_CMBX_ITALPHA             0x08
#define GR_CMBX_ITRGB               0x09
#define GR_CMBX_LOCAL_TEXTURE_ALPHA 0x0a
#define GR_CMBX_LOCAL_TEXTURE_RGB   0x0b
#define GR_CMBX_LOD_FRAC            0x0c
#define GR_CMBX_OTHER_TEXTURE_ALPHA 0x0d
#define GR_CMBX_OTHER_TEXTURE_RGB   0x0e
#define GR_CMBX_TEXTURE_RGB         0x0f
#define GR_CMBX_TMU_CALPHA          0x10
#define GR_CMBX_TMU_CCOLOR          0x11
#define GR_FUNC_MODE_ZERO           0x00
#define GR_FUNC_MODE_X              0x01
#define GR_FUNC_MODE_ONE_MINUS_X    0x02
#define GR_BLEND_OP_ADD             0x00
#define GR_BLEND_OP_SUB             0x01
#define GR_BLEND_OP_REVSUB          0x02
#endif
#ifndef GR_TEXFMT_ARGB_8888
#define GR_TEXFMT_ARGB_CMP_FXT1 0x11
#define GR_TEXFMT_ARGB_8888     0x12
#define GR_TEXFMT_YUYV_422      0x13
#define GR_TEXFMT_UYVY_422      0x14
#define GR_TEXFMT_AYUV_444      0x15
#define GR_TEXFMT_ARGB_CMP_DXT1 0x16
#define GR_TEXFMT_ARGB_CMP_DXT2 0x17
#define GR_TEXFMT_ARGB_CMP_DXT3 0x18
#define GR_TEXFMT_ARGB_CMP_DXT4 0x19
#define GR_TEXFMT_ARGB_CMP_DXT5 0x1A
#endif
#ifndef GR_LOD_LOG2_2048
#define GR_LOD_LOG2_2048 0xb
#define GR_LOD_LOG2_1024 0xa
#define GR_LOD_LOG2_512  0x9
#endif

/* extension entry points, fetched with grGetProcAddress */
typedef GrContext_t (FX_CALL *PfnWinOpenExt)(FxU32, GrScreenResolution_t, GrScreenRefresh_t, GrColorFormat_t,
                                             GrOriginLocation_t, FxU32, int, int);
typedef void (FX_CALL *PfnStencilFunc)(GrCmpFnc_t, FxU8, FxU8);
typedef void (FX_CALL *PfnStencilMask)(FxU8);
typedef void (FX_CALL *PfnStencilOp)(FxU32, FxU32, FxU32);
typedef void (FX_CALL *PfnLfbConstStencil)(FxU8);
typedef void (FX_CALL *PfnBufferClearExt)(GrColor_t, GrAlpha_t, FxU32, FxU8);
typedef void (FX_CALL *PfnCombineExt)(FxU32, FxU32, FxU32, FxU32, FxU32, FxBool, FxU32, FxBool, FxU32, FxBool);
typedef void (FX_CALL *PfnTexCombineExt)(GrChipID_t, FxU32, FxU32, FxU32, FxU32, FxU32, FxBool, FxU32, FxBool, FxU32, FxBool);
typedef void (FX_CALL *PfnConstColorExt)(GrChipID_t, GrColor_t);
typedef void (FX_CALL *PfnColorMaskExt)(FxBool, FxBool, FxBool, FxBool);
typedef void (FX_CALL *PfnBlendExt)(GrAlphaBlendFnc_t, GrAlphaBlendFnc_t, FxU32, GrAlphaBlendFnc_t, GrAlphaBlendFnc_t, FxU32);
typedef void (FX_CALL *PfnTBufMask)(FxU32);
typedef void (FX_CALL *PfnTexChromaMode)(GrChipID_t, GrChromakeyMode_t);
typedef void (FX_CALL *PfnTexChromaRange)(GrChipID_t, GrColor_t, GrColor_t, GrTexChromakeyMode_t);
typedef void (FX_CALL *PfnChromaRangeMode)(GrChromakeyMode_t);
typedef void (FX_CALL *PfnChromaRange)(GrColor_t, GrColor_t, GrChromaRangeMode_t);
typedef void (FX_CALL *PfnTextureBuffer)(GrChipID_t, FxU32, GrLOD_t, GrLOD_t, GrAspectRatio_t, GrTextureFormat_t, FxU32);
typedef void (FX_CALL *PfnGetGamma)(FxU32, FxU32 *, FxU32 *, FxU32 *);
typedef void (FX_CALL *PfnTableExt)(GrChipID_t, GrTexTable_t, void *);

/* exported by both builds; h5 lists it in grGetProcAddress only with POINTCAST compiled in */
#ifdef ISLAND_SCENE_COVERAGE
extern void FX_CALL grTexDownloadTableExt(GrChipID_t, GrTexTable_t, void *) __attribute__((weak));
#endif

static struct {
  PfnWinOpenExt      winOpen;
  PfnStencilFunc     stencilFunc;
  PfnStencilMask     stencilMask;
  PfnStencilOp       stencilOp;
  PfnLfbConstStencil lfbConstStencil;
  PfnBufferClearExt  bufferClear;
  PfnCombineExt      colorCombine, alphaCombine;
  PfnTexCombineExt   texColorCombine, texAlphaCombine;
  PfnConstColorExt   constColor;
  PfnColorMaskExt    colorMask;
  PfnBlendExt        blend;
  PfnTBufMask        tbufMask;
  PfnTexChromaMode   texChromaMode;
  PfnTexChromaRange  texChromaRange;
  PfnChromaRangeMode chromaRangeMode;
  PfnChromaRange     chromaRange;
  PfnTextureBuffer   textureBuffer;
  PfnGetGamma        getGamma;
  PfnTableExt        tableExt;
} X;

/* ------------------------------------------------------------------------------
** features, in the stable report order; bit k of -m disables feature k
*/
enum {
  F_SKY, F_SUN, F_CLOUDS, F_SEA_VA, F_SEA_MT, F_COMBINE_EXT, F_FOG, F_FOAM, F_SAND_TRI,
  F_DETAIL, F_MIRROR, F_BIGTEX, F_TREES, F_TEXCHROMA, F_CHROMARANGE, F_SHADOW, F_PIX32,
  F_WBUF, F_DEPTHBIAS, F_CLIP, F_COLORMASK, F_GALLERY, F_TEX8888, F_FXT1, F_DXT,
  F_PARTICLES, F_AAPRIM, F_HUD, F_LFBCHECK, F_TBUFFER, F_BLEND_EXT, F_TEXBUFFER, F_GAMMA,
  F_TBUF_DOF,
  /* h5_coverage.md section 7 */
  F_CMBX_FX, F_BLEND_OPS, F_STENCIL_OPS, F_FOG2, F_TBUF_SAMPLE, F_TEX_YUV, F_UMA, F_TEX_ANIM,
  F_TMU_TABLES, F_TEXCHROMA_RANGE, F_RTT_DEPTH, F_COLORMASK_EXT, F_STIPPLE, F_DITHER, F_LFB_EXT,
  F_CLIPSPACE, F_OGL_SMOOTH, F_COUNT
};
static const char *const featName[F_COUNT] = {
  "sky_dome", "sun_additive_glow", "clouds_4444_ap88", "sea_vertex_arrays", "sea_two_tmu",
  "combine_ext", "fog_table_fogcoord", "foam_alpha_blend", "sand_trilinear_evenodd",
  "detail_texture", "mirror_wrap", "large_texture", "palm_trees", "tex_chroma_key",
  "chroma_range", "stencil_shadows", "pix32_z24_s8", "wbuffer_zbuffer_alternate",
  "depth_bias_decals", "clip_rectangles", "color_masks", "format_gallery", "tex_argb8888",
  "tex_fxt1", "tex_dxt", "spray_points_lines", "aa_primitives", "hud_lfb_writes",
  "lfb_roundtrip_checks", "tbuffer_fsaa", "alpha_blend_ext", "render_to_texture", "gamma_table",
  "tbuffer_depth_of_field",
  "combine_ext_effects", "blend_sub_revsub_samecolor", "stencil_wrap_invert_masks", "fog_mult2_add2",
  "tbuffer_per_sample", "tex_yuv", "tex_uma_atlas", "tex_partial_row_anim",
  "per_tmu_palette_ncc", "tex_chroma_range_tmu1", "render_to_texture_depth", "color_mask_per_channel",
  "stipple_screen_door", "dither_variants", "lfb_8888_pipeline_stencil",
  "clip_space_viewport", "per_primitive_smoothing"
};
static int  featDet[F_COUNT];        /* the board / Glide offers it                */
static int  featUsed[F_COUNT];       /* the scene actually issued it               */
static char featNote[F_COUNT][96];   /* why not, or which fallback                 */
static unsigned long long featMask;  /* -m: disabled features                      */
static unsigned long long liveMask;
static int interactive, showControls = 1, paused, quitScene, highDetail, cinematic;
static int perfHud;
static float shotDuration;
#define ON(f)  (featDet[f] && !((featMask | liveMask) >> (f) & 1))
#define USE(f) (featUsed[f] = 1)

static void note(int f, const char *s)
{
  if (!featNote[f][0]) { strncpy(featNote[f], s, sizeof featNote[f] - 1); }
}

/* ------------------------------------------------------------------------------
** self checks
*/
static int nChecks, nFail, nInfo, nInfoBad;
static void check(const char *what, int good, const char *fmt, ...)
{
  nChecks++;
  if (good) { printf("CHECK %s: ok\n", what); return; }
  nFail++;
  printf("CHECK %s: FAIL", what);
  if (fmt) {
    va_list ap; va_start(ap, fmt); printf(" ("); vprintf(fmt, ap); printf(")"); va_end(ap);
  }
  printf("\n");
}
static void info(const char *what, int good, const char *fmt, ...)
{
  nInfo++;
  if (!good) nInfoBad++;
  printf("INFO %s: %s", what, good ? "as expected" : "differs");
  if (fmt) {
    va_list ap; va_start(ap, fmt); printf(" ("); vprintf(fmt, ap); printf(")"); va_end(ap);
  }
  printf("\n");
}

/* ------------------------------------------------------------------------------
** deterministic procedural helpers
*/
static unsigned seedBase = 1;
static unsigned hash32(unsigned x)
{
  x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
  return x;
}
static float hashf(unsigned a, unsigned b)          /* [0,1) */
{
  return (float)(hash32(a * 0x9e3779b9u ^ hash32(b + seedBase * 0x85ebca6bu)) >> 8) / 16777216.0f;
}
static float smoothf(float t) { return t * t * (3.0f - 2.0f * t); }
/* tileable value noise on a period-p lattice */
static float vnoise(float x, float y, int p, unsigned salt)
{
  int   xi = (int)floorf(x), yi = (int)floorf(y);
  float fx = smoothf(x - (float)xi), fy = smoothf(y - (float)yi);
  int   x0 = ((xi % p) + p) % p, y0 = ((yi % p) + p) % p, x1 = (x0 + 1) % p, y1 = (y0 + 1) % p;
  float a = hashf((unsigned)(x0 + y0 * 977), salt), b = hashf((unsigned)(x1 + y0 * 977), salt);
  float c = hashf((unsigned)(x0 + y1 * 977), salt), d = hashf((unsigned)(x1 + y1 * 977), salt);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static float fbm(float x, float y, int p, unsigned salt)
{
  float s = 0.0f, amp = 0.5f;
  int   o;
  for (o = 0; o < 4; o++) { s += amp * vnoise(x, y, p, salt + (unsigned)o); x *= 2.0f; y *= 2.0f; p *= 2; amp *= 0.5f; }
  return s;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ------------------------------------------------------------------------------
** RGBA images and texel packers
*/
typedef struct { int w, h; unsigned char *p; } Img;     /* RGBA8, row major */

enum { K_WATER, K_SPARKLE, K_FOAM, K_SAND, K_DETAIL, K_CLOUD, K_LEAF, K_SHELL, K_BIG };

static void genImg(Img *im, int w, int h, int kind, unsigned salt)
{
  int x, y;
  im->w = w; im->h = h; im->p = (unsigned char *)malloc((size_t)w * h * 4);
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
      unsigned char *q = im->p + ((size_t)y * w + x) * 4;
      float u = (float)x / (float)w, v = (float)y / (float)h, n;
      int r = 0, g = 0, b = 0, a = 255;
      switch (kind) {
      case K_WATER: {
        float swell = fbm(u * 4.0f, v * 4.0f, 4, salt);
        float ridge = 0.5f + 0.5f * sinf(6.2831853f * (2.0f * u + 5.0f * v) + swell * 4.0f);
        n = 0.62f * swell + 0.25f * ridge * ridge + 0.13f * vnoise(u * 32.0f, v * 32.0f, 32, salt + 17);
        r = (int)(49 + 70 * n); g = (int)(128 + 82 * n); b = (int)(169 + 76 * n);
        break; }
      case K_SPARKLE:
        n = vnoise(u * 16.0f, v * 16.0f, 16, salt);
        n = n > 0.78f ? (n - 0.78f) * 4.5f : 0.0f;
        r = g = b = (int)(255 * clampf(n, 0, 1)); a = (int)(255 * clampf(n * 1.3f, 0, 1));
        break;
      case K_FOAM:
        n = fbm(u * 4.0f, v * 4.0f, 4, salt);
        r = g = b = 235 + (int)(20 * n); a = (int)(255 * clampf((n - 0.35f) * 3.0f, 0, 1));
        break;
      case K_SAND:
        n = fbm(u * 8.0f, v * 8.0f, 8, salt);
        r = (int)(200 + 45 * n); g = (int)(170 + 40 * n); b = (int)(110 + 30 * n);
        if (hashf((unsigned)(x + y * w), salt) > 0.97f) { r -= 50; g -= 50; b -= 40; }   /* grains */
        break;
      case K_DETAIL:
        n = vnoise(u * 32.0f, v * 32.0f, 32, salt);
        r = g = b = (int)(96 + 64 * n);
        break;
      case K_CLOUD: {
        float e = 0.0f, crown, shadow;
        for (int lobe = 0; lobe < 5; lobe++) {
          float cx = 0.19f + 0.155f * (float)lobe;
          float cy = 0.57f - 0.28f * hashf((unsigned)lobe, salt + 1);
          float dx = (u - cx) / (0.15f + 0.05f * hashf((unsigned)lobe, salt + 2));
          float dy = (v - cy) / (0.23f + 0.10f * hashf((unsigned)lobe, salt + 3));
          e = fmaxf(e, 1.0f - dx * dx - dy * dy);
        }
        n = fbm(u * 8.0f, v * 8.0f, 8, salt);
        crown = smoothf(clampf((0.73f - v) * 1.9f + 0.20f * n, 0, 1));
        shadow = 0.75f + 0.25f * crown;
        r = (int)(255 * shadow); g = (int)(247 * shadow); b = (int)(239 + 16 * crown);
        a = (int)(255 * smoothf(clampf((e - 0.19f + (n - 0.5f) * e * 0.9f) * 2.5f, 0, 1)));
        if (cinematic) {
          float edge = fminf(fminf(u, 1.0f - u), fminf(v, 1.0f - v));
          a = (int)(a * smoothf(clampf(edge * 12.0f, 0.0f, 1.0f)));
        }
        break; }
      case K_LEAF: {
        /* a frond: central rib along v, leaflets fanning out, transparent outside */
        float dx = fabsf(u - 0.5f) * 2.0f, rib = v;
        float width = 0.9f * sinf(3.14159f * rib);
        int   inside = dx < width && ((int)(rib * 18.0f + dx * 6.0f) & 1) == 0;
        if (dx < 0.06f && rib < 0.95f) inside = 1;
        n = vnoise(u * 8.0f, v * 8.0f, 8, salt);
        r = (int)(40 + 40 * n); g = (int)(110 + 90 * n * (1.0f - dx)); b = (int)(30 + 20 * n);
        a = inside ? 255 : 0;
        break; }
      case K_SHELL: {
        float dx = u - 0.5f, dy = v - 0.5f, d = sqrtf(dx * dx + dy * dy) * 2.0f;
        float ang = atan2f(dy, dx);
        n = 0.5f + 0.5f * sinf(ang * 7.0f + d * 9.0f + (float)(salt & 7));
        r = (int)(230 - 60 * d + 25 * n); g = (int)(180 - 70 * d + 30 * n);
        b = (int)(150 - 40 * d + 60 * hashf(salt, 3));
        a = d < 1.0f ? (int)(255 * clampf((1.0f - d) * 4.0f, 0, 1)) : 0;
        break; }
      case K_BIG:
        n = fbm(u * 32.0f, v * 32.0f, 32, salt);
        r = (int)(180 + 60 * n); g = (int)(160 + 50 * n); b = (int)(110 + 50 * n);
        if (((x >> 6) ^ (y >> 6)) & 1) { r -= 18; g -= 12; }      /* ripples visible at LOD 0 */
        break;
      }
      q[0] = (unsigned char)clampi(r, 0, 255); q[1] = (unsigned char)clampi(g, 0, 255);
      q[2] = (unsigned char)clampi(b, 0, 255); q[3] = (unsigned char)clampi(a, 0, 255);
    }
}

/* box-filter half size */
static void halfImg(const Img *s, Img *d)
{
  int x, y, c, w = s->w > 1 ? s->w / 2 : 1, h = s->h > 1 ? s->h / 2 : 1;
  d->w = w; d->h = h; d->p = (unsigned char *)malloc((size_t)w * h * 4);
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      for (c = 0; c < 4; c++) {
        int x0 = x * s->w / w, y0 = y * s->h / h;
        int x1 = s->w > 1 ? x0 + 1 : x0, y1 = s->h > 1 ? y0 + 1 : y0;
        d->p[((size_t)y * w + x) * 4 + c] = (unsigned char)((s->p[((size_t)y0 * s->w + x0) * 4 + c] +
          s->p[((size_t)y0 * s->w + x1) * 4 + c] + s->p[((size_t)y1 * s->w + x0) * 4 + c] +
          s->p[((size_t)y1 * s->w + x1) * 4 + c] + 2) / 4);
      }
}

/* palettes and NCC tables */
static GuTexPalette palSunset, palLeaf;
static GuNccTable   nccLeaf, nccShell;

static void buildPalettes(void)
{
  int i;
  for (i = 0; i < 256; i++) {          /* 0..127 cloud greys to warm white; 128..191 parasol reds; 192..255 cream */
    int r, g, b;
    if (i < 128) { r = 150 + i * 105 / 127; g = 156 + i * 96 / 127; b = 172 + i * 76 / 127; }
    else if (i < 192) { int k = i - 128; r = 170 + k; g = 40 + k / 3; b = 35 + k / 4; }
    else { int k = i - 192; r = 235 + k / 4; g = 225 + k / 4; b = 200 + k / 3; }
    palSunset.data[i] = 0xff000000u | ((FxU32)clampi(r, 0, 255) << 16) | ((FxU32)clampi(g, 0, 255) << 8) | (FxU32)clampi(b, 0, 255);
  }
  for (i = 0; i < 256; i++) {          /* leaf greens; index 0 fully transparent */
    int g = 70 + (i * 150) / 255, r = 20 + (i * 60) / 255, b = 10 + (i * 40) / 255;
    palLeaf.data[i] = (i ? 0xff000000u : 0u) | ((FxU32)r << 16) | ((FxU32)g << 8) | (FxU32)b;
  }
}

static void packNcc(GuNccTable *t)
{
  int i;
  for (i = 0; i < 4; i++)
    t->packed_data[i] = (FxU32)t->yRGB[4 * i] | ((FxU32)t->yRGB[4 * i + 1] << 8) |
                        ((FxU32)t->yRGB[4 * i + 2] << 16) | ((FxU32)t->yRGB[4 * i + 3] << 24);
  for (i = 0; i < 4; i++) {
    t->packed_data[4 + i] = ((FxU32)(t->iRGB[i][0] & 0x1ff) << 18) | ((FxU32)(t->iRGB[i][1] & 0x1ff) << 9) |
                            (FxU32)(t->iRGB[i][2] & 0x1ff);
    t->packed_data[8 + i] = ((FxU32)(t->qRGB[i][0] & 0x1ff) << 18) | ((FxU32)(t->qRGB[i][1] & 0x1ff) << 9) |
                            (FxU32)(t->qRGB[i][2] & 0x1ff);
  }
}

static void buildNcc(void)
{
  int i;
  /* leaves: Y ramp, I/Q bend toward green; code 0x00 decodes to magenta (the key) */
  for (i = 0; i < 16; i++) nccLeaf.yRGB[i] = (FxU8)(i * 13);
  nccLeaf.iRGB[0][0] = 255; nccLeaf.iRGB[0][1] = 0;   nccLeaf.iRGB[0][2] = 255;
  for (i = 1; i < 4; i++) { nccLeaf.iRGB[i][0] = (FxI16)(-20 + 5 * i); nccLeaf.iRGB[i][1] = (FxI16)(30 + 10 * i); nccLeaf.iRGB[i][2] = (FxI16)(-30 + 4 * i); }
  for (i = 0; i < 4; i++) { nccLeaf.qRGB[i][0] = (FxI16)(i * 6); nccLeaf.qRGB[i][1] = (FxI16)(i * 9); nccLeaf.qRGB[i][2] = 0; }
  packNcc(&nccLeaf);
  /* shells: neutral luminance with warm/cool chroma steps */
  for (i = 0; i < 16; i++) nccShell.yRGB[i] = (FxU8)(i * 17);
  for (i = 0; i < 4; i++) {
    nccShell.iRGB[i][0] = (FxI16)(-45 + 30 * i); nccShell.iRGB[i][1] = (FxI16)(-10 + 6 * i); nccShell.iRGB[i][2] = (FxI16)(40 - 25 * i);
    nccShell.qRGB[i][0] = (FxI16)(-20 + 12 * i); nccShell.qRGB[i][1] = (FxI16)(25 - 15 * i); nccShell.qRGB[i][2] = (FxI16)(-15 + 10 * i);
  }
  packNcc(&nccShell);
}

static void nccDecode(const GuNccTable *t, int code, int *r, int *g, int *b)
{
  int y = (code >> 4) & 15, ii = (code >> 2) & 3, qq = code & 3;
  *r = clampi(t->yRGB[y] + t->iRGB[ii][0] + t->qRGB[qq][0], 0, 255);
  *g = clampi(t->yRGB[y] + t->iRGB[ii][1] + t->qRGB[qq][1], 0, 255);
  *b = clampi(t->yRGB[y] + t->iRGB[ii][2] + t->qRGB[qq][2], 0, 255);
}
static int nccEncode(const GuNccTable *t, int r, int g, int b, int firstCode)
{
  int c, best = firstCode, bd = 1 << 30;
  for (c = firstCode; c < 256; c++) {
    int R, G, B, d;
    nccDecode(t, c, &R, &G, &B);
    d = (R - r) * (R - r) + (G - g) * (G - g) + (B - b) * (B - b);
    if (d < bd) { bd = d; best = c; }
  }
  return best;
}
static int lum(const unsigned char *q) { return (q[0] * 77 + q[1] * 150 + q[2] * 29) >> 8; }
static int paletteIndex(const GuTexPalette *p, const unsigned char *q)
{
  int i, best = 1, bd = 1 << 30;
  if (p == &palLeaf && q[3] < 128) return 0;
  for (i = (p == &palLeaf) ? 1 : 0; i < 256; i++) {
    int r = (int)(p->data[i] >> 16 & 255), g = (int)(p->data[i] >> 8 & 255), b = (int)(p->data[i] & 255);
    int d = (r - q[0]) * (r - q[0]) + (g - q[1]) * (g - q[1]) + (b - q[2]) * (b - q[2]);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

/* ---- block compressors (small, deterministic, not optimal) ---------------- */
static unsigned short to565(int r, int g, int b) { return (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }
static void from565(unsigned c, int *r, int *g, int *b)
{
  *r = (int)((c >> 11) & 31) * 255 / 31; *g = (int)((c >> 5) & 63) * 255 / 63; *b = (int)(c & 31) * 255 / 31;
}
static const unsigned char *texel(const Img *im, int x, int y)
{
  x = x < im->w ? x : im->w - 1; y = y < im->h ? y : im->h - 1;
  return im->p + ((size_t)y * im->w + x) * 4;
}
/* DXT colour block for the 4x4 at (bx,by); premul: multiply colour by alpha (DXT2/4) */
static void dxtColor(const Img *im, int bx, int by, int premul, unsigned char *out)
{
  int i, lo = 1 << 30, hi = -1, li = 0, hiI = 0;
  unsigned c0, c1, idx = 0;
  int pr[4], pg[4], pb[4];
  for (i = 0; i < 16; i++) {
    const unsigned char *q = texel(im, bx + (i & 3), by + (i >> 2));
    int l = lum(q);
    if (l < lo) { lo = l; li = i; }
    if (l > hi) { hi = l; hiI = i; }
  }
  {
    const unsigned char *a = texel(im, bx + (hiI & 3), by + (hiI >> 2)), *b = texel(im, bx + (li & 3), by + (li >> 2));
    int ma = premul ? a[3] : 255, mb = premul ? b[3] : 255;
    c0 = to565(a[0] * ma / 255, a[1] * ma / 255, a[2] * ma / 255);
    c1 = to565(b[0] * mb / 255, b[1] * mb / 255, b[2] * mb / 255);
  }
  if (c0 < c1) { unsigned t = c0; c0 = c1; c1 = t; }
  if (c0 == c1) { if (c0 > 0) c1 = c0 - 1; else c0 = 1; }     /* keep 4-colour mode */
  from565(c0, &pr[0], &pg[0], &pb[0]); from565(c1, &pr[1], &pg[1], &pb[1]);
  pr[2] = (2 * pr[0] + pr[1]) / 3; pg[2] = (2 * pg[0] + pg[1]) / 3; pb[2] = (2 * pb[0] + pb[1]) / 3;
  pr[3] = (pr[0] + 2 * pr[1]) / 3; pg[3] = (pg[0] + 2 * pg[1]) / 3; pb[3] = (pb[0] + 2 * pb[1]) / 3;
  for (i = 0; i < 16; i++) {
    const unsigned char *q = texel(im, bx + (i & 3), by + (i >> 2));
    int m = premul ? q[3] : 255, k, best = 0, bd = 1 << 30;
    for (k = 0; k < 4; k++) {
      int dr = pr[k] - q[0] * m / 255, dg = pg[k] - q[1] * m / 255, db = pb[k] - q[2] * m / 255;
      int d = dr * dr + dg * dg + db * db;
      if (d < bd) { bd = d; best = k; }
    }
    idx |= (unsigned)best << (2 * i);
  }
  out[0] = (unsigned char)c0; out[1] = (unsigned char)(c0 >> 8);
  out[2] = (unsigned char)c1; out[3] = (unsigned char)(c1 >> 8);
  out[4] = (unsigned char)idx; out[5] = (unsigned char)(idx >> 8);
  out[6] = (unsigned char)(idx >> 16); out[7] = (unsigned char)(idx >> 24);
}
/* returns bytes written; level stored as standard row-major 4x4 blocks */
static int encodeDxt(const Img *im, int fmt, unsigned char *out)
{
  int bw = (im->w + 3) / 4, bh = (im->h + 3) / 4, bx, by, n = 0, i;
  int premul = (fmt == GR_TEXFMT_ARGB_CMP_DXT2 || fmt == GR_TEXFMT_ARGB_CMP_DXT4);
  for (by = 0; by < bh; by++)
    for (bx = 0; bx < bw; bx++) {
      if (fmt == GR_TEXFMT_ARGB_CMP_DXT2 || fmt == GR_TEXFMT_ARGB_CMP_DXT3) {
        for (i = 0; i < 8; i++) {
          int a0 = texel(im, bx * 4 + ((2 * i) & 3), by * 4 + ((2 * i) >> 2))[3] >> 4;
          int a1 = texel(im, bx * 4 + ((2 * i + 1) & 3), by * 4 + ((2 * i + 1) >> 2))[3] >> 4;
          out[n++] = (unsigned char)(a0 | (a1 << 4));
        }
      } else if (fmt == GR_TEXFMT_ARGB_CMP_DXT4 || fmt == GR_TEXFMT_ARGB_CMP_DXT5) {
        int lo = 255, hi = 0, pal[8], k;
        unsigned long long bits = 0;
        for (i = 0; i < 16; i++) { int a = texel(im, bx * 4 + (i & 3), by * 4 + (i >> 2))[3]; if (a < lo) lo = a; if (a > hi) hi = a; }
        if (hi == lo) { if (hi < 255) hi++; else lo--; }
        pal[0] = hi; pal[1] = lo;                       /* a0 > a1: 8-value mode */
        for (k = 1; k < 7; k++) pal[k + 1] = ((7 - k) * hi + k * lo) / 7;
        for (i = 0; i < 16; i++) {
          int a = texel(im, bx * 4 + (i & 3), by * 4 + (i >> 2))[3], best = 0, bd = 1 << 30;
          for (k = 0; k < 8; k++) { int d = abs(pal[k] - a); if (d < bd) { bd = d; best = k; } }
          bits |= (unsigned long long)best << (3 * i);
        }
        out[n++] = (unsigned char)hi; out[n++] = (unsigned char)lo;
        for (i = 0; i < 6; i++) out[n++] = (unsigned char)(bits >> (8 * i));
      }
      dxtColor(im, bx * 4, by * 4, premul, out + n);
      n += 8;
    }
  return n;
}
/* FXT1, CC_CHROMA mode (mode bits 127:125 = 010): four RGB555 colours and a
   2-bit index per texel.  Texel t of the 8x4 block: t = (x&3) + 4*y + (x&4 ? 16 : 0). */
static int encodeFxt1(const Img *im, unsigned char *out)
{
  int bw = (im->w + 7) / 8, bh = (im->h + 3) / 4, bx, by, n = 0, i, k;
  for (by = 0; by < bh; by++)
    for (bx = 0; bx < bw; bx++) {
      int lo = 1 << 30, hi = -1, li = 0, hiI = 0, cr[4], cg[4], cb[4];
      unsigned w[4] = { 0, 0, 0, 0 };
      unsigned col[4];
      for (i = 0; i < 32; i++) {
        int x = (i & 3) + ((i & 16) ? 4 : 0), y = (i >> 2) & 3, l = lum(texel(im, bx * 8 + x, by * 4 + y));
        if (l < lo) { lo = l; li = i; }
        if (l > hi) { hi = l; hiI = i; }
      }
      {
        const unsigned char *a = texel(im, bx * 8 + (li & 3) + ((li & 16) ? 4 : 0), by * 4 + ((li >> 2) & 3));
        const unsigned char *b = texel(im, bx * 8 + (hiI & 3) + ((hiI & 16) ? 4 : 0), by * 4 + ((hiI >> 2) & 3));
        for (k = 0; k < 4; k++) {
          cr[k] = (a[0] * (3 - k) + b[0] * k) / 3; cg[k] = (a[1] * (3 - k) + b[1] * k) / 3; cb[k] = (a[2] * (3 - k) + b[2] * k) / 3;
          col[k] = ((unsigned)(cr[k] >> 3) << 10) | ((unsigned)(cg[k] >> 3) << 5) | (unsigned)(cb[k] >> 3);
        }
      }
      for (i = 0; i < 32; i++) {
        int x = (i & 3) + ((i & 16) ? 4 : 0), y = (i >> 2) & 3, best = 0, bd = 1 << 30;
        const unsigned char *q = texel(im, bx * 8 + x, by * 4 + y);
        for (k = 0; k < 4; k++) {
          int d = (cr[k] - q[0]) * (cr[k] - q[0]) + (cg[k] - q[1]) * (cg[k] - q[1]) + (cb[k] - q[2]) * (cb[k] - q[2]);
          if (d < bd) { bd = d; best = k; }
        }
        w[i >> 4] |= (unsigned)best << (2 * (i & 15));
      }
      /* colours at bits 64 + 15k */
      {
        unsigned long long hiBits = 0;
        for (k = 0; k < 4; k++) hiBits |= (unsigned long long)col[k] << (15 * k);
        hiBits |= 2ull << 61;                          /* bits 127:125 = 010 */
        w[2] = (unsigned)hiBits; w[3] = (unsigned)(hiBits >> 32);
      }
      for (k = 0; k < 4; k++) { out[n++] = (unsigned char)w[k]; out[n++] = (unsigned char)(w[k] >> 8);
                                out[n++] = (unsigned char)(w[k] >> 16); out[n++] = (unsigned char)(w[k] >> 24); }
    }
  return n;
}

static int isCompressed(int fmt) { return fmt == GR_TEXFMT_ARGB_CMP_FXT1 || (fmt >= GR_TEXFMT_ARGB_CMP_DXT1 && fmt <= GR_TEXFMT_ARGB_CMP_DXT5); }
static int texelBytes(int fmt)
{
  if (fmt == GR_TEXFMT_ARGB_8888) return 4;
  if (fmt >= GR_TEXFMT_16BIT && fmt <= GR_TEXFMT_AP_88) return 2;
  return 1;
}

/* pack one level into dst; returns bytes.  table6666: P8 against the leaf palette */
static int packLevel(const Img *im, int fmt, int table6666, unsigned char *dst)
{
  int i, n = im->w * im->h;
  if (fmt == GR_TEXFMT_ARGB_CMP_FXT1) return encodeFxt1(im, dst);
  if (isCompressed(fmt)) return encodeDxt(im, fmt, dst);
  for (i = 0; i < n; i++) {
    const unsigned char *q = im->p + (size_t)i * 4;
    int r = q[0], g = q[1], b = q[2], a = q[3], l = lum(q);
    unsigned v = 0;
    switch (fmt) {
    case GR_TEXFMT_RGB_332:        v = (unsigned)((r >> 5) << 5 | (g >> 5) << 2 | (b >> 6)); break;
    case GR_TEXFMT_YIQ_422:        v = (unsigned)(table6666 ? (a < 128 ? 0 : nccEncode(&nccLeaf, r, g, b, 0x14))
                                                         : nccEncode(&nccShell, r, g, b, 0)); break;
    case GR_TEXFMT_ALPHA_8:        v = (unsigned)a; break;
    case GR_TEXFMT_INTENSITY_8:    v = (unsigned)l; break;
    case GR_TEXFMT_ALPHA_INTENSITY_44: v = (unsigned)((a >> 4) << 4 | (l >> 4)); break;
    case GR_TEXFMT_P_8:            v = (unsigned)paletteIndex(table6666 ? &palLeaf : &palSunset, q); break;
    case GR_TEXFMT_ARGB_8332:      v = (unsigned)(a << 8 | (r >> 5) << 5 | (g >> 5) << 2 | (b >> 6)); break;
    case GR_TEXFMT_AYIQ_8422:      v = (unsigned)(table6666 ? (a < 128 ? 0 : (a << 8 | nccEncode(&nccLeaf, r, g, b, 0x14)))
                                                         : (a << 8 | nccEncode(&nccShell, r, g, b, 0))); break;
    case GR_TEXFMT_RGB_565:        v = to565(r, g, b); break;
    case GR_TEXFMT_ARGB_1555:      v = (unsigned)((a >= 128) << 15 | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)); break;
    case GR_TEXFMT_ARGB_4444:      v = (unsigned)((a >> 4) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4)); break;
    case GR_TEXFMT_ALPHA_INTENSITY_88: v = (unsigned)(a << 8 | l); break;
    case GR_TEXFMT_AP_88:          v = (unsigned)(a << 8 | paletteIndex(&palSunset, q)); break;
    case GR_TEXFMT_ARGB_8888:      v = (unsigned)a << 24 | (unsigned)r << 16 | (unsigned)g << 8 | (unsigned)b; break;
    }
    if (texelBytes(fmt) == 1) dst[i] = (unsigned char)v;
    else if (texelBytes(fmt) == 2) { dst[2 * i] = (unsigned char)v; dst[2 * i + 1] = (unsigned char)(v >> 8); }
    else { dst[4 * i] = (unsigned char)v; dst[4 * i + 1] = (unsigned char)(v >> 8);
           dst[4 * i + 2] = (unsigned char)(v >> 16); dst[4 * i + 3] = (unsigned char)(v >> 24); }
  }
  return n * texelBytes(fmt);
}

/* ------------------------------------------------------------------------------
** texture memory: a bump allocator per TMU
*/
static int   nTmu = 1;
static FxU32 texCur[2], texEnd[2], texAlign = 16;
static int   texAllocFails, perLevelUsed;
static int   fmtUsed[0x20];       /* texture formats sourced for drawing */

typedef struct {
  int        ok, tmu;
  FxU32      addr;
  FxU32      mask;        /* GR_MIPMAPLEVELMASK_* */
  GrTexInfo  info;
  int        table6666;
} Tex;

static int lodOf(int size) { int l = 0; while ((1 << l) < size) l++; return l; }
static GrAspectRatio_t aspectOf(int w, int h) { return (GrAspectRatio_t)(lodOf(w) - lodOf(h)); }

static int texReserve(Tex *t, int tmu, FxU32 mask)
{
  FxU32 need = grTexTextureMemRequired(mask, &t->info), a = (texCur[tmu] + texAlign - 1) & ~(texAlign - 1);
  if (tmu >= nTmu || a + need > texEnd[tmu]) return 0;
  t->tmu = tmu; t->addr = a; t->mask = mask; t->ok = 1;
  texCur[tmu] = a + need;
  return 1;
}

/* build a full chain from an RGBA image and download it with grTexDownloadMipMap
   (or level by level when perLevel is set or the format is compressed) */
static int texCreate(Tex *t, const Img *base, int fmt, int smallSize, int tmu, FxU32 mask, int table6666, int perLevel)
{
  Img   lv[12];
  int   nl = 0, i, total = 0, off = 0, sz;
  unsigned char *buf, *p;
  int   w = base->w, h = base->h, big = w > h ? w : h;

  memset(t, 0, sizeof *t);
  t->table6666 = table6666;
  t->info.largeLodLog2 = (GrLOD_t)lodOf(big);
  t->info.smallLodLog2 = (GrLOD_t)lodOf(smallSize);
  t->info.aspectRatioLog2 = aspectOf(w, h);
  t->info.format = (GrTextureFormat_t)fmt;
  if (!texReserve(t, tmu, mask)) { texAllocFails++; return 0; }
  lv[0] = *base;
  while ((lv[nl].w > smallSize || lv[nl].h > smallSize) && (lv[nl].w > 1 || lv[nl].h > 1)) {
    if ((lv[nl].w > lv[nl].h ? lv[nl].w : lv[nl].h) <= smallSize) break;
    halfImg(&lv[nl], &lv[nl + 1]); nl++;
  }
  nl++;
  for (i = 0; i < nl; i++) {
    int bw = lv[i].w, bh = lv[i].h;
    if (fmt == GR_TEXFMT_ARGB_CMP_FXT1) { bw = bw < 8 ? 8 : bw; bh = bh < 4 ? 4 : bh; total += bw * bh / 2; }
    else if (fmt == GR_TEXFMT_ARGB_CMP_DXT1) { bw = bw < 4 ? 4 : bw; bh = bh < 4 ? 4 : bh; total += bw * bh / 2; }
    else if (isCompressed(fmt)) { bw = bw < 4 ? 4 : bw; bh = bh < 4 ? 4 : bh; total += bw * bh; }
    else total += bw * bh * texelBytes(fmt);
  }
  buf = (unsigned char *)calloc((size_t)total + 64, 1);
  for (i = 0; i < nl; i++) {
    Img e = lv[i];
    if (fmt == GR_TEXFMT_ARGB_CMP_FXT1 && (e.w < 8 || e.h < 4)) { e.w = e.w < 8 ? 8 : e.w; e.h = e.h < 4 ? 4 : e.h; e.p = lv[i].p; }
    p = buf + off;
    if (isCompressed(fmt) && (e.w != lv[i].w || e.h != lv[i].h)) {
      /* pad small compressed levels by clamping texel reads */
      Img pad; int x, y;
      pad.w = e.w; pad.h = e.h; pad.p = (unsigned char *)malloc((size_t)pad.w * pad.h * 4);
      for (y = 0; y < pad.h; y++) for (x = 0; x < pad.w; x++) memcpy(pad.p + ((size_t)y * pad.w + x) * 4, texel(&lv[i], x, y), 4);
      sz = packLevel(&pad, fmt, table6666, p);
      free(pad.p);
    } else sz = packLevel(&lv[i], fmt, table6666, p);
    if (perLevel || isCompressed(fmt)) perLevelUsed = 1;
    if (perLevel || isCompressed(fmt))
      grTexDownloadMipMapLevel((GrChipID_t)tmu, t->addr, (GrLOD_t)(t->info.largeLodLog2 - i), t->info.largeLodLog2,
                               t->info.aspectRatioLog2, (GrTextureFormat_t)fmt, mask, p);
    off += sz;
  }
  t->info.data = buf;
  if (!perLevel && !isCompressed(fmt)) grTexDownloadMipMap((GrChipID_t)tmu, t->addr, mask, &t->info);
  for (i = 1; i < nl; i++) free(lv[i].p);
  return 1;
}

/* ------------------------------------------------------------------------------
** vertices, camera, projection
*/
typedef struct {
  float x, y, ooz, oow;
  float r, g, b, a;
  float fog;
  float s0, t0, s1, t1;
} V;
#define OFS(f) ((FxI32)(size_t)&((V *)0)->f)

static float scrW = 640.0f, scrH = 480.0f, focal;
static FxU32 zrange[2], wrange[2];
static int   useW;                 /* this frame: W buffer */
static float tNow;                 /* animation time (frame based) */

static struct { float eye[3], f[3], r[3], u[3], fh[3], rh[3]; } cam;
#define NEARZ 1.0f

static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void norm3(float *a) { float l = sqrtf(dot3(a, a)); a[0] /= l; a[1] /= l; a[2] /= l; }

static float camJit[2];                /* T-buffer depth of field: eye offset (right, up) */
static float sunD[3];                  /* sun direction, from the unjittered camera */
static void setCamera(int frame)
{
  float ang = 0.75f + (float)frame * 0.004f, radius = 40.0f, tgt[3] = { 0.0f, 2.4f, 0.0f };
  float eyeHeight = 5.4f + 0.25f * sinf((float)frame * 0.05f);
  if (cinematic) {
    float u = smoothf(clampf(tNow / shotDuration, 0.0f, 1.0f));
    ang = 0.68f + 0.52f * u;
    radius = 43.0f - 5.0f * u;
    eyeHeight = 6.1f - 0.7f * u;
  }
  cam.eye[0] = radius * cosf(ang); cam.eye[1] = eyeHeight; cam.eye[2] = radius * sinf(ang);
  /* depth of field: move the eye around the focus point (the hut stays sharp) */
  cam.eye[0] += -sinf(ang) * camJit[0]; cam.eye[2] += cosf(ang) * camJit[0]; cam.eye[1] += camJit[1];
  cam.f[0] = tgt[0] - cam.eye[0]; cam.f[1] = tgt[1] - cam.eye[1]; cam.f[2] = tgt[2] - cam.eye[2]; norm3(cam.f);
  cam.r[0] = -cam.f[2]; cam.r[1] = 0.0f; cam.r[2] = cam.f[0]; norm3(cam.r);           /* f x up */
  cam.u[0] = cam.r[1] * cam.f[2] - cam.r[2] * cam.f[1];
  cam.u[1] = cam.r[2] * cam.f[0] - cam.r[0] * cam.f[2];
  cam.u[2] = cam.r[0] * cam.f[1] - cam.r[1] * cam.f[0];
  cam.fh[0] = cam.f[0]; cam.fh[1] = 0.0f; cam.fh[2] = cam.f[2]; norm3(cam.fh);
  cam.rh[0] = cam.r[0]; cam.rh[1] = 0.0f; cam.rh[2] = cam.r[2];
  /* The sun keeps its place relative to the heading, so it stays in view.  It is taken from
     the unjittered camera only: the depth-of-field passes move the eye per sample buffer and
     must not move the sun with it (the shadows would differ per sample, and with them the
     triangle count each chip receives). */
  if (cinematic) {
    /* Keep the sun anchored in world space as the camera orbits. */
    sunD[0] = -0.92f * cosf(0.94f) + 0.38f * sinf(0.94f);
    sunD[1] = 0.28f;
    sunD[2] = -0.92f * sinf(0.94f) - 0.38f * cosf(0.94f);
  } else if (camJit[0] == 0.0f && camJit[1] == 0.0f) {
    sunD[0] = cam.fh[0] * 0.92f + cam.rh[0] * 0.38f; sunD[1] = highDetail ? 0.28f : 0.16f; sunD[2] = cam.fh[2] * 0.92f + cam.rh[2] * 0.38f;
  }
}

/* project a world point; returns view depth */
static float proj(const float *p, V *v)
{
  float d[3], xc, yc, zc;
  d[0] = p[0] - cam.eye[0]; d[1] = p[1] - cam.eye[1]; d[2] = p[2] - cam.eye[2];
  xc = dot3(d, cam.r); yc = dot3(d, cam.u); zc = dot3(d, cam.f);
  if (zc < NEARZ) zc = NEARZ;
  /* Packet-3 setup stores screen coordinates in signed 12.4. An offscreen
     vertex beyond +2047 would otherwise wrap to the opposite screen edge. */
  v->x = clampf(scrW * 0.5f + focal * xc / zc, -2000.0f, 2047.9375f);
  v->y = clampf(scrH * 0.5f - focal * yc / zc, -2000.0f, 2047.9375f);
  v->oow = 1.0f / zc;
  /* The RTL triangle path carries 16 significant Z bits into the 32-bit
     fragment pipeline. Keep projected cinematic geometry within that range. */
  float depthMax = cinematic ? fminf((float)zrange[0], 65535.0f) : (float)zrange[0];
  v->ooz = depthMax * (NEARZ / zc);
  v->fog = 1.0f / (zc * 1.6f);
  return zc;
}
static void col(V *v, float r, float g, float b, float a) { v->r = r; v->g = g; v->b = b; v->a = a; }
static void st0(V *v, float s, float t) { v->s0 = s * v->oow; v->t0 = t * v->oow; }
static void st1(V *v, float s, float t) { v->s1 = s * v->oow; v->t1 = t * v->oow; }
/* screen-space vertex at the far plane (sky, clouds, HUD-like overlays) */
static void scr(V *v, float x, float y)
{
  memset(v, 0, sizeof *v);
  float depthMax = cinematic ? fminf((float)zrange[0], 65535.0f) : (float)zrange[0];
  v->x = x; v->y = y; v->oow = 1.0f / 900.0f; v->ooz = depthMax * (NEARZ / 900.0f); v->fog = v->oow;
}

/* vertex layouts, saved with grGlideGetVertexLayout and switched with grGlideSetVertexLayout */
static void *layoutCol, *layoutTex, *layoutTex2;
static int   haveFogCoord;
/* Two layout sets: with the fog coordinate (Z-buffer frames: fog table on the fog
   coordinate) and without it (W-buffer frames: fog table on Q).  With the fog parameter in
   the layout, grDepthBufferMode(WBUFFER) sets fbzMode DEPTH_FLOAT_SEL (depth from the
   floating Z, gglide.c) because W then carries the fog coordinate, but Glide only sends Z
   while fogging is on, so unfogged W-buffer primitives would test a stale Z. */
static void *layoutSet[2][3];
static void *layoutClip;               /* clip-space vertices: x y z w (GR_PARAM_W), colour */
static int   fogCoordNow;
static void useLayouts(int fogcoord)
{
  fogCoordNow = fogcoord && haveFogCoord;
  layoutCol = layoutSet[fogCoordNow][0]; layoutTex = layoutSet[fogCoordNow][1]; layoutTex2 = layoutSet[fogCoordNow][2];
}
static void makeLayouts(void)
{
  FxI32 sz = 0;
  int f, k;
  grGet(GR_GLIDE_VERTEXLAYOUT_SIZE, 4, &sz);
  for (f = 0; f < 2; f++) for (k = 0; k < 3; k++) layoutSet[f][k] = calloc((size_t)sz + 16, 1);
  grCoordinateSpace(GR_WINDOW_COORDS);
  for (f = 0; f < 2; f++) {
  grVertexLayout(GR_PARAM_XY,  OFS(x),   GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_Z,   OFS(ooz), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_Q,   OFS(oow), GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_RGB, OFS(r),   GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_A,   OFS(a),   GR_PARAM_ENABLE);
  grVertexLayout(GR_PARAM_FOG_EXT, OFS(fog), (f && haveFogCoord) ? GR_PARAM_ENABLE : GR_PARAM_DISABLE);
  grVertexLayout(GR_PARAM_ST0, OFS(s0),  GR_PARAM_DISABLE);
  grVertexLayout(GR_PARAM_ST1, OFS(s1),  GR_PARAM_DISABLE);
  grGlideGetVertexLayout(layoutSet[f][0]);
  grVertexLayout(GR_PARAM_ST0, OFS(s0),  GR_PARAM_ENABLE);
  grGlideGetVertexLayout(layoutSet[f][1]);
  grVertexLayout(GR_PARAM_ST1, OFS(s1),  GR_PARAM_ENABLE);
  grGlideGetVertexLayout(layoutSet[f][2]);
  }
  /* clip space: Glide divides by GR_PARAM_W (clip w); no Q, no fog coordinate, no textures */
  layoutClip = calloc((size_t)sz + 16, 1);
  grVertexLayout(GR_PARAM_ST0, OFS(s0),  GR_PARAM_DISABLE);
  grVertexLayout(GR_PARAM_ST1, OFS(s1),  GR_PARAM_DISABLE);
  grVertexLayout(GR_PARAM_FOG_EXT, OFS(fog), GR_PARAM_DISABLE);
  grVertexLayout(GR_PARAM_Q,   OFS(oow), GR_PARAM_DISABLE);
  grVertexLayout(GR_PARAM_W,   OFS(oow), GR_PARAM_ENABLE);
  grGlideGetVertexLayout(layoutClip);
  grVertexLayout(GR_PARAM_W,   OFS(oow), GR_PARAM_DISABLE);
  grVertexLayout(GR_PARAM_Q,   OFS(oow), GR_PARAM_ENABLE);
  useLayouts(1);
}

/* ------------------------------------------------------------------------------
** render state helpers
*/
static int vsa, aaSamples, pix32, pix15, haveStencil;
static unsigned long statTris, statPoints, statLines;

static void ccIterated(void)
{
  grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE);
  grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE);
}
/* texture (from the TMU chain) modulated by the iterated colour/alpha */
static void ccTexMod(void)
{
  grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
  grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
}
static void tmuPass(GrChipID_t t)
{
  grTexCombine(t, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
}
static void tmu1Off(void)
{
  if (nTmu > 1)
    grTexCombine(GR_TMU1, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE, GR_COMBINE_FUNCTION_ZERO, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
}
static void blendOff(void) { grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO); }
static void blendAlpha(void) { grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO); }
static void depthOn(int write)
{
  grDepthBufferMode(useW ? GR_DEPTHBUFFER_WBUFFER : GR_DEPTHBUFFER_ZBUFFER);
  grDepthBufferFunction(useW ? GR_CMP_LEQUAL : GR_CMP_GEQUAL);
  grDepthMask(write ? FXTRUE : FXFALSE);
}
static void depthOff(void) { grDepthBufferFunction(GR_CMP_ALWAYS); grDepthMask(FXFALSE); }
static void source(const Tex *t)
{
  if (!t->ok) return;
  grTexSource((GrChipID_t)t->tmu, t->addr, t->mask, (GrTexInfo *)&t->info);
  fmtUsed[t->info.format & 0x1f] = 1;
  if (t->info.format == GR_TEXFMT_ARGB_8888) USE(F_TEX8888);
  if (t->info.format == GR_TEXFMT_ARGB_CMP_FXT1) USE(F_FXT1);
  if (t->info.format >= GR_TEXFMT_ARGB_CMP_DXT1 && t->info.format <= GR_TEXFMT_ARGB_CMP_DXT5) USE(F_DXT);
}
static void tri(const V *a, const V *b, const V *c) { grDrawTriangle(a, b, c); statTris++; }
static void quad(const V *a, const V *b, const V *c, const V *d) { tri(a, b, c); tri(a, c, d); }

/* ------------------------------------------------------------------------------
** textures of the scene.  Every format is used by something in the picture:
**   sand RGB565 (trilinear EVEN/ODD on TMU0/TMU1, and BOTH + I8 detail on TMU1),
**   water RGB565 + AI44 sparkle (TMU1), foam AI88, clouds ARGB4444 and AP88,
**   fronds P8 (PALETTE6666, alpha test) and AYIQ8422 (alpha test + texture chroma key),
**   hut walls ARGB8888 (VSA) / ARGB1555, hut thatch YIQ422 (NCC1), jetty deck RGB332,
**   jetty posts ARGB1555, towels ARGB8332 + DXT2 + DXT4, parasol P8 (plain palette),
**   footprints A8, starfish DXT5 / ARGB4444, beach ball DXT3 / ARGB4444,
**   dinghy hull DXT1 / ARGB1555, hut door FXT1 / RGB332, rock outcrop: the largest
**   texture the board takes (2048x2048 RGB565 on VSA), tide pool: render-to-texture
**   target (VSA) or water, mirror-wrapped.
*/
static Tex texWater, texSparkle, texFoam, texSandBoth, texSandEven, texSandOdd, texDetail, texBig;
static Tex texCloudA, texCloudB, texLeafP8, texLeafNcc, texRefl;
static Tex texWall, texThatch, texDeck, texPost, texTowelA, texTowelB, texTowelC, texParasol;
static Tex texFoot, texStar, texBall, texHull, texDoor;
static int bigSize, galleryMode;

typedef struct { int fmt; const char *name; int vsaOnly; int t6666; Tex tex; } Shell;
static Shell shells[] = {
  { GR_TEXFMT_RGB_332, "RGB332", 0, 0 },          { GR_TEXFMT_YIQ_422, "YIQ422", 0, 0 },
  { GR_TEXFMT_ALPHA_8, "A8", 0, 0 },              { GR_TEXFMT_INTENSITY_8, "I8", 0, 0 },
  { GR_TEXFMT_ALPHA_INTENSITY_44, "AI44", 0, 0 }, { GR_TEXFMT_P_8, "P8", 0, 0 },
  { GR_TEXFMT_P_8, "P8_6666", 0, 1 },             { GR_TEXFMT_ARGB_8332, "ARGB8332", 0, 0 },
  { GR_TEXFMT_AYIQ_8422, "AYIQ8422", 0, 0 },      { GR_TEXFMT_RGB_565, "RGB565", 0, 0 },
  { GR_TEXFMT_ARGB_1555, "ARGB1555", 0, 0 },      { GR_TEXFMT_ARGB_4444, "ARGB4444", 0, 0 },
  { GR_TEXFMT_ALPHA_INTENSITY_88, "AI88", 0, 0 }, { GR_TEXFMT_AP_88, "AP88", 0, 0 },
  { GR_TEXFMT_ARGB_8888, "ARGB8888", 1, 0 },      { GR_TEXFMT_ARGB_CMP_FXT1, "FXT1", 1, 0 },
  { GR_TEXFMT_ARGB_CMP_DXT1, "DXT1", 1, 0 },      { GR_TEXFMT_ARGB_CMP_DXT2, "DXT2", 1, 0 },
  { GR_TEXFMT_ARGB_CMP_DXT3, "DXT3", 1, 0 },      { GR_TEXFMT_ARGB_CMP_DXT4, "DXT4", 1, 0 },
  { GR_TEXFMT_ARGB_CMP_DXT5, "DXT5", 1, 0 },
};
#define NSHELL (int)(sizeof shells / sizeof shells[0])
static void setTable(int t6666, int ncc)
{
  if (ncc == 1) { grTexDownloadTable(GR_TEXTABLE_NCC0, &nccLeaf);  grTexNCCTable(GR_NCCTABLE_NCC0); }
  else if (ncc == 2) { grTexDownloadTable(GR_TEXTABLE_NCC1, &nccShell); grTexNCCTable(GR_NCCTABLE_NCC1); }
  else if (t6666) grTexDownloadTable(GR_TEXTABLE_PALETTE_6666_EXT, &palLeaf);
  else grTexDownloadTable(GR_TEXTABLE_PALETTE, &palSunset);
}

/* extra procedural images for the props */
enum { K_PLANK = 100, K_THATCH, K_DECK, K_POST, K_TOWEL, K_TOWEL2, K_PARASOL, K_FOOT, K_STAR, K_BALL, K_HULL, K_DOOR, K_ROCK };
static void genProp(Img *im, int w, int h, int kind, unsigned salt)
{
  int x, y;
  im->w = w; im->h = h; im->p = (unsigned char *)malloc((size_t)w * h * 4);
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
      unsigned char *q = im->p + ((size_t)y * w + x) * 4;
      float u = (float)x / (float)w, v = (float)y / (float)h, n = fbm(u * 8.0f, v * 8.0f, 8, salt);
      int r = 128, g = 128, b = 128, a = 255;
      switch (kind) {
      case K_PLANK: {            /* weathered horizontal boards, pale blue paint flaking */
        int board = (int)(v * 6.0f);
        float grain = 0.5f + 0.5f * sinf(u * 60.0f + board * 3.1f + n * 6.0f), seam = fmodf(v * 6.0f, 1.0f) < 0.07f ? 0.55f : 1.0f;
        float paint = n > 0.48f ? 1.0f : 0.0f;
        r = (int)(seam * (paint ? 150 + 30 * grain : 140 + 40 * grain)); g = (int)(seam * (paint ? 185 + 25 * grain : 110 + 30 * grain));
        b = (int)(seam * (paint ? 200 + 20 * grain : 80 + 20 * grain));
        break; }
      case K_THATCH: {           /* dry palm thatch: diagonal straw strands */
        float s = 0.5f + 0.5f * sinf((u * 3.0f + v) * 40.0f + n * 9.0f);
        r = (int)(150 + 70 * s * n + 20 * n); g = (int)(115 + 55 * s * n); b = (int)(60 + 30 * s);
        if (fmodf(v * 5.0f, 1.0f) < 0.08f) { r -= 40; g -= 35; b -= 20; }
        break; }
      case K_DECK: {             /* sun-bleached deck planks along u */
        float seam = fmodf(v * 4.0f, 1.0f) < 0.08f ? 0.5f : 1.0f, grain = 0.5f + 0.5f * sinf(u * 90.0f + n * 8.0f);
        r = (int)(seam * (170 + 50 * grain)); g = (int)(seam * (150 + 40 * grain)); b = (int)(seam * (115 + 30 * grain));
        break; }
      case K_POST: {             /* dark wet timber */
        float grain = 0.5f + 0.5f * sinf(u * 30.0f + n * 10.0f);
        r = (int)(80 + 40 * grain); g = (int)(62 + 30 * grain); b = (int)(45 + 20 * grain); a = 255;
        break; }
      case K_TOWEL: case K_TOWEL2: {   /* striped towel with a fringe */
        int stripe = (int)(u * (kind == K_TOWEL ? 7.0f : 5.0f));
        static const int pal[2][4][3] = { { { 230, 70, 60 }, { 250, 245, 230 }, { 40, 120, 200 }, { 250, 245, 230 } },
                                          { { 250, 200, 40 }, { 30, 160, 150 }, { 250, 200, 40 }, { 240, 110, 150 } } };
        const int *c = pal[kind == K_TOWEL2][stripe & 3];
        float fold = 0.85f + 0.15f * n;
        r = (int)(c[0] * fold); g = (int)(c[1] * fold); b = (int)(c[2] * fold);
        a = (v < 0.04f || v > 0.96f) ? (((x >> 1) & 1) ? 255 : 0) : 255;
        break; }
      case K_PARASOL: {          /* eight red/cream gores, colours taken from the palette */
        float ang = atan2f(v - 0.5f, u - 0.5f);
        int gore = (int)floorf((ang + 3.14159f) / 6.28318f * 8.0f) & 1;
        FxU32 c = palSunset.data[gore ? 200 : 140 + (int)(n * 30.0f)];
        r = (int)(c >> 16 & 255); g = (int)(c >> 8 & 255); b = (int)(c & 255);
        break; }
      case K_FOOT: {             /* a footprint: heel + sole + five toes, as alpha */
        float dx = (u - 0.5f) * 2.0f, dy = (v - 0.5f) * 2.0f, d;
        float inside = 0.0f;
        d = (dx * dx) / 0.16f + ((dy - 0.45f) * (dy - 0.45f)) / 0.12f; if (d < 1.0f) inside = 1.0f - d;
        d = (dx * dx) / 0.20f + ((dy + 0.05f) * (dy + 0.05f)) / 0.22f; if (d < 1.0f && 1.0f - d > inside) inside = 1.0f - d;
        { int t; for (t = 0; t < 5; t++) { float tx = -0.36f + 0.18f * (float)t, ty = -0.62f - 0.06f * (float)(t == 1 || t == 2);
            float dd = ((dx - tx) * (dx - tx) + (dy - ty) * (dy - ty)) / (0.01f + 0.004f * (float)(t == 0));
            if (dd < 1.0f && 1.0f - dd > inside) inside = 1.0f - dd; } }
        r = g = b = 255; a = (int)(255 * clampf(inside * 2.5f, 0, 1) * (0.7f + 0.3f * n));
        break; }
      case K_STAR: {             /* five-armed starfish */
        float dx = u - 0.5f, dy = v - 0.5f, rr = sqrtf(dx * dx + dy * dy) * 2.0f, ang = atan2f(dy, dx);
        float arm = 0.35f + 0.6f * powf(fabsf(cosf(ang * 2.5f)), 3.0f);
        float in = rr < arm ? 1.0f : 0.0f;
        r = (int)(235 - 60 * rr + 20 * n); g = (int)(110 - 40 * rr + 30 * n); b = (int)(60 + 20 * n);
        if (((x * 7 + y * 3) % 9) == 0) { r += 15; g += 25; b += 20; }        /* knobbly dots */
        a = in ? 255 : 0;
        break; }
      case K_BALL: {             /* beach ball: coloured segments in a disc */
        float dx = u - 0.5f, dy = v - 0.5f, rr = sqrtf(dx * dx + dy * dy) * 2.0f, ang = atan2f(dy, dx);
        static const int bc[6][3] = { { 230, 40, 40 }, { 250, 250, 245 }, { 40, 110, 230 }, { 250, 250, 245 }, { 250, 210, 30 }, { 250, 250, 245 } };
        int seg = (int)floorf((ang + 3.14159f) / 6.28318f * 6.0f) % 6;
        float shade = clampf(1.15f - rr * 0.6f - (dx + dy) * 0.5f, 0.45f, 1.2f);
        r = (int)(bc[seg][0] * shade); g = (int)(bc[seg][1] * shade); b = (int)(bc[seg][2] * shade);
        if (rr < 0.12f) { r = g = b = (int)(240 * shade); }
        a = rr < 1.0f ? 255 : 0;
        break; }
      case K_HULL: {             /* painted dinghy planks: white topside, red waterline */
        float grain = 0.5f + 0.5f * sinf(u * 50.0f + n * 6.0f);
        if (v < 0.62f) { r = 225 + (int)(20 * grain); g = 225 + (int)(18 * grain); b = 215 + (int)(15 * grain); }
        else if (v < 0.72f) { r = 30; g = 80; b = 150; }
        else { r = 170 + (int)(30 * grain); g = 50; b = 40; }
        if (fmodf(v * 8.0f, 1.0f) < 0.06f) { r -= 50; g -= 50; b -= 50; }
        break; }
      case K_DOOR: {             /* plank door with a rope handle */
        float grain = 0.5f + 0.5f * sinf(v * 70.0f + n * 5.0f), seam = fmodf(u * 5.0f, 1.0f) < 0.06f ? 0.5f : 1.0f;
        r = (int)(seam * (120 + 40 * grain)); g = (int)(seam * (80 + 30 * grain)); b = (int)(seam * (50 + 20 * grain));
        if (fabsf(u - 0.82f) < 0.04f && fabsf(v - 0.5f) < 0.06f) { r = 200; g = 180; b = 120; }
        break; }
      case K_ROCK: {             /* layered sandstone with cracks */
        float layer = 0.5f + 0.5f * sinf(v * 40.0f + n * 8.0f), crack = vnoise(u * 24.0f, v * 24.0f, 24, salt + 9);
        r = (int)(150 + 50 * layer - 40 * n); g = (int)(125 + 40 * layer - 35 * n); b = (int)(100 + 30 * layer - 30 * n);
        if (crack > 0.82f) { r -= 60; g -= 55; b -= 50; }
        if (hashf((unsigned)(x + y * w), salt) > 0.985f) { r += 30; g += 28; b += 25; }
        break; }
      }
      q[0] = (unsigned char)clampi(r, 0, 255); q[1] = (unsigned char)clampi(g, 0, 255);
      q[2] = (unsigned char)clampi(b, 0, 255); q[3] = (unsigned char)clampi(a, 0, 255);
    }
}

/* prop texture: VSA-only format where the board has TEXFMT, a classic fallback otherwise */
static void propTex(Tex *t, int kind, int w, int h, int vsaFmt, int fallbackFmt, unsigned salt)
{
  Img im;
  int fmt = (featDet[F_TEX8888] && vsaFmt >= 0 &&
             !(vsaFmt == GR_TEXFMT_ARGB_8888 && !ON(F_TEX8888)) &&
             !(vsaFmt == GR_TEXFMT_ARGB_CMP_FXT1 && !ON(F_FXT1)) &&
             !(vsaFmt >= GR_TEXFMT_ARGB_CMP_DXT1 && vsaFmt <= GR_TEXFMT_ARGB_CMP_DXT5 && !ON(F_DXT))) ? vsaFmt : fallbackFmt;
  if (highDetail) { w *= 2; h *= 2; }
  if (kind >= K_PLANK) genProp(&im, w, h, kind, salt); else genImg(&im, w, h, kind, salt);
  texCreate(t, &im, fmt, isCompressed(fmt) ? 4 : 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 0, isCompressed(fmt));
  free(im.p);
}

/* ------------------------------------------------------------------------------
** VSA corpus textures (h5_coverage.md section 7): a shell atlas shared by both TMUs under
** unified texture memory, a P8 pinwheel on TMU1 with its own palette, the thatch again on
** TMU1 with its own NCC table, a depth target for the render-to-texture pass, and the
** YUV formats (download only: see the tex_yuv note).
*/
typedef FxBool (FX_CALL *PfnPartialRowX)(GrChipID_t, FxU32, GrLOD_t, GrLOD_t, GrAspectRatio_t, GrTextureFormat_t, FxU32, void *, int, int, int);
typedef void   (FX_CALL *PfnAuxBufferX)(GrBuffer_t);
static PfnPartialRowX   xPartialRow;
static PfnTextureBuffer xTexAux;
static PfnAuxBufferX    xAuxBuffer;
static Tex texAtlas, texPin, texThatch1, texReflZ, texYuv[3];
static GuTexPalette palPin;
static GuNccTable   nccAlt;
static int umaOk, wetExt;
static unsigned char *waterL0;          /* level 0 of the water texture (RGB565), for the animation */

static void loadTexturesExt(void)
{
  Img im, cell;
  int k, x, y;
  /* shell atlas: four 64x64 cells in one 128x128 ARGB4444 level */
  if (ON(F_UMA) || ON(F_TREES)) {
    im.w = im.h = 128; im.p = (unsigned char *)calloc(128 * 128, 4);
    for (k = 0; k < 4; k++) {
      genImg(&cell, 64, 64, K_SHELL, 60u + (unsigned)k);
      for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
          memcpy(im.p + ((size_t)(y + 64 * (k >> 1)) * 128 + (size_t)(x + 64 * (k & 1))) * 4, cell.p + ((size_t)y * 64 + x) * 4, 4);
      free(cell.p);
    }
    texCreate(&texAtlas, &im, GR_TEXFMT_ARGB_4444, 128, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 0, 0);
    free(im.p);
  }
  if (ON(F_UMA) && texAtlas.ok) {
    /* unified texture memory: both TMUs address one range that starts where TMU0's does */
    FxU32 lo0 = grTexMinAddress(GR_TMU0), hi0 = grTexMaxAddress(GR_TMU0), lo1 = grTexMinAddress(GR_TMU1), hi1 = grTexMaxAddress(GR_TMU1);
    FxU32 ulo0, uhi0, ulo1, uhi1;
    grEnable(GR_TEXTURE_UMA_EXT);
    ulo0 = grTexMinAddress(GR_TMU0); uhi0 = grTexMaxAddress(GR_TMU0); ulo1 = grTexMinAddress(GR_TMU1); uhi1 = grTexMaxAddress(GR_TMU1);
    grDisable(GR_TEXTURE_UMA_EXT);
    check("UMA: both TMUs address one range, TMU0 addresses unchanged", ulo0 == ulo1 && uhi0 == uhi1 && ulo0 == lo0 && uhi0 > hi0,
          "split %x..%x/%x..%x uma %x..%x/%x..%x", (unsigned)lo0, (unsigned)hi0, (unsigned)lo1, (unsigned)hi1,
          (unsigned)ulo0, (unsigned)uhi0, (unsigned)ulo1, (unsigned)uhi1);
    check("UMA off: split texture memory restored", grTexMaxAddress(GR_TMU0) == hi0 && grTexMinAddress(GR_TMU1) == lo1 && grTexMaxAddress(GR_TMU1) == hi1, NULL);
    umaOk = ulo0 == ulo1 && ulo0 == lo0;
  }
  if (nTmu > 1 && ON(F_TMU_TABLES)) {
    /* pinwheel: P8 indices written directly (blade g = 100+g, gaps = 200), one level */
    static unsigned char pin[64 * 64];
    static const FxU32 blade[4] = { 0xe83030u, 0x30a0e8u, 0xf0c020u, 0x38b848u };
    for (y = 0; y < 64; y++)
      for (x = 0; x < 64; x++) {
        float ang = atan2f((float)y - 31.5f, (float)x - 31.5f);
        int g = (int)floorf((ang + 3.14159f) / 6.28318f * 8.0f) & 7;
        pin[y * 64 + x] = (unsigned char)((g & 1) ? 200 : 100 + (g >> 1));
      }
    for (k = 0; k < 256; k++) palPin.data[k] = 0x808080u;
    for (k = 0; k < 4; k++) palPin.data[100 + k] = blade[k];
    palPin.data[200] = 0xf8f0dcu;                      /* cream: cut out by the TMU1 chroma range */
    memset(&texPin, 0, sizeof texPin);
    texPin.info.largeLodLog2 = texPin.info.smallLodLog2 = GR_LOD_LOG2_64; texPin.info.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
    texPin.info.format = GR_TEXFMT_P_8; texPin.info.data = pin;
    if (texReserve(&texPin, GR_TMU1, GR_MIPMAPLEVELMASK_BOTH)) grTexDownloadMipMap(GR_TMU1, texPin.addr, GR_MIPMAPLEVELMASK_BOTH, &texPin.info);
    else texAllocFails++;
    /* thatch on TMU1, decoded there through a second (weathered) NCC table */
    genProp(&im, 64, 64, K_THATCH, 22);
    texCreate(&texThatch1, &im, GR_TEXFMT_YIQ_422, 1, GR_TMU1, GR_MIPMAPLEVELMASK_BOTH, 0, 0);
    free(im.p);
    nccAlt = nccShell;
    for (k = 0; k < 16; k++) nccAlt.yRGB[k] = (FxU8)(nccShell.yRGB[k] * 3 / 4);
    packNcc(&nccAlt);
  }
  if (nTmu > 1 && ON(F_RTT_DEPTH) && texRefl.ok && xTexAux && xAuxBuffer) {
    texReflZ.info = texRefl.info;
    if (!texReserve(&texReflZ, GR_TMU1, GR_MIPMAPLEVELMASK_BOTH)) { note(F_RTT_DEPTH, "no TMU1 memory for the depth target"); texAllocFails++; }
  }
  if (ON(F_TEX_YUV)) {
    /* a 64x64 "video frame" (colour bars) in each YUV format: downloaded, never sourced */
    static const int fm[3] = { GR_TEXFMT_YUYV_422, GR_TEXFMT_UYVY_422, GR_TEXFMT_AYUV_444 };
    static unsigned char yuv[64 * 64 * 4];
    int okm = 1;
    for (k = 0; k < 3; k++) {
      FxU32 need;
      for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
          int bar = x >> 3, Y = 235 - bar * 27, U = (bar & 1) ? 90 : 166, Vv = (bar & 2) ? 80 : 176;
          unsigned char *q = yuv + (size_t)(y * 64 + x) * (k == 2 ? 4 : 2);
          if (k == 0) { q[0] = (unsigned char)Y; q[1] = (unsigned char)((x & 1) ? Vv : U); }
          else if (k == 1) { q[0] = (unsigned char)((x & 1) ? Vv : U); q[1] = (unsigned char)Y; }
          else { q[0] = (unsigned char)Vv; q[1] = (unsigned char)U; q[2] = (unsigned char)Y; q[3] = 255; }
        }
      memset(&texYuv[k], 0, sizeof texYuv[k]);
      texYuv[k].info.largeLodLog2 = texYuv[k].info.smallLodLog2 = GR_LOD_LOG2_64; texYuv[k].info.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
      texYuv[k].info.format = (GrTextureFormat_t)fm[k]; texYuv[k].info.data = yuv;
      need = grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &texYuv[k].info);
      if (need < (FxU32)(64 * 64 * (k == 2 ? 4 : 2)) || need > (FxU32)(64 * 64 * (k == 2 ? 4 : 2) + 64)) okm = 0;
      if (texReserve(&texYuv[k], GR_TMU0, GR_MIPMAPLEVELMASK_BOTH)) grTexDownloadMipMap(GR_TMU0, texYuv[k].addr, GR_MIPMAPLEVELMASK_BOTH, &texYuv[k].info);
      else texAllocFails++;
    }
    check("YUYV/UYVY take 2 bytes per texel, AYUV 4", okm, NULL);
    USE(F_TEX_YUV);
  }
  wetExt = ON(F_CMBX_FX) && nTmu > 1 && texDetail.ok && texSandBoth.ok;
  waterL0 = texWater.ok ? (unsigned char *)texWater.info.data : NULL;
}

/* animated texture: a swell crest travels down the water texture, three rows per frame.
   The rows left behind are restored with grTexDownloadMipMapLevelPartial, the crest rows go
   through grTexDownloadMipMapLevelPartialRowExt where the build has it. */
static void animTextures(int frame)
{
  static FxU16 crest[3][128];
  static int prev = -1, rowOk = 1, said;
  static const int lift[3] = { 3, 7, 3 };
  int k, x, r0 = (frame * 3) % 128;
  if (!ON(F_TEX_ANIM) || !waterL0) return;
  if (prev >= 0)
    for (k = 0; k < 3; k++) {
      int r = (prev + k) % 128;
      grTexDownloadMipMapLevelPartial(GR_TMU0, texWater.addr, GR_LOD_LOG2_128, GR_LOD_LOG2_128, GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565,
                                      GR_MIPMAPLEVELMASK_BOTH, waterL0 + (size_t)r * 128 * 2, r, r);
    }
  for (k = 0; k < 3; k++) {
    int r = (r0 + k) % 128;
    const FxU16 *src = (const FxU16 *)(waterL0 + (size_t)r * 128 * 2);
    for (x = 0; x < 128; x++) {
      int rr = (src[x] >> 11) + lift[k], gg = (src[x] >> 5 & 63) + 2 * lift[k], bb = (src[x] & 31) + lift[k];
      crest[k][x] = (FxU16)((clampi(rr, 0, 31) << 11) | (clampi(gg, 0, 63) << 5) | clampi(bb, 0, 31));
    }
    if (xPartialRow)
      rowOk &= xPartialRow(GR_TMU0, texWater.addr, GR_LOD_LOG2_128, GR_LOD_LOG2_128, GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565,
                           GR_MIPMAPLEVELMASK_BOTH, crest[k], r, 0, 127) != 0;
    else
      grTexDownloadMipMapLevelPartial(GR_TMU0, texWater.addr, GR_LOD_LOG2_128, GR_LOD_LOG2_128, GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565,
                                      GR_MIPMAPLEVELMASK_BOTH, crest[k], r, r);
  }
  prev = r0;
  if (!said && frame >= 2) { said = 1; check("animated texture: partial-row downloads accepted", rowOk, NULL); }
  if (!xPartialRow) note(F_TEX_ANIM, "no grTexDownloadMipMapLevelPartialRowExt: whole rows with grTexDownloadMipMapLevelPartial");
  USE(F_TEX_ANIM);
}

static void loadTextures(void)
{
  Img im;
  int i, s;
  FxU32 maxTex = 256;

  grGet(GR_MAX_TEXTURE_SIZE, 4, (FxI32 *)&maxTex);
  int detailScale = highDetail ? 2 : 1;
  int sandSize = highDetail && maxTex >= 512 ? 512 : 256;
  genImg(&im, 128, 128, K_WATER, 11);  texCreate(&texWater, &im, GR_TEXFMT_RGB_565, 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 0, 0); free(im.p);
  genImg(&im, 64 * detailScale, 64 * detailScale, K_FOAM, 12); texCreate(&texFoam, &im, GR_TEXFMT_ALPHA_INTENSITY_88, 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 0, 0); free(im.p);
  /* Preserve the split LOD chains for single-pass trilinear minification. */
  genImg(&im, sandSize, sandSize, K_SAND, 13);
  texCreate(&texSandBoth, &im, GR_TEXFMT_RGB_565, 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 0, 0);
  texCreate(&texSandEven, &im, GR_TEXFMT_RGB_565, 1, GR_TMU0, GR_MIPMAPLEVELMASK_EVEN, 0, 0);
  if (nTmu > 1) texCreate(&texSandOdd, &im, GR_TEXFMT_RGB_565, 1, GR_TMU1, GR_MIPMAPLEVELMASK_ODD, 0, 0);
  free(im.p);
  genImg(&im, 64 * detailScale, 32 * detailScale, K_CLOUD, 14); texCreate(&texCloudA, &im, GR_TEXFMT_ARGB_4444, 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 0, 0); free(im.p);
  genImg(&im, 64 * detailScale, 32 * detailScale, K_CLOUD, 15); texCreate(&texCloudB, &im, GR_TEXFMT_AP_88, 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 0, 1); free(im.p);
  genImg(&im, 64 * detailScale, 64 * detailScale, K_LEAF, 16);
  texCreate(&texLeafP8, &im, cinematic ? GR_TEXFMT_ARGB_1555 : GR_TEXFMT_P_8,
            1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, !cinematic, 0);
  if (!cinematic)
    texCreate(&texLeafNcc, &im, GR_TEXFMT_AYIQ_8422, 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, 1, 0);
  free(im.p);
  if (nTmu > 1) {
    genImg(&im, 64, 64, K_SPARKLE, 17); texCreate(&texSparkle, &im, GR_TEXFMT_ALPHA_INTENSITY_44, 1, GR_TMU1, GR_MIPMAPLEVELMASK_BOTH, 0, 0); free(im.p);
    genImg(&im, 64, 64, K_DETAIL, 18);  texCreate(&texDetail, &im, GR_TEXFMT_INTENSITY_8, 1, GR_TMU1, GR_MIPMAPLEVELMASK_BOTH, 0, 1); free(im.p);
    if (ON(F_TEXBUFFER)) {
      /* render target for the tide pool / sea reflection: frame-buffer pixel format, 128x128 */
      texRefl.info.largeLodLog2 = texRefl.info.smallLodLog2 = GR_LOD_LOG2_128;
      texRefl.info.aspectRatioLog2 = GR_ASPECT_LOG2_1x1;
      texRefl.info.format = (GrTextureFormat_t)(pix32 ? GR_TEXFMT_ARGB_8888 : pix15 ? GR_TEXFMT_ARGB_1555 : GR_TEXFMT_RGB_565);
      if (!texReserve(&texRefl, GR_TMU1, GR_MIPMAPLEVELMASK_BOTH)) { note(F_TEXBUFFER, "no TMU1 memory for the 128x128 target"); texAllocFails++; }
    }
  }
  /* props */
  propTex(&texWall,    K_PLANK,   64, 64, cinematic ? -1 : GR_TEXFMT_ARGB_8888,
          cinematic ? GR_TEXFMT_RGB_565 : GR_TEXFMT_ARGB_1555, 21);
  propTex(&texThatch,  K_THATCH,  64, 64, -1, cinematic ? GR_TEXFMT_RGB_565 : GR_TEXFMT_YIQ_422, 22);
  propTex(&texDeck,    K_DECK,    128, 32, -1,                      GR_TEXFMT_RGB_332,   23);
  propTex(&texPost,    K_POST,    32, 32, -1,                       GR_TEXFMT_ARGB_1555, 24);
  int towelFormat = cinematic ? GR_TEXFMT_ARGB_4444 : GR_TEXFMT_ARGB_8332;
  propTex(&texTowelA,  K_TOWEL,   64, 32, -1, towelFormat, 25);
  propTex(&texTowelB,  K_TOWEL2,  64, 32, cinematic ? -1 : GR_TEXFMT_ARGB_CMP_DXT2, towelFormat, 26);
  propTex(&texTowelC,  K_TOWEL,   64, 32, cinematic ? -1 : GR_TEXFMT_ARGB_CMP_DXT4, towelFormat, 27);
  propTex(&texParasol, K_PARASOL, 64, 64, -1,                       GR_TEXFMT_P_8,       28);
  propTex(&texFoot,    K_FOOT,    32, 32, -1,                       GR_TEXFMT_ALPHA_8,   29);
  propTex(&texStar,    K_STAR,    32, 32, cinematic ? -1 : GR_TEXFMT_ARGB_CMP_DXT5, GR_TEXFMT_ARGB_4444, 30);
  propTex(&texBall,    K_BALL,    32, 32, cinematic ? -1 : GR_TEXFMT_ARGB_CMP_DXT3, GR_TEXFMT_ARGB_4444, 31);
  propTex(&texHull,    K_HULL,    64, 32, cinematic ? -1 : GR_TEXFMT_ARGB_CMP_DXT1, GR_TEXFMT_ARGB_1555, 32);
  propTex(&texDoor,    K_DOOR,    32, 64, cinematic ? -1 : GR_TEXFMT_ARGB_CMP_FXT1,
          cinematic ? GR_TEXFMT_RGB_565 : GR_TEXFMT_RGB_332, 33);
  loadTexturesExt();
  /* the rock outcrop: try the board maximum, halve until it fits */
  if (ON(F_BIGTEX)) {
    for (s = cinematic && maxTex > 256 ? 256 : (int)maxTex; s >= 256; s /= 2) {
      Tex probe; int tmu;
      memset(&probe, 0, sizeof probe);
      probe.info.largeLodLog2 = probe.info.smallLodLog2 = (GrLOD_t)lodOf(s);
      probe.info.aspectRatioLog2 = GR_ASPECT_LOG2_1x1; probe.info.format = GR_TEXFMT_RGB_565;
      for (tmu = 0; tmu < nTmu; tmu++) {
        FxU32 need = grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &probe.info);
        FxU32 a = (texCur[tmu] + texAlign - 1) & ~(texAlign - 1);
        if (a + need <= texEnd[tmu]) {
          genProp(&im, s, s, K_ROCK, 19);
          texCreate(&texBig, &im, GR_TEXFMT_RGB_565, s, tmu, GR_MIPMAPLEVELMASK_BOTH, 0, 0);
          free(im.p);
          break;
        }
      }
      if (texBig.ok) break;
    }
    bigSize = texBig.ok ? (1 << texBig.info.largeLodLog2) : 0;
    if (texBig.ok && bigSize < (int)maxTex) {
      static char m[96]; snprintf(m, sizeof m, "%dx%d: %s", bigSize, bigSize,
                                 cinematic ? "cinematic rock material" : "TMU memory limit"); note(F_BIGTEX, m);
    }
  }
  /* -g: the plain format gallery, one 32x32 texture per format, downloaded level by level */
  if (galleryMode)
    for (i = 0; i < NSHELL; i++) {
      Shell *sh = &shells[i];
      if (sh->vsaOnly && !featDet[F_TEX8888]) continue;          /* TEXFMT */
      if (sh->fmt == GR_TEXFMT_ARGB_8888 && !ON(F_TEX8888)) continue;
      if (sh->fmt == GR_TEXFMT_ARGB_CMP_FXT1 && !ON(F_FXT1)) continue;
      if (sh->fmt >= GR_TEXFMT_ARGB_CMP_DXT1 && sh->fmt <= GR_TEXFMT_ARGB_CMP_DXT5 && !ON(F_DXT)) continue;
      genImg(&im, 32, 32, K_SHELL, 40u + (unsigned)i);
      texCreate(&sh->tex, &im, sh->fmt, isCompressed(sh->fmt) ? 4 : 1, GR_TMU0, GR_MIPMAPLEVELMASK_BOTH, sh->t6666, 1);
      free(im.p);
    }
}

/* ------------------------------------------------------------------------------
** scene geometry: an island heightfield with an irregular coast, a sandstone outcrop on
** the shore, a plateau with palms, a hut, a jetty, a dinghy and beach things.
*/
#define ROCK_X  (-5.2f)
#define ROCK_Z  (11.2f)
static float coastR(float a)
{
  return 15.5f + 2.2f * sinf(a * 2.0f + 0.7f) + 1.2f * sinf(a * 5.0f + 1.3f) + 0.5f * sinf(a * 9.0f + 2.0f);
}
static float rockMask(float x, float z)
{
  float dx = x - ROCK_X, dz = z - ROCK_Z, d = sqrtf(dx * dx * 0.7f + dz * dz) / 5.0f;
  return clampf(1.0f - d, 0.0f, 1.0f);
}
static float islandH(float x, float z)
{
  float r = sqrtf(x * x + z * z), a = atan2f(z, x), t = r / coastR(a), h, rm;
  if (t <= 1.0f) h = 4.2f * powf(smoothf(clampf((1.0f - t) / 0.62f, 0.0f, 1.0f)), 1.35f);
  else h = -(t - 1.0f) * 14.0f;
  h += 0.45f * (fbm(x * 0.08f + 3.0f, z * 0.08f + 7.0f, 64, 5) - 0.5f) * clampf((1.05f - t) * 3.0f, 0.0f, 1.0f);
  rm = rockMask(x, z);
  if (rm > 0.0f) h += 4.2f * smoothf(clampf(rm * 1.6f, 0.0f, 1.0f)) * (0.7f + 0.6f * vnoise(x * 0.7f, z * 0.7f, 64, 9));
  return h;
}
static float waveAmp(float x, float z)
{
  float r = sqrtf(x * x + z * z), c = coastR(atan2f(z, x));
  return clampf((r - c) / 12.0f, 0.12f, 1.0f);
}
typedef struct { float h, dx, dz; } SeaWave;

/* Shoreward wave train. The same phase drives the surface and the foam strips. */
static const float breakerPitch = 3.4f;
static float breakerTravel(float angle)
{
  return 1.55f * tNow + 0.48f * sinf(3.0f * angle) + 0.20f * sinf(7.0f * angle);
}

/* Directional swells and their derivatives share phase with the reflected-light pass. */
static SeaWave seaWave(float x, float z)
{
  static const float waves[][4] = {
    {0.29f, 0.11f, 1.55f, 0.19f}, {-0.17f, 0.24f, 1.20f, 0.13f},
    {0.63f, 0.36f, 2.85f, 0.055f}
  };
  SeaWave w = { -0.05f, 0, 0 };
  float amp = waveAmp(x, z);
  for (int i = 0; i < 3; i++) {
    float p = waves[i][0] * x + waves[i][1] * z - waves[i][2] * tNow;
    float a = amp * waves[i][3], s = sinf(p), c = cosf(p);
    float steep = highDetail ? 0.36f : 0.22f;
    float slope = a * c * (1.0f + 2.0f * steep * s);
    w.h += a * (s + steep * (s * s - 0.5f));
    w.dx += slope * waves[i][0]; w.dz += slope * waves[i][1];
  }
  if (highDetail) {
    float radius = sqrtf(x * x + z * z), angle = atan2f(z, x);
    float shore = radius - coastR(angle);
    float envelope = smoothf(clampf((shore + 0.4f) / 1.6f, 0, 1)) *
                     smoothf(clampf((10.0f - shore) / 5.0f, 0, 1));
    if (envelope > 0.0f && radius > 0.0f) {
      float phase = 6.2831853f / breakerPitch * (shore + breakerTravel(angle));
      float crest = 0.5f + 0.5f * cosf(phase);
      float slope = -0.24f * envelope * 3.0f * crest * crest * sinf(phase) * (3.14159265f / breakerPitch);
      w.h += 0.24f * envelope * (crest * crest * crest - 0.3125f);
      w.dx += slope * x / radius;
      w.dz += slope * z / radius;
    }
  }
  return w;
}
static float waveH(float x, float z) { return seaWave(x, z).h; }

static void sunScreen(float *sx, float *sy);

static void drawSky(void)
{
  V v[2 * 9];
  enum { BANDS = 8 };
  int i, j;
  float horizon, sx, sy;
  V hv;
  float far[3];
  if (!ON(F_SKY)) return;
  far[0] = cam.eye[0] + cam.fh[0] * 4000.0f; far[1] = 0.0f; far[2] = cam.eye[2] + cam.fh[2] * 4000.0f;
  proj(far, &hv);
  horizon = clampf(hv.y, 8.0f, scrH - 8.0f);
  sunScreen(&sx, &sy);
  if (ON(F_CLIP)) { grClipWindow(0, 0, (FxU32)scrW, (FxU32)clampf(horizon + 24.0f, 1, scrH)); USE(F_CLIP); }
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOff(); blendOff(); grFogMode(GR_FOG_DISABLE);
  for (j = 0; j < BANDS; j++) {
    for (i = 0; i < 9; i++) {
      float x = scrW * (float)i / 8.0f;
      for (int row = 0; row < 2; row++) {
        float t = (float)(j + row) / BANDS, y = t * horizon;
        float dx = (x - sx) / scrW, dy = (y - sy) / scrH;
        float haze = powf(t, 2.0f);
        float warm = expf(-5.0f * dx * dx - 9.0f * dy * dy);
        V *p = &v[2 * i + row];
        scr(p, x, y + (j + row == BANDS ? 24.0f : 0.0f));
        col(p, 30 + 155 * haze + 45 * warm, 75 + 112 * haze + 26 * warm,
             150 + 54 * haze + 4 * warm, 255);
      }
    }
    grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 18, v, sizeof(V));
    statTris += 16;
  }
  USE(F_SKY);
  grClipWindow(0, 0, (FxU32)scrW, (FxU32)scrH);
}

/* sun direction: fixed relative to the camera heading, so it stays in view */
static void sunDir(float *d)
{
  d[0] = sunD[0]; d[1] = sunD[1]; d[2] = sunD[2];
}
static void sunScreen(float *sx, float *sy)
{
  float p[3], d[3];
  V v;
  sunDir(d);
  p[0] = cam.eye[0] + d[0] * 3000.0f; p[1] = cam.eye[1] + d[1] * 3000.0f; p[2] = cam.eye[2] + d[2] * 3000.0f;
  proj(p, &v);
  *sx = v.x; *sy = v.y;
}

static void drawSun(void)
{
  V fan[18], c, pts[24], a, b;
  float sx, sy, r = scrH * 0.035f;
  int i, ring;
  if (!ON(F_SUN)) return;
  sunScreen(&sx, &sy);
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOff(); grFogMode(GR_FOG_DISABLE);
  if (ON(F_BLEND_EXT) && X.blend) {
    X.blend(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_OP_ADD, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_OP_ADD);
    USE(F_BLEND_EXT);
  } else grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE);    /* additive */
  for (ring = 2; ring >= 0; ring--) {        /* halo (wide, faint) then disc (small, bright) */
    float rr = r * (ring == 2 ? 5.0f : ring == 1 ? 2.2f : 1.0f);
    void *ptr[18];
    scr(&c, sx, sy);
    if (ring == 2) col(&c, 70, 60, 40, 255); else if (ring == 1) col(&c, 120, 100, 60, 255); else col(&c, 255, 250, 225, 255);
    fan[0] = c; ptr[0] = &fan[0];
    for (i = 0; i <= 16; i++) {
      float an = 6.2831853f * (float)i / 16.0f;
      scr(&fan[i + 1], sx + rr * cosf(an), sy + rr * sinf(an));
      if (ring == 0) col(&fan[i + 1], 230, 210, 160, 255); else col(&fan[i + 1], 0, 0, 0, 255);
      ptr[i + 1] = &fan[i + 1];
    }
    grDrawVertexArray(GR_TRIANGLE_FAN, 18, ptr);
    statTris += 16;
  }
  /* glow: a faint twinkling ring of points and short rays as lines */
  for (i = 0; i < 24; i++) {
    float an = 6.2831853f * (float)i / 24.0f + tNow * 0.2f, rr = r * (1.5f + 0.12f * sinf(tNow * 3.0f + (float)i));
    scr(&pts[i], sx + rr * cosf(an), sy + rr * sinf(an)); col(&pts[i], 40, 34, 20, 255);
    grDrawPoint(&pts[i]); statPoints++;
  }
  for (i = 0; i < 12; i++) {
    float an = 6.2831853f * (float)i / 12.0f - tNow * 0.1f;
    scr(&a, sx + r * 1.2f * cosf(an), sy + r * 1.2f * sinf(an)); col(&a, 34, 30, 18, 255);
    scr(&b, sx + r * 3.2f * cosf(an), sy + r * 3.2f * sinf(an)); col(&b, 0, 0, 0, 255);
    grDrawLine(&a, &b); statLines++;
  }
  blendOff();
  USE(F_SUN);
}

static void drawClouds(void)
{
  int k;
  if (!ON(F_CLOUDS) || !texCloudA.ok || !texCloudB.ok) return;
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOff(); blendAlpha(); grFogMode(GR_FOG_DISABLE);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_DISABLE, FXFALSE);
  if (cinematic) grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  else if (ON(F_MIRROR)) { grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_MIRROR_EXT, GR_TEXTURECLAMP_CLAMP); USE(F_MIRROR); }
  else grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_CLAMP);
  setTable(0, 0);
  if (ON(F_DITHER)) { grDitherMode(GR_DITHER_2x2); USE(F_DITHER); }    /* clouds 2x2, the rest 4x4 */
  for (k = 0; k < 8; k++) {
    const Tex *t = (k & 1) ? &texCloudB : &texCloudA;
    int far = k < 4;
    float w = scrW * ((far ? 0.30f : 0.36f) + 0.10f * hashf((unsigned)k, seedBase));
    float h = w * (far ? 0.15f : 0.34f);
    float x = fmodf(scrW * 0.34f * (float)k + tNow * (far ? 4.0f : 11.0f) + 37.0f, scrW + w) - w;
    float y = scrH * (far ? 0.32f + 0.05f * hashf((unsigned)k, 3) : 0.015f + 0.12f * hashf((unsigned)k, 7));
    float scroll = cinematic ? 0.0f : tNow * (far ? 1.0f : 3.0f);
    V a, b, c, d;
    source(t);
    scr(&a, x, y); scr(&b, x + w, y); scr(&c, x + w, y + h); scr(&d, x, y + h);
    col(&a, 247, 244, 255, far ? 110 : 230); col(&b, 255, 243, 224, far ? 125 : 240);
    col(&c, 232, 223, 230, far ? 85 : 205); col(&d, 202, 214, 239, far ? 75 : 195);
    st0(&a, scroll, 0); st0(&b, scroll + 255.0f, 0); st0(&c, scroll + 255.0f, 127.0f); st0(&d, scroll, 127.0f);
    quad(&a, &b, &c, &d);
  }
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  if (ON(F_DITHER)) grDitherMode(GR_DITHER_4x4);
  blendOff();
  USE(F_CLOUDS);
}

/* fog: a table on the per-vertex fog coordinate (FOGCOORD), else on Q */
static GrFog_t fogTable[128];
static int     fogEntries = 64;
static void fogOn(int on)
{
  if (!ON(F_FOG) || !on) { grFogMode(GR_FOG_DISABLE); return; }
  grFogMode(fogCoordNow ? GR_FOG_WITH_TABLE_ON_FOGCOORD_EXT : GR_FOG_WITH_TABLE_ON_Q);
  USE(F_FOG);
}

/* world-space helpers */
static void wv(V *v, float x, float y, float z, float r, float g, float b, float a, float s, float t)
{
  float p[3];
  memset(v, 0, sizeof *v);
  p[0] = x; p[1] = y; p[2] = z;
  proj(p, v); col(v, r, g, b, a); st0(v, s, t);
}

/* distant islands on the horizon (fogged, Gouraud): a fan cap and a strip skirt each,
   placed relative to the heading so they stay on the horizon */
static void drawDistant(void)
{
  int k, i, two = ON(F_FOG) && ON(F_FOG2);
  /* Fog on Q here every frame (the rest of a Z frame fogs on the fog coordinate).  The
     last (rightmost) island is fogged in two passes: colour * (1 - f) with GR_FOG_ADD2, then fog
     colour * f added with GR_FOG_MULT2 at equal depth.  The sum is the one-pass fog
     equation, so it must look like its neighbours. */
  grGlideSetVertexLayout(two ? layoutSet[0][0] : layoutCol);
  ccIterated(); depthOn(1); blendOff(); fogOn(1);
  if (two) { grFogMode(GR_FOG_WITH_TABLE_ON_Q); USE(F_FOG2); }
  for (k = 0; k < 3; k++) {
    float hd = -0.62f + 0.5f * (float)k, dist = 620.0f + 110.0f * (float)k, rad = 110.0f + 40.0f * (float)(k & 1), hgt = 9.0f + 5.0f * (float)(k == 1);
    float cx = cam.eye[0] + (cam.fh[0] * cosf(hd) + cam.rh[0] * sinf(hd)) * dist;
    float cz = cam.eye[2] + (cam.fh[2] * cosf(hd) + cam.rh[2] * sinf(hd)) * dist;
    V fan[22], skirt[42];
    void *ptr[22];
    wv(&fan[0], cx, hgt, cz, 92, 118, 104, 255, 0, 0); ptr[0] = &fan[0];
    for (i = 0; i <= 20; i++) {
      float an = 6.2831853f * (float)i / 20.0f, wob = 0.75f + 0.3f * hashf((unsigned)(i % 20), 50u + (unsigned)k);
      float rx = rad * wob, rz = rad * 0.45f * wob;
      wv(&fan[i + 1], cx + 0.45f * rx * cosf(an), hgt * (0.72f + 0.2f * wob - 0.15f), cz + 0.45f * rz * sinf(an), 100, 126, 110, 255, 0, 0);
      ptr[i + 1] = &fan[i + 1];
      skirt[2 * i] = fan[i + 1];
      wv(&skirt[2 * i + 1], cx + rx * cosf(an), -1.0f, cz + rz * sinf(an), 128, 146, 128, 255, 0, 0);
    }
    grDrawVertexArray(GR_TRIANGLE_FAN, 22, ptr);
    grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 42, skirt, sizeof(V));
    statTris += 20 + 40;
    if (two && k == 1) grFogMode(GR_FOG_WITH_TABLE_ON_Q | GR_FOG_ADD2);      /* first pass of island 2 */
    if (two && k == 2) {
      grFogMode(GR_FOG_WITH_TABLE_ON_Q | GR_FOG_MULT2);
      grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE);
      grDepthMask(FXFALSE);
      grDrawVertexArray(GR_TRIANGLE_FAN, 22, ptr);
      grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 42, skirt, sizeof(V));
      statTris += 20 + 40;
      blendOff(); grDepthMask(FXTRUE);
      grFogMode(GR_FOG_WITH_TABLE_ON_Q);
    }
  }
  if (two) grFogMode(GR_FOG_DISABLE);
}

/* sea: view-aligned wave grid; even row pairs as pointer strips, odd pairs as contiguous strips */
#define SEA_COLS 57
#define SEA_ROWS 45
static V seaV[SEA_ROWS][SEA_COLS];

static void seaCombine(int multi)
{
  if (multi && ON(F_COMBINE_EXT) && X.texColorCombine) {
    X.constColor(GR_TMU1, 0xffe0f0ffu);
    X.texColorCombine(GR_TMU1, GR_CMBX_LOCAL_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                      GR_CMBX_TMU_CCOLOR, FXFALSE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.texAlphaCombine(GR_TMU1, GR_CMBX_LOCAL_TEXTURE_ALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                      GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.texColorCombine(GR_TMU0, GR_CMBX_OTHER_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_LOCAL_TEXTURE_RGB, GR_FUNC_MODE_ZERO,
                      GR_CMBX_OTHER_TEXTURE_ALPHA, FXFALSE, GR_CMBX_B, FXFALSE, 0, FXFALSE);
    X.texAlphaCombine(GR_TMU0, GR_CMBX_LOCAL_TEXTURE_ALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                      GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.colorCombine(GR_CMBX_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                   GR_CMBX_ITRGB, FXFALSE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.alphaCombine(GR_CMBX_ITALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                   GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    USE(F_COMBINE_EXT);
    return;
  }
  if (multi) {
    tmuPass(GR_TMU1);
    grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_OTHER_ALPHA,
                 GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
  } else { tmu1Off(); tmuPass(GR_TMU0); }
  grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
  grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE);
}
static void classicCombine(void)
{
  if (featUsed[F_COMBINE_EXT]) {
    grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
    if (nTmu > 1) tmu1Off();
  }
}

static void drawSea(void)
{
  int i, j, multi;
  int cols = highDetail ? SEA_COLS : 37, rows = highDetail ? SEA_ROWS : 34;
  float zr0 = 1.5f, zr1 = 1400.0f;
  if (!ON(F_SEA_VA)) return;
  /* Keep the first sea row below the viewport and inside the setup range,
     even when the larger focal length projects it far below the screen. */
  if (scrW >= 800.0f)
    zr0 = fmaxf(zr0, focal * (cam.eye[1] + 0.6f) / (1900.0f - scrH * 0.5f));
  multi = ON(F_SEA_MT) && nTmu > 1 && texSparkle.ok;
  for (j = 0; j < rows; j++) {
    float zf = zr0 * powf(zr1 / zr0, (float)j / (float)(rows - 1));
    for (i = 0; i < cols; i++) {
      float xf = zf * (1.7f * ((float)i / (float)(cols - 1)) - 0.85f) * (scrW / scrH);
      float p[3], r, shallow, normal[3], view[3], halfVec[3], grazing, glint, crest;
      SeaWave wave;
      V *v = &seaV[j][i];
      p[0] = cam.eye[0] + cam.rh[0] * xf + cam.fh[0] * zf;
      p[2] = cam.eye[2] + cam.rh[2] * xf + cam.fh[2] * zf;
      wave = seaWave(p[0], p[2]); p[1] = wave.h;
      memset(v, 0, sizeof *v);
      proj(p, v);
      r = sqrtf(p[0] * p[0] + p[2] * p[2]);
      shallow = clampf(1.0f - (r - coastR(atan2f(p[2], p[0]))) / 14.0f, 0.0f, 1.0f);
      normal[0] = -wave.dx; normal[1] = 1.0f; normal[2] = -wave.dz;
      norm3(normal);
      for (int axis = 0; axis < 3; axis++) view[axis] = cam.eye[axis] - p[axis];
      norm3(view);
      for (int axis = 0; axis < 3; axis++) halfVec[axis] = view[axis] + sunD[axis];
      norm3(halfVec);
      grazing = 1.0f - clampf(dot3(view, normal), 0, 1);
      float fresnel = grazing * grazing * grazing * grazing;
      if (highDetail) {
        /* Water reflects about 2% straight on, rising sharply at grazing angles.
           More of the seabed colour comes through in thin water at the shore. */
        fresnel = 0.02f + 0.98f * fresnel * grazing;
        if (shallow > 0.0f)
          shallow = expf(-0.72f * fmaxf(p[1] - islandH(p[0], p[2]), 0.0f)) * shallow;
      }
      grazing = fresnel;
      glint = powf(clampf(dot3(normal, halfVec), 0, 1), 48.0f) * grazing;
      crest = clampf((wave.h + 0.22f) * 2.0f, 0, 1);
      // Gouraud light over the mipmapped base, with a warmer grazing reflection.
      col(v, 35 + 52 * shallow + 62 * grazing + 76 * glint + 20 * crest,
             fminf(255.0f, 80 + 98 * shallow + 42 * grazing + 32 * glint + 16 * crest),
            120 + 54 * shallow + 37 * grazing + 24 * glint + 15 * crest, 255);
      st0(v, (p[0] * 0.94f + p[2] * 0.34f) * 10.0f - tNow * 8.0f,
             (p[2] * 0.94f - p[0] * 0.34f) * 10.0f + tNow * 2.0f);
      st1(v, p[0] * 6.0f + p[2] * 1.2f + tNow * 11.0f, p[2] * 7.0f - tNow * 4.0f);
    }
  }
  grGlideSetVertexLayout(multi ? layoutTex2 : layoutTex);
  seaCombine(multi);
  source(&texWater);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  if (multi) {
    source(&texSparkle);
    grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    grTexMipMapMode(GR_TMU1, GR_MIPMAP_NEAREST, FXFALSE);
    grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
    USE(F_SEA_MT);
  }
  depthOn(1); blendOff(); fogOn(1);
  for (j = 0; j + 1 < rows; j++) {
    if ((j & 1) == 0) {
      void *ptr[2 * SEA_COLS];
      for (i = 0; i < cols; i++) { ptr[2 * i] = &seaV[j + 1][i]; ptr[2 * i + 1] = &seaV[j][i]; }
      grDrawVertexArray(GR_TRIANGLE_STRIP, 2 * cols, ptr);
    } else {
      V strip[2 * SEA_COLS];
      for (i = 0; i < cols; i++) { strip[2 * i] = seaV[j + 1][i]; strip[2 * i + 1] = seaV[j][i]; }
      grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 2 * cols, strip, sizeof(V));
    }
    statTris += 2 * cols - 2;
  }
  classicCombine();
  USE(F_SEA_VA);
}

/* the island heightfield */
#define HF_N_MAX 96
#define HF_HALF 26.0f
static V   hfV[HF_N_MAX + 1][HF_N_MAX + 1];
static float hfH[HF_N_MAX + 1][HF_N_MAX + 1], hfRock[HF_N_MAX + 1][HF_N_MAX + 1];
static V   triBuf[12 * HF_N_MAX * HF_N_MAX];
static int hfSize = 64;

static float terrainCoord(int index)
{
  return -HF_HALF + 2.0f * HF_HALF * (float)index / hfSize;
}

static float sunLight(float nx, float ny, float nz)
{
  float facing = (nx * sunD[0] + ny * sunD[1] + nz * sunD[2]) /
                 sqrtf(nx * nx + ny * ny + nz * nz);
  return 0.62f + 0.38f * clampf(facing, 0.0f, 1.0f);
}

static void hfBuild(void)
{
  int i, j;
  for (j = 0; j <= hfSize; j++)
    for (i = 0; i <= hfSize; i++) {
      float x = terrainCoord(i), z = terrainCoord(j);
      float h = islandH(x, z), rm = rockMask(x, z), p[3], wet, grass, cr, cg, cb, shade;
      V *v = &hfV[j][i];
      hfH[j][i] = h; hfRock[j][i] = rm;
      memset(v, 0, sizeof *v);
      p[0] = x; p[1] = h; p[2] = z;
      proj(p, v);
      /* slope shading from the height gradient, lit from the sun side */
      if (highDetail) {
        float dx = islandH(x - 0.3f, z) - islandH(x + 0.3f, z);
        float dz = islandH(x, z - 0.3f) - islandH(x, z + 0.3f);
        shade = sunLight(dx, 0.6f, dz);
      } else {
        shade = clampf(0.82f + 0.12f * (islandH(x - 0.6f, z - 0.4f) - islandH(x + 0.6f, z + 0.4f)), 0.55f, 1.1f);
      }
      wet = clampf(1.0f - (h - 0.05f) / 0.55f, 0.0f, 1.0f);
      grass = clampf((h - 3.2f) / 0.8f, 0.0f, 1.0f) * (1.0f - clampf(rm * 3.0f, 0, 1));
      cr = 255 * (1.0f - 0.38f * wet) * (1.0f - 0.25f * grass); cg = 245 * (1.0f - 0.36f * wet) * (1.0f - 0.05f * grass); cb = 225 * (1.0f - 0.32f * wet) * (1.0f - 0.35f * grass);
      if (h < 0.0f) { float d = clampf(-h / 0.8f, 0, 1); cr += (60 - cr) * d; cg += (150 - cg) * d; cb += (160 - cb) * d; }
      col(v, clampf(cr * shade, 0, 255), clampf(cg * shade, 0, 255), clampf(cb * shade, 0, 255), 255);
      st0(v, x * 36.0f, z * 36.0f);                      /* sand: 256 texels per ~7 units */
      st1(v, x * 36.0f, z * 36.0f);
    }
}

/* Intersect a terrain edge with sea level before projection. Projecting the
   intersection preserves depth and perspective-correct texture coordinates. */
static V shoreVertex(const V *a, const V *b, float ha, float hb)
{
  float t = ha / (ha - hb);
  float ax = a->s0 / a->oow, az = a->t0 / a->oow;
  float s = ax + t * (b->s0 / b->oow - ax);
  float u = az + t * (b->t0 / b->oow - az);
  V v;
  wv(&v, s / 36.0f, 0.0f, u / 36.0f,
     a->r + t * (b->r - a->r), a->g + t * (b->g - a->g),
     a->b + t * (b->b - a->b), 255, s, u);
  st1(&v, s, u);
  return v;
}

static int terrainTriangle(V *out, const V *a, const V *b, const V *c, float ha, float hb, float hc)
{
  if (!cinematic || (ha >= 0 && hb >= 0 && hc >= 0)) {
    out[0] = *a; out[1] = *b; out[2] = *c;
    return 3;
  }
  const V *vertices[3] = {a, b, c};
  float height[3] = {ha, hb, hc};
  V visible[4];
  int count = 0, written = 0;
  for (int i = 0; i < 3; i++) {
    int j = (i + 1) % 3;
    if (height[i] >= 0) visible[count++] = *vertices[i];
    if ((height[i] < 0 && height[j] > 0) || (height[i] > 0 && height[j] < 0))
      visible[count++] = shoreVertex(vertices[i], vertices[j], height[i], height[j]);
  }
  for (int i = 1; i + 1 < count; i++) {
    out[written++] = visible[0];
    out[written++] = visible[i];
    out[written++] = visible[i + 1];
  }
  return written;
}

/* cells by class: 0 = near sand (detail), 1 = far sand (trilinear), 2 = rock */
static int hfCells(int cls, V *out)
{
  int i, j, n = 0;
  for (j = 0; j < hfSize; j++)
    for (i = 0; i < hfSize; i++) {
      float hmax = hfH[j][i], rk = hfRock[j][i], d, cx, cz;
      int c;
      if (hfH[j][i + 1] > hmax) hmax = hfH[j][i + 1];
      if (hfH[j + 1][i] > hmax) hmax = hfH[j + 1][i];
      if (hfH[j + 1][i + 1] > hmax) hmax = hfH[j + 1][i + 1];
      if (hmax < (cinematic ? 0.0f : -1.6f)) continue;
      if (hfRock[j][i + 1] > rk) rk = hfRock[j][i + 1];
      if (hfRock[j + 1][i] > rk) rk = hfRock[j + 1][i];
      if (hfRock[j + 1][i + 1] > rk) rk = hfRock[j + 1][i + 1];
      cx = -HF_HALF + 2.0f * HF_HALF * ((float)i + 0.5f) / hfSize - cam.eye[0];
      cz = -HF_HALF + 2.0f * HF_HALF * ((float)j + 0.5f) / hfSize - cam.eye[2];
      d = sqrtf(cx * cx + cz * cz);
      c = rk > 0.18f ? 2 : (cinematic || d < 30.0f) ? 0 : 1;
      if (wetExt && c != 2 && hmax < 0.8f) c = 3;        /* the wet band along the waterline */
      if (c != cls) continue;
      n += terrainTriangle(out + n, &hfV[j][i], &hfV[j + 1][i], &hfV[j][i + 1],
                           hfH[j][i], hfH[j + 1][i], hfH[j][i + 1]);
      n += terrainTriangle(out + n, &hfV[j][i + 1], &hfV[j + 1][i], &hfV[j + 1][i + 1],
                           hfH[j][i + 1], hfH[j + 1][i], hfH[j + 1][i + 1]);
    }
  return n;
}

/* wet sand: wind ripples embossed with the combine extension.  TMU1 = detail texture
   tinted by TMU1's constant colour; TMU0 = sand + (ripple - 0.5) * detail factor (the
   signed X_MINUS_HALF mode), so ripples both darken and lighten and fade with distance. */
static void drawWetSand(void)
{
  int n;
  if (!wetExt) return;
  grGlideSetVertexLayout(layoutTex2);
  source(&texSandBoth); source(&texDetail);
  X.constColor(GR_TMU1, 0xffd0f0ffu);
  X.texColorCombine(GR_TMU1, GR_CMBX_LOCAL_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                    GR_CMBX_TMU_CCOLOR, FXFALSE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
  X.texAlphaCombine(GR_TMU1, GR_CMBX_LOCAL_TEXTURE_ALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                    GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
  X.texColorCombine(GR_TMU0, GR_CMBX_OTHER_TEXTURE_RGB, GR_FUNC_MODE_X_MINUS_HALF, GR_CMBX_LOCAL_TEXTURE_RGB, GR_FUNC_MODE_ZERO,
                    GR_CMBX_DETAIL_FACTOR, FXFALSE, GR_CMBX_B, FXFALSE, 0, FXFALSE);
  X.texAlphaCombine(GR_TMU0, GR_CMBX_LOCAL_TEXTURE_ALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                    GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
  X.colorCombine(GR_CMBX_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                 GR_CMBX_ITRGB, FXFALSE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
  X.alphaCombine(GR_CMBX_ITALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                 GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
  grTexDetailControl(GR_TMU0, 1, 6, 1.0f);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU1, GR_MIPMAP_NEAREST, FXFALSE);
  n = hfCells(3, triBuf);
  if (n) { grDrawVertexArrayContiguous(GR_TRIANGLES, (FxU32)n, triBuf, sizeof(V)); statTris += (unsigned long)n / 3; }
  grTexMipMapMode(GR_TMU1, GR_MIPMAP_DISABLE, FXFALSE);
  grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
  tmu1Off();
  ccTexMod();
  USE(F_CMBX_FX);
}

static void drawIsland(void)
{
  int n;
  depthOn(1); blendOff(); fogOn(1);
  grTexLodBiasValue(GR_TMU0, 0.25f * sinf(tNow * 0.7f));
  ccTexMod();
  /* near sand: detail texture (TMU1 = detail, TMU0 blends by the detail factor) */
  if (!cinematic && ON(F_DETAIL) && nTmu > 1 && texDetail.ok) {
    grGlideSetVertexLayout(layoutTex2);
    source(&texSandBoth); source(&texDetail);
    tmuPass(GR_TMU1);
    grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_DETAIL_FACTOR,
                 GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
    grTexDetailControl(GR_TMU0, 2, 7, 0.45f);
    grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    grTexMipMapMode(GR_TMU1, GR_MIPMAP_NEAREST, FXFALSE);
    if (ON(F_MIRROR)) grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_MIRROR_EXT, GR_TEXTURECLAMP_MIRROR_EXT);
    USE(F_DETAIL);
  } else {
    grGlideSetVertexLayout(layoutTex);
    source(&texSandBoth); tmu1Off(); tmuPass(GR_TMU0);
  }
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, cinematic ? GR_MIPMAP_NEAREST : GR_MIPMAP_NEAREST_DITHER, FXFALSE);
  if (!cinematic) grEnable(GR_ALLOW_MIPMAP_DITHER);
  n = hfCells(0, triBuf);
  if (n) { grDrawVertexArrayContiguous(GR_TRIANGLES, (FxU32)n, triBuf, sizeof(V)); statTris += (unsigned long)n / 3; }
  if (!cinematic) grDisable(GR_ALLOW_MIPMAP_DITHER);
  if (nTmu > 1) grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  /* far sand: single-pass trilinear, even LODs on TMU0 and odd LODs on TMU1 */
  if (ON(F_SAND_TRI) && nTmu > 1 && texSandEven.ok && texSandOdd.ok) {
    grGlideSetVertexLayout(layoutTex2);
    source(&texSandEven); source(&texSandOdd);
    grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXTRUE);
    grTexMipMapMode(GR_TMU1, GR_MIPMAP_NEAREST, FXTRUE);
    grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    grTexLodBiasValue(GR_TMU1, 0.25f * sinf(tNow * 0.7f));
    tmuPass(GR_TMU1);
    grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_LOD_FRACTION,
                 GR_COMBINE_FUNCTION_BLEND, GR_COMBINE_FACTOR_LOD_FRACTION, FXFALSE, FXFALSE);
    USE(F_SAND_TRI);
  } else {
    grGlideSetVertexLayout(layoutTex);
    source(&texSandBoth); tmu1Off(); tmuPass(GR_TMU0);
    grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  }
  if (!cinematic) {
    n = hfCells(1, triBuf);
    if (n) { grDrawVertexArrayContiguous(GR_TRIANGLES, (FxU32)n, triBuf, sizeof(V)); statTris += (unsigned long)n / 3; }
  }
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  if (nTmu > 1) { grTexMipMapMode(GR_TMU1, GR_MIPMAP_DISABLE, FXFALSE); grTexLodBiasValue(GR_TMU1, 0.0f); }
  grTexLodBiasValue(GR_TMU0, 0.0f);
  tmu1Off();
  /* the sandstone outcrop: the largest texture (2048x2048 on VSA), mirror wrapped */
  {
    Tex *rt = texBig.ok && ON(F_BIGTEX) ? &texBig : &texSandBoth;
    int k;
    grGlideSetVertexLayout(rt->tmu == GR_TMU1 ? layoutTex2 : layoutTex);
    if (rt->tmu == GR_TMU1) { tmuPass(GR_TMU1); grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE,
                              GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE); }
    else tmuPass(GR_TMU0);
    source(rt);
    grTexFilterMode((GrChipID_t)rt->tmu, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    grTexMipMapMode((GrChipID_t)rt->tmu, rt == &texBig ? GR_MIPMAP_DISABLE : GR_MIPMAP_NEAREST, FXFALSE);
    if (ON(F_MIRROR)) grTexClampMode((GrChipID_t)rt->tmu, GR_TEXTURECLAMP_MIRROR_EXT, GR_TEXTURECLAMP_MIRROR_EXT);
    n = hfCells(2, triBuf);
    for (k = 0; k < n; k++) {                          /* rock colour and side-on mapping */
      V *v = &triBuf[k];
      float s = v->s0 / v->oow, t = v->t0 / v->oow;    /* world x*36, z*36 */
      v->r *= 0.62f; v->g *= 0.58f; v->b *= 0.56f;
      if (rt->tmu == GR_TMU1) st1(v, (s + t) * 0.9f, (s - t) * 0.9f);
      else st0(v, (s + t) * 0.9f, (s - t) * 0.9f);
    }
    if (n) { grDrawVertexArrayContiguous(GR_TRIANGLES, (FxU32)n, triBuf, sizeof(V)); statTris += (unsigned long)n / 3; }
    grTexClampMode((GrChipID_t)rt->tmu, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
    if (rt->tmu == GR_TMU1) tmu1Off();
    if (rt == &texBig) USE(F_BIGTEX);
  }
  drawWetSand();
}

/* A breaking rim and a fading backwash separated by translucent water. */
static void drawFoam(void)
{
  enum { SEGMENTS = 96, RINGS = 5 };
  int k, n = SEGMENTS;
  static V ring[RINGS][SEGMENTS + 1], breaker[2][3][SEGMENTS + 1];
  float crestDistance[SEGMENTS + 1];
  if (!ON(F_FOAM) || !texFoam.ok) return;
  for (k = 0; k <= n; k++) {
    static const float offsets[RINGS] = {-0.55f, 0.18f, 0.85f, 1.62f, 2.55f};
    static const float opacity[RINGS] = {0, 215, 17, 95, 0};
    float a = 6.2831853f * (float)k / (float)n, c = coastR(a), lo = c, hi = c * 1.3f, mid, pulse, patch, cs = cosf(a), sn = sinf(a);
    int it, j;
    for (it = 0; it < 14; it++) { mid = 0.5f * (lo + hi); if (islandH(mid * cs, mid * sn) > 0.0f) lo = mid; else hi = mid; }
    mid = 0.5f * (lo + hi);                             /* the waterline radius */
    pulse = highDetail ? 0.5f + 0.5f * cosf(6.2831853f * (mid - c + breakerTravel(a)) / breakerPitch) :
                        0.5f + 0.5f * sinf(tNow * 1.6f + a * 7.0f + 0.45f * sinf(3.0f * a));
    patch = 0.50f + 0.50f * smoothf(vnoise((float)k * 12.0f / n, tNow * 0.3f, 12, seedBase));
    for (j = 0; j < RINGS; j++) {
      float rr = mid + offsets[j] + (0.45f + 0.10f * (float)j) * pulse, p[3];
      V *v = &ring[j][k];
      memset(v, 0, sizeof *v);
      p[0] = rr * cs; p[2] = rr * sn;
      p[1] = fmaxf(islandH(p[0], p[2]), waveH(p[0], p[2])) + 0.08f;
      proj(p, v);
      col(v, 246, 251, 255, opacity[j] * patch * (0.66f + 0.34f * pulse));
      st0(v, (float)k * (64.0f * 43.0f / n) + tNow * 21.0f, (float)j * 51.0f + tNow * 9.0f);
    }
    if (highDetail) {
      float travel = breakerTravel(a) / breakerPitch;
      float nearest = (ceilf(travel) - travel) * breakerPitch;
      crestDistance[k] = nearest;
      for (int band = 0; band < 2; band++) {
        float distance = nearest + band * breakerPitch;
        /* Fade at both ends of each crest's lifetime, including at wrap. */
        float life = smoothf(clampf(nearest / 0.8f, 0, 1)) *
                     smoothf(clampf((breakerPitch - nearest) / 0.8f, 0, 1));
        float gain = band ? 0.54f : 1.0f;
        for (j = 0; j < 3; j++) {
          float rr = c + distance + (j - 1) * 0.28f;
          float x = rr * cs, z = rr * sn;
          V *v = &breaker[band][j][k];
          wv(v, x, waveH(x, z) + 0.065f, z, 240, 249, 255,
             j == 1 ? 220.0f * patch * gain * life : 0.0f,
             (float)k * (256.0f * 12.0f / n), 32.0f + (float)j * 68.0f);
        }
      }
    }
  }
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOn(0); fogOn(1);
  source(&texFoam);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  if (ON(F_BLEND_EXT) && X.blend) {
    X.blend(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_OP_ADD, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_OP_ADD);
    USE(F_BLEND_EXT);
  } else blendAlpha();
  if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -8 : 8);
  for (k = 0; k < RINGS - 1; k++) {
    V strip[2 * (SEGMENTS + 1)];
    int i;
    for (i = 0; i <= n; i++) { strip[2 * i] = ring[k + 1][i]; strip[2 * i + 1] = ring[k][i]; }
    grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 2 * (n + 1), strip, sizeof(V));
    statTris += 2 * n;
  }
  if (highDetail) for (int band = 0; band < 2; band++) {
    for (int edge = 0; edge < 2; edge++) {
      V strip[2 * (SEGMENTS + 1)];
      int count = 0;
      for (int i = 0; i <= n + 1; i++) {
        /* Never connect a fading segment to the next crest offshore. */
        if (i == n + 1 || (i > 0 && fabsf(crestDistance[i] - crestDistance[i - 1]) > breakerPitch * 0.5f)) {
          if (count >= 4) {
            grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, count, strip, sizeof(V));
            statTris += count - 2;
          }
          count = 0;
        }
        if (i <= n) {
          strip[count++] = breaker[band][edge + 1][i];
          strip[count++] = breaker[band][edge][i];
        }
      }
    }
  }
  grDepthBiasLevel(0);
  blendOff();
  USE(F_FOAM);
}

/* positions on the island */
#define NPALM 6
static float palmPos[NPALM][3], palmH[NPALM], palmLean[NPALM][2];
static float hutP[3], hutAng, poolP[3], jettyA, boatP[3];
static void placeProps(void)
{
  static const float pa[NPALM] = { 0.3f, 1.3f, 0.7f, 2.75f, 4.35f, 5.5f }, pr[NPALM] = { 0.55f, 0.7f, 0.45f, 0.62f, 0.5f, 0.72f };
  int i;
  for (i = 0; i < NPALM; i++) {
    float r = pr[i] * coastR(pa[i]);
    palmPos[i][0] = r * cosf(pa[i]); palmPos[i][2] = r * sinf(pa[i]); palmPos[i][1] = islandH(palmPos[i][0], palmPos[i][2]);
    palmH[i] = 6.5f + 2.5f * hashf((unsigned)i, 9);
    palmLean[i][0] = cosf(pa[i]) * (1.2f + 1.2f * hashf((unsigned)i, 10)); palmLean[i][1] = sinf(pa[i]) * (1.2f + 1.2f * hashf((unsigned)i, 10));
  }
  hutAng = 0.95f;
  hutP[0] = 0.42f * coastR(hutAng) * cosf(hutAng); hutP[2] = 0.42f * coastR(hutAng) * sinf(hutAng); hutP[1] = islandH(hutP[0], hutP[2]);
  jettyA = 0.15f;
  { float a = 2.2f, r = coastR(a) - 2.6f; poolP[0] = r * cosf(a); poolP[2] = r * sinf(a); poolP[1] = islandH(poolP[0], poolP[2]); }
  { float a = -0.55f, r = coastR(a) - 1.4f; boatP[0] = r * cosf(a); boatP[2] = r * sinf(a); boatP[1] = islandH(boatP[0], boatP[2]); }
}

/* a textured quad from four world points (s,t per corner in texels) */
static void wquad(const float *a, const float *b, const float *c, const float *d, float s0, float t0, float s1, float t1,
                  float r, float g, float bl, float al)
{
  V va, vb, vc, vd;
  wv(&va, a[0], a[1], a[2], r, g, bl, al, s0, t0); wv(&vb, b[0], b[1], b[2], r, g, bl, al, s1, t0);
  wv(&vc, c[0], c[1], c[2], r * 0.92f, g * 0.92f, bl * 0.92f, al, s1, t1); wv(&vd, d[0], d[1], d[2], r * 0.92f, g * 0.92f, bl * 0.92f, al, s0, t1);
  quad(&va, &vb, &vc, &vd);
}
/* a flat decal on the terrain: centre, half sizes, heading */
static void decal(float x, float z, float hw, float hl, float ang, float lift, float r, float g, float b, float a)
{
  float c = cosf(ang), s = sinf(ang), p[4][3];
  int k;
  static const float sx[4] = { -1, 1, 1, -1 }, sz[4] = { -1, -1, 1, 1 };
  for (k = 0; k < 4; k++) {
    float lx = sx[k] * hw, lz = sz[k] * hl;
    p[k][0] = x + lx * c - lz * s; p[k][2] = z + lx * s + lz * c;
    p[k][1] = fmaxf(islandH(p[k][0], p[k][2]), 0.0f) + lift;
  }
  wquad(p[0], p[1], p[2], p[3], 0, 0, 255, 255, r, g, b, a);
}

/* hut: four plank walls, a door, a thatch pyramid roof */
static void hutCorner(int k, float y, float *p, float grow)
{
  static const float cx[4] = { -1, 1, 1, -1 }, cz[4] = { -1, -1, 1, 1 };
  float c = cosf(hutAng + 1.57f), s = sinf(hutAng + 1.57f), lx = cx[k] * 1.7f * grow, lz = cz[k] * 1.4f * grow;
  p[0] = hutP[0] + lx * c - lz * s; p[2] = hutP[2] + lx * s + lz * c; p[1] = y;
}
static void drawHutWalls(void)
{
  int k;
  float base = hutP[1] - 0.3f, top = hutP[1] + (highDetail ? 1.78f : 2.2f);
  for (k = 0; k < 4; k++) {
    float a[3], b[3], c[3], d[3], lit = 0.75f + 0.25f * cosf(hutAng + (float)k * 1.57f);
    hutCorner(k, top, a, 1.0f); hutCorner((k + 1) & 3, top, b, 1.0f);
    hutCorner((k + 1) & 3, base, c, 1.0f); hutCorner(k, base, d, 1.0f);
    if (highDetail) {
      float dx = b[0] - a[0], dz = b[2] - a[2];
      lit = sunLight(dz, 0.0f, -dx);
    }
    wquad(a, b, c, d, 0, 0, 255, 200, 255 * lit, 250 * lit, 240 * lit, 255);
  }
}

static void hutWindow(int side)
{
  /* Layer a timber frame, dark recess and crossbars on the actual wall plane. */
  static const float parts[][7] = {
    {0.22f, 0.78f, 0.66f, 1.68f, 99, 66, 40},
    {0.27f, 0.73f, 0.75f, 1.59f, 34, 55, 57},
    {0.48f, 0.52f, 0.75f, 1.59f, 142, 104, 62},
    {0.27f, 0.73f, 1.13f, 1.20f, 142, 104, 62},
    {0.18f, 0.82f, 0.60f, 0.70f, 162, 118, 70}
  };
  for (unsigned i = 0; i < sizeof parts / sizeof parts[0]; i++) {
    const float *s = parts[i];
    float a[3], b[3], c[3], d[3];
    float scale = 1.012f + 0.006f * i;
    hutCorner(side, hutP[1] + s[3], a, scale);
    hutCorner((side + 1) & 3, hutP[1] + s[3], b, scale);
    for (int axis = 0; axis <= 2; axis += 2) {
      float edge = b[axis] - a[axis];
      b[axis] = a[axis] + s[1] * edge;
      a[axis] += s[0] * edge;
      c[axis] = b[axis]; d[axis] = a[axis];
    }
    c[1] = d[1] = hutP[1] + s[2];
    wquad(a, b, c, d, 0, 0, 0, 0, s[4], s[5], s[6], 255);
  }
}
static void drawHut(void)
{
  int k;
  float apex[3];
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOn(1); blendOff(); fogOn(1);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  if (ON(F_COLORMASK) && !haveStencil) {
    /* depth-only pre-pass of the walls (colour writes off), then the textured pass */
    grColorMask(FXFALSE, FXFALSE);
    source(&texWall); drawHutWalls();
    grColorMask(FXTRUE, FXFALSE);
    grDepthBufferFunction(useW ? GR_CMP_LEQUAL : GR_CMP_GEQUAL);
    USE(F_COLORMASK);
  }
  source(&texWall); drawHutWalls();
  /* door on the sea-facing wall, pulled forward with depth bias */
  {
    float a[3], b[3], c[3], d[3], u0, u1, base = hutP[1] - 0.1f;
    float wa[3], wb[3];
    float doorOffset = highDetail ? 1.012f : 1.0f;
    hutCorner(1, base, wa, doorOffset); hutCorner(2, base, wb, doorOffset);
    u0 = 0.3f; u1 = 0.62f;
    a[0] = wa[0] + (wb[0] - wa[0]) * u0; a[2] = wa[2] + (wb[2] - wa[2]) * u0; a[1] = base + 1.7f;
    b[0] = wa[0] + (wb[0] - wa[0]) * u1; b[2] = wa[2] + (wb[2] - wa[2]) * u1; b[1] = base + 1.7f;
    c[0] = b[0]; c[2] = b[2]; c[1] = base; d[0] = a[0]; d[2] = a[2]; d[1] = base;
    source(&texDoor);
    if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -4 : 4);
    wquad(a, b, c, d, 0, 0, 255, 255, 255, 255, 255, 255);
    grDepthBiasLevel(0);
  }
  /* thatch roof with overhang */
  setTable(0, 2);
  source(&texThatch);
  if (!cinematic && texThatch1.ok && ON(F_TMU_TABLES) && X.tableExt) {
    X.tableExt(GR_TMU1, GR_TEXTABLE_NCC1, &nccAlt);          /* TMU1 only: the weathered table */
    source(&texThatch1);
    grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    grTexMipMapMode(GR_TMU1, GR_MIPMAP_NEAREST, FXFALSE);
  }
  apex[0] = hutP[0]; apex[2] = hutP[2]; apex[1] = hutP[1] + 3.6f;
  for (k = 0; k < 4; k++) {
    float a[3], b[3];
    V va, vb, vc;
    float lit = 0.7f + 0.3f * cosf(hutAng + (float)k * 1.57f + 0.6f);
    hutCorner(k, hutP[1] + 1.95f, a, 1.35f); hutCorner((k + 1) & 3, hutP[1] + 1.95f, b, 1.35f);
    if (highDetail) {
      float dx = b[0] - a[0], dz = b[2] - a[2], rise = apex[1] - a[1];
      float ny = dz * (apex[0] - a[0]) - dx * (apex[2] - a[2]);
      /* The corners wind clockwise when viewed from above. */
      lit = sunLight(dz * rise, -ny, -dx * rise);
    }
    wv(&va, apex[0], apex[1], apex[2], 255 * lit, 245 * lit, 225 * lit, 255, 128, 0);
    wv(&vb, a[0], a[1], a[2], 255 * lit, 245 * lit, 225 * lit, 255, 0, 255);
    wv(&vc, b[0], b[1], b[2], 255 * lit, 245 * lit, 225 * lit, 255, 255, 255);
    if (!cinematic && (k & 1) && texThatch1.ok && ON(F_TMU_TABLES) && X.tableExt) {
      st1(&va, 128, 0); st1(&vb, 0, 255); st1(&vc, 255, 255);
      grGlideSetVertexLayout(layoutTex2);
      tmuPass(GR_TMU1);
      grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE);
      tri(&va, &vb, &vc);
      tmu1Off(); tmuPass(GR_TMU0);
      grGlideSetVertexLayout(layoutTex);
      USE(F_TMU_TABLES);
    } else tri(&va, &vb, &vc);
  }
  /* Exposed timber fascia gives the overhang thickness and a shaded underside. */
  source(&texWall);
  for (k = 0; k < 4; k++) {
    float a[3], b[3], c[3], d[3];
    hutCorner(k, hutP[1] + 1.95f, a, 1.35f);
    hutCorner((k + 1) & 3, hutP[1] + 1.95f, b, 1.35f);
    hutCorner((k + 1) & 3, hutP[1] + 1.78f, c, 1.33f);
    hutCorner(k, hutP[1] + 1.78f, d, 1.33f);
    wquad(a, b, c, d, 0, 24, 255, 54, 130, 112, 98, 255);
    if (highDetail) {
      hutCorner(k, hutP[1] + 1.78f, a, 1.0f);
      hutCorner((k + 1) & 3, hutP[1] + 1.78f, b, 1.0f);
      wquad(d, c, b, a, 0, 0, 255, 80, 101, 87, 72, 255);
    }
  }
  if (highDetail) {
    grGlideSetVertexLayout(layoutCol);
    ccIterated();
    hutWindow(0);
    hutWindow(2);
    hutWindow(3);
  }
}

/* jetty: deck planks on posts from the beach out over the water */
static void jettyDeck(float p[4][3])
{
  float cs = cosf(jettyA), sn = sinf(jettyA);
  float start = coastR(jettyA) - 2.0f;
  for (int i = 0; i < 4; i++) {
    float r = start + ((i == 1 || i == 2) ? 11.0f : 0.0f);
    float side = i < 2 ? 0.9f : -0.9f;
    p[i][0] = r * cs - side * sn;
    p[i][2] = r * sn + side * cs;
    p[i][1] = 0.85f;
    if (i == 0 || i == 3)
      p[i][1] = fmaxf(p[i][1], islandH(p[i][0], p[i][2]) + 0.15f);
  }
}

static void drawJetty(void)
{
  float cs = cosf(jettyA), sn = sinf(jettyA), r0 = coastR(jettyA) - 2.0f, w = 0.9f, y = 0.85f;
  float deck[4][3];
  int k;
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOn(1); blendOff(); fogOn(1);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  source(&texDeck);
  jettyDeck(deck);
  wquad(deck[0], deck[1], deck[2], deck[3], 0, 0, 255 * 3.0f, 255, 255, 250, 240, 255);
  if (highDetail) {
    float underside[4][3];
    for (int i = 0; i < 4; i++) {
      memcpy(underside[i], deck[i], sizeof underside[i]);
      underside[i][1] -= 0.25f;
    }
    for (int i = 0; i < 4; i++) {
      int next = (i + 1) & 3;
      wquad(deck[i], deck[next], underside[next], underside[i], 0, 0, 255, 45, 146, 111, 76, 255);
    }
    /* Match the top's diagonal so the two surfaces retain constant thickness. */
    wquad(underside[0], underside[3], underside[2], underside[1], 0, 0, 255, 255, 103, 80, 61, 255);
  }
  source(&texPost);
  for (k = 0; k < 6; k++) {
    float rr = r0 + 1.0f + 2.0f * (float)k;
    if (highDetail && k == 5) rr -= 0.14f;
    int sd;
    for (sd = -1; sd <= 1; sd += 2) {
      float px = rr * cs - (float)sd * (w - 0.12f) * sn, pz = rr * sn + (float)sd * (w - 0.12f) * cs, q = 0.12f;
      float p0[3], p1[3], p2[3], p3[3];
      if (highDetail) {
        static const int along[] = {-1, 1, 1, -1}, across[] = {-1, -1, 1, 1};
        float top[4][3], foot[4][3];
        int bank = sd < 0 ? 0 : 3;
        float topY = deck[bank][1] + (y - deck[bank][1]) * ((rr - r0) / 11.0f) - 0.14f;
        for (int i = 0; i < 4; i++) {
          top[i][0] = foot[i][0] = px + q * (along[i] * cs - across[i] * sn);
          top[i][1] = topY;
          foot[i][1] = -2.0f;
          top[i][2] = foot[i][2] = pz + q * (along[i] * sn + across[i] * cs);
        }
        for (int i = 0; i < 4; i++) {
          int next = (i + 1) & 3;
          wquad(top[i], top[next], foot[next], foot[i], 0, 0, 64, 255, 200, 190, 180, 255);
        }
        wquad(top[0], top[1], top[2], top[3], 0, 0, 64, 64, 160, 145, 126, 255);
        wquad(foot[0], foot[3], foot[2], foot[1], 0, 0, 64, 64, 132, 119, 101, 255);
        continue;
      }
      p0[0] = px - q * cs; p0[2] = pz - q * sn; p0[1] = y;
      p1[0] = px + q * cs; p1[2] = pz + q * sn; p1[1] = y;
      p2[0] = p1[0]; p2[2] = p1[2]; p2[1] = -2.0f;
      p3[0] = p0[0]; p3[2] = p0[2]; p3[1] = -2.0f;
      wquad(p0, p1, p2, p3, 0, 0, 64, 255, 200, 190, 180, 255);
    }
  }
}

/* palms: curved trunks (rings + a Gouraud cone fan at the crown) and frond pairs */
static void palmTop(int i, float *top)
{
  float sway = 0.18f * sinf(tNow * 1.1f + (float)i);
  top[0] = palmPos[i][0] + palmLean[i][0] + sway; top[1] = palmPos[i][1] + palmH[i]; top[2] = palmPos[i][2] + palmLean[i][1];
}

static float palmTrunkCenter(int palm, const float *top, float t, float *p)
{
  const float *base = palmPos[palm];
  p[0] = base[0] + (top[0] - base[0]) * t * t;
  p[2] = base[2] + (top[2] - base[2]) * t * t;
  p[1] = base[1] - 0.3f + (top[1] - base[1] + 0.3f) * t;
  return 0.34f - 0.16f * t;
}

static void drawPalms(int frame)
{
  int i, k, seg;
  enum { PALM_MAX_SIDES = 12 };
  int sides = highDetail ? PALM_MAX_SIDES : 8;
  int rings = highDetail ? 10 : 6;
  if (!ON(F_TREES)) return;
  depthOn(1); fogOn(1);
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); blendOff();
  for (i = 0; i < NPALM; i++) {
    float top[3];
    V fan[PALM_MAX_SIDES + 2];
    void *ptr[PALM_MAX_SIDES + 2];
    palmTop(i, top);
    for (seg = 0; seg < rings; seg++) {
      float t0 = (float)seg / rings, t1 = (float)(seg + 1) / rings;
      V strip[2 * (PALM_MAX_SIDES + 1)];
      for (k = 0; k <= sides; k++) {
        int e;
        for (e = 0; e < 2; e++) {
          float t = e ? t0 : t1, an = 6.2831853f * (float)(k % sides) / sides, p[3];
          float nx = cosf(an), nz = sinf(an);
          float rad = palmTrunkCenter(i, top, t, p);
          float sh = highDetail ? sunLight(nx, 0.0f, nz) : 0.55f + 0.45f * cosf(an - 0.8f);
          float ringd = (seg & 1) ? 0.85f : 1.0f;
          p[0] += rad * nx;
          p[2] += rad * nz;
          memset(&strip[2 * k + e], 0, sizeof(V));
          proj(p, &strip[2 * k + e]);
          col(&strip[2 * k + e], (125 + 50 * sh) * ringd, (98 + 38 * sh) * ringd, (66 + 24 * sh) * ringd, 255);
        }
      }
      grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 2 * (sides + 1), strip, sizeof(V));
      statTris += 2 * sides;
    }
    /* crown: a dark cone fan (coconuts and frond bases) */
    wv(&fan[0], top[0], top[1] + 0.25f, top[2], 70, 80, 40, 255, 0, 0); ptr[0] = &fan[0];
    for (k = 0; k <= sides; k++) {
      float an = 6.2831853f * (float)k / sides;
      wv(&fan[k + 1], top[0] + 0.45f * cosf(an), top[1] - 0.35f, top[2] + 0.45f * sinf(an), 95 + 30 * (k & 1), 75, 35, 255, 0, 0);
      ptr[k + 1] = &fan[k + 1];
    }
    grDrawVertexArray(GR_TRIANGLE_FAN, sides + 2, ptr);
    statTris += sides;
  }
  /* fronds: even palms P8 + PALETTE6666, odd palms AYIQ8422; alpha test removes the
     transparent texels (key colour included), point sampling keeps edges clean */
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod();
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_POINT_SAMPLED, GR_TEXTUREFILTER_POINT_SAMPLED);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_DISABLE, FXFALSE);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  grAlphaTestFunction(GR_CMP_GREATER);
  grAlphaTestReferenceValue(0x80);
  for (i = 0; i < NPALM; i++) {
    int ncc = (i & 1) && texLeafNcc.ok;
    float top[3];
    if (!texLeafP8.ok) break;
    palmTop(i, top);
    if (ncc) {
      int kr, kg, kb;
      FxU32 key;
      setTable(0, 1);
      source(&texLeafNcc);
      nccDecode(&nccLeaf, 0, &kr, &kg, &kb);
      key = 0xff000000u | ((FxU32)kr << 16) | ((FxU32)kg << 8) | (FxU32)kb;
      if (ON(F_TEXCHROMA) && X.texChromaMode) {
        X.texChromaRange(GR_TMU0, key, key, GR_TEXCHROMARANGE_RGB_ALL_EXT);
        X.texChromaMode(GR_TMU0, GR_TEXCHROMA_ENABLE_EXT);
        USE(F_TEXCHROMA);
      }
      if ((frame & 1) == 0) {
        /* frame-buffer chroma key / range on the combined colour as well (white iterated) */
        grChromakeyValue(key);
        grChromakeyMode(GR_CHROMAKEY_ENABLE);
        if (ON(F_CHROMARANGE) && X.chromaRange) {
          X.chromaRange(key, 0xff000000u | ((FxU32)clampi(kr + 8, 0, 255) << 16) | ((FxU32)clampi(kg + 8, 0, 255) << 8) | (FxU32)clampi(kb + 8, 0, 255),
                        GR_CHROMARANGE_RGB_ALL_EXT);
          X.chromaRangeMode(GR_CHROMARANGE_ENABLE_EXT);
          USE(F_CHROMARANGE);
        }
      }
    } else {
      setTable(1, 0);
      source(&texLeafP8);
    }
    for (k = 0; k < 9; k++) {
      /* frond k: inner half rising, outer half drooping, swaying with the wind */
      float an = 6.2831853f * (float)k / 9.0f + 0.25f * (float)i + 0.12f * sinf(tNow * 0.9f + (float)(i * 3 + k));
      float dx = cosf(an), dz = sinf(an), len = 2.6f, wid = 0.95f, p[3], droop = 0.7f + 0.25f * (float)(k % 3);
      V a, b, c, d, e, f;
      float wl = ncc && (frame & 1) == 0 ? 255.0f : 225.0f;
      memset(&a, 0, sizeof a);
      p[0] = top[0] - dz * 0.2f; p[1] = top[1] + 0.1f; p[2] = top[2] + dx * 0.2f; wv(&a, p[0], p[1], p[2], 255, 255, 255, 255, 0, 0);
      p[0] = top[0] + dz * 0.2f; p[2] = top[2] - dx * 0.2f; wv(&b, p[0], p[1], p[2], 255, 255, 255, 255, 255, 0);
      p[0] = top[0] + dx * len + dz * wid; p[1] = top[1] + 0.5f; p[2] = top[2] + dz * len - dx * wid; wv(&c, p[0], p[1], p[2], wl, 255, wl, 255, 255, 128);
      p[0] = top[0] + dx * len - dz * wid; p[2] = top[2] + dz * len + dx * wid; wv(&d, p[0], p[1], p[2], wl, 255, wl, 255, 0, 128);
      p[0] = top[0] + dx * len * 1.9f + dz * 0.1f; p[1] = top[1] - 1.6f * droop; p[2] = top[2] + dz * len * 1.9f - dx * 0.1f; wv(&e, p[0], p[1], p[2], wl * 0.85f, 235, wl * 0.8f, 255, 255, 255);
      p[0] = top[0] + dx * len * 1.9f - dz * 0.1f; p[2] = top[2] + dz * len * 1.9f + dx * 0.1f; wv(&f, p[0], p[1], p[2], wl * 0.85f, 235, wl * 0.8f, 255, 0, 255);
      quad(&a, &b, &c, &d);
      quad(&d, &c, &e, &f);
    }
    if (ncc) {
      if (featUsed[F_TEXCHROMA] && X.texChromaMode) X.texChromaMode(GR_TMU0, GR_TEXCHROMA_DISABLE_EXT);
      if (featUsed[F_CHROMARANGE]) X.chromaRangeMode(GR_CHROMARANGE_DISABLE_EXT);
      grChromakeyMode(GR_CHROMAKEY_DISABLE);
    }
  }
  grAlphaTestFunction(GR_CMP_ALWAYS);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  USE(F_TREES);
}

/* shadows of palms and hut laid on the sand along the sun direction.  VSA: stencil, two
   passes (colour writes off to mark, then darken each marked pixel once, RGB only);
   h3: one blended decal pass. */
static void shadowBlob(float x, float z, float rx, float rz, float ang)
{
  V c, rim[14];
  int k;
  float ca = cosf(ang), sa = sinf(ang);
  wv(&c, x, fmaxf(islandH(x, z), 0.0f) + 0.05f, z, 255, 255, 255, 255, 0, 0);
  for (k = 0; k <= 12; k++) {
    float an = 6.2831853f * (float)k / 12.0f, wob = 0.8f + 0.3f * sinf(an * 3.0f + x), lx = rx * wob * cosf(an), lz = rz * wob * sinf(an);
    float px = x + lx * ca - lz * sa, pz = z + lx * sa + lz * ca;
    wv(&rim[k], px, fmaxf(islandH(px, pz), 0.0f) + 0.05f, pz, 255, 255, 255, 255, 0, 0);
  }
  for (k = 0; k < 12; k++) tri(&c, &rim[k], &rim[k + 1]);
}

static float terrainHeight(float x, float z)
{
  float u = (x + HF_HALF) * hfSize / (2.0f * HF_HALF);
  float v = (z + HF_HALF) * hfSize / (2.0f * HF_HALF);
  if (u < 0 || v < 0 || u >= hfSize || v >= hfSize) return islandH(x, z);
  int i = (int)u, j = (int)v;
  float fx = u - i, fz = v - j;
  float h10 = hfH[j][i + 1], h01 = hfH[j + 1][i];
  if (fx + fz <= 1.0f) {
    float h00 = hfH[j][i];
    return h00 + fx * (h10 - h00) + fz * (h01 - h00);
  }
  float h11 = hfH[j + 1][i + 1];
  return h11 + (1.0f - fx) * (h01 - h11) + (1.0f - fz) * (h10 - h11);
}

static float receiverHeight(float x, float z, int water)
{
  return fmaxf(terrainHeight(x, z), water ? waveH(x, z) : 0.0f);
}

static void shadowReceiver(float *p, const float *light, int water)
{
  if (p[1] <= receiverHeight(p[0], p[2], water)) {
    p[1] = receiverHeight(p[0], p[2], water) + 0.05f;
    return;
  }
  float lo = 0.0f, hi = (p[1] + 1.0f) / fmaxf(light[1], 0.05f);
  /* Find the first hill or wave along the ray before refining its crossing. */
  float stepSize = HF_HALF / hfSize;
  for (float t = stepSize; t < hi; t += stepSize) {
    float x = p[0] - light[0] * t, z = p[2] - light[2] * t;
    if (p[1] - light[1] * t <= receiverHeight(x, z, water)) {
      hi = t;
      break;
    }
    lo = t;
  }
  for (int step = 0; step < 8; step++) {
    float t = (lo + hi) * 0.5f;
    float x = p[0] - light[0] * t, z = p[2] - light[2] * t;
    if (p[1] - light[1] * t > receiverHeight(x, z, water)) lo = t;
    else hi = t;
  }
  p[0] -= light[0] * hi;
  p[2] -= light[2] * hi;
  p[1] = receiverHeight(p[0], p[2], water) + 0.05f;
}

typedef struct { float x, z, alpha; } ShadowPoint;

static float shadowSide(ShadowPoint a, ShadowPoint b, ShadowPoint p)
{
  return (b.x - a.x) * (p.z - a.z) - (b.z - a.z) * (p.x - a.x);
}

/* Clip the footprint to each receiver triangle. All resulting pieces sit
   on the same planes as the heightfield, even along sharp slope changes. */
static void shadowFacet(ShadowPoint a, ShadowPoint b, ShadowPoint c)
{
  float scale = hfSize / (2.0f * HF_HALF);
  int x0 = (int)clampf(floorf((fminf(a.x, fminf(b.x, c.x)) + HF_HALF) * scale), 0, hfSize - 1);
  int x1 = (int)clampf(floorf((fmaxf(a.x, fmaxf(b.x, c.x)) + HF_HALF) * scale), 0, hfSize - 1);
  int z0 = (int)clampf(floorf((fminf(a.z, fminf(b.z, c.z)) + HF_HALF) * scale), 0, hfSize - 1);
  int z1 = (int)clampf(floorf((fmaxf(a.z, fmaxf(b.z, c.z)) + HF_HALF) * scale), 0, hfSize - 1);
  for (int j = z0; j <= z1; j++) for (int i = x0; i <= x1; i++) {
    if (fmaxf(fmaxf(hfH[j][i], hfH[j][i + 1]), fmaxf(hfH[j + 1][i], hfH[j + 1][i + 1])) < 0.06f)
      continue;
    ShadowPoint grid[4] = {
      {terrainCoord(i), terrainCoord(j), 0}, {terrainCoord(i + 1), terrainCoord(j), 0},
      {terrainCoord(i), terrainCoord(j + 1), 0}, {terrainCoord(i + 1), terrainCoord(j + 1), 0}
    };
    static const int corners[2][3] = {{0, 1, 2}, {2, 1, 3}};
    for (int half = 0; half < 2; half++) {
      ShadowPoint points[2][6] = {{a, b, c}};
      int n = 3;
      for (int edge = 0; edge < 3 && n; edge++) {
        ShadowPoint start = grid[corners[half][edge]], end = grid[corners[half][(edge + 1) % 3]];
        ShadowPoint *in = points[edge & 1], *out = points[(edge + 1) & 1];
        int count = 0;
        for (int k = 0; k < n; k++) {
          ShadowPoint p = in[k], q = in[(k + 1) % n];
          float dp = shadowSide(start, end, p), dq = shadowSide(start, end, q);
          if (dp >= 0.0f) out[count++] = p;
          if ((dp < 0.0f && dq > 0.0f) || (dp > 0.0f && dq < 0.0f)) {
            float f = dp / (dp - dq);
            out[count++] = (ShadowPoint){p.x + f * (q.x - p.x), p.z + f * (q.z - p.z),
                                        p.alpha + f * (q.alpha - p.alpha)};
          }
        }
        n = count;
      }
      if (n < 3) continue;
      V v[6];
      for (int k = 0; k < n; k++) {
        ShadowPoint p = points[1][k];
        float h = terrainHeight(p.x, p.z);
        float alpha = p.alpha * clampf((h - 0.06f) / 0.25f, 0, 1);
        wv(&v[k], p.x, h + 0.015f, p.z, 255, 255, 255, alpha, 0, 0);
      }
      grDrawVertexArrayContiguous(GR_TRIANGLE_FAN, (FxU32)n, v, sizeof(V));
      statTris += (unsigned long)(n - 2);
    }
  }
}

static void featheredShadow(const ShadowPoint *points, int n, float spread, float opacity)
{
  ShadowPoint inner[5], outer[5];
  float cx = 0.0f, cz = 0.0f;
  for (int i = 0; i < n; i++) { cx += points[i].x / n; cz += points[i].z / n; }
  for (int i = 0; i < n; i++) {
    float x = points[i].x, z = points[i].z;
    inner[i] = (ShadowPoint){x, z, opacity};
    x += (x - cx) * spread;
    z += (z - cz) * spread;
    outer[i] = (ShadowPoint){x, z, 0};
  }
  for (int i = 1; i + 1 < n; i++) shadowFacet(inner[0], inner[i], inner[i + 1]);
  for (int i = 0; i < n; i++) {
    int next = (i + 1) % n;
    shadowFacet(inner[i], outer[i], outer[next]);
    shadowFacet(inner[i], outer[next], inner[next]);
  }
}

static void palmShadowFronds(int palm, const float *crown, const float *light)
{
  for (int k = 0; k < 9; k++) {
    float angle = 6.2831853f * k / 9.0f + 0.25f * palm +
                  0.12f * sinf(tNow * 0.9f + palm * 3 + k);
    float dx = cosf(angle), dz = sinf(angle);
    float points[4][3] = {
      {crown[0], crown[1], crown[2]},
      {crown[0] + 2.6f * dx + 0.55f * dz, crown[1] + 0.5f, crown[2] + 2.6f * dz - 0.55f * dx},
      {crown[0] + 4.9f * dx, crown[1] - 1.6f * (0.7f + 0.25f * (k % 3)), crown[2] + 4.9f * dz},
      {crown[0] + 2.6f * dx - 0.55f * dz, crown[1] + 0.5f, crown[2] + 2.6f * dz + 0.55f * dx}
    };
    ShadowPoint footprint[4];
    for (int j = 0; j < 4; j++) {
      shadowReceiver(points[j], light, 0);
      footprint[j] = (ShadowPoint){points[j][0], points[j][2], 0};
    }
    featheredShadow(footprint, 4, 0.42f, 42.0f);
  }
}

static void palmTrunkShadow(int palm, const float *crown, const float *light)
{
  enum { SECTIONS = 5 };
  float length = hypotf(light[0], light[2]);
  float nx = light[2] / length, nz = -light[0] / length;
  ShadowPoint rows[2][4];
  for (int i = 0; i <= SECTIONS; i++) {
    float t = (float)i / SECTIONS, p[3];
    float radius = palmTrunkCenter(palm, crown, t, p);
    shadowReceiver(p, light, 0);
    float softness = 0.04f + 0.30f * t;
    float offset[4] = {-radius - softness, -radius, radius, radius + softness};
    float opacity = 104.0f - 49.0f * t;
    ShadowPoint *row = rows[i & 1];
    for (int j = 0; j < 4; j++)
      row[j] = (ShadowPoint){p[0] + offset[j] * nx, p[2] + offset[j] * nz,
                             (j == 1 || j == 2) ? opacity : 0};
    if (i) {
      ShadowPoint *prev = rows[(i - 1) & 1];
      for (int j = 0; j < 3; j++) {
        shadowFacet(prev[j], row[j], row[j + 1]);
        shadowFacet(prev[j], row[j + 1], prev[j + 1]);
      }
    }
  }
}

static void hutRoofShadow(const float *light)
{
  enum { POINTS = 5 };
  ShadowPoint caster[POINTS], hull[2 * POINTS];
  for (int i = 0; i < POINTS; i++) {
    float p[3] = {hutP[0], hutP[1] + 3.6f, hutP[2]};
    if (i < 4) hutCorner(i, hutP[1] + 1.95f, p, 1.35f);
    shadowReceiver(p, light, 0);
    ShadowPoint point = {p[0], p[2], 0};
    int j = i;
    while (j > 0 && caster[j - 1].x > point.x) {
      caster[j] = caster[j - 1];
      j--;
    }
    caster[j] = point;
  }
  int n = 0;
  for (int i = 0; i < POINTS; i++) {
    while (n > 1 && shadowSide(hull[n - 2], hull[n - 1], caster[i]) <= 0.0f) n--;
    hull[n++] = caster[i];
  }
  int lower = n;
  for (int i = POINTS - 2; i >= 0; i--) {
    while (n > lower && shadowSide(hull[n - 2], hull[n - 1], caster[i]) <= 0.0f) n--;
    hull[n++] = caster[i];
  }
  if (n > 3) featheredShadow(hull, n - 1, 0.17f, 70.0f);
}

static void contactShadow(float x, float z, float rx, float rz)
{
  enum { EDGES = 16 };
  V center, rim[EDGES + 1];
  wv(&center, x, islandH(x, z) + 0.06f, z, 24, 31, 43, 94, 0, 0);
  for (int i = 0; i <= EDGES; i++) {
    float angle = 6.2831853f * i / EDGES;
    float px = x + rx * cosf(angle), pz = z + rz * sinf(angle);
    wv(&rim[i], px, islandH(px, pz) + 0.06f, pz, 24, 31, 43, 0, 0, 0);
  }
  for (int i = 0; i < EDGES; i++) tri(&center, &rim[i], &rim[i + 1]);
}

static void shadowGeom(void)
{
  float sd[3], sx, sz, l;
  int i;
  sunDir(sd);
  l = sqrtf(sd[0] * sd[0] + sd[2] * sd[2]);
  sx = -sd[0] / l; sz = -sd[2] / l;
  for (i = 0; i < NPALM; i++) {
    float top[3], L = palmH[i] * 0.9f, cx, cz;
    palmTop(i, top);
    if (highDetail) {
      palmTrunkShadow(i, top, sd);
      palmShadowFronds(i, top, sd);
      continue;
    }
    cx = top[0] + sx * L; cz = top[2] + sz * L;
    if (islandH(cx, cz) < 0.3f) { cx = top[0] + sx * L * 0.5f; cz = top[2] + sz * L * 0.5f; }
    if (islandH(cx, cz) < 0.3f) continue;
    /* trunk: a thin strip from the foot to the crown shadow */
    decal(0.5f * (palmPos[i][0] + cx), 0.5f * (palmPos[i][2] + cz), 0.16f,
          0.5f * sqrtf((cx - palmPos[i][0]) * (cx - palmPos[i][0]) + (cz - palmPos[i][2]) * (cz - palmPos[i][2])),
          atan2f(cz - palmPos[i][2], cx - palmPos[i][0]) - 1.5708f, 0.05f, 255, 255, 255, 255);
    shadowBlob(cx, cz, 2.4f, 1.6f, atan2f(sz, sx));
  }
  if (highDetail) {
    hutRoofShadow(sd);
  } else {
    shadowBlob(hutP[0] + sx * 2.6f, hutP[2] + sz * 2.6f, 2.6f, 2.0f, atan2f(sz, sx));
  }
}
static void drawShadows(void)
{
  if (liveMask & (1ull << F_SHADOW)) return;
  if (!ON(F_SHADOW) && !ON(F_DEPTHBIAS)) return;
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0);
  grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE);
  grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                 highDetail ? GR_COMBINE_LOCAL_ITERATED : GR_COMBINE_LOCAL_CONSTANT,
                 GR_COMBINE_OTHER_NONE, FXFALSE);
  grConstantColorValue(0x58182030u);           /* alpha 0x58, dark cool grey */
  depthOn(0); fogOn(0);
  if (ON(F_DEPTHBIAS)) { grDepthBiasLevel(useW ? -16 : 16); USE(F_DEPTHBIAS); }
  if (ON(F_SHADOW) && haveStencil && !cinematic) {
    grEnable(GR_STENCIL_MODE_EXT);
    X.stencilMask(0xff);
    /* pass 1: colour writes off, mark the shadow footprint */
    blendOff();
    if (ON(F_COLORMASK)) { grColorMask(FXFALSE, FXFALSE); USE(F_COLORMASK); }
    if (ON(F_STENCIL_OPS)) {
      /* count the shadow layers in the low four stencil bits (write mask 0x0f) */
      X.stencilMask(0x0f);
      X.stencilFunc(GR_CMP_ALWAYS, 0, 0x0f);
      X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_KEEP, GR_STENCILOP_INCR_WRAP);
    } else {
      X.stencilFunc(GR_CMP_ALWAYS, 1, 0xff);
      X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_KEEP, GR_STENCILOP_REPLACE);
    }
    shadowGeom();
    /* pass 2: darken marked pixels once (RGB only), clearing the mark */
    grColorMask(FXTRUE, FXFALSE);
    if (X.colorMask && ON(F_COLORMASK)) X.colorMask(FXTRUE, FXTRUE, FXTRUE, FXFALSE);
    blendAlpha();
    if (ON(F_STENCIL_OPS)) {
      /* darken once per layer and count back down (compare mask 0x0f): where two shadows
         overlap the sand gets two layers; hidden fragments count down too (DECR_CLAMP) */
      grConstantColorValue(0x48182030u);
      X.stencilFunc(GR_CMP_NOTEQUAL, 0, 0x0f);
      X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_DECR_CLAMP, GR_STENCILOP_DECR_WRAP);
      USE(F_STENCIL_OPS);
    } else {
      X.stencilFunc(GR_CMP_EQUAL, 1, 0xff);
      X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_KEEP, GR_STENCILOP_ZERO);
    }
    shadowGeom();
    X.stencilMask(0xff);
    if (X.colorMask && ON(F_COLORMASK)) X.colorMask(FXTRUE, FXTRUE, FXTRUE, FXTRUE);
    grDisable(GR_STENCIL_MODE_EXT);
    USE(F_SHADOW);
  } else {
    blendAlpha();
    shadowGeom();
    if (ON(F_SHADOW)) {
      if (!cinematic) note(F_SHADOW, "fallback: blended decals (no PIXEXT stencil)");
      USE(F_SHADOW);
    }
  }
  if (highDetail && ON(F_SHADOW)) {
    grGlideSetVertexLayout(layoutCol);
    ccIterated();
    blendAlpha();
    for (int i = 0; i < NPALM; i++) contactShadow(palmPos[i][0], palmPos[i][2], 0.85f, 0.68f);
    contactShadow(hutP[0], hutP[2], 3.0f, 2.1f);
  }
  grDepthBiasLevel(0);
  blendOff();
}

/* beach things: towels, footprints and starfish (depth-biased decals), parasol, ball, dinghy */
static void drawBeach(void)
{
  int k;
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOn(0); fogOn(1);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  blendAlpha();
  if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -12 : 12);
  {
    float a = 0.45f, r = coastR(a) * 0.8f, x = r * cosf(a), z = r * sinf(a);
    source(&texTowelA); decal(x, z, 0.75f, 1.5f, a + 0.2f, 0.04f, 255, 255, 255, 255);
    source(&texTowelB); decal(x + 2.1f * sinf(a), z - 2.1f * cosf(a), 0.75f, 1.5f, a - 0.15f, 0.04f, 255, 255, 255, 255);
    source(&texTowelC); decal(x - 2.6f * sinf(a), z + 2.6f * cosf(a), 0.75f, 1.5f, a + 0.05f, 0.04f, 255, 255, 255, 255);
  }
  /* footprints from the hut down to the water */
  source(&texFoot);
  for (k = 0; k < 14; k++) {
    float t = (float)k / 13.0f, a = hutAng + 0.15f * t, r = (0.42f + 0.55f * t) * coastR(a), side = (k & 1) ? 0.18f : -0.18f;
    float x = r * cosf(a) - side * sinf(a), z = r * sinf(a) + side * cosf(a);
    decal(x, z, 0.16f, 0.28f, a - 1.5708f, 0.03f, 110, 95, 75, 170);
  }
  source(&texStar);
  for (k = 0; k < 3; k++) {
    float a = 2.75f + 0.3f * (float)k, r = coastR(a) - 0.9f - 0.4f * (float)k;
    decal(r * cosf(a), r * sinf(a), 0.32f, 0.32f, (float)k * 1.3f, 0.04f, 255, 255, 255, 255);
  }
  grDepthBiasLevel(0);
  /* tide pool: render-to-texture target (VSA) or the water texture, mirror wrapped */
  if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -10 : 10);
  {
    Tex *pt = !cinematic && texRefl.ok && featUsed[F_TEXBUFFER] ? &texRefl : &texWater;
    V c, rim[20];
    void *ptr[20];
    if (pt->tmu == GR_TMU1) {
      grGlideSetVertexLayout(layoutTex2);
      tmuPass(GR_TMU1);
      grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE,
                   GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE);
    }
    source(pt);
    if (ON(F_MIRROR)) grTexClampMode((GrChipID_t)pt->tmu, GR_TEXTURECLAMP_MIRROR_EXT, GR_TEXTURECLAMP_MIRROR_EXT);
    wv(&c, poolP[0], poolP[1] + 0.05f, poolP[2], 170, 215, 235, 200, 128 + tNow * 10.0f, 128);
    if (pt->tmu == GR_TMU1) st1(&c, 128 + tNow * 10.0f, 128);
    ptr[0] = &c;
    for (k = 0; k <= 18; k++) {
      float an = 6.2831853f * (float)k / 18.0f, rx = 1.7f + 0.2f * sinf(an * 3.0f), rz = 1.1f;
      float x = poolP[0] + rx * cosf(an), z = poolP[2] + rz * sinf(an);
      wv(&rim[k], x, fmaxf(islandH(x, z), 0.0f) + 0.05f, z, 150, 190, 205, 120, 128 + 300.0f * cosf(an) + tNow * 10.0f, 128 + 300.0f * sinf(an));
      if (pt->tmu == GR_TMU1) st1(&rim[k], 128 + 300.0f * cosf(an) + tNow * 10.0f, 128 + 300.0f * sinf(an));
      ptr[k + 1] = &rim[k];
    }
    if (!cinematic && haveStencil && ON(F_STENCIL_OPS)) {
      /* pool outline as a stencil mask: INVERT sets bit 7 (write mask 0x80) with colour
         writes off, the reflection is then a plain square tested against that bit (compare
         mask 0x80), and a second INVERT clears the bit */
      float q[4][3];
      int e;
      static const float ex[4] = { -1, 1, 1, -1 }, ez[4] = { -1, -1, 1, 1 };
      V qv[4];
      grEnable(GR_STENCIL_MODE_EXT);
      X.stencilMask(0x80);
      X.stencilFunc(GR_CMP_ALWAYS, 0x80, 0x80);
      X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_KEEP, GR_STENCILOP_INVERT);
      grColorMask(FXFALSE, FXFALSE);
      grDrawVertexArray(GR_TRIANGLE_FAN, 20, ptr);
      grColorMask(FXTRUE, FXFALSE);
      X.stencilFunc(GR_CMP_EQUAL, 0x80, 0x80);
      X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_KEEP, GR_STENCILOP_KEEP);
      for (e = 0; e < 4; e++) {
        q[e][0] = poolP[0] + ex[e] * 2.2f; q[e][2] = poolP[2] + ez[e] * 1.5f; q[e][1] = fmaxf(islandH(q[e][0], q[e][2]), 0.0f) + 0.05f;
        wv(&qv[e], q[e][0], q[e][1], q[e][2], 160, 200, 220, 170, 128 + 300.0f * ex[e] + tNow * 10.0f, 128 + 300.0f * ez[e]);
        if (pt->tmu == GR_TMU1) st1(&qv[e], 128 + 300.0f * ex[e] + tNow * 10.0f, 128 + 300.0f * ez[e]);
      }
      quad(&qv[0], &qv[1], &qv[2], &qv[3]);
      X.stencilFunc(GR_CMP_ALWAYS, 0x80, 0x80);
      X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_KEEP, GR_STENCILOP_INVERT);
      grColorMask(FXFALSE, FXFALSE);
      grDrawVertexArray(GR_TRIANGLE_FAN, 20, ptr);
      grColorMask(FXTRUE, FXFALSE);
      X.stencilMask(0xff);
      grDisable(GR_STENCIL_MODE_EXT);
      statTris += 18 * 2 + 2;
      USE(F_STENCIL_OPS);
    } else {
    grDrawVertexArray(GR_TRIANGLE_FAN, 20, ptr);
    statTris += 18;
    }
    grTexClampMode((GrChipID_t)pt->tmu, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
    if (pt->tmu == GR_TMU1) {
      tmu1Off(); tmuPass(GR_TMU0);
      grGlideSetVertexLayout(layoutTex);
    }
  }
  grDepthBiasLevel(0);
  depthOn(1);
  /* parasol: P8 cone fan on a pole */
  {
    float a = 0.62f, r = coastR(a) * 0.74f, x = r * cosf(a), z = r * sinf(a), y = islandH(x, z), p0[3], p1[3], p2[3], p3[3];
    V fan[18];
    void *ptr[18];
    blendOff();
    source(&texPost);
    p0[0] = x - 0.05f; p0[2] = z; p0[1] = y + 2.6f; p1[0] = x + 0.05f; p1[2] = z; p1[1] = y + 2.6f;
    p2[0] = x + 0.05f; p2[2] = z; p2[1] = y - 0.2f; p3[0] = x - 0.05f; p3[2] = z; p3[1] = y - 0.2f;
    wquad(p0, p1, p2, p3, 0, 0, 32, 255, 230, 225, 215, 255);
    setTable(0, 0);
    source(&texParasol);
    wv(&fan[0], x, y + 2.75f, z, 255, 255, 255, 255, 128, 128); ptr[0] = &fan[0];
    for (k = 0; k <= 16; k++) {
      float an = 6.2831853f * (float)k / 16.0f, lit = 0.75f + 0.25f * cosf(an - 0.7f);
      wv(&fan[k + 1], x + 1.6f * cosf(an), y + 2.05f, z + 1.6f * sinf(an), 255 * lit, 255 * lit, 255 * lit, 255,
         128 + 127 * cosf(an), 128 + 127 * sinf(an));
      ptr[k + 1] = &fan[k + 1];
    }
    grDrawVertexArray(GR_TRIANGLE_FAN, 18, ptr);
    statTris += 16;
  }
  /* beach ball: a camera-facing disc with alpha */
  {
    float a = 0.3f, r = coastR(a) * 0.84f, x = r * cosf(a), z = r * sinf(a), y = islandH(x, z) + 0.32f;
    float rx = cam.r[0] * 0.33f, rz = cam.r[2] * 0.33f, p0[3], p1[3], p2[3], p3[3];
    source(&texBall);
    grAlphaTestFunction(GR_CMP_GREATER); grAlphaTestReferenceValue(0x40);
    p0[0] = x - rx; p0[2] = z - rz; p0[1] = y + 0.33f; p1[0] = x + rx; p1[2] = z + rz; p1[1] = y + 0.33f;
    p2[0] = x + rx; p2[2] = z + rz; p2[1] = y - 0.33f; p3[0] = x - rx; p3[2] = z - rz; p3[1] = y - 0.33f;
    wquad(p0, p1, p2, p3, 0, 0, 255, 255, 255, 255, 255, 255);
    grAlphaTestFunction(GR_CMP_ALWAYS);
  }
  /* dinghy pulled up on the sand: two hull sides and a transom */
  {
    float hd = -0.55f + 1.4f, c = cosf(hd), s = sinf(hd), L = 1.9f, W = 0.75f, Hh = 0.6f, y = boatP[1];
    float bow[3], sl[3], sr[3], tl[3], tr[3], kl[3], kr[3];
    source(&texHull);
    bow[0] = boatP[0] + c * L; bow[2] = boatP[2] + s * L; bow[1] = y + Hh + 0.15f;
    tl[0] = boatP[0] - c * L - s * W; tl[2] = boatP[2] - s * L + c * W; tl[1] = y + Hh;
    tr[0] = boatP[0] - c * L + s * W; tr[2] = boatP[2] - s * L - c * W; tr[1] = y + Hh;
    sl[0] = boatP[0] - s * W; sl[2] = boatP[2] + c * W; sl[1] = y + Hh;
    sr[0] = boatP[0] + s * W; sr[2] = boatP[2] - c * W; sr[1] = y + Hh;
    kl[0] = boatP[0] - c * L * 0.8f; kl[2] = boatP[2] - s * L * 0.8f; kl[1] = y + 0.05f;
    kr[0] = boatP[0] + c * L * 0.7f; kr[2] = boatP[2] + s * L * 0.7f; kr[1] = y + 0.1f;
    wquad(tl, sl, kr, kl, 0, 0, 128, 255, 255, 255, 255, 255);
    wquad(sl, bow, bow, kr, 128, 0, 255, 255, 255, 255, 255, 255);
    wquad(sr, tr, kl, kr, 0, 0, 128, 255, 235, 235, 235, 255);
    wquad(bow, sr, kr, kr, 0, 0, 128, 255, 235, 235, 235, 255);
    wquad(tl, tr, kl, kl, 0, 0, 255, 160, 200, 200, 200, 255);
    if (highDetail) {
      /* Shared gunwale vertices close the open hull with a deck. */
      wquad(tl, tr, sr, sl, 0, 0, 255, 128, 158, 123, 91, 255);
      wquad(sl, sr, bow, bow, 0, 128, 255, 255, 158, 123, 91, 255);
    }
  }
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  blendOff();
}

/* -g only: the plain format gallery as a strip along the bottom edge */
static void drawGallery(void)
{
  int i, n = 0;
  float w = scrW / (float)NSHELL;
  if (!galleryMode) return;
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOff(); fogOn(0);
  blendAlpha();
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  for (i = 0; i < NSHELL; i++) {
    Shell *sh = &shells[i];
    V a, b, c, d;
    if (!sh->tex.ok) continue;
    if (sh->fmt == GR_TEXFMT_YIQ_422 || sh->fmt == GR_TEXFMT_AYIQ_8422) setTable(0, 2);
    else if (sh->fmt == GR_TEXFMT_P_8 || sh->fmt == GR_TEXFMT_AP_88) setTable(sh->t6666, 0);
    source(&sh->tex);
    scr(&a, w * i + 1, scrH - w); scr(&b, w * (i + 1) - 1, scrH - w); scr(&c, w * (i + 1) - 1, scrH - 1); scr(&d, w * i + 1, scrH - 1);
    col(&a, 255, 255, 255, 255); col(&b, 255, 255, 255, 255); col(&c, 255, 255, 255, 255); col(&d, 255, 255, 255, 255);
    st0(&a, 0, 0); st0(&b, 255, 0); st0(&c, 255, 255); st0(&d, 0, 255);
    quad(&a, &b, &c, &d);
    n++;
  }
  blendOff();
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
}

/* far sailboat: anti-aliased triangles; in the T-buffer mode one copy per sample,
   time-offset (motion blur).  It sails on the far side of the island. */
static FxU32 tbufAll;
static int   inDof;                    /* inside a per-sample depth-of-field pass */
static void boatHull(const float *base, float c, float s)
{
  static const float length[] = {-2.2f, -1.5f, 0.25f, 1.6f, 2.4f};
  static const float width[] = {0.22f, 0.66f, 0.78f, 0.48f, 0.0f};
  V rim[2][5], keel[2][5], deck[2][5];
  for (int side = 0; side < 2; side++) {
    float sign = side ? 1.0f : -1.0f;
    float light = 0.57f + 0.43f * fmaxf(0, sign * (-s * sunD[0] + c * sunD[2]));
    for (int i = 0; i < 5; i++) {
      float x = base[0] + length[i] * c, z = base[2] + length[i] * s;
      float wx = -sign * width[i] * s, wz = sign * width[i] * c;
      float sheer = i == 4 ? 0.70f : 0.50f;
      wv(&rim[side][i], x + wx, base[1] + sheer, z + wz,
         235 * light, 228 * light, 212 * light, 255, 0, 0);
      wv(&keel[side][i], x + 0.44f * wx, base[1] - 0.28f, z + 0.44f * wz,
         40 * light, 87 * light, 99 * light, 255, 0, 0);
      deck[side][i] = rim[side][i];
      col(&deck[side][i], 146, 116, 77, 255);
    }
  }
  for (int i = 0; i < 4; i++) {
    for (int side = 0; side < 2; side++)
      quad(&rim[side][i], &rim[side][i + 1], &keel[side][i + 1], &keel[side][i]);
    quad(&deck[0][i], &deck[0][i + 1], &deck[1][i + 1], &deck[1][i]);
    quad(&keel[0][i], &keel[1][i], &keel[1][i + 1], &keel[0][i + 1]);
  }
  quad(&rim[0][0], &rim[1][0], &keel[1][0], &keel[0][0]);
}

static void boatTris(float tt)
{
  float base[3], hd, R = 52.0f;
  float heading = cinematic ? 0.94f : atan2f(-cam.fh[2], -cam.fh[0]);
  float ang = heading + 3.14159f + 0.55f + tt * 0.01f;
  float p[3], c, s;
  V h0, h1, h2, m0, m1, m2, j0, j1, j2;
  base[0] = R * cosf(ang); base[2] = R * sinf(ang); base[1] = 0.25f + 0.12f * sinf(tt * 2.0f);
  hd = ang + 1.5708f; c = cosf(hd); s = sinf(hd);
  if (highDetail) {
    boatHull(base, c, s);
  } else {
  memset(&h0, 0, sizeof h0);
  p[0] = base[0] - 2.2f * c; p[1] = base[1] + 0.5f; p[2] = base[2] - 2.2f * s; proj(p, &h0);
  p[0] = base[0] + 2.4f * c; p[2] = base[2] + 2.4f * s; proj(p, &h1);
  p[0] = base[0] - 1.6f * c; p[1] = base[1] - 0.3f; p[2] = base[2] - 1.6f * s; proj(p, &h2);
  col(&h0, 245, 245, 240, 255); col(&h1, 245, 245, 240, 255); col(&h2, 140, 150, 160, 255);
  grAADrawTriangle(&h0, &h1, &h2, FXTRUE, FXTRUE, FXTRUE); statTris++;
  p[0] = base[0] + 1.6f * c; p[1] = base[1] - 0.3f; p[2] = base[2] + 1.6f * s; proj(p, &m2);
  col(&m2, 140, 150, 160, 255);
  grAADrawTriangle(&h1, &m2, &h2, FXTRUE, FXTRUE, FXTRUE); statTris++;
  }
  p[0] = base[0]; p[1] = base[1] + 6.0f; p[2] = base[2]; proj(p, &m0);
  p[1] = base[1] + 0.7f; proj(p, &m1);
  p[0] = base[0] - 2.0f * c; p[2] = base[2] - 2.0f * s; proj(p, &j0);
  col(&m0, 255, 255, 250, 255); col(&m1, 235, 235, 228, 255); col(&j0, 225, 222, 212, 255);
  grAADrawTriangle(&m0, &m1, &j0, FXTRUE, FXTRUE, FXTRUE); statTris++;
  p[0] = base[0] + 0.2f * c; p[1] = base[1] + 5.2f; p[2] = base[2] + 0.2f * s; proj(p, &j1);
  p[0] = base[0] + 2.0f * c; p[1] = base[1] + 0.9f; p[2] = base[2] + 2.0f * s; proj(p, &j2);
  col(&j1, 255, 120, 90, 255); col(&j2, 235, 95, 70, 255); col(&m1, 240, 110, 80, 255);
  grAADrawTriangle(&j1, &m1, &j2, FXTRUE, FXTRUE, FXTRUE); statTris++;
}
static void drawBoat(void)
{
  if (!ON(F_AAPRIM)) return;
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOn(1); fogOn(1);
  grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
  grEnable(GR_AA_ORDERED);
  if (aaSamples > 1 && ON(F_TBUFFER) && X.tbufMask && !inDof) {
    int s;
    for (s = 0; s < aaSamples; s++) {
      X.tbufMask(1u << s);
      boatTris(tNow + 0.03f * (float)s);
    }
    X.tbufMask(tbufAll);
    USE(F_TBUFFER);
  } else boatTris(tNow);
  grDisable(GR_AA_ORDERED);
  blendOff();
  USE(F_AAPRIM);
}

/* spray where the swell hits the outcrop: AA points on arcs, streaks as lines and line strips */
static void drawSpray(void)
{
  int i;
  V pts[64], ln[2];
  void *ptr[64];
  float base = atan2f(ROCK_Z, ROCK_X), cr = coastR(base) + 0.6f;
  if (!ON(F_PARTICLES)) return;
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOn(0); fogOn(0);
  blendAlpha();
  grEnable(GR_AA_ORDERED);                     /* anti-aliased points and lines */
  for (i = 0; i < 64; i++) {
    float an = base + 0.55f * (hashf((unsigned)i, 21) - 0.5f), ph = hashf((unsigned)i, 22);
    float life = fmodf(tNow * 0.8f + ph, 1.0f), r = cr + 0.8f * life * (hashf((unsigned)i, 25) - 0.3f), p[3];
    p[0] = r * cosf(an); p[2] = r * sinf(an); p[1] = 0.3f + 3.6f * life * (1.0f - life) * (0.6f + 0.6f * hashf((unsigned)i, 26));
    memset(&pts[i], 0, sizeof pts[i]);
    proj(p, &pts[i]); col(&pts[i], 245, 250, 255, 230 * (1.0f - life));
    ptr[i] = &pts[i];
  }
  for (i = 0; i < 32; i++) { grDrawPoint(&pts[i]); statPoints++; }
  grDrawVertexArray(GR_POINTS, 32, &ptr[32]); statPoints += 32;
  for (i = 0; i < 16; i++) {
    float p[3], an = base + 0.5f * (hashf((unsigned)i, 23) - 0.5f), life = fmodf(tNow * 1.1f + hashf((unsigned)i, 24), 1.0f);
    memset(ln, 0, sizeof ln);
    p[0] = cr * cosf(an); p[2] = cr * sinf(an); p[1] = 0.3f + 2.4f * life * (1.0f - life) * 2.0f;
    proj(p, &ln[0]); col(&ln[0], 255, 255, 255, 170 * (1.0f - life));
    p[1] -= 0.6f; p[0] *= 1.01f; p[2] *= 1.01f;
    proj(p, &ln[1]); col(&ln[1], 220, 235, 255, 0);
    grDrawLine(&ln[0], &ln[1]); statLines++;
  }
  {                                             /* a wash line where the spray falls back */
    V wl[12];
    void *wp[12];
    for (i = 0; i < 12; i++) {
      float an = base - 0.3f + 0.6f * (float)i / 11.0f, rr = cr + 0.2f * sinf(tNow * 2.0f + (float)i), p[3];
      p[0] = rr * cosf(an); p[2] = rr * sinf(an); p[1] = 0.12f;
      memset(&wl[i], 0, sizeof wl[i]);
      proj(p, &wl[i]); col(&wl[i], 240, 248, 255, 110); wp[i] = &wl[i];
    }
    grDrawVertexArray(GR_LINE_STRIP, 12, wp); statLines += 11;
  }
  grDisable(GR_AA_ORDERED);
  blendOff();
  USE(F_PARTICLES);
}

/* render-to-texture: sky and sun into a 128x128 target for the tide pool */
static void renderReflection(void)
{
  V v[4], c, fan[18];
  void *ptr[18];
  int i;
  float sx, sy;
  if (!ON(F_TEXBUFFER) || !texRefl.ok || !X.textureBuffer) return;
  X.textureBuffer(GR_TMU1, texRefl.addr, GR_LOD_LOG2_128, GR_LOD_LOG2_128, GR_ASPECT_LOG2_1x1, texRefl.info.format, GR_MIPMAPLEVELMASK_BOTH);
  grRenderBuffer(GR_BUFFER_TEXTUREBUFFER_EXT);
  grClipWindow(0, 0, 128, 128);
  if (texReflZ.ok && ON(F_RTT_DEPTH)) {
    /* the target's own depth buffer, in texture memory next to it */
    xTexAux(GR_TMU1, texReflZ.addr, GR_LOD_LOG2_128, GR_LOD_LOG2_128, GR_ASPECT_LOG2_1x1, texReflZ.info.format, GR_MIPMAPLEVELMASK_BOTH);
    xAuxBuffer(GR_BUFFER_TEXTUREAUXBUFFER_EXT);
    grDepthBufferMode(GR_DEPTHBUFFER_ZBUFFER); grDepthMask(FXTRUE);
    if (haveStencil) X.bufferClear(0xff8fb8e0u, 0, zrange[1], 0); else grBufferClear(0xff8fb8e0u, 0, zrange[1]);
  }
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOff(); blendOff(); grFogMode(GR_FOG_DISABLE);
  scr(&v[0], 0, 0); scr(&v[1], 128, 0); scr(&v[2], 128, 128); scr(&v[3], 0, 128);
  col(&v[0], 200, 220, 235, 255); col(&v[1], 200, 220, 235, 255); col(&v[2], 60, 110, 190, 255); col(&v[3], 60, 110, 190, 255);
  quad(&v[0], &v[1], &v[2], &v[3]);
  sunScreen(&sx, &sy);
  sx = clampf(sx / scrW * 128.0f, 8, 120); sy = 128.0f - clampf(sy / scrH * 128.0f, 8, 120);
  grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE);
  scr(&c, sx, sy); col(&c, 255, 240, 200, 255); fan[0] = c; ptr[0] = &fan[0];
  for (i = 0; i <= 16; i++) {
    float an = 6.2831853f * (float)i / 16.0f;
    scr(&fan[i + 1], sx + 9.0f * cosf(an), sy + 4.0f * sinf(an)); col(&fan[i + 1], 120, 80, 30, 255); ptr[i + 1] = &fan[i + 1];
  }
  grDrawVertexArray(GR_TRIANGLE_FAN, 18, ptr); statTris += 16;
  blendOff();
  if (texReflZ.ok && ON(F_RTT_DEPTH)) {
    /* depth tested, drawn front to back: a near palm silhouette, then the far headland
       that must not cover it */
    V t0, t1, t2;
    grDepthBufferMode(GR_DEPTHBUFFER_ZBUFFER); grDepthBufferFunction(GR_CMP_GEQUAL); grDepthMask(FXTRUE);
    scr(&t0, 30, 126); scr(&t1, 44, 126); scr(&t2, 40, 30);
    t0.ooz = t1.ooz = t2.ooz = (float)zrange[0] * 0.5f;
    col(&t0, 50, 70, 40, 255); col(&t1, 50, 70, 40, 255); col(&t2, 70, 110, 50, 255);
    tri(&t0, &t1, &t2);
    scr(&t0, 20, 60); scr(&t1, 62, 60); scr(&t2, 41, 34);
    t0.ooz = t1.ooz = t2.ooz = (float)zrange[0] * 0.5f;
    col(&t0, 60, 120, 50, 255); col(&t1, 60, 120, 50, 255); col(&t2, 80, 140, 60, 255);
    tri(&t0, &t1, &t2);
    scr(&t0, 0, 128); scr(&t1, 128, 128); scr(&t2, 64, 78);
    t0.ooz = t1.ooz = t2.ooz = (float)zrange[0] * 0.1f;
    col(&t0, 190, 170, 120, 255); col(&t1, 190, 170, 120, 255); col(&t2, 150, 160, 120, 255);
    tri(&t0, &t1, &t2);
    xAuxBuffer(GR_BUFFER_AUXBUFFER);
    USE(F_RTT_DEPTH);
  }
  grRenderBuffer(GR_BUFFER_BACKBUFFER);
  grClipWindow(0, 0, (FxU32)scrW, (FxU32)scrH);
  USE(F_TEXBUFFER);
}

/* ------------------------------------------------------------------------------
** HUD and LFB checks
*/
static int lfbLockFails;
#ifdef ISLAND_SCENE_COVERAGE
static const unsigned char font3x5[10][5] = {
  {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,3,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7}
};
static void alphaDepthSwatch(int x, int y, float depth, FxU32 rgb, unsigned alpha)
{
  V v[4];
  scr(&v[0], (float)x, (float)y);
  scr(&v[1], (float)x + 12, (float)y);
  scr(&v[2], (float)x + 12, (float)y + 12);
  scr(&v[3], (float)x, (float)y + 12);
  for (int i = 0; i < 4; i++) {
    v[i].ooz = depth;
    col(&v[i], (float)(rgb >> 16), (float)(rgb >> 8 & 0xff), (float)(rgb & 0xff), (float)alpha);
  }
  quad(&v[0], &v[1], &v[2], &v[3]);
}

/* Glide 3.0 Programming Guide, release 021, Ch. 8 pp. 83–84.
   The comparison uses combined alpha. Rejection also preserves destination depth.
   The later, farther green quad is visible only where the near red quad failed. */
static void alphaDepthProbe(int extended)
{
  const int width = 24, x = (int)scrW - 32 - 28 * extended, y = 8;
  const float depth = (float)zrange[0];
  FxU32 pixels32[24] = {0};
  FxU16 pixels16[24] = {0};
  const FxU32 rgbMask = pix32 ? 0xffffff : pix15 ? 0x7fff : 0xffff;
  const FxU32 red = pix32 ? 0xff0000 : pix15 ? 0x7c00 : 0xf800;
  const FxU32 green = pix32 ? 0xff00 : pix15 ? 0x03e0 : 0x07e0;
  const char *mode = extended ? "VSA extended combine" : "classic combine";
  FxU32 rejected, accepted;
  FxBool read;

  grRenderBuffer(GR_BUFFER_BACKBUFFER);
  grClipWindow(0, 0, (FxU32)scrW, (FxU32)scrH);
  grCoordinateSpace(GR_WINDOW_COORDS);
  grGlideSetVertexLayout(layoutSet[0][0]);
  grDitherMode(GR_DITHER_DISABLE);
  grFogMode(GR_FOG_DISABLE);
  grChromakeyMode(GR_CHROMAKEY_DISABLE);
  if (haveStencil) {
    X.stencilMask(0);
    grDisable(GR_STENCIL_MODE_EXT);
  }
  grColorMask(FXTRUE, FXFALSE);
  grDepthBufferMode(GR_DEPTHBUFFER_ZBUFFER);
  grDepthMask(FXTRUE);
  grDepthBufferFunction(GR_CMP_ALWAYS);
  grAlphaTestFunction(GR_CMP_ALWAYS);
  blendOff();
  ccIterated();
  alphaDepthSwatch(x, y, depth * 0.25f, 0x0000ff, 255);
  alphaDepthSwatch(x + 12, y, depth * 0.25f, 0x0000ff, 255);

  grDepthBufferFunction(GR_CMP_GREATER);
  grAlphaTestFunction(GR_CMP_GREATER);
  grAlphaTestReferenceValue(0x80);
  if (extended) {
    /* The VSA expression is iterated alpha + constant alpha (0x40). */
    grConstantColorValue(0x40ffffffu);
    X.colorCombine(GR_CMBX_ITRGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                   GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.alphaCombine(GR_CMBX_ITALPHA, GR_FUNC_MODE_X, GR_CMBX_CONSTANT_ALPHA, GR_FUNC_MODE_X,
                   GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
  }
  alphaDepthSwatch(x, y, depth * 0.75f, 0xff0000, extended ? 0x40 : 0x80);
  alphaDepthSwatch(x + 12, y, depth * 0.75f, 0xff0000, extended ? 0x41 : 0x81);

  grAlphaTestFunction(GR_CMP_ALWAYS);
  ccIterated();
  alphaDepthSwatch(x, y, depth * 0.5f, 0x00ff00, 255);
  alphaDepthSwatch(x + 12, y, depth * 0.5f, 0x00ff00, 255);
  grFinish();
  read = grLfbReadRegion(GR_BUFFER_BACKBUFFER, (FxU32)x, (FxU32)y + 6, width, 1,
                         pix32 ? sizeof pixels32 : sizeof pixels16,
                         pix32 ? (void *)pixels32 : (void *)pixels16);
  rejected = (pix32 ? pixels32[6] : pixels16[6]) & rgbMask;
  accepted = (pix32 ? pixels32[18] : pixels16[18]) & rgbMask;
  check(mode, read && rejected == green && accepted == red,
        "alpha 0x80 left 0x%06x (want green 0x%06x), alpha 0x81 right 0x%06x (want red 0x%06x)",
        (unsigned)rejected, (unsigned)green, (unsigned)accepted, (unsigned)red);
}
/* draw the 4-digit number n (3x5 font, 4 px per character) into a 16/32-bit pixel buffer */
static void digits(void *buf, int bpp, int stride, int n, FxU32 fg, FxU32 bg, int w, int h)
{
  char s[16];
  int x, y, len;
  snprintf(s, sizeof s, "%04d", n % 10000);
  len = (int)strlen(s);
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++) {
      int c = x / 4, cx = x % 4, cy = y;
      int on = c < len && cx < 3 && cy < 5 && ((font3x5[s[c] - '0'][cy] >> (2 - cx)) & 1);
      FxU32 v = on ? fg : bg;
      if (bpp == 16) ((FxU16 *)((char *)buf + y * stride))[x] = (FxU16)v;
      else ((FxU32 *)((char *)buf + y * stride))[x] = v;
    }
}
#endif

static const char *controlState(int enabled)
{
  return enabled ? "ON" : "OFF";
}

static void drawHudText(const GrLfbInfo_t *li, int top, const char *const *lines, int count)
{
  static const char *font[] = {
    "25755","65656","34443","65556","74647","74644","34553","55755","72227",
    "11152","55655","44447","57755","57775","75557","75744","75573","65655",
    "34616","72222","55557","55552","55575","55255","55222","71247"
  };
  static const char *numbers[] = {
    "75557","26227","71747","71717","55711","74717","74757","71111","75757","75717"
  };
  for (int y = top; y < top + count * 18 && y < scrH; y++) {
    char *row = (char *)li->lfbPtr + y * li->strideInBytes;
    int line = (y - top) / 18, offset = (y - top) % 18;
    int length = (int)strlen(lines[line]);
    for (int x = 4; x < 8 + 8 * 33 && x < scrW; x++) {
      int column = (x - 8) / 8, gx = (x - 8) % 8 / 2;
      int c = x >= 8 && column < length ? lines[line][column] : ' ';
      const char *glyph = c >= 'A' && c <= 'Z' ? font[c - 'A'] :
                          c >= '0' && c <= '9' ? numbers[c - '0'] :
                          c == '.' ? "00002" : c == '/' ? "11244" : "00000";
      int ink = offset >= 3 && offset < 13 && gx < 3 &&
                ((glyph[(offset - 3) / 2] - '0') & (4 >> gx));
      if (pix32) ((FxU32 *)row)[x] = ink ? 0xffe9f3efu : 0xff142832u;
      else if (pix15) ((FxU16 *)row)[x] = ink ? 0xfbbdu : 0x88a6u;
      else ((FxU16 *)row)[x] = ink ? 0xef9du : 0x1146u;
    }
  }
}

static void drawControlPanel(const GrLfbInfo_t *li, int dof)
{
  char shadows[48], foam[48], keys[48];
  const char *lines[] = {shadows, foam, keys};
  snprintf(shadows, sizeof shadows, "S SHADOWS %s   F FOG %s", controlState(ON(F_SHADOW)), controlState(ON(F_FOG)));
  snprintf(foam, sizeof foam, "W FOAM %s   D FOCUS %s", controlState(ON(F_FOAM)),
           aaSamples < 2 ? "NA" : controlState(dof && ON(F_TBUF_DOF)));
  snprintf(keys, sizeof keys, "SPACE %s   H HELP   Q QUIT", paused ? "PLAY" : "PAUSE");
  drawHudText(li, 18, lines, 3);
}

static char perfRate[40] = "FPS / FBI MPIX/S SAMPLING";

static void samplePerfHud(void)
{
  static double last;
  static unsigned frames, pixelsIn, pixelsOut;
  static double pin, pout;
  struct timespec ts;
  FxI32 in, out;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return;
  double now = ts.tv_sec + ts.tv_nsec * 1.0e-9;
  if (grGet(GR_STATS_PIXELS_IN, sizeof in, &in) != sizeof in ||
      grGet(GR_STATS_PIXELS_OUT, sizeof out, &out) != sizeof out) {
    snprintf(perfRate, sizeof perfRate, "FBI PIXEL COUNTERS N/A");
    return;
  }
  if (last) {
    frames++;
    pin += ((unsigned)in - pixelsIn) & 0x00ffffffu;
    pout += ((unsigned)out - pixelsOut) & 0x00ffffffu;
    double elapsed = now - last;
    if (elapsed >= 0.5) {
      snprintf(perfRate, sizeof perfRate, "%.1f FPS   %.1f / %.1f MPIX/S",
               frames / elapsed, pin / (elapsed * 1.0e6), pout / (elapsed * 1.0e6));
      last = now;
      frames = 0;
      pin = pout = 0.0;
    }
  } else {
    last = now;
  }
  pixelsIn = (unsigned)in;
  pixelsOut = (unsigned)out;
}

#ifdef ISLAND_SCENE_COVERAGE
static void pollControls(int frame, int *dof)
{
  static unsigned char held[128];
  int key, down;
  if (!interactive) return;
  if (islandPlatform->present) islandPlatform->present(islandPlatform->context);
  while (islandPlatform->poll_key(islandPlatform->context, &key, &down)) {
    if (key < 0 || key >= (int)sizeof held || held[key] == down) continue;
    held[key] = (unsigned char)down;
    if (!down) continue;
    switch (key) {
      case 's': liveMask ^= 1ull << F_SHADOW; break;
      case 'f': liveMask ^= 1ull << F_FOG; break;
      case 'w': liveMask ^= 1ull << F_FOAM; break;
      case 'd': if (aaSamples >= 2 && ON(F_TBUF_DOF)) *dof = !*dof; break;
      case 'h': showControls = !showControls; break;
      case ' ': paused = !paused; break;
      case 'q': case 27: quitScene = 1; break;
      default: continue;
    }
    printf("controls frame %d key %d shadows %d fog %d foam %d focus %d paused %d help %d quit %d\n",
           frame, key, ON(F_SHADOW), ON(F_FOG), ON(F_FOAM), *dof, paused, showControls, quitScene);
  }
}
#endif

/* Draw the performance and controls overlay. The coverage build also writes
   the frame number through several LFB source formats. */
static void drawHud(int frame, int dof)
{
  GrLfbInfo_t li;
  if (!ON(F_HUD)) return;
#ifdef ISLAND_SCENE_COVERAGE
  static FxU16 b16[5 * 16];
  static FxU32 b32[5 * 16];
  int y, x;
  li.size = sizeof li;
  blendAlpha(); depthOff(); grFogMode(GR_FOG_DISABLE);
  grLfbConstantAlpha(0x70);
  if (grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_1555, GR_ORIGIN_UPPER_LEFT, FXTRUE, &li)) {
    for (y = 4; y < 13; y++)
      for (x = 4; x < 98; x++)
        ((FxU16 *)((char *)li.lfbPtr + y * li.strideInBytes))[x] = (FxU16)(0x8000 | (3 << 10) | (6 << 5) | 10);
    grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER);
  } else lfbLockFails++;
  blendOff();
#else
  (void)frame;
  if (!perfHud && !(interactive && showControls)) return;
#endif
  li.size = sizeof li;
  if (grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, pix32 ? GR_LFBWRITEMODE_8888 : pix15 ? GR_LFBWRITEMODE_1555 : GR_LFBWRITEMODE_565,
                GR_ORIGIN_UPPER_LEFT, FXFALSE, &li)) {
#ifdef ISLAND_SCENE_COVERAGE
    for (x = 4; x < 98; x++) {
      if (pix32) ((FxU32 *)((char *)li.lfbPtr + 13 * li.strideInBytes))[x] = 0xffd0e0f0u;
      else ((FxU16 *)((char *)li.lfbPtr + 13 * li.strideInBytes))[x] = (FxU16)0xd71e;
    }
#endif
    if (interactive && showControls) drawControlPanel(&li, dof);
    if (perfHud) {
      const char *lines[] = {perfRate, "FBI PIXELS IN / OUT", "TEXELS/S N/A"};
      drawHudText(&li, interactive && showControls ? 76 : 18, lines, 3);
    }
    grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER);
  } else lfbLockFails++;
#ifdef ISLAND_SCENE_COVERAGE
  digits(b16, 16, 32, frame, 0xffff, 0x18ca, 16, 5);                         /* 565 */
  grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 6, 6, GR_LFB_SRC_FMT_565, 16, 5, FXFALSE, 32, b16);
  digits(b16, 16, 32, frame, 0x7fe0, 0x0c65, 16, 5);                         /* 555 */
  grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 24, 6, GR_LFB_SRC_FMT_555, 16, 5, FXFALSE, 32, b16);
  digits(b16, 16, 32, frame, 0xfc1f, 0x8c65, 16, 5);                         /* 1555 */
  grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 42, 6, GR_LFB_SRC_FMT_1555, 16, 5, FXFALSE, 32, b16);
  digits(b32, 32, 64, frame, 0xff80ffa0u, 0xff183050u, 16, 5);              /* 8888 */
  grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 60, 6, GR_LFB_SRC_FMT_8888, 16, 5, FXFALSE, 64, b32);
  if (pix32 && ON(F_LFB_EXT)) {
    /* the count once more through the pixel pipeline: 8888 source, alpha blended (alpha
       from the pixels), with a constant stencil value for the written pixels */
    if (X.lfbConstStencil) X.lfbConstStencil(0x00);
    blendAlpha(); depthOff();
    digits(b32, 32, 64, frame, 0xe0ffe080u, 0x50102040u, 16, 5);
    grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 78, 6, GR_LFB_SRC_FMT_8888, 16, 5, FXTRUE, 64, b32);
    blendOff();
    USE(F_LFB_EXT);
  }
#endif
  USE(F_HUD);
}

/* write-then-read round trips.  A write lock in the frame-buffer format without the pixel
   pipeline returns the raw (linear/tiled) aperture: that round trip must match exactly.
   grLfbWriteRegion always locks GR_LFB_WRITE_ONLY_EXPLICIT_EXT, i.e. the 3D LFB aperture
   (lfbMode format, 4096/8192-byte rows); in the frame-buffer format with the pipeline off
   that is a bypass write and must round-trip too.  Format conversions need a backend that
   implements the LFB pixel path, so they are INFO lines. */
/* The 3D LFB is in memBaseAddr0 (BAR0 + 16 MB), and memBaseAddr0 writes are not snooped
   (VSA-100 spec 16.24, cfgInitEnable bits 12/24:15): with SLI active a grLfbWriteRegion
   (EXPLICIT lock = 3D LFB) reaches only the addressed chip, so bands owned by the other
   chips keep their old pixels.  Hard check without SLI, informational with it. */
static int sliUnitsNow = 1;
#ifdef ISLAND_SCENE_COVERAGE
static void regionResult(const char *nm, int bad)
{
  if (sliUnitsNow > 1) info(nm, bad == 0, "%d of 128 pixels differ; %d-way SLI: 3D LFB writes are not snooped to other chips", bad, sliUnitsNow);
  else if (aaSamples > 1) info(nm, bad == 0, "%d of 128 pixels differ; %d-sample AA: 3D LFB writes follow colBufferAddr, which the T-buffer mask moves between the sample buffers", bad, aaSamples);
  else if (pix15) info(nm, bad == 0, "%d of 128 pixels differ; ARGB1555 frame buffer: a 1555 3D-LFB write needs a card model with that LFB format (the C model stores 565, ZA16 and 8888 bypass writes)", bad);
  else check(nm, bad == 0, "%d of 128 pixels differ", bad);
}

static void lfbChecks(int frame)
{
  int x, y, bad = 0, n = 0;
  GrLfbInfo_t li;
  FxU32 rd32[16 * 8];
  FxU16 rd16[16 * 8], src16[16 * 8];
  int X0 = (int)scrW - 40, Y0 = (int)scrH - 20;
  char nm[64];
  if (!ON(F_LFBCHECK)) return;
  /* native: write lock pixels, read lock back */
  li.size = sizeof li;
  if (grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER, pix32 ? GR_LFBWRITEMODE_8888 : pix15 ? GR_LFBWRITEMODE_1555 : GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXFALSE, &li)) {
    for (y = 0; y < 8; y++)
      for (x = 0; x < 16; x++) {
        FxU32 v = hash32((unsigned)(x + 16 * y + frame * 977)) ;
        if (pix32) ((FxU32 *)((char *)li.lfbPtr + (Y0 + y) * li.strideInBytes))[X0 + x] = v | 0xff000000u;
        else ((FxU16 *)((char *)li.lfbPtr + (Y0 + y) * li.strideInBytes))[X0 + x] = (FxU16)v;
      }
    grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER);
  } else lfbLockFails++;
  li.size = sizeof li;
  if (grLfbLock(GR_LFB_READ_ONLY, GR_BUFFER_BACKBUFFER, GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &li)) {
    int expMode = pix32 ? GR_LFBWRITEMODE_8888 : pix15 ? GR_LFBWRITEMODE_1555 : GR_LFBWRITEMODE_565;
    for (y = 0; y < 8; y++)
      for (x = 0; x < 16; x++) {
        FxU32 v = hash32((unsigned)(x + 16 * y + frame * 977)), got;
        if (pix32) { v |= 0xff000000u; got = ((FxU32 *)((char *)li.lfbPtr + (Y0 + y) * li.strideInBytes))[X0 + x]; }
        else { v &= 0xffff; got = ((FxU16 *)((char *)li.lfbPtr + (Y0 + y) * li.strideInBytes))[X0 + x]; }
        if (got != v) bad++;
        n++;
      }
    grLfbUnlock(GR_LFB_READ_ONLY, GR_BUFFER_BACKBUFFER);
    snprintf(nm, sizeof nm, "lfb %d-bit lock write/read frame %d", pix32 ? 32 : 16, frame);
    check(nm, bad == 0, "%d of %d pixels differ", bad, n);
    snprintf(nm, sizeof nm, "lfb read lock reports %s", pix32 ? "8888" : pix15 ? "1555" : "565");
    check(nm, (int)li.writeMode == expMode, "writeMode %d", (int)li.writeMode);
  } else { lfbLockFails++; check("lfb read lock granted", 0, NULL); }
  /* native: grLfbWriteRegion then grLfbReadRegion */
  for (y = 0; y < 8; y++) for (x = 0; x < 16; x++) {
    src16[y * 16 + x] = (FxU16)hash32((unsigned)(x * 31 + y * 7 + frame));
    rd32[y * 16 + x] = 0xff000000u | hash32((unsigned)(x * 13 + y * 5 + frame));
  }
  if (pix32) {
    FxU32 src32[16 * 8];
    memcpy(src32, rd32, sizeof src32);
    memset(rd32, 0, sizeof rd32);
    grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, GR_LFB_SRC_FMT_8888, 16, 8, FXFALSE, 64, src32);
    grLfbReadRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, 16, 8, 64, rd32);
    for (bad = 0, x = 0; x < 128; x++) bad += rd32[x] != src32[x];
    snprintf(nm, sizeof nm, "lfb 32-bit region write/read (3D LFB bypass) frame %d", frame);
    regionResult(nm, bad);
    /* converted: 565 source into the 8888 buffer */
    grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, GR_LFB_SRC_FMT_565, 16, 8, FXFALSE, 32, src16);
    grLfbReadRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, 16, 8, 64, rd32);
    for (bad = 0, x = 0; x < 128; x++) {
      int r, g, b; from565(src16[x], &r, &g, &b);
      bad += (rd32[x] & 0xf8fcf8u) != ((((FxU32)r << 16) | ((FxU32)g << 8) | (FxU32)b) & 0xf8fcf8u);
    }
    snprintf(nm, sizeof nm, "lfb 565->8888 conversion frame %d", frame);
    info(nm, bad == 0, "%d of 128 pixels differ", bad);
  } else {
    memset(rd16, 0, sizeof rd16);
    grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, pix15 ? GR_LFB_SRC_FMT_1555 : GR_LFB_SRC_FMT_565, 16, 8, FXFALSE, 32, src16);
    grLfbReadRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, 16, 8, 32, rd16);
    for (bad = 0, x = 0; x < 128; x++) bad += rd16[x] != src16[x];
    snprintf(nm, sizeof nm, "lfb 16-bit region write/read (3D LFB bypass) frame %d", frame);
    regionResult(nm, bad);
    /* converted: 1555 source into the 565 buffer */
    for (x = 0; x < 128; x++) src16[x] = (FxU16)(0x8000 | (src16[x] & 0x7fff));
    grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, GR_LFB_SRC_FMT_1555, 16, 8, FXFALSE, 32, src16);
    grLfbReadRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, (FxU32)Y0 - 10, 16, 8, 32, rd16);
    for (bad = 0, x = 0; x < 128; x++) {
      FxU16 e = (FxU16)(((src16[x] >> 10 & 31) << 11) | ((src16[x] >> 5 & 31) << 6) | (src16[x] & 31));
      bad += (rd16[x] & 0xffdf) != e;
    }
    snprintf(nm, sizeof nm, "lfb 1555->565 conversion frame %d", frame);
    if (!pix15) info(nm, bad == 0, "%d of 128 pixels differ", bad);
  }
  /* the remaining source formats once each: 888, the colour+depth formats, and depth into
     the aux buffer (ZA16 on 16bpp, Z32 write lock on 32bpp); then an aux read-back */
  {
    static FxU32 w32[8 * 8];
    static FxU16 w16[8 * 8];
    int k, okw = 1;
    GrLfbInfo_t li;
    for (k = 0; k < 64; k++) { w32[k] = 0x00406080u + (FxU32)k * 0x010101u; w16[k] = (FxU16)(0x7fff - k * 37); }
    okw &= grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0, 2, GR_LFB_SRC_FMT_888, 8, 8, FXFALSE, 32, w32);
    okw &= grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0 + 8, 2, GR_LFB_SRC_FMT_565_DEPTH, 8, 8, FXFALSE, 32, w32);
    okw &= grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0 + 16, 2, GR_LFB_SRC_FMT_555_DEPTH, 8, 8, FXFALSE, 32, w32);
    okw &= grLfbWriteRegion(GR_BUFFER_BACKBUFFER, (FxU32)X0 + 24, 2, GR_LFB_SRC_FMT_1555_DEPTH, 8, 8, FXFALSE, 32, w32);
    if (!pix32) okw &= grLfbWriteRegion(GR_BUFFER_AUXBUFFER, (FxU32)X0, 12, GR_LFB_SRC_FMT_ZA16, 8, 8, FXFALSE, 16, w16);
    snprintf(nm, sizeof nm, "lfb region writes 888 / 565+Z / 555+Z / 1555+Z%s frame %d", pix32 ? "" : " / ZA16", frame);
    check(nm, okw, NULL);
    if (pix32) {
      li.size = sizeof li;
      if (X.lfbConstStencil && ON(F_LFB_EXT)) X.lfbConstStencil(0x33);       /* stencil byte of the Z32 write */
      if (grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_AUXBUFFER, GR_LFBWRITEMODE_Z32, GR_ORIGIN_UPPER_LEFT, FXFALSE, &li)) {
        for (y = 0; y < 4; y++) for (x = 0; x < 8; x++) ((FxU32 *)((char *)li.lfbPtr + (12 + y) * li.strideInBytes))[X0 + x] = 0x00800000u + (FxU32)(x + y);
        grLfbUnlock(GR_LFB_WRITE_ONLY, GR_BUFFER_AUXBUFFER);
        snprintf(nm, sizeof nm, "lfb Z32 aux write lock frame %d", frame);
        check(nm, 1, NULL);
      } else { lfbLockFails++; snprintf(nm, sizeof nm, "lfb Z32 aux write lock frame %d", frame); check(nm, 0, "refused"); }
      if (X.lfbConstStencil && ON(F_LFB_EXT)) X.lfbConstStencil(0x00);
    }
    {
      FxU32 rb[8 * 4];
      snprintf(nm, sizeof nm, "lfb aux buffer read region frame %d", frame);
      check(nm, grLfbReadRegion(GR_BUFFER_AUXBUFFER, (FxU32)X0, 12, 8, 4, 32, rb), NULL);
    }
  }
  USE(F_LFBCHECK);
}

/* ------------------------------------------------------------------------------
** setup
*/
#endif

static int hasTok(const char *ext, const char *tok)
{
  const char *p = ext;
  size_t n = strlen(tok);
  while ((p = strstr(p, tok)) != NULL) {
    if ((p == ext || p[-1] == ' ') && (p[n] == ' ' || p[n] == 0)) return 1;
    p += n;
  }
  return 0;
}

static GrScreenResolution_t parseRes(const char *s)
{
  if (!strcmp(s, "640x480"))  { scrW = 640;  scrH = 480; return GR_RESOLUTION_640x480; }
  if (!strcmp(s, "800x600"))  { scrW = 800;  scrH = 600; return GR_RESOLUTION_800x600; }
  if (!strcmp(s, "1024x768")) { scrW = 1024; scrH = 768; return GR_RESOLUTION_1024x768; }
  if (!strcmp(s, "1600x1200")) { scrW = 1600; scrH = 1200; return GR_RESOLUTION_1600x1200; }
  if (!strcmp(s, "512x384"))  { scrW = 512;  scrH = 384; return GR_RESOLUTION_512x384; }
  printf("unsupported resolution %s, using 640x480\n", s);
  scrW = 640; scrH = 480; return GR_RESOLUTION_640x480;
}

static void queryBoard(void)
{
#ifdef ISLAND_SCENE_COVERAGE
  FxI32 v = 0, rgba[4] = { 0, 0, 0, 0 }, v2[2] = { 0, 0 };
  char  m[96];
  grGet(GR_NUM_TMU, 4, &v); nTmu = v;
  check("grGet GR_NUM_TMU == 2", v == 2, "got %d", (int)v);
  v = 0; grGet(GR_MAX_TEXTURE_SIZE, 4, &v);
  check(vsa ? "grGet GR_MAX_TEXTURE_SIZE == 2048" : "grGet GR_MAX_TEXTURE_SIZE == 256", v == (vsa ? 2048 : 256), "got %d", (int)v);
  v = 0; grGet(GR_MAX_TEXTURE_ASPECT_RATIO, 4, &v);
  check("grGet GR_MAX_TEXTURE_ASPECT_RATIO == 3", v == 3, "got %d", (int)v);
  grGet(GR_BITS_RGBA, 16, rgba);
  snprintf(m, sizeof m, "grGet GR_BITS_RGBA == %s", pix32 ? "8,8,8,8" : pix15 ? "5,5,5,1" : "5,6,5,0");
  check(m, pix32 ? (rgba[0] == 8 && rgba[1] == 8 && rgba[2] == 8 && rgba[3] == 8)
                 : pix15 ? (rgba[0] == 5 && rgba[1] == 5 && rgba[2] == 5 && rgba[3] == 1)
                 : (rgba[0] == 5 && rgba[1] == 6 && rgba[2] == 5 && rgba[3] == 0),
        "got %d,%d,%d,%d", (int)rgba[0], (int)rgba[1], (int)rgba[2], (int)rgba[3]);
  v = 0; grGet(GR_BITS_DEPTH, 4, &v);
  snprintf(m, sizeof m, "grGet GR_BITS_DEPTH == %d", pix32 ? 32 : 16);
  check(m, v == (pix32 ? 32 : 16), "got %d", (int)v);
  grGet(GR_ZDEPTH_MIN_MAX, 8, (FxI32 *)zrange);
  grGet(GR_WDEPTH_MIN_MAX, 8, (FxI32 *)wrange);
  snprintf(m, sizeof m, "grGet GR_ZDEPTH_MIN_MAX == %s", pix32 ? "0xffffff,0 (24-bit Z)" : "0xffff,0");
  check(m, zrange[0] == (pix32 ? 0xffffffu : 0xffffu) && zrange[1] == 0, "got 0x%x,0x%x", (unsigned)zrange[0], (unsigned)zrange[1]);
  snprintf(m, sizeof m, "grGet GR_WDEPTH_MIN_MAX == %s", pix32 ? "0,0xffffff" : "0,0xffff");
  check(m, wrange[0] == 0 && wrange[1] == (pix32 ? 0xffffffu : 0xffffu), "got 0x%x,0x%x", (unsigned)wrange[0], (unsigned)wrange[1]);
  if (vsa) {
    /* Glide has no stencil-depth query; the PIXEXT stencil entry points stand in for it */
    check("stencil entry points (grStencilFuncExt/OpExt/MaskExt/LfbConstantStencilExt)",
          X.stencilFunc && X.stencilOp && X.stencilMask && X.lfbConstStencil, NULL);
  }
  v = 0; grGet(GR_FOG_TABLE_ENTRIES, 4, &v); fogEntries = v;
  check("grGet GR_FOG_TABLE_ENTRIES == 64", v == 64, "got %d", (int)v);
  v = 0; grGet(GR_GAMMA_TABLE_ENTRIES, 4, &v);
  check("grGet GR_GAMMA_TABLE_ENTRIES > 0", v > 0, "got %d", (int)v);
  v = 0; grGet(GR_BITS_GAMMA, 4, &v);
  check("grGet GR_BITS_GAMMA == 8", v == 8, "got %d", (int)v);
  v = 0; grGet(GR_TEXTURE_ALIGN, 4, &v); texAlign = v > 0 ? (FxU32)v : 16;
  check("grGet GR_TEXTURE_ALIGN power of two", v > 0 && (v & (v - 1)) == 0, "got %d", (int)v);
  v = 0; grGet(GR_NUM_BOARDS, 4, &v);
  check("grGet GR_NUM_BOARDS >= 1", v >= 1, "got %d", (int)v);
  v = 0; grGet(GR_NUM_FB, 4, &v);
  check("grGet GR_NUM_FB >= 1", v >= 1, "got %d", (int)v);
  v = 0; grGet(GR_MEMORY_FB, 4, &v);
  check("grGet GR_MEMORY_FB > 0", v > 0, "got %d", (int)v);
  v = 0; grGet(GR_MEMORY_TMU, 4, &v);
  check("grGet GR_MEMORY_TMU > 0", v > 0, "got %d", (int)v);
  printf("memory: tmu %d bytes\n", (int)v);
  v = 0; grGet(GR_GLIDE_STATE_SIZE, 4, &v);
  check("grGet GR_GLIDE_STATE_SIZE > 0", v > 0, "got %d", (int)v);
  v = 0; grGet(GR_GLIDE_VERTEXLAYOUT_SIZE, 4, &v);
  check("grGet GR_GLIDE_VERTEXLAYOUT_SIZE > 0", v > 0, "got %d", (int)v);
  grGet(GR_VIEWPORT, 16, rgba);
  check("grGet GR_VIEWPORT == window", rgba[2] == (FxI32)scrW && rgba[3] == (FxI32)scrH,
        "got %d,%d,%d,%d", (int)rgba[0], (int)rgba[1], (int)rgba[2], (int)rgba[3]);
  v2[0] = v2[1] = 0; grGet(GR_REVISION_FB, 4, &v2[0]); grGet(GR_REVISION_TMU, 4, &v2[1]);
  printf("revisions: fb %d tmu %d\n", (int)v2[0], (int)v2[1]);
#else
  FxI32 value = 0;
  grGet(GR_NUM_TMU, sizeof value, &value);
  nTmu = value > 1 ? 2 : 1;
  grGet(GR_ZDEPTH_MIN_MAX, sizeof zrange, (FxI32 *)zrange);
  grGet(GR_WDEPTH_MIN_MAX, sizeof wrange, (FxI32 *)wrange);
  value = 0;
  grGet(GR_FOG_TABLE_ENTRIES, sizeof value, &value);
  fogEntries = value;
  value = 0;
  grGet(GR_TEXTURE_ALIGN, sizeof value, &value);
  texAlign = value > 0 && (value & (value - 1)) == 0 ? (FxU32)value : 16;
#endif
}

static void detectFeatures(const char *ext, int aaReq)
{
  int pixext = hasTok(ext, "PIXEXT"), combine = hasTok(ext, "COMBINE"), texfmt = hasTok(ext, "TEXFMT");
  int f;
  vsa = pixext || texfmt;
  if (pixext) {
    X.winOpen = (PfnWinOpenExt)grGetProcAddress("grSstWinOpenExt");
    X.stencilFunc = (PfnStencilFunc)grGetProcAddress("grStencilFuncExt");
    X.stencilMask = (PfnStencilMask)grGetProcAddress("grStencilMaskExt");
    X.stencilOp = (PfnStencilOp)grGetProcAddress("grStencilOpExt");
    X.lfbConstStencil = (PfnLfbConstStencil)grGetProcAddress("grLfbConstantStencilExt");
    X.bufferClear = (PfnBufferClearExt)grGetProcAddress("grBufferClearExt");
    X.colorMask = (PfnColorMaskExt)grGetProcAddress("grColorMaskExt");
    X.tbufMask = (PfnTBufMask)grGetProcAddress("grTBufferWriteMaskExt");
  }
  if (combine) {
    X.colorCombine = (PfnCombineExt)grGetProcAddress("grColorCombineExt");
    X.alphaCombine = (PfnCombineExt)grGetProcAddress("grAlphaCombineExt");
    X.texColorCombine = (PfnTexCombineExt)grGetProcAddress("grTexColorCombineExt");
    X.texAlphaCombine = (PfnTexCombineExt)grGetProcAddress("grTexAlphaCombineExt");
    X.constColor = (PfnConstColorExt)grGetProcAddress("grConstantColorValueExt");
    X.blend = (PfnBlendExt)grGetProcAddress("grAlphaBlendFunctionExt");
  }
  if (hasTok(ext, "TEXCHROMA")) {
    X.texChromaMode = (PfnTexChromaMode)grGetProcAddress("grTexChromaModeExt");
    X.texChromaRange = (PfnTexChromaRange)grGetProcAddress("grTexChromaRangeExt");
  }
  if (hasTok(ext, "CHROMARANGE")) {
    X.chromaRangeMode = (PfnChromaRangeMode)grGetProcAddress("grChromaRangeModeExt");
    X.chromaRange = (PfnChromaRange)grGetProcAddress("grChromaRangeExt");
  }
  if (hasTok(ext, "TEXTUREBUFFER")) X.textureBuffer = (PfnTextureBuffer)grGetProcAddress("grTextureBufferExt");
  if (hasTok(ext, "GETGAMMA")) X.getGamma = (PfnGetGamma)grGetProcAddress("grGetGammaTableExt");
  X.tableExt = (PfnTableExt)grGetProcAddress("grTexDownloadTableExt");
#ifdef ISLAND_SCENE_COVERAGE
  if (!X.tableExt && grTexDownloadTableExt) X.tableExt = (PfnTableExt)grTexDownloadTableExt;
#endif
  xPartialRow = (PfnPartialRowX)grGetProcAddress("grTexDownloadMipMapLevelPartialRowExt");
  if (hasTok(ext, "TEXTUREBUFFER")) {
    xTexAux = (PfnTextureBuffer)grGetProcAddress("grTextureAuxBufferExt");
    xAuxBuffer = (PfnAuxBufferX)grGetProcAddress("grAuxBufferExt");
  }

  for (f = 0; f < F_COUNT; f++) featDet[f] = 1;              /* core Glide 3 */
  featDet[F_COMBINE_EXT] = X.colorCombine && X.alphaCombine && X.texColorCombine && X.texAlphaCombine && X.constColor;
  featDet[F_BLEND_EXT] = X.blend != NULL;
  featDet[F_MIRROR] = hasTok(ext, "TEXMIRROR");
  featDet[F_TEXCHROMA] = X.texChromaMode && X.texChromaRange;
  featDet[F_CHROMARANGE] = X.chromaRangeMode && X.chromaRange;
  featDet[F_PIX32] = X.winOpen != NULL;
  featDet[F_COLORMASK] = 1;                                   /* grColorMask; grColorMaskExt on PIXEXT */
  featDet[F_TEX8888] = featDet[F_FXT1] = featDet[F_DXT] = texfmt;
  featDet[F_TBUFFER] = X.tbufMask != NULL;
  featDet[F_TBUF_DOF] = X.tbufMask != NULL;
  if (!featDet[F_TBUF_DOF]) note(F_TBUF_DOF, "no PIXEXT (grTBufferWriteMaskExt)");
  else note(F_TBUF_DOF, "only with -d in an AA mode");
  featDet[F_TEXBUFFER] = X.textureBuffer != NULL;
  featDet[F_GAMMA] = 1;                                       /* guGammaCorrectionRGB; read-back needs GETGAMMA */
  featDet[F_SHADOW] = 1;                                      /* stencil on PIXEXT, blended decals otherwise */
  featDet[F_CMBX_FX] = featDet[F_COMBINE_EXT];
  featDet[F_BLEND_OPS] = X.blend != NULL;
  featDet[F_STENCIL_OPS] = X.stencilOp != NULL;
  featDet[F_TBUF_SAMPLE] = X.tbufMask != NULL;
  featDet[F_TEX_YUV] = texfmt;
  featDet[F_UMA] = hasTok(ext, "TEXUMA");
  featDet[F_TEXCHROMA_RANGE] = featDet[F_TEXCHROMA];
  featDet[F_RTT_DEPTH] = xTexAux && xAuxBuffer;
  featDet[F_COLORMASK_EXT] = X.colorMask != NULL;
  featDet[F_LFB_EXT] = X.lfbConstStencil != NULL;
  if (!featDet[F_CMBX_FX]) note(F_CMBX_FX, "no COMBINE extension: glitter as one classic modulated pass, no embossed wet sand");
  if (!featDet[F_BLEND_OPS]) note(F_BLEND_OPS, "no COMBINE extension: alpha-blended jetty shadow, plain orange buoy, no flare ghosts");
  if (!featDet[F_STENCIL_OPS]) note(F_STENCIL_OPS, "no PIXEXT stencil");
  if (!featDet[F_TBUF_SAMPLE]) note(F_TBUF_SAMPLE, "no PIXEXT (grTBufferWriteMaskExt)");
  else if (aaReq < 2) note(F_TBUF_SAMPLE, "only in the AA run modes");
  if (!texfmt) note(F_TEX_YUV, "no TEXFMT extension");
  else note(F_TEX_YUV, "download only: h5 grTexSource has no textureMode code for 0x13-0x15 (they overflow the 4-bit tformat field)");
  if (!featDet[F_UMA]) note(F_UMA, "no TEXUMA: the atlas is read by TMU0 only");
  if (!featDet[F_TEXCHROMA_RANGE]) note(F_TEXCHROMA_RANGE, "no TEXCHROMA extension: the pinwheel keeps its cream blades");
  if (!featDet[F_RTT_DEPTH]) note(F_RTT_DEPTH, "no TEXTUREBUFFER extension");
  if (!featDet[F_COLORMASK_EXT]) note(F_COLORMASK_EXT, "no PIXEXT (grColorMaskExt)");
  else note(F_COLORMASK_EXT, "32bpp only");
  if (!featDet[F_LFB_EXT]) note(F_LFB_EXT, "no PIXEXT (8888 pipeline writes, grLfbConstantStencilExt)");
  else note(F_LFB_EXT, "32bpp only");
  if (!vsa) note(F_OGL_SMOOTH, "h3: GR_AA_ORDERED for all primitives");
  haveFogCoord = hasTok(ext, "FOGCOORD") && !getenv("SCENE_NO_FOGCOORD");   /* env: diagnosis only */
  if (!hasTok(ext, "PALETTE6666")) note(F_TREES, "no PALETTE6666: P8 leaves use the plain palette");
  if (!featDet[F_COMBINE_EXT]) note(F_COMBINE_EXT, "no COMBINE extension: classic grTexCombine/grColorCombine used");
  if (!featDet[F_BLEND_EXT]) note(F_BLEND_EXT, "no COMBINE extension (grAlphaBlendFunctionExt)");
  if (!featDet[F_PIX32]) note(F_PIX32, "no PIXEXT: 16bpp colour, 16-bit depth, no stencil");
  if (!texfmt) {
    note(F_TEX8888, "no TEXFMT extension"); note(F_FXT1, "no TEXFMT extension"); note(F_DXT, "no TEXFMT extension");
    note(F_BIGTEX, "no TEXFMT: limited to 256x256");
  }
  if (!featDet[F_TBUFFER]) note(F_TBUFFER, "no PIXEXT (grTBufferWriteMaskExt)");
  else if (aaReq < 2) note(F_TBUFFER, "only in the AA run mode (-a 2|4)");
  if (!featDet[F_TEXBUFFER]) note(F_TEXBUFFER, "no TEXTUREBUFFER extension: procedural sparkle layer instead");
  if (!X.getGamma) note(F_GAMMA, "no GETGAMMA: gamma set, read-back skipped");
  if (!featDet[F_TEXCHROMA]) note(F_TEXCHROMA, "no TEXCHROMA extension");
  if (!featDet[F_MIRROR]) note(F_MIRROR, "no TEXMIRROR extension");
  if (!haveFogCoord) note(F_FOG, "no FOGCOORD: fog table on Q");
}

static void gammaSetup(void)
{
  FxI32 n = 0;
  if (!ON(F_GAMMA)) return;
  guGammaCorrectionRGB(1.25f, 1.2f, 1.15f);
  USE(F_GAMMA);
  grGet(GR_GAMMA_TABLE_ENTRIES, 4, &n);
  if (X.getGamma && n > 0 && n <= 1024) {
    FxU32 *r = (FxU32 *)calloc((size_t)n, 4), *g = (FxU32 *)calloc((size_t)n, 4), *b = (FxU32 *)calloc((size_t)n, 4);
    int i, mono = 1;
    X.getGamma((FxU32)n, r, g, b);
    for (i = 1; i < n; i++) if (r[i] < r[i - 1] || g[i] < g[i - 1] || b[i] < b[i - 1]) mono = 0;
    info("gamma table read back monotonic, rising to 255", mono && r[0] < r[n - 1] && r[n - 1] == 255 && g[n - 1] == 255 && b[n - 1] == 255,
         "r[0]=%u r[%d]=%u g[%d]=%u", (unsigned)r[0], (int)n - 1, (unsigned)r[n - 1], (int)n - 1, (unsigned)g[n - 1]);
    free(r); free(g); free(b);
  }
}

#ifdef ISLAND_SCENE_COVERAGE
static void api(const char *n);
#else
#define api(name) ((void)0)
#endif

/* ------------------------------------------------------------------------------
** VSA corpus elements (h5_coverage.md section 7).  Each is a thing on the beach; each
** falls back to a classic-Glide version (or is left out) where the extension is missing.
*/
static void cmbxLeave(void)            /* back to one classic texture unit */
{
  grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE);
  tmu1Off();
  ccTexMod();
}
static void tmu1Through(void)          /* TMU1's texel out through TMU0 */
{
  tmuPass(GR_TMU1);
  grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, FXFALSE, FXFALSE);
}

/* sun glitter on the water: a tapering band from the eye toward the sun, a soft sheen
   (iterated colour) plus the sparkle texture.  Combine extension: the sparkles are tinted
   by TMU1's own constant colour and scaled by 1 - LOD fraction in TMU0, and the colour
   combine output (texture + iterated) is shifted 4x on the near half and 2x on the far half
   (vertex colours pre-divided, so the sheen is continuous and only the glints burn out);
   drawn twice through per-channel colour masks (red left, green+blue right: a chromatic
   fringe); additive, fogged with GR_FOG_ADD2 (attenuate only: no fog colour is added by a
   second pass).  Classic boards: one texture + iterated additive pass. */
static void drawGlitter(void)
{
  enum { N = 10 };
  V row[3][N + 1], strip[2 * (N + 1)];
  float d[3], l, px, pz;
  int i, k, pass, npass, part;
  int ext = ON(F_CMBX_FX) && featDet[F_COMBINE_EXT];
  if (!ON(F_SEA_MT) || nTmu < 2 || !texSparkle.ok) return;
  sunDir(d); l = sqrtf(d[0] * d[0] + d[2] * d[2]); d[0] /= l; d[2] /= l; px = -d[2]; pz = d[0];
  grGlideSetVertexLayout(layoutTex2);
  source(&texSparkle);
  grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU1, GR_MIPMAP_NEAREST, FXFALSE);
  grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  if (ext) {
    X.constColor(GR_TMU1, 0xff90d8ffu);             /* ABGR: warm gold */
    X.texColorCombine(GR_TMU1, GR_CMBX_LOCAL_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                      GR_CMBX_TMU_CCOLOR, FXFALSE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.texAlphaCombine(GR_TMU1, GR_CMBX_LOCAL_TEXTURE_ALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                      GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.texColorCombine(GR_TMU0, GR_CMBX_OTHER_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                      GR_CMBX_LOD_FRAC, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.texAlphaCombine(GR_TMU0, GR_CMBX_OTHER_TEXTURE_ALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                      GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    X.alphaCombine(GR_CMBX_ITALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO,
                   GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
    USE(F_CMBX_FX);
  } else {
    tmu1Through();
    grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
    grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE);
  }
  depthOn(0);
  if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -24 : 24);
  grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE);
  if (ON(F_FOG) && ON(F_FOG2)) {
    grFogMode((fogCoordNow ? GR_FOG_WITH_TABLE_ON_FOGCOORD_EXT : GR_FOG_WITH_TABLE_ON_Q) | GR_FOG_ADD2);
    USE(F_FOG2);
  } else grFogMode(GR_FOG_DISABLE);
  npass = (pix32 && X.colorMask && ON(F_COLORMASK_EXT)) ? 2 : 1;
  for (pass = 0; pass < npass; pass++) {
    float off = npass == 2 ? (pass ? 0.10f : -0.10f) : 0.0f;
    if (npass == 2) { X.colorMask(pass == 0, pass == 1, pass == 1, FXFALSE); USE(F_COLORMASK_EXT); }
    for (i = 0; i <= N; i++) {
      float dist = 9.0f * powf(45.0f, (float)i / (float)N), w = 0.8f + dist * 0.09f;
      float fade = sinf(3.14159f * ((float)i + 0.5f) / (float)(N + 1));
      for (k = 0; k < 3; k++) {
        float lat = (float)(k - 1) * w + off * (1.0f + dist * 0.05f);
        float x = cam.eye[0] + d[0] * dist + px * lat, z = cam.eye[2] + d[2] * dist + pz * lat;
        V *v = &row[k][i];
        wv(v, x, waveH(x, z) + 0.30f, z, 175, 165, 135, k == 1 ? 200.0f * fade : 0.0f, x * 80.0f + tNow * 40.0f, z * 80.0f - tNow * 25.0f);
        st1(v, x * 80.0f + tNow * 40.0f, z * 80.0f - tNow * 25.0f);
      }
    }
    for (part = 0; part < 2; part++) {
      int i0 = part ? N / 2 : 0, i1 = part ? N : N / 2, n = i1 - i0 + 1;
      if (ext)
        X.colorCombine(GR_CMBX_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ITRGB, GR_FUNC_MODE_X,
                       GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, part ? 1 : 2, FXFALSE);
      for (k = 0; k < 2; k++) {
        for (i = 0; i < n; i++) { strip[2 * i] = row[k][i0 + i]; strip[2 * i + 1] = row[k + 1][i0 + i]; }
        if (ext) for (i = 0; i < 2 * n; i++) { float sc = part ? 0.5f : 0.25f; strip[i].r *= sc; strip[i].g *= sc; strip[i].b *= sc; }
        grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, (FxU32)(2 * n), strip, sizeof(V));
        statTris += (unsigned long)(2 * n - 2);
      }
    }
  }
  if (npass == 2) { X.colorMask(FXTRUE, FXTRUE, FXTRUE, FXFALSE); grColorMask(FXTRUE, FXFALSE); }
  grDepthBiasLevel(0);
  blendOff();
  grFogMode(GR_FOG_DISABLE);
  cmbxLeave();
}

/* the jetty's shadow on the water, subtractive: dst - src (GR_BLEND_OP_REVSUB) takes the
   warm light out of the sea under the deck.  Without the blend extension: an alpha decal. */
static void drawDeckShadow(const float *light)
{
  enum { LENGTH = 16, WIDTH = 6 };
  float deck[4][3];
  V rows[2][WIDTH + 1];
  jettyDeck(deck);
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOn(0); fogOn(1); blendAlpha();
  if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -4 : 4);
  for (int i = 0; i <= LENGTH; i++) {
    float u = (float)i / LENGTH;
    for (int j = 0; j <= WIDTH; j++) {
      float v = (float)j / WIDTH;
      float a = u * 1.06f - 0.03f, b = v * 1.34f - 0.17f;
      float p[3];
      for (int axis = 0; axis < 3; axis++) {
        float left = deck[0][axis] + a * (deck[1][axis] - deck[0][axis]);
        float right = deck[3][axis] + a * (deck[2][axis] - deck[3][axis]);
        p[axis] = left + b * (right - left);
      }
      shadowReceiver(p, light, 1);
      float edge = fminf(fminf(u, 1.0f - u) * LENGTH, fminf(v, 1.0f - v) * WIDTH);
      float shore = clampf((waveH(p[0], p[2]) - terrainHeight(p[0], p[2])) * 4.0f, 0, 1);
      wv(&rows[i & 1][j], p[0], p[1], p[2], 13, 34, 57, 104 * clampf(edge, 0, 1) * shore, 0, 0);
    }
    if (i) {
      V *prev = rows[(i - 1) & 1], *next = rows[i & 1];
      for (int j = 0; j < WIDTH; j++) quad(&prev[j], &next[j], &next[j + 1], &prev[j + 1]);
    }
  }
  grDepthBiasLevel(0);
  blendOff();
}

static void drawJettyShadow(void)
{
  float cs = cosf(jettyA), sn = sinf(jettyA), r0 = coastR(jettyA) - 0.4f, r1 = coastR(jettyA) + 9.0f, w = 1.0f;
  float sd[3], l, ox, oz, a[3], b[3], c[3], d[3];
  int sub = ON(F_BLEND_OPS) && X.blend != NULL;
  if (!ON(F_SHADOW)) return;
  if (highDetail) {
    drawDeckShadow(sunD);
    return;
  }
  sunDir(sd); l = sqrtf(sd[0] * sd[0] + sd[2] * sd[2]); ox = -sd[0] / l * 0.9f; oz = -sd[2] / l * 0.9f;
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0);
  grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE);
  grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE);
  depthOn(0); fogOn(0);
  if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -6 : 6);
  if (sub) {
    grConstantColorValue(0xff142838u);              /* ABGR: subtract r 0x38, g 0x28, b 0x14 */
    X.blend(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_OP_REVSUB, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_OP_ADD);
    USE(F_BLEND_OPS);
  } else { grConstantColorValue(0x40182030u); blendAlpha(); }
  a[0] = r0 * cs - w * sn + ox; a[2] = r0 * sn + w * cs + oz; a[1] = 0.10f;
  b[0] = r1 * cs - w * sn + ox; b[2] = r1 * sn + w * cs + oz; b[1] = 0.10f;
  c[0] = r1 * cs + w * sn + ox; c[2] = r1 * sn - w * cs + oz; c[1] = 0.10f;
  d[0] = r0 * cs + w * sn + ox; d[2] = r0 * sn - w * cs + oz; d[1] = 0.10f;
  wquad(a, b, c, d, 0, 0, 255, 255, 255, 255, 255, 255);
  grDepthBiasLevel(0);
  blendOff();
}

/* a marker buoy off the beach.  Its float is drawn src - dst (GR_BLEND_OP_SUB) in near
   white, so it always carries the complement of the water behind it (orange on blue); the
   pennant is a smoothed triangle (OpenGL-style per-primitive enable on h5).  Classic
   boards: a plain orange float. */
static void drawBuoy(void)
{
  float a = 0.47f, r = coastR(a) + 6.5f, x = r * cosf(a), z = r * sinf(a), y = waveH(x, z) + 0.05f;
  float rx = cam.r[0] * 0.26f, rz = cam.r[2] * 0.26f, tilt = 0.12f * sinf(tNow * 1.6f);
  int sub = ON(F_BLEND_OPS) && X.blend != NULL;
  V v[5], m0, m1, f0, f1, f2;
  if (!ON(F_PARTICLES)) return;
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOn(1); fogOn(0);
  if (sub) { X.blend(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_OP_SUB, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_OP_ADD); USE(F_BLEND_OPS); }
  else blendOff();
  wv(&v[0], x - rx, y, z - rz, 255, 250, 240, 255, 0, 0);
  wv(&v[1], x + rx, y, z + rz, 255, 250, 240, 255, 0, 0);
  wv(&v[2], x + rx * 0.3f + tilt, y + 0.55f, z + rz * 0.3f, 255, 250, 240, 255, 0, 0);
  wv(&v[3], x - rx * 0.3f + tilt, y + 0.55f, z - rz * 0.3f, 255, 250, 240, 255, 0, 0);
  if (!sub) { int k; for (k = 0; k < 4; k++) col(&v[k], 235, 120, 45, 255); }
  quad(&v[0], &v[1], &v[2], &v[3]);
  blendOff();
  /* mast (a line) and pennant */
  wv(&m0, x + tilt, y + 0.55f, z, 60, 50, 40, 255, 0, 0);
  wv(&m1, x + 3.0f * tilt, y + 1.9f, z, 60, 50, 40, 255, 0, 0);
  wv(&f0, x + 3.0f * tilt, y + 1.9f, z, 225, 40, 35, 255, 0, 0);
  wv(&f1, x + 2.4f * tilt, y + 1.45f, z, 225, 40, 35, 255, 0, 0);
  wv(&f2, x + 2.7f * tilt + cam.r[0] * 0.7f, y + 1.68f + 0.06f * sinf(tNow * 5.0f), z + cam.r[2] * 0.7f, 190, 30, 30, 255, 0, 0);
  blendAlpha();
  if (vsa && ON(F_OGL_SMOOTH)) {
    grEnable(GR_AA_ORDERED_LINES_OGL); grEnable(GR_AA_ORDERED_TRIANGLES_OGL);
    grDrawLine(&m0, &m1); statLines++;
    grDrawTriangle(&f0, &f1, &f2); statTris++;
    grDisable(GR_AA_ORDERED_LINES_OGL); grDisable(GR_AA_ORDERED_TRIANGLES_OGL);
    USE(F_OGL_SMOOTH);
  } else {
    grEnable(GR_AA_ORDERED);
    grDrawLine(&m0, &m1); statLines++;
    grAADrawTriangle(&f0, &f1, &f2, FXTRUE, FXTRUE, FXTRUE); statTris++;
    grDisable(GR_AA_ORDERED);
  }
  blendOff();
}

/* shells along the waterline from one atlas.  With unified texture memory (TEXUMA) the
   atlas is sourced at the same address on both TMUs: even shells through TMU0, odd shells
   through TMU1.  Then the sunglasses left on the towel: two lenses blended dst * dst
   (factor GR_BLEND_SAME_COLOR_EXT on the destination), i.e. tinted glass. */
static void atlasDecal(float x, float z, float h, float ang, int cell, int tmu)
{
  float c = cosf(ang), s = sinf(ang), p[4][3], s0 = (float)(cell & 1) * 128.0f + 2.0f, t0 = (float)(cell >> 1) * 128.0f + 2.0f;
  static const float sx[4] = { -1, 1, 1, -1 }, sz[4] = { -1, -1, 1, 1 };
  V v[4];
  int k;
  for (k = 0; k < 4; k++) {
    p[k][0] = x + sx[k] * h * c - sz[k] * h * s; p[k][2] = z + sx[k] * h * s + sz[k] * h * c;
    p[k][1] = fmaxf(islandH(p[k][0], p[k][2]), 0.0f) + 0.04f;
    wv(&v[k], p[k][0], p[k][1], p[k][2], 255, 255, 255, 255, s0 + (sx[k] > 0 ? 124.0f : 0.0f), t0 + (sz[k] > 0 ? 124.0f : 0.0f));
    if (tmu) st1(&v[k], s0 + (sx[k] > 0 ? 124.0f : 0.0f), t0 + (sz[k] > 0 ? 124.0f : 0.0f));
  }
  quad(&v[0], &v[1], &v[2], &v[3]);
}
static void drawShells(void)
{
  int k, uma = umaOk && ON(F_UMA);
  if (!texAtlas.ok) return;
  depthOn(0); fogOn(1); blendAlpha();
  if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -12 : 12);
  if (uma) grEnable(GR_TEXTURE_UMA_EXT);
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod();
  source(&texAtlas);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_DISABLE, FXFALSE);
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  for (k = 0; k < 10; k += uma ? 2 : 1) {
    float a = 0.12f + 0.15f * (float)k, r = coastR(a) - 0.7f - 1.6f * hashf((unsigned)k, 71);
    atlasDecal(r * cosf(a), r * sinf(a), 0.24f + 0.08f * hashf((unsigned)k, 72), 6.28f * hashf((unsigned)k, 73), k & 3, 0);
  }
  if (uma) {
    /* the same atlas address, now read by TMU1 */
    grGlideSetVertexLayout(layoutTex2);
    grTexSource(GR_TMU1, texAtlas.addr, texAtlas.mask, &texAtlas.info);
    grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
    grTexMipMapMode(GR_TMU1, GR_MIPMAP_DISABLE, FXFALSE);
    grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
    tmu1Through();
    for (k = 1; k < 10; k += 2) {
      float a = 0.12f + 0.15f * (float)k, r = coastR(a) - 0.7f - 1.6f * hashf((unsigned)k, 71);
      atlasDecal(r * cosf(a), r * sinf(a), 0.24f + 0.08f * hashf((unsigned)k, 72), 6.28f * hashf((unsigned)k, 73), k & 3, 1);
    }
    grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
    tmu1Off(); tmuPass(GR_TMU0);
    grDisable(GR_TEXTURE_UMA_EXT);
    grGlideSetVertexLayout(layoutTex);
    USE(F_UMA);
  }
  grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  /* sunglasses on the first towel */
  {
    float a = 0.45f, r = coastR(a) * 0.8f, x = r * cosf(a) + 0.25f, z = r * sinf(a) + 0.5f;
    int same = ON(F_BLEND_OPS) && X.blend != NULL;
    grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE);
    grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE);
    grConstantColorValue(0xa0201810u);
    if (ON(F_DEPTHBIAS)) grDepthBiasLevel(useW ? -14 : 14);
    if (same) { X.blend(GR_BLEND_ZERO, GR_BLEND_SAME_COLOR_EXT, GR_BLEND_OP_ADD, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_OP_ADD); USE(F_BLEND_OPS); }
    else blendAlpha();
    decal(x - 0.14f * sinf(a), z + 0.14f * cosf(a), 0.11f, 0.09f, a + 0.2f, 0.07f, 255, 255, 255, 255);
    decal(x + 0.14f * sinf(a), z - 0.14f * cosf(a), 0.11f, 0.09f, a + 0.2f, 0.07f, 255, 255, 255, 255);
  }
  grDepthBiasLevel(0);
  blendOff();
}

/* a fishing net hung to dry between two posts by the jetty: screen-door transparency.
   Left half GR_STIPPLE_PATTERN (a diamond mesh), right half GR_STIPPLE_ROTATE. */
static void drawNet(void)
{
  float a0 = -0.02f, a1 = -0.17f, r = coastR(-0.1f) * 0.86f, p[2][3], m[3], q0[3], q1[3], q2[3], q3[3];
  int k;
  if (!ON(F_STIPPLE)) return;
  p[0][0] = r * cosf(a0); p[0][2] = r * sinf(a0); p[0][1] = islandH(p[0][0], p[0][2]);
  p[1][0] = r * cosf(a1); p[1][2] = r * sinf(a1); p[1][1] = islandH(p[1][0], p[1][2]);
  m[0] = 0.5f * (p[0][0] + p[1][0]); m[2] = 0.5f * (p[0][2] + p[1][2]); m[1] = 0.5f * (p[0][1] + p[1][1]);
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOn(1); blendOff(); fogOn(1);
  grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR);
  grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE);
  source(&texPost);
  for (k = 0; k < 2; k++) {
    q0[0] = p[k][0] - 0.05f; q0[2] = p[k][2]; q0[1] = p[k][1] + 1.75f; q1[0] = p[k][0] + 0.05f; q1[2] = p[k][2]; q1[1] = q0[1];
    q2[0] = q1[0]; q2[2] = q1[2]; q2[1] = p[k][1] - 0.2f; q3[0] = q0[0]; q3[2] = q0[2]; q3[1] = q2[1];
    wquad(q0, q1, q2, q3, 0, 0, 32, 255, 215, 205, 190, 255);
  }
  grGlideSetVertexLayout(layoutCol);
  ccIterated();
  for (k = 0; k < 2; k++) {
    V v[4];
    float sag = 0.22f + 0.04f * sinf(tNow * 1.3f);
    const float *e = p[k];
    grStipplePattern(k ? 0xa5a5a5a5u : 0x88552255u);
    grStippleMode(k ? GR_STIPPLE_ROTATE : GR_STIPPLE_PATTERN);
    wv(&v[0], e[0], e[1] + 1.65f, e[2], 205, 190, 150, 255, 0, 0);
    wv(&v[1], m[0], m[1] + 1.65f - sag, m[2], 195, 180, 140, 255, 0, 0);
    wv(&v[2], m[0], m[1] + 0.45f, m[2], 150, 140, 110, 255, 0, 0);
    wv(&v[3], e[0], e[1] + 0.35f, e[2], 160, 150, 118, 255, 0, 0);
    quad(&v[0], &v[1], &v[2], &v[3]);
  }
  grStippleMode(GR_STIPPLE_DISABLE);
  USE(F_STIPPLE);
}

/* a toy pinwheel in the sand: a P8 disc on TMU1 decoded through TMU1's own palette (the
   parasol's P8 on TMU0 keeps the sunset palette), the cream blades removed by a texture
   chroma range on TMU1; it spins by rotating the texture coordinates. */
static void drawPinwheel(void)
{
  float a = 0.80f, r = coastR(a) * 0.87f, x = r * cosf(a), z = r * sinf(a), y = islandH(x, z);
  float p0[3], p1[3], p2[3], p3[3], rot = tNow * 3.0f;
  V fan[14];
  void *ptr[14];
  int k;
  if (!texPin.ok || !ON(F_TMU_TABLES)) return;
  grGlideSetVertexLayout(layoutTex);
  tmu1Off(); tmuPass(GR_TMU0); ccTexMod(); depthOn(1); blendOff(); fogOn(1);
  source(&texPost);
  p0[0] = x - 0.03f; p0[2] = z; p0[1] = y + 1.25f; p1[0] = x + 0.03f; p1[2] = z; p1[1] = y + 1.25f;
  p2[0] = x + 0.03f; p2[2] = z; p2[1] = y - 0.1f; p3[0] = x - 0.03f; p3[2] = z; p3[1] = y - 0.1f;
  wquad(p0, p1, p2, p3, 0, 0, 32, 255, 240, 235, 225, 255);
  if (X.tableExt) { X.tableExt(GR_TMU1, GR_TEXTABLE_PALETTE, &palPin); USE(F_TMU_TABLES); }
  else { grTexDownloadTable(GR_TEXTABLE_PALETTE, &palPin); note(F_TMU_TABLES, "no grTexDownloadTableExt: one palette for both TMUs, swapped per object"); USE(F_TMU_TABLES); }
  grGlideSetVertexLayout(layoutTex2);
  source(&texPin);
  tmu1Through();
  grTexFilterMode(GR_TMU1, GR_TEXTUREFILTER_POINT_SAMPLED, GR_TEXTUREFILTER_POINT_SAMPLED);
  grTexMipMapMode(GR_TMU1, GR_MIPMAP_DISABLE, FXFALSE);
  grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_CLAMP, GR_TEXTURECLAMP_CLAMP);
  if (ON(F_TEXCHROMA_RANGE) && X.texChromaMode) {
    X.texChromaRange(GR_TMU1, 0xffd0e8f0u, 0xffe8f8ffu, GR_TEXCHROMARANGE_RGB_ALL_EXT);     /* ABGR: the cream entry +-8 */
    X.texChromaMode(GR_TMU1, GR_TEXCHROMA_ENABLE_EXT);
    USE(F_TEXCHROMA_RANGE);
  }
  wv(&fan[0], x, y + 1.25f, z, 255, 255, 255, 255, 128, 128); st1(&fan[0], 128, 128); ptr[0] = &fan[0];
  for (k = 0; k <= 12; k++) {
    float an = 6.2831853f * (float)k / 12.0f, s = 128.0f + 124.0f * cosf(an + rot), t = 128.0f + 124.0f * sinf(an + rot);
    wv(&fan[k + 1], x + cam.r[0] * 0.42f * cosf(an), y + 1.25f + 0.42f * sinf(an), z + cam.r[2] * 0.42f * cosf(an), 255, 255, 255, 255, s, t);
    st1(&fan[k + 1], s, t);
    ptr[k + 1] = &fan[k + 1];
  }
  grDrawVertexArray(GR_TRIANGLE_FAN, 14, ptr); statTris += 12;
  if (featUsed[F_TEXCHROMA_RANGE]) X.texChromaMode(GR_TMU1, GR_TEXCHROMA_DISABLE_EXT);
  grTexClampMode(GR_TMU1, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP);
  tmu1Off(); tmuPass(GR_TMU0);
  grGlideSetVertexLayout(layoutTex);
}

/* gulls: clip-space geometry.  grCoordinateSpace(GR_CLIP_COORDS) with a viewport over the
   upper part of the window and its own depth range; wings are smoothed lines, the body a
   smoothed point (per-primitive enables on h5).  In the T-buffer modes each sample buffer
   (masks 1, 2, 4, 8 ...) gets its own wing phase: motion-blurred wing beats. */
static void gullSet(float phase)
{
  int i;
  for (i = 0; i < 5; i++) {
    V b, l, r;
    float x = -0.78f + 0.36f * (float)i + 0.06f * sinf(tNow * 0.4f + (float)i * 1.7f);
    float y = -0.45f + 0.30f * hashf((unsigned)i, 81) * 2.0f + 0.05f * sinf(tNow * 0.6f + (float)i);
    float flap = 0.10f * sinf(phase * 7.0f + (float)i * 2.1f), span = 0.035f + 0.012f * hashf((unsigned)i, 82);
    memset(&b, 0, sizeof b);
    b.x = x; b.y = y; b.ooz = 0.5f; b.oow = 1.0f; b.fog = 1.0f; col(&b, 70, 72, 84, 255);
    l = b; r = b;
    l.x = x - span; l.y = y - 0.05f - flap; r.x = x + span; r.y = y - 0.05f - flap;
    col(&l, 110, 112, 124, 255); col(&r, 110, 112, 124, 255);
    grDrawLine(&l, &b); grDrawLine(&b, &r); statLines += 2;
    grDrawPoint(&b); statPoints++;
  }
}
static void drawGulls(int frame)
{
  FxI32 vp[4] = { 0, 0, 0, 0 };
  int ogl = vsa && ON(F_OGL_SMOOTH);
  if (!ON(F_CLIPSPACE)) return;
  grGlideSetVertexLayout(layoutClip);
  ccIterated(); depthOff(); grFogMode(GR_FOG_DISABLE); blendAlpha();
  grViewport(0, 0, (FxI32)scrW, (FxI32)(scrH * 0.45f));
  grDepthRange(0.25f, 0.75f);
  grCoordinateSpace(GR_CLIP_COORDS);
  if (frame == 0 && !inDof) {
    grGet(GR_VIEWPORT, 16, vp);
    check("clip space: grGet GR_VIEWPORT returns the gull viewport", vp[0] == 0 && vp[1] == 0 && vp[2] == (FxI32)scrW && vp[3] == (FxI32)(scrH * 0.45f),
          "got %d,%d,%d,%d", (int)vp[0], (int)vp[1], (int)vp[2], (int)vp[3]);
  }
  if (ogl) { grEnable(GR_AA_ORDERED_LINES_OGL); grEnable(GR_AA_ORDERED_POINTS_OGL); USE(F_OGL_SMOOTH); }
  else grEnable(GR_AA_ORDERED);
  if (aaSamples > 1 && ON(F_TBUF_SAMPLE) && X.tbufMask && !inDof) {
    int s;
    for (s = 0; s < aaSamples; s++) { X.tbufMask(1u << s); gullSet(tNow + 0.035f * (float)s); }
    X.tbufMask(tbufAll);
    USE(F_TBUF_SAMPLE); USE(F_TBUFFER);
  } else gullSet(tNow);
  if (ogl) { grDisable(GR_AA_ORDERED_LINES_OGL); grDisable(GR_AA_ORDERED_POINTS_OGL); }
  else grDisable(GR_AA_ORDERED);
  grCoordinateSpace(GR_WINDOW_COORDS);
  grViewport(0, 0, (FxI32)scrW, (FxI32)scrH);
  grDepthRange(0.0f, 1.0f);
  blendOff();
  USE(F_CLIPSPACE);
}

/* lens-flare ghosts on the line from the sun through the screen centre.  Blend factors on
   the source's own colour (SAME_COLOR: src*src, ONE_MINUS_SAME_COLOR: src*(1-src)) and a
   separate alpha-channel blend with each operator (ADD, SUB, REVSUB); undithered. */
static void drawFlare(void)
{
  static const float along[3] = { 0.55f, 1.25f, 1.7f }, rad[3] = { 15.0f, 30.0f, 20.0f };
  static const float tint[3][3] = { { 215, 195, 150 }, { 110, 160, 170 }, { 150, 120, 190 } };
  float sx, sy;
  int k, i;
  if (!ON(F_SUN) || !ON(F_BLEND_OPS) || !X.blend) return;
  sunScreen(&sx, &sy);
  grGlideSetVertexLayout(layoutCol);
  ccIterated(); depthOff(); grFogMode(GR_FOG_DISABLE);
  if (ON(F_DITHER)) grDitherMode(GR_DITHER_DISABLE);
  for (k = 0; k < 3; k++) {
    V fan[14];
    void *ptr[14];
    float cx = sx + (scrW * 0.5f - sx) * along[k], cy = sy + (scrH * 0.5f - sy) * along[k], rr = rad[k] * scrH / 480.0f;
    if (k == 0) X.blend(GR_BLEND_SAME_COLOR_EXT, GR_BLEND_ONE, GR_BLEND_OP_ADD, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_OP_ADD);
    else if (k == 1) X.blend(GR_BLEND_ONE_MINUS_SAME_COLOR_EXT, GR_BLEND_ONE, GR_BLEND_OP_ADD, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_OP_SUB);
    else X.blend(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_OP_ADD, GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_OP_REVSUB);
    scr(&fan[0], cx, cy); col(&fan[0], tint[k][0], tint[k][1], tint[k][2], 90); ptr[0] = &fan[0];
    for (i = 0; i <= 12; i++) {
      float an = 6.2831853f * (float)i / 12.0f;
      scr(&fan[i + 1], cx + rr * cosf(an), cy + rr * sinf(an)); col(&fan[i + 1], 0, 0, 0, 0); ptr[i + 1] = &fan[i + 1];
    }
    grDrawVertexArray(GR_TRIANGLE_FAN, 14, ptr); statTris += 12;
  }
  if (ON(F_DITHER)) { grDitherMode(GR_DITHER_4x4); USE(F_DITHER); }
  blendOff();
  USE(F_BLEND_OPS);
}

static void drawScene(int frame)
{
  hfBuild();
  renderReflection();
  drawSky();
  drawSun();
  drawClouds();
  drawDistant();
  drawSea();
  drawIsland();
  drawFoam();
  drawGlitter();
  drawJettyShadow();
  drawJetty();
  drawHut();
  drawShadows();
  drawBeach();
  drawShells();
  drawNet();
  drawPinwheel();
  drawPalms(cinematic ? 0 : frame);
  drawBuoy();
  drawBoat();
  drawSpray();
  drawGulls(frame);
  drawFlare();
}

#ifdef ISLAND_SCENE_COVERAGE
/* VSA only: reopen at 16bpp RGB565 for the 16-bit LFB read-back, then close */
static void lfb16Probe(GrScreenResolution_t res)
{
  GrContext_t c;
  int savePix = pix32;
  if (!X.winOpen || !ON(F_LFBCHECK)) return;
  c = grSstWinOpen(0, res, GR_REFRESH_60Hz, GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, 2, 1);   /* default 16bpp */
  check("16bpp reopen (grSstWinOpen)", c != 0, NULL); api("grSstWinOpen");
  if (!c) return;
  pix32 = 0;
  { FxI32 nfb = 1; grGet(GR_NUM_FB, 4, &nfb); sliUnitsNow = (int)nfb; }
  grGet(GR_ZDEPTH_MIN_MAX, 8, (FxI32 *)zrange);
  check("16bpp grGet GR_ZDEPTH_MIN_MAX == 0xffff,0", zrange[0] == 0xffffu, "got 0x%x", (unsigned)zrange[0]);
  grBufferClear(0x00406080u, 0, zrange[1]);
  lfbChecks(9999);
  grBufferSwap(1);
  grSstWinClose(c);
  pix32 = savePix;
}

/* ------------------------------------------------------------------------------
** API completeness: every entry point a Linux Glide 3 application can reach is called
** at least once (most by the scene, all again here with checked results).  Classes:
**   H  prototype in glide.h / glideutl.h: required
**   X  grGetProcAddress table entry of this build: required when the table offers it
**   U  exported but undeclared (old/internal names): called when linked, not required
**   S  skipped, with the reason
*/
typedef struct { const char *name; char cls; int called; const char *why; } ApiEnt;
static ApiEnt apiTab[] = {
  {"grAADrawTriangle",'H'},{"grAlphaBlendFunction",'H'},{"grAlphaCombine",'H'},{"grAlphaControlsITRGBLighting",'H'},
  {"grAlphaTestFunction",'H'},{"grAlphaTestReferenceValue",'H'},{"grBufferClear",'H'},{"grBufferSwap",'H'},
  {"grChromakeyMode",'H'},{"grChromakeyValue",'H'},{"grClipWindow",'H'},{"grColorCombine",'H'},{"grColorMask",'H'},
  {"grConstantColorValue",'H'},{"grCoordinateSpace",'H'},{"grCullMode",'H'},{"grDepthBiasLevel",'H'},
  {"grDepthBufferFunction",'H'},{"grDepthBufferMode",'H'},{"grDepthMask",'H'},{"grDepthRange",'H'},{"grDisable",'H'},
  {"grDisableAllEffects",'H'},{"grDitherMode",'H'},{"grDrawLine",'H'},{"grDrawPoint",'H'},{"grDrawTriangle",'H'},
  {"grDrawVertexArray",'H'},{"grDrawVertexArrayContiguous",'H'},{"grEnable",'H'},{"grErrorSetCallback",'H'},
  {"grFinish",'H'},{"grFlush",'H'},{"grFogColorValue",'H'},{"grFogMode",'H'},{"grFogTable",'H'},{"grGet",'H'},
  {"grGetProcAddress",'H'},{"grGetString",'H'},{"grGlideGetState",'H'},{"grGlideGetVertexLayout",'H'},
  {"grGlideInit",'H'},{"grGlideSetState",'H'},{"grGlideSetVertexLayout",'H'},{"grGlideShutdown",'H'},
  {"grLfbConstantAlpha",'H'},{"grLfbConstantDepth",'H'},{"grLfbLock",'H'},{"grLfbReadRegion",'H'},{"grLfbUnlock",'H'},
  {"grLfbWriteColorFormat",'H'},{"grLfbWriteColorSwizzle",'H'},{"grLfbWriteRegion",'H'},{"grLoadGammaTable",'H'},
  {"grQueryResolutions",'H'},{"grRenderBuffer",'H'},{"grReset",'H'},{"grSelectContext",'H'},{"grSetNumPendingBuffers",'H'},
  {"grSplash",'H'},{"grSstOrigin",'H'},{"grSstSelect",'H'},{"grSstWinClose",'H'},{"grSstWinOpen",'H'},
  {"grStippleMode",'H'},{"grStipplePattern",'H'},{"grTexCalcMemRequired",'H'},{"grTexClampMode",'H'},
  {"grTexCombine",'H'},{"grTexDetailControl",'H'},{"grTexDownloadMipMap",'H'},{"grTexDownloadMipMapLevel",'H'},
  {"grTexDownloadMipMapLevelPartial",'H'},{"grTexDownloadTable",'H'},{"grTexDownloadTablePartial",'H'},
  {"grTexFilterMode",'H'},{"grTexLodBiasValue",'H'},{"grTexMaxAddress",'H'},{"grTexMinAddress",'H'},
  {"grTexMipMapMode",'H'},{"grTexMultibase",'H'},{"grTexMultibaseAddress",'H'},{"grTexNCCTable",'H'},
  {"grTexSource",'H'},{"grTexTextureMemRequired",'H'},{"grVertexLayout",'H'},{"grViewport",'H'},
  {"gu3dfGetInfo",'H'},{"gu3dfLoad",'H'},{"guFogGenerateExp",'H'},{"guFogGenerateExp2",'H'},
  {"guFogGenerateLinear",'H'},{"guFogTableIndexToW",'H'},{"guGammaCorrectionRGB",'H'},
  /* grGetProcAddress table (h5 diget.c _extensionTable, Linux build) */
  {"grGetRegistryOrEnvironmentStringExt",'X'},{"grGetGammaTableExt",'X'},{"grChromaRangeModeExt",'X'},
  {"grChromaRangeExt",'X'},{"grTexChromaModeExt",'X'},{"grTexChromaRangeExt",'X'},{"grTextureBufferExt",'X'},
  {"grTextureAuxBufferExt",'X'},{"grAuxBufferExt",'X'},{"grCommandTransportInfoExt2",'X'},
  {"grCommandTransportMakeRoomExt2",'X'},{"grSstWinOpenExt",'X'},{"grStencilFuncExt",'X'},{"grStencilMaskExt",'X'},
  {"grStencilOpExt",'X'},{"grLfbConstantStencilExt",'X'},{"grBufferClearExt",'X'},{"grColorCombineExt",'X'},
  {"grAlphaCombineExt",'X'},{"grTexColorCombineExt",'X'},{"grTexAlphaCombineExt",'X'},{"grConstantColorValueExt",'X'},
  {"grColorMaskExt",'X'},{"grAlphaBlendFunctionExt",'X'},{"grTBufferWriteMaskExt",'X'},{"grTexDownloadTableExt",'X'},
  {"grTexDownloadMipMapLevelPartialRowExt",'X'},{"grSetNumPendingBuffers(ext)",'X'},
  /* exported but not declared by the Glide 3 headers */
  {"grGlideGetVersion",'U'},{"grSstScreenWidth",'U'},{"grSstScreenHeight",'U'},{"grTriStats",'U'},
  {"guAlphaSource",'U'},{"guEncodeRLE16",'U'},{"guEndianSwapBytes",'U'},{"guEndianSwapWords",'U'},
  {"guTexCreateColorMipMap",'U'},{"grCheckForRoom",'U'},{"grSstConfigPipeline",'U'},{"grSstVidMode",'U'},
  /* skipped */
  {"grSstQueryHardware",'S',0,"Glide 2 interface: GrHwConfiguration is not declared by the Glide 3 headers"},
  {"grSurfaceCreateContextExt..grDeviceQueryExt (7)",'S',0,"SURFACE procs exist only in the Win32/MacOS build (GLIDE_OS_WIN32)"},
  {"grSurfaceCreateExt/ReleaseExt/GetDescExt",'S',0,"MacOS build only"},
  {"grDRIBufferSwap",'S',0,"XFree86 DRI build only (DRI_BUILD)"},
  {"grSstQueryBoards",'S',0,"QUERY extension not compiled (QUERY_EXTENSION_SUPPORTED unset)"},
  {"grTexDownloadTablePartialExt/grTexNCCTableExt",'S',0,"POINTCAST extension not compiled (GLIDE_POINTCAST_PALETTE=0)"},
  {"txImgQuantize/txMipQuantize/txPalToNcc",'S',0,"texus2 helpers not linked (HAVE_TEXUS2 unset)"},
  {"grGlideShamelessPlug/grHints/grGammaCorrectionValue",'S',0,"not compiled in this build (Glide 2 relics / GLIDE_PLUG)"},
  {"grChromaMode/grResetTriStats/grLfbReadRegionOrigin",'S',0,"internal or static, not exported"},
};
#define NAPI (int)(sizeof apiTab / sizeof apiTab[0])
static void api(const char *n)
{
  int i;
  for (i = 0; i < NAPI; i++) if (!strcmp(apiTab[i].name, n)) { apiTab[i].called++; return; }
  printf("api: unknown name %s\n", n);
}
static void apiWhy(const char *n, const char *why)
{
  int i;
  for (i = 0; i < NAPI; i++) if (!strcmp(apiTab[i].name, n)) { apiTab[i].why = why; return; }
}

/* exported-but-undeclared entry points: weak, so the h3 build links whatever it has */
extern void   FX_CALL grGlideGetVersion(char version[80]) __attribute__((weak));
extern int    FX_CALL grSstScreenWidth(void) __attribute__((weak));
extern int    FX_CALL grSstScreenHeight(void) __attribute__((weak));
extern void   FX_CALL grTriStats(FxU32 *, FxU32 *) __attribute__((weak));
extern void   FX_CALL guAlphaSource(GrAlphaSource_t) __attribute__((weak));
extern int    FX_CALL guEncodeRLE16(void *, void *, FxU32, FxU32) __attribute__((weak));
extern FxU16  FX_CALL guEndianSwapBytes(FxU16) __attribute__((weak));
extern FxU32  FX_CALL guEndianSwapWords(FxU32) __attribute__((weak));
extern FxU16 *FX_CALL guTexCreateColorMipMap(void) __attribute__((weak));
extern void   FX_CALL grCheckForRoom(FxI32) __attribute__((weak));
extern void   FX_CALL grSstConfigPipeline(GrChipID_t, FxU32, FxU32) __attribute__((weak));
extern void   FX_CALL grSstVidMode(FxU32, void *) __attribute__((weak));
extern void   FX_CALL grSetNumPendingBuffers(FxI32) __attribute__((weak));   /* h3 headers omit it */

typedef char *(FX_CALL *PfnRegistry)(char *);
typedef void *(FX_CALL *PfnCtiInfo)(void);
typedef void  (FX_CALL *PfnCtiRoom)(const FxI32, const char *, const int);
typedef FxBool (FX_CALL *PfnPartialRow)(GrChipID_t, FxU32, GrLOD_t, GrLOD_t, GrAspectRatio_t, GrTextureFormat_t, FxU32, void *, int, int, int);
typedef void  (FX_CALL *PfnAuxBuffer)(GrBuffer_t);
typedef void  (FX_CALL *PfnPending)(FxI32);

static int errCalls;
static void FX_CALL errCallback(const char *msg, FxBool fatal)
{
  errCalls++;
  printf("glide error callback: %s%s\n", msg, fatal ? " (fatal)" : "");
}

/* calls made before a window is open */
static void apiPreOpen(void)
{
  GlideResolution tmpl, *list;
  FxI32 sz, n;
  char  ver[80];
  api("grGlideInit"); api("grSstSelect"); api("grGetString");
  grErrorSetCallback(errCallback); api("grErrorSetCallback");
  tmpl.resolution = GR_QUERY_ANY; tmpl.refresh = GR_QUERY_ANY; tmpl.numColorBuffers = GR_QUERY_ANY; tmpl.numAuxBuffers = GR_QUERY_ANY;
  sz = grQueryResolutions(&tmpl, NULL); api("grQueryResolutions");
  list = (GlideResolution *)calloc((size_t)(sz > 0 ? sz : 1), 1);
  n = sz > 0 ? grQueryResolutions(&tmpl, list) : 0;
  check("grQueryResolutions lists modes", sz > 0 && n > 0, "size %d, returned %d", (int)sz, (int)n);
  free(list);
  if (grGlideGetVersion) {
    memset(ver, 0, sizeof ver);
    grGlideGetVersion(ver); api("grGlideGetVersion");
    check("grGlideGetVersion matches GR_VERSION", strstr(ver, grGetString(GR_VERSION)) != NULL, "got '%s'", ver);
  }
}

/* write a tiny RGB565 16x16 .3df (procedural) and read it back with gu3dfGetInfo/gu3dfLoad */
static void api3df(void)
{
  char path[64];
  FILE *f;
  int i;
  Gu3dfInfo inf;
  FxU16 px[16 * 16 + 8 * 8 + 4 * 4 + 2 * 2 + 1];
  snprintf(path, sizeof path, "/tmp/scene_island_%u.3df", (unsigned)seedBase);
  for (i = 0; i < (int)(sizeof px / sizeof px[0]); i++) px[i] = (FxU16)hash32((unsigned)i * 7u + seedBase);
  f = fopen(path, "wb");
  if (!f) { check("gu3dfLoad (temp file)", 0, "cannot write %s", path); return; }
  fprintf(f, "3df v1.0\nrgb565\nlod range: 1 16\naspect ratio: 1 1\n");
  for (i = 0; i < (int)(sizeof px / sizeof px[0]); i++) { fputc(px[i] >> 8, f); fputc(px[i] & 255, f); }   /* big endian */
  fclose(f);
  memset(&inf, 0, sizeof inf);
  i = gu3dfGetInfo(path, &inf); api("gu3dfGetInfo");
  check("gu3dfGetInfo on a generated 16x16 RGB565 .3df", i && inf.header.format == GR_TEXFMT_RGB_565 &&
        (inf.header.large_lod == GR_LOD_LOG2_16 || inf.header.large_lod == 16) && inf.header.width == 16 && inf.header.height == 16 && inf.mem_required == sizeof px,
        "ok=%d fmt %d lod %d..%d mem %u", i, (int)inf.header.format, (int)inf.header.small_lod, (int)inf.header.large_lod, (unsigned)inf.mem_required);
  inf.data = calloc(inf.mem_required ? inf.mem_required : 1, 1);
  i = gu3dfLoad(path, &inf); api("gu3dfLoad");
  check("gu3dfLoad texels match", i && inf.data && !memcmp(inf.data, px, sizeof px), "ok=%d", i);
  free(inf.data);
  remove(path);
}

static void apiSweep(GrContext_t ctx)
{
  static const FxU32 gets[] = { GR_BITS_DEPTH, GR_BITS_RGBA, GR_FIFO_FULLNESS, GR_FOG_TABLE_ENTRIES, GR_BITS_GAMMA,
    GR_GAMMA_TABLE_ENTRIES, GR_GLIDE_STATE_SIZE, GR_GLIDE_VERTEXLAYOUT_SIZE, GR_IS_BUSY, GR_LFB_PIXEL_PIPE,
    GR_MAX_TEXTURE_SIZE, GR_MAX_TEXTURE_ASPECT_RATIO, GR_MEMORY_FB, GR_MEMORY_TMU, GR_MEMORY_UMA, GR_NUM_BOARDS,
    GR_NON_POWER_OF_TWO_TEXTURES, GR_NUM_FB, GR_NUM_TMU, GR_PENDING_BUFFERSWAPS, GR_REVISION_FB, GR_REVISION_TMU,
    GR_STATS_LINES, GR_STATS_PIXELS_AFUNC_FAIL, GR_STATS_PIXELS_CHROMA_FAIL, GR_STATS_PIXELS_DEPTHFUNC_FAIL,
    GR_STATS_PIXELS_IN, GR_STATS_PIXELS_OUT, GR_STATS_POINTS, GR_STATS_TRIANGLES_IN, GR_STATS_TRIANGLES_OUT,
    GR_NUM_SWAP_HISTORY_BUFFER, GR_SWAP_HISTORY, GR_SUPPORTS_PASSTHRU, GR_TEXTURE_ALIGN, GR_VIDEO_POSITION,
    GR_VIEWPORT, GR_WDEPTH_MIN_MAX, GR_ZDEPTH_MIN_MAX };
  static const FxU32 strs[] = { GR_EXTENSION, GR_HARDWARE, GR_RENDERER, GR_VENDOR, GR_VERSION };
  FxI32 buf[64];
  int i, bad, okq = 0;
  V a, b, c, d;
  GrFog_t ft[128];
  void *stBuf, *lay;
  FxI32 sz = 0;
  char m[96];

  printf("--- API sweep\n");
  /* queries */
  for (i = 0; i < (int)(sizeof gets / sizeof gets[0]); i++) {
    FxU32 len = (gets[i] == GR_BITS_RGBA || gets[i] == GR_VIEWPORT) ? 16 : (gets[i] == GR_SWAP_HISTORY) ? 4 * 8 :
                (gets[i] == GR_FIFO_FULLNESS || gets[i] == GR_VIDEO_POSITION || gets[i] == GR_WDEPTH_MIN_MAX ||
                 gets[i] == GR_ZDEPTH_MIN_MAX) ? 8 : 4;
    memset(buf, 0, sizeof buf);
    if (grGet(gets[i], len, buf) == len) okq++;
    else printf("  grGet 0x%x returned no data\n", (unsigned)gets[i]);
  }
  api("grGet");
  check("grGet answers every query enum", okq == (int)(sizeof gets / sizeof gets[0]), "%d of %d", okq, (int)(sizeof gets / sizeof gets[0]));
  for (bad = 0, i = 0; i < 5; i++) { const char *s = grGetString(strs[i]); if (!s || !strcmp(s, "ERROR")) bad++; }
  check("grGetString answers all 5 names", bad == 0, "%d failed", bad);
  check("grReset GR_STATS_PIXELS/TRIANGLES/POINTS/LINES", grReset(GR_STATS_PIXELS) && grReset(GR_STATS_TRIANGLES) &&
        grReset(GR_STATS_POINTS) && grReset(GR_STATS_LINES), NULL);
  api("grReset");
  check("grSelectContext(current)", grSelectContext(ctx), NULL); api("grSelectContext");
  if (grSstScreenWidth) {
    int w = grSstScreenWidth(), h = grSstScreenHeight();
    api("grSstScreenWidth"); api("grSstScreenHeight");
    check("grSstScreenWidth/Height", w == (int)scrW && h == (int)scrH, "%dx%d", w, h);
  }
  /* state save/restore and vertex layout save/restore */
  grGet(GR_GLIDE_STATE_SIZE, 4, &sz);
  stBuf = calloc((size_t)sz + 16, 1);
  grGlideGetState(stBuf); api("grGlideGetState");
  grGet(GR_GLIDE_VERTEXLAYOUT_SIZE, 4, &sz);
  lay = calloc((size_t)sz + 16, 1);
  grGlideGetVertexLayout(lay); api("grGlideGetVertexLayout");
  /* a small test patch in the lower-left corner exercising the remaining state calls */
  grClipWindow(0, (FxU32)(scrH - 64), 64, (FxU32)scrH); api("grClipWindow");
  grSstOrigin(GR_ORIGIN_UPPER_LEFT); api("grSstOrigin");
  grGlideSetVertexLayout(layoutCol); api("grGlideSetVertexLayout");
  grVertexLayout(GR_PARAM_XY, OFS(x), GR_PARAM_ENABLE); api("grVertexLayout");
  grDisableAllEffects(); api("grDisableAllEffects");
  grCullMode(GR_CULL_NEGATIVE); api("grCullMode");
  grDitherMode(GR_DITHER_2x2); api("grDitherMode");
  grColorMask(FXTRUE, FXFALSE); api("grColorMask");
  grDepthBufferMode(GR_DEPTHBUFFER_ZBUFFER); api("grDepthBufferMode");
  grDepthBufferFunction(GR_CMP_ALWAYS); api("grDepthBufferFunction");
  grDepthMask(FXFALSE); api("grDepthMask");
  grDepthBiasLevel(0); api("grDepthBiasLevel");
  grConstantColorValue(0xff336699u); api("grConstantColorValue");
  grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE); api("grColorCombine");
  grAlphaCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_LOCAL_CONSTANT, GR_COMBINE_OTHER_NONE, FXFALSE); api("grAlphaCombine");
  grAlphaControlsITRGBLighting(FXTRUE); api("grAlphaControlsITRGBLighting");
  grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO); api("grAlphaBlendFunction");
  grAlphaTestFunction(GR_CMP_GEQUAL); api("grAlphaTestFunction");
  grAlphaTestReferenceValue(0x10); api("grAlphaTestReferenceValue");
  grChromakeyValue(0xff00ff00u); api("grChromakeyValue");
  grChromakeyMode(GR_CHROMAKEY_ENABLE); api("grChromakeyMode");
  grFogColorValue(0xff808080u); api("grFogColorValue");
  grFogMode(GR_FOG_DISABLE); api("grFogMode");
  grStipplePattern(0xaa55aa55u); api("grStipplePattern");
  grStippleMode(GR_STIPPLE_PATTERN); api("grStippleMode");
  grEnable(GR_ALLOW_MIPMAP_DITHER); api("grEnable");
  grDisable(GR_ALLOW_MIPMAP_DITHER); api("grDisable");
  scr(&a, 4, scrH - 60); scr(&b, 60, scrH - 60); scr(&c, 60, scrH - 4); scr(&d, 4, scrH - 4);
  col(&a, 255, 0, 0, 255); col(&b, 0, 255, 0, 255); col(&c, 0, 0, 255, 255); col(&d, 255, 255, 0, 255);
  grDrawTriangle(&a, &b, &c); api("grDrawTriangle");
  grStippleMode(GR_STIPPLE_DISABLE);
  grChromakeyMode(GR_CHROMAKEY_DISABLE);
  grAlphaControlsITRGBLighting(FXFALSE);
  grCullMode(GR_CULL_DISABLE);
  grAADrawTriangle(&a, &c, &d, FXTRUE, FXTRUE, FXTRUE); api("grAADrawTriangle");
  grDrawLine(&a, &c); api("grDrawLine");
  grDrawPoint(&b); api("grDrawPoint");
  { void *p[4] = { &a, &b, &c, &d }; grDrawVertexArray(GR_TRIANGLE_FAN, 4, p); api("grDrawVertexArray"); }
  { V s[4]; s[0] = a; s[1] = b; s[2] = d; s[3] = c; grDrawVertexArrayContiguous(GR_TRIANGLE_STRIP, 4, s, sizeof(V)); api("grDrawVertexArrayContiguous"); }
  /* clip-space coordinates: viewport and depth range apply */
  grViewport(0, 0, (FxI32)scrW, (FxI32)scrH); api("grViewport");
  grDepthRange(0.0f, 1.0f); api("grDepthRange");
  grCoordinateSpace(GR_CLIP_COORDS); api("grCoordinateSpace");
  grGlideSetVertexLayout(layoutClip);          /* clip space divides by GR_PARAM_W */
  {
    V p, q, r;
    memset(&p, 0, sizeof p); memset(&q, 0, sizeof q); memset(&r, 0, sizeof r);
    p.x = -0.98f; p.y = -0.98f; q.x = -0.85f; q.y = -0.98f; r.x = -0.98f; r.y = -0.85f;
    p.oow = q.oow = r.oow = 1.0f; p.ooz = q.ooz = r.ooz = 0.5f;
    col(&p, 255, 255, 255, 255); col(&q, 255, 255, 255, 255); col(&r, 255, 255, 255, 255);
    grDrawTriangle(&p, &q, &r);
  }
  grCoordinateSpace(GR_WINDOW_COORDS);
  grGlideSetVertexLayout(lay);
  /* textures */
  {
    GrTexInfo ti = texSandBoth.info;
    FxU32 m1 = grTexCalcMemRequired(ti.smallLodLog2, ti.largeLodLog2, ti.aspectRatioLog2, ti.format);
    FxU32 m2 = grTexTextureMemRequired(GR_MIPMAPLEVELMASK_BOTH, &ti);
    api("grTexCalcMemRequired"); api("grTexTextureMemRequired");
    check("grTexCalcMemRequired == grTexTextureMemRequired(BOTH)", m1 == m2, "0x%x vs 0x%x", (unsigned)m1, (unsigned)m2);
    check("grTexMinAddress < grTexMaxAddress", grTexMinAddress(GR_TMU0) < grTexMaxAddress(GR_TMU0), NULL);
    api("grTexMinAddress"); api("grTexMaxAddress");
    if (texSandBoth.ok) {
      grTexDownloadMipMapLevelPartial(GR_TMU0, texSandBoth.addr, GR_LOD_LOG2_256, GR_LOD_LOG2_256, GR_ASPECT_LOG2_1x1,
                                      GR_TEXFMT_RGB_565, GR_MIPMAPLEVELMASK_BOTH, texSandBoth.info.data, 0, 3);
      api("grTexDownloadMipMapLevelPartial");
      grTexMultibase(GR_TMU0, FXTRUE); api("grTexMultibase");
      grTexMultibaseAddress(GR_TMU0, GR_TEXBASE_256, texSandBoth.addr, GR_MIPMAPLEVELMASK_BOTH, &ti);
      grTexMultibaseAddress(GR_TMU0, GR_TEXBASE_128, texSandBoth.addr, GR_MIPMAPLEVELMASK_BOTH, &ti);
      grTexMultibaseAddress(GR_TMU0, GR_TEXBASE_64, texSandBoth.addr, GR_MIPMAPLEVELMASK_BOTH, &ti);
      grTexMultibaseAddress(GR_TMU0, GR_TEXBASE_32_TO_1, texSandBoth.addr, GR_MIPMAPLEVELMASK_BOTH, &ti);
      api("grTexMultibaseAddress");
      grTexSource(GR_TMU0, texSandBoth.addr, GR_MIPMAPLEVELMASK_BOTH, &ti); api("grTexSource");
      grTexMultibase(GR_TMU0, FXFALSE);
    }
    grTexDownloadTablePartial(GR_TEXTABLE_PALETTE, &palSunset, 16, 31); api("grTexDownloadTablePartial");
    grTexDownloadTable(GR_TEXTABLE_PALETTE, &palSunset); api("grTexDownloadTable");
    grTexNCCTable(GR_NCCTABLE_NCC0); api("grTexNCCTable");
    grTexCombine(GR_TMU0, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE, FXFALSE, FXFALSE); api("grTexCombine");
    grTexFilterMode(GR_TMU0, GR_TEXTUREFILTER_BILINEAR, GR_TEXTUREFILTER_BILINEAR); api("grTexFilterMode");
    grTexClampMode(GR_TMU0, GR_TEXTURECLAMP_WRAP, GR_TEXTURECLAMP_WRAP); api("grTexClampMode");
    grTexMipMapMode(GR_TMU0, GR_MIPMAP_NEAREST, FXFALSE); api("grTexMipMapMode");
    grTexLodBiasValue(GR_TMU0, 0.0f); api("grTexLodBiasValue");
    grTexDetailControl(GR_TMU0, 0, 1, 0.0f); api("grTexDetailControl");
    if (texWater.ok) api("grTexDownloadMipMap");
    if (perLevelUsed) api("grTexDownloadMipMapLevel");
  }
  /* fog utilities */
  {
    FxI32 n = 64;
    int mono;
    grGet(GR_FOG_TABLE_ENTRIES, 4, &n);
    if (n > 128) n = 128;
    guFogGenerateExp(ft, 0.01f); api("guFogGenerateExp");
    for (mono = 1, i = 1; i < n; i++) if (ft[i] < ft[i - 1]) mono = 0;
    check("guFogGenerateExp table non-decreasing", mono && ft[n - 1] > ft[0], "first %d last %d", ft[0], ft[n - 1]);
    guFogGenerateExp2(ft, 0.002f); api("guFogGenerateExp2");
    for (mono = 1, i = 1; i < n; i++) if (ft[i] < ft[i - 1]) mono = 0;
    check("guFogGenerateExp2 table non-decreasing", mono, NULL);
    guFogGenerateLinear(ft, 10.0f, 500.0f); api("guFogGenerateLinear");
    check("guFogGenerateLinear ends at 255", ft[n - 1] == 255, "last %d", ft[n - 1]);
    check("guFogTableIndexToW increasing", guFogTableIndexToW(10) < guFogTableIndexToW(40), NULL); api("guFogTableIndexToW");
    grFogTable(ft); api("grFogTable");
    grFogTable(fogTable);
  }
  /* gamma: load a table, read it back where GETGAMMA exists */
  {
    FxI32 n = 0;
    grGet(GR_GAMMA_TABLE_ENTRIES, 4, &n);
    if (n > 0 && n <= 1024) {
      FxU32 *r = (FxU32 *)calloc((size_t)n, 4), *g = (FxU32 *)calloc((size_t)n, 4), *bl = (FxU32 *)calloc((size_t)n, 4);
      for (i = 0; i < n; i++) { r[i] = (FxU32)(i * 255 / (n - 1)); g[i] = r[i]; bl[i] = (FxU32)((n - 1 - i) * 255 / (n - 1)); }
      grLoadGammaTable((FxU32)n, r, g, bl); api("grLoadGammaTable");
      if (X.getGamma) {
        FxU32 *r2 = (FxU32 *)calloc((size_t)n, 4), *g2 = (FxU32 *)calloc((size_t)n, 4), *b2 = (FxU32 *)calloc((size_t)n, 4);
        X.getGamma((FxU32)n, r2, g2, b2); api("grGetGammaTableExt");
        info("grLoadGammaTable / grGetGammaTableExt round trip", !memcmp(r, r2, (size_t)n * 4) && !memcmp(bl, b2, (size_t)n * 4),
             "r[1] %u/%u b[0] %u/%u (read back from the CLUT through the card model)", (unsigned)r[1], (unsigned)r2[1], (unsigned)bl[0], (unsigned)b2[0]);
        free(r2); free(g2); free(b2);
      }
      free(r); free(g); free(bl);
    }
    guGammaCorrectionRGB(1.0f, 1.0f, 1.0f); api("guGammaCorrectionRGB");
  }
  /* LFB: colour format/swizzle, constant depth, a read lock */
  {
    GrLfbInfo_t li;
    FxU16 px[4] = { 0x1234, 0x5678, 0x9abc, 0xdef0 };
    grLfbWriteColorFormat(GR_COLORFORMAT_ARGB); api("grLfbWriteColorFormat");
    grLfbWriteColorSwizzle(FXFALSE, FXFALSE); api("grLfbWriteColorSwizzle");
    grLfbConstantDepth(0x8000); api("grLfbConstantDepth");
    grLfbConstantAlpha(0xff); api("grLfbConstantAlpha");
    grLfbWriteRegion(GR_BUFFER_BACKBUFFER, 0, (FxU32)scrH - 1, GR_LFB_SRC_FMT_565, 4, 1, FXFALSE, 8, px); api("grLfbWriteRegion");
    grLfbWriteColorFormat(GR_COLORFORMAT_ABGR);
    li.size = sizeof li;
    i = grLfbLock(GR_LFB_READ_ONLY, GR_BUFFER_FRONTBUFFER, GR_LFBWRITEMODE_ANY, GR_ORIGIN_UPPER_LEFT, FXFALSE, &li); api("grLfbLock");
    check("front-buffer read lock", i && li.lfbPtr != NULL, NULL);
    if (i) { grLfbUnlock(GR_LFB_READ_ONLY, GR_BUFFER_FRONTBUFFER); api("grLfbUnlock"); }
    { FxU16 rb[8]; i = grLfbReadRegion(GR_BUFFER_FRONTBUFFER, 0, 0, 2, 2, 4, rb); api("grLfbReadRegion");
      check("grLfbReadRegion front buffer", i, NULL); }
  }
  /* render buffer selection: draw into the front buffer once, back again */
  grRenderBuffer(GR_BUFFER_FRONTBUFFER); api("grRenderBuffer");
  grDrawPoint(&a);
  grRenderBuffer(GR_BUFFER_BACKBUFFER);
  grBufferClear(0, 0, zrange[1]); api("grBufferClear");
  grSplash(0.0f, 0.0f, scrW, scrH, 0); api("grSplash");
  grFlush(); api("grFlush");
  grFinish(); api("grFinish");
  api3df();
  /* undeclared exports */
  if (grTriStats) { FxU32 tp = 0, td = 0; grTriStats(&tp, &td); api("grTriStats"); check("grTriStats processed > 0", tp > 0, "processed %u drawn %u", (unsigned)tp, (unsigned)td); }
  if (guAlphaSource) { guAlphaSource(GR_ALPHASOURCE_ITERATED_ALPHA); api("guAlphaSource"); }
  if (guEndianSwapBytes) { check("guEndianSwapBytes", guEndianSwapBytes(0x1234) == 0x3412, NULL); api("guEndianSwapBytes"); }
  if (guEndianSwapWords) { check("guEndianSwapWords", guEndianSwapWords(0x12345678u) == 0x56781234u, NULL); api("guEndianSwapWords"); }
  if (guEncodeRLE16) {
    static FxU16 src[64], dst[256];
    int n;
    for (i = 0; i < 64; i++) src[i] = (FxU16)(i < 40 ? 0x1111 : i);
    n = guEncodeRLE16(dst, src, 64, 1); api("guEncodeRLE16");
    {
      /* decode: one dword per run, length << 16 | colour */
      FxU16 back[64];
      int k, o = 0, okr = n > 0 && n <= 4 * 64;
      for (k = 0; okr && k < n / 4; k++) {
        FxU32 w = ((FxU32 *)dst)[k];
        int len = (int)(w >> 16), j;
        for (j = 0; j < len && o < 64; j++) back[o++] = (FxU16)w;
      }
      okr = okr && o == 64 && !memcmp(back, src, sizeof back) && n == 4 * 25;
      check("guEncodeRLE16 run 40 + 24 singles decodes back", okr, "%d bytes, %d words", n, o);
    }
  }
  if (guTexCreateColorMipMap) { FxU16 *p = guTexCreateColorMipMap(); api("guTexCreateColorMipMap"); check("guTexCreateColorMipMap", p != NULL, NULL); free(p); }
  if (grCheckForRoom) { grCheckForRoom(16); api("grCheckForRoom"); }
  if (grSstConfigPipeline) { grSstConfigPipeline(GR_TMU0, 0, 0); api("grSstConfigPipeline"); }   /* empty body in this build */
  /* every extension in the string: fetch its procs; call those the scene did not */
  {
    const char *ext = grGetString(GR_EXTENSION);
    static const struct { const char *tok, *procs; } ex[] = {
      { "CHROMARANGE", "grChromaRangeModeExt grChromaRangeExt" }, { "TEXCHROMA", "grTexChromaModeExt grTexChromaRangeExt" },
      { "TEXMIRROR", "" }, { "TEXUMA", "" }, { "PALETTE6666", "" }, { "FOGCOORD", "" }, { "SURFACE", "" },
      { "COMMAND_TRANSPORT", "grCommandTransportInfoExt2 grCommandTransportMakeRoomExt2" },
      { "TEXTUREBUFFER", "grTextureBufferExt grTextureAuxBufferExt grAuxBufferExt" }, { "GETGAMMA", "grGetGammaTableExt" },
      { "GETREGISTRY", "grGetRegistryOrEnvironmentStringExt" }, { "ALPHAFOG", "" },
      { "PIXEXT", "grSstWinOpenExt grStencilFuncExt grStencilMaskExt grStencilOpExt grLfbConstantStencilExt grBufferClearExt grColorMaskExt grTBufferWriteMaskExt" },
      { "COMBINE", "grColorCombineExt grAlphaCombineExt grTexColorCombineExt grTexAlphaCombineExt grConstantColorValueExt grAlphaBlendFunctionExt" },
      { "TEXFMT", "" } };
    char tokbuf[64];
    const char *p = ext;
    printf("extension procs:\n");
    while (p && *p) {
      int len, k, found = 0;
      while (*p == ' ') p++;
      if (!*p) break;
      for (len = 0; p[len] && p[len] != ' '; len++) { }
      snprintf(tokbuf, sizeof tokbuf, "%.*s", len, p);
      p += len;
      for (k = 0; k < (int)(sizeof ex / sizeof ex[0]); k++) if (!strcmp(ex[k].tok, tokbuf)) {
        char procs[512], *q, *save = NULL;
        found = 1;
        snprintf(procs, sizeof procs, "%s", ex[k].procs);
        printf("  %-18s", tokbuf);
        if (!procs[0]) printf(" (modes/formats only)");
        for (q = strtok_r(procs, " ", &save); q; q = strtok_r(NULL, " ", &save)) {
          GrProc pr = grGetProcAddress(q);
          printf(" %s=%s", q, pr ? "ok" : "NULL");
          snprintf(m, sizeof m, "grGetProcAddress(%s)", q);
          if (!pr) check(m, 0, "advertised by %s", tokbuf);
        }
        printf("\n");
      }
      if (!found) printf("  %-18s (unknown to this program)\n", tokbuf);
    }
    api("grGetProcAddress");
    /* procs not used by the scene */
    {
      PfnRegistry reg = (PfnRegistry)grGetProcAddress("grGetRegistryOrEnvironmentStringExt");
      PfnCtiInfo  cti = (PfnCtiInfo)grGetProcAddress("grCommandTransportInfoExt2");
      PfnCtiRoom  ctr = (PfnCtiRoom)grGetProcAddress("grCommandTransportMakeRoomExt2");
      PfnPartialRow prow = (PfnPartialRow)grGetProcAddress("grTexDownloadMipMapLevelPartialRowExt");
      PfnPending  pend = (PfnPending)grGetProcAddress("grSetNumPendingBuffers");
      PfnTextureBuffer tab = (PfnTextureBuffer)grGetProcAddress("grTextureAuxBufferExt");
      PfnAuxBuffer aux = (PfnAuxBuffer)grGetProcAddress("grAuxBufferExt");
      if (reg) { char *v = reg((char *)"FX_GLIDE_NO_SPLASH"); api("grGetRegistryOrEnvironmentStringExt");
                 check("grGetRegistryOrEnvironmentStringExt(FX_GLIDE_NO_SPLASH)", v && !strcmp(v, "1"), "got %s", v ? v : "(null)"); }
      if (cti) { void *t = cti(); api("grCommandTransportInfoExt2"); check("grCommandTransportInfoExt2 non-null", t != NULL, NULL); }
      if (ctr) { ctr(8, __FILE__, __LINE__); api("grCommandTransportMakeRoomExt2"); }
      if (prow && texSandBoth.ok) {
        check("grTexDownloadMipMapLevelPartialRowExt", prow(GR_TMU0, texSandBoth.addr, GR_LOD_LOG2_256, GR_LOD_LOG2_256,
              GR_ASPECT_LOG2_1x1, GR_TEXFMT_RGB_565, GR_MIPMAPLEVELMASK_BOTH, texSandBoth.info.data, 5, 0, 255), NULL);
        api("grTexDownloadMipMapLevelPartialRowExt");
      }
      if (pend) { pend(2); api("grSetNumPendingBuffers(ext)"); }
      if (grSetNumPendingBuffers) { grSetNumPendingBuffers(2); api("grSetNumPendingBuffers"); }
      if (X.tableExt) api("grTexDownloadTableExt");
      if (X.textureBuffer) api("grTextureBufferExt");
      if (tab && aux && texRefl.ok) {
        /* depth target for render-to-texture: aux buffer redirected into TMU memory and back */
        tab(GR_TMU1, texRefl.addr, GR_LOD_LOG2_128, GR_LOD_LOG2_128, GR_ASPECT_LOG2_1x1, texRefl.info.format, GR_MIPMAPLEVELMASK_BOTH);
        api("grTextureAuxBufferExt");
        aux(0x7 /* GR_BUFFER_TEXTUREAUXBUFFER_EXT */); api("grAuxBufferExt");
        aux(GR_BUFFER_AUXBUFFER);
      }
      if (X.chromaRangeMode) { X.chromaRangeMode(GR_CHROMARANGE_DISABLE_EXT); api("grChromaRangeModeExt"); }
      if (X.chromaRange) { X.chromaRange(0xff00ff00u, 0xff10ff10u, GR_CHROMARANGE_RGB_ALL_EXT); api("grChromaRangeExt"); }
      if (X.texChromaMode) { X.texChromaMode(GR_TMU0, GR_TEXCHROMA_DISABLE_EXT); api("grTexChromaModeExt"); }
      if (X.texChromaRange) { X.texChromaRange(GR_TMU0, 0xff00ff00u, 0xff00ff00u, GR_TEXCHROMARANGE_RGB_ALL_EXT); api("grTexChromaRangeExt"); }
      if (X.winOpen && pix32) api("grSstWinOpenExt");
      if (X.stencilFunc) { X.stencilFunc(GR_CMP_ALWAYS, 0, 0xff); api("grStencilFuncExt"); }
      if (X.stencilMask) { X.stencilMask(0xff); api("grStencilMaskExt"); }
      if (X.stencilOp) { X.stencilOp(GR_STENCILOP_KEEP, GR_STENCILOP_KEEP, GR_STENCILOP_KEEP); api("grStencilOpExt"); }
      if (X.lfbConstStencil) { X.lfbConstStencil(0x5a); api("grLfbConstantStencilExt"); }
      if (X.bufferClear) { X.bufferClear(0, 0, zrange[1], 0); api("grBufferClearExt"); }
      if (X.colorMask) { X.colorMask(FXTRUE, FXTRUE, FXTRUE, FXTRUE); api("grColorMaskExt"); }
      if (X.tbufMask) { X.tbufMask(tbufAll ? tbufAll : 0xf); api("grTBufferWriteMaskExt"); }
      if (X.colorCombine) {
        X.colorCombine(GR_CMBX_ITRGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO, GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
        X.alphaCombine(GR_CMBX_ITALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO, GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
        X.texColorCombine(GR_TMU0, GR_CMBX_LOCAL_TEXTURE_RGB, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO, GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
        X.texAlphaCombine(GR_TMU0, GR_CMBX_LOCAL_TEXTURE_ALPHA, GR_FUNC_MODE_X, GR_CMBX_ZERO, GR_FUNC_MODE_ZERO, GR_CMBX_ZERO, FXTRUE, GR_CMBX_ZERO, FXFALSE, 0, FXFALSE);
        X.constColor(GR_TMU0, 0xffffffffu);
        api("grColorCombineExt"); api("grAlphaCombineExt"); api("grTexColorCombineExt"); api("grTexAlphaCombineExt"); api("grConstantColorValueExt");
      }
      if (X.blend) {
        X.blend(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_OP_SUB, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_OP_REVSUB);
        grDrawPoint(&b);
        X.blend(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_OP_ADD, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_OP_ADD);
        api("grAlphaBlendFunctionExt");
      }
      if (hasTok(ext, "TEXUMA")) { grEnable(0x06 /* GR_TEXTURE_UMA_EXT */); grDisable(0x06); }
      if (hasTok(ext, "ALPHAFOG")) { grFogMode(GR_FOG_WITH_ITERATED_ALPHA_EXT); grFogMode(GR_FOG_DISABLE); }
    }
  }
  if (grSstVidMode) { grSstVidMode(0, NULL); api("grSstVidMode"); }   /* NULL: default timings */
  grGlideSetState(stBuf); api("grGlideSetState");
  grClipWindow(0, 0, (FxU32)scrW, (FxU32)scrH);
  free(stBuf); free(lay);
  (void)m;
}

/* entry points reached only through one maskable feature: -m on that feature exempts them */
static const char *apiMasked(const char *n)
{
  static const struct { const char *api; int feat; } dep[] = {
    { "grSstWinOpen", F_LFBCHECK }, { "grSstWinOpenExt", F_PIX32 },
    { "grTextureAuxBufferExt", F_TEXBUFFER }, { "grAuxBufferExt", F_TEXBUFFER } };
  static char why[96];
  int i;
  for (i = 0; i < (int)(sizeof dep / sizeof dep[0]); i++)
    if (!strcmp(n, dep[i].api) && (featMask >> dep[i].feat & 1)) {
      snprintf(why, sizeof why, "not called: feature %s disabled with -m", featName[dep[i].feat]);
      return why;
    }
  return NULL;
}

/* final table; returns the number of required entry points never called */
static int apiReport(void)
{
  int i, miss = 0, nH = 0, nX = 0, nU = 0, nS = 0, cH = 0, cX = 0, cU = 0;
  for (i = 0; i < NAPI; i++) {
    char nm[64];
    ApiEnt *e = &apiTab[i];
    if (e->cls != 'X' || e->called || e->why) continue;
    snprintf(nm, sizeof nm, "%s", e->name);
    if (strchr(nm, '(')) *strchr(nm, '(') = 0;
    if (!grGetProcAddress(nm)) e->why = "not in this build's grGetProcAddress table";
  }
  printf("--- API coverage (class: H header, X extension table, U undeclared export, S skipped)\n");
  for (i = 0; i < NAPI; i++) {
    ApiEnt *e = &apiTab[i];
    const char *st;
    if (e->cls == 'S') { nS++; st = "skipped"; }
    else if (e->called) { st = "called"; if (e->cls == 'H') cH++; else if (e->cls == 'X') cX++; else cU++; }
    else if (e->why) st = "skipped";
    else if (e->cls == 'U') st = "not linked";
    else if (apiMasked(e->name)) { st = "skipped"; e->why = apiMasked(e->name); }
    else { st = "MISSING"; miss++; }
    if (e->cls == 'H') nH++; else if (e->cls == 'X') nX++; else if (e->cls == 'U') nU++;
    printf("API %c %-46s %-10s %5d %s\n", e->cls, e->name, st, e->called, e->why ? e->why : "");
  }
  printf("API summary: header %d/%d, extension table %d/%d, undeclared exports %d/%d, skipped %d, missing %d\n",
         cH, nH, cX, nX, cU, nU, nS, miss);
  return miss;
}
#endif

static void printUsage(const char *program)
{
  printf("Usage: %s [-n frames] [-r WIDTHxHEIGHT] [-a 0|2|4|8] [-p 15|16|32]\n"
         "       [--detail|--showcase] [--cinematic] [--perf-hud] [-d] [-g] [-q]\n"
         "       [-m disabled-feature-mask] [-s seed]\n", program);
#ifdef ISLAND_SCENE_COVERAGE
  puts("Simulator options: --interactive, --record DIRECTORY FPS");
  puts("Coverage probes: -P, -A, -A-only");
#endif
}

#ifdef ISLAND_SCENE_COVERAGE
int island_scene_run_with_platform(int argc, char **argv, const IslandScenePlatform *platform)
#else
int island_scene_run(int argc, char **argv)
#endif
{
  static int started;
  if (started) return 2;
  started = 1;
#ifdef ISLAND_SCENE_COVERAGE
  islandPlatform = platform;
  FxU32 window = platform ? platform->window : 0;
#else
  cinematic = highDetail = 1;
  FxU32 window = 0;
#endif
  int   frames = 60, quiet = 0, aaReq = 0, i, frame, f, dofReq = 0, bppReq = 32, envCfg = -1, nChips = 1;
  int   aaRefused = 0;
#ifdef ISLAND_SCENE_COVERAGE
  int   aaArg, probeAA = 0, probeAlpha = 0;
#endif
  int   showcase = 0, resolutionSpecified = 0, aaSpecified = 0, frameLimitSpecified = 0;
  int   recordFps = 30;
#ifdef ISLAND_SCENE_COVERAGE
  const char *recordDir = NULL;
#endif
  GrScreenResolution_t res = GR_RESOLUTION_640x480;
  GrContext_t ctx = 0;
  const char *ext;
  FxU32 pixfmt = 0;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      printUsage(argv[0]);
      return 0;
    }
    if (!strcmp(argv[i], "-n") && i + 1 < argc) { frames = atoi(argv[++i]); frameLimitSpecified = 1; }
    else if (!strcmp(argv[i], "-r") && i + 1 < argc) { res = parseRes(argv[++i]); resolutionSpecified = 1; }
    else if (!strcmp(argv[i], "-m") && i + 1 < argc) featMask = strtoull(argv[++i], NULL, 16);
    else if (!strcmp(argv[i], "-s") && i + 1 < argc) seedBase = (unsigned)strtoul(argv[++i], NULL, 0);
    else if (!strcmp(argv[i], "-a") && i + 1 < argc) { aaReq = atoi(argv[++i]); aaSpecified = 1; }
    else if (!strcmp(argv[i], "-q")) quiet = 1;
    else if (!strcmp(argv[i], "-g")) galleryMode = 1;
    else if (!strcmp(argv[i], "--detail")) highDetail = 1;
    else if (!strcmp(argv[i], "--perf-hud")) perfHud = 1;
    else if (!strcmp(argv[i], "--cinematic")) cinematic = highDetail = 1;
#ifdef ISLAND_SCENE_COVERAGE
    else if (!strcmp(argv[i], "--interactive")) interactive = 1;
    else if (!strcmp(argv[i], "--record") && i + 2 < argc) {
      recordDir = argv[++i];
      recordFps = atoi(argv[++i]);
      if (recordFps < 1 || recordFps > 120) { fprintf(stderr, "--record FPS must be 1 through 120\n"); return 2; }
    }
#endif
    else if (!strcmp(argv[i], "--showcase")) showcase = highDetail = dofReq = 1;
    else if (!strcmp(argv[i], "-d")) dofReq = 1;
    else if (!strcmp(argv[i], "-p") && i + 1 < argc) bppReq = atoi(argv[++i]);
#ifdef ISLAND_SCENE_COVERAGE
    else if (!strcmp(argv[i], "-P")) probeAA = 1;
    else if (!strcmp(argv[i], "-A")) probeAlpha = 1;
    else if (!strcmp(argv[i], "-A-only")) probeAlpha = 2;
#endif
    else {
      fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
      printUsage(argv[0]);
      return 2;
    }
  }
#ifdef ISLAND_SCENE_COVERAGE
  if (interactive && (!platform || !platform->poll_key)) {
    fprintf(stderr, "--interactive requires a platform key callback\n");
    return 2;
  }
  if (recordDir && (!platform || !platform->scanout || !platform->save_frame)) {
    fprintf(stderr, "--record requires platform scanout and capture callbacks\n");
    return 2;
  }
#endif
  if (highDetail) hfSize = HF_N_MAX;
  if (interactive && !frameLimitSpecified) frames = -1;
  if (cinematic) {
    shotDuration = frames > 1 ? (float)(frames - 1) / (float)recordFps : 1.0f;
    /* The scene's coverage variants deliberately alternate depth, update
       bright texture rows in steps and draw a diagnostic frame counter. */
    featMask |= (1ull << F_WBUF) | (1ull << F_TEX_ANIM) | (1ull << F_FOG2);
    if (!interactive && !perfHud) featMask |= 1ull << F_HUD;
  }
  if (showcase && !resolutionSpecified) res = parseRes("1024x768");
  printf("scene_island: seaside island scene, Glide 3 feature coverage\n");

  grGlideInit();
  grSstSelect(0);
  if (showcase && !aaSpecified && !getenv("SSTH3_SLI_AA_CONFIGURATION") && scrH >= 480.0f) {
    FxI32 physicalChips = 0;
    const char *extensions = grGetString(GR_EXTENSION);
    if (extensions && strstr(extensions, "PIXEXT")) {
      grGet(GR_NUM_FB, 4, &physicalChips);
      aaReq = physicalChips >= 4 ? 8 : physicalChips >= 2 ? 4 : 2;
    }
  }
#ifdef ISLAND_SCENE_COVERAGE
  aaArg = aaReq;
#endif
  printf("args: frames %d res %dx%d mask 0x%llx seed %u aa %d%s\n",
         frames, (int)scrW, (int)scrH, featMask, seedBase, aaReq, highDetail ? " detail" : "");
#ifdef ISLAND_SCENE_COVERAGE
  apiPreOpen();
#endif
  printf("version: %s\n", grGetString(GR_VERSION));
  ext = grGetString(GR_EXTENSION);
  printf("hardware: %s\n", grGetString(GR_HARDWARE));
  printf("extensions:%s\n", ext);
  check("grGetString(GR_EXTENSION) available", ext && strcmp(ext, "ERROR") != 0, "got %s", ext ? ext : "(null)");
  detectFeatures(ext ? ext : "", aaReq);
  printf("personality: %s\n", vsa ? "VSA-100 (h5 extensions present)" : "Avenger (no PIXEXT/TEXFMT)");
  /* VSA: the 16bpp LFB probe runs first in its own window, so the last presented frame
     (the exit screenshot) is the scene.  In the AA mode it runs after the scene instead:
     the shim's SLI programming log (SIM_SLIREPORT, 160 entries per chip) must hold the AA
     window's set-up for the board checks. */
#ifdef ISLAND_SCENE_COVERAGE
  if (vsa && aaReq < 2 && probeAlpha != 2) lfb16Probe(res);
#endif

  /* open: 32bpp with 24Z/8S (or -p 16: RGB565), optionally a 2/4/8-sample AA format, on
     PIXEXT; plain 16bpp grSstWinOpen otherwise.  SSTH3_SLI_AA_CONFIGURATION (the control-panel
     variable Glide reads in gpci.c) can also force AA or a single chip: 0 = one chip, 1 = one
     chip 2xAA, 2 = SLI (default), 3/6 = 2xAA, 4/7 = 4xAA (2+ chips), 8 = 8xAA (4 chips). */
  if (getenv("SSTH3_SLI_AA_CONFIGURATION")) envCfg = atoi(getenv("SSTH3_SLI_AA_CONFIGURATION"));
  if (aaReq >= 2 && !featDet[F_TBUFFER]) { printf("AA mode needs PIXEXT; running without AA\n"); aaReq = 0; }
  if (ON(F_PIX32) || aaReq >= 2 || (X.winOpen && (bppReq == 16 || bppReq == 15))) {
    /* GR_PIXFMT_*: rows ARGB8888 / RGB565 / ARGB1555, columns 1, 2, 4, 8 samples */
    static const FxU32 fmtTab[3][4] = { { 0x5, 0x8, 0xb, 0xe }, { 0x3, 0x6, 0x9, 0xc }, { 0x4, 0x7, 0xa, 0xd } };
    int p16 = bppReq == 16 || bppReq == 15, row = bppReq == 16 ? 1 : bppReq == 15 ? 2 : 0;
    FxI32 phys = 0;
    grGet(GR_NUM_FB, 4, &phys);
    pixfmt = fmtTab[row][aaReq >= 8 ? 3 : aaReq >= 4 ? 2 : aaReq >= 2 ? 1 : 0];
    ctx = X.winOpen(window, res, GR_REFRESH_60Hz, GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, pixfmt, 2, 1);
    api("grSstWinOpenExt");
    if (aaReq >= 2 && phys >= 1 && envCfg < 0 && scrH >= 480.0f) {
      /* one chip holds two samples: Glide h5 takes at most 2 x chips samples per pixel */
      char nm[96];
      int maxS = phys >= 4 ? 8 : 2 * (int)phys;
      snprintf(nm, sizeof nm, "%d-sample format on %d chip%s: %s", aaReq, (int)phys, phys > 1 ? "s" : "", aaReq <= maxS ? "opens" : "refused by Glide");
      check(nm, (ctx != 0) == (aaReq <= maxS), "ctx %s", ctx ? "opened" : "refused");
    }
    if (!ctx && aaReq >= 2) {
      /* refused: run the scene without AA in the same colour depth */
      static char m[96];
      snprintf(m, sizeof m, "%d-sample format refused on this board: scene runs without AA", aaReq);
      note(F_TBUFFER, m); note(F_TBUF_SAMPLE, m); note(F_TBUF_DOF, m);
      aaRefused = aaReq; aaReq = 0;
      pixfmt = fmtTab[row][0];
      ctx = X.winOpen(window, res, GR_REFRESH_60Hz, GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, pixfmt, 2, 1);
    }
    pix32 = ctx != 0 && !p16;
    pix15 = ctx != 0 && row == 2;
    if (ctx && pix32) USE(F_PIX32);
    if (ctx && p16) note(F_PIX32, row == 2 ? "-p 15: ARGB1555 window requested" : "-p 16: RGB565 window requested");
    aaSamples = ctx ? 1 : 0;
    if (ctx) {
      FxI32 nfb = 1;
      grGet(GR_NUM_FB, 4, &nfb); nChips = (int)nfb;
      if (aaReq >= 2) aaSamples = aaReq;
      else if (envCfg >= 0) {                       /* what Glide forced (gpci.c, gsst.c) */
        int es = (envCfg == 1 || envCfg == 3 || envCfg == 6) ? 2 : (envCfg == 4 || envCfg == 7) ? 4 : envCfg == 8 ? 8 : 0;
        if (es == 8 && nChips <= 2) es = 0;
        if (es == 4 && nChips <= 1) es = 0;
        if (es) aaSamples = es;
      }
    }
  }
  if (ctx) api("grSstWinOpenExt");
  if (!ctx) { ctx = grSstWinOpen(window, res, GR_REFRESH_60Hz, GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, 2, 1); api("grSstWinOpen"); }
  check("window opened", ctx != 0, NULL);
  if (!ctx) { printf("scene_island: 0 frames -> FAIL\n"); grGlideShutdown(); return 1; }
  printf("pixel format: %s%s\n", pix32 ? "ARGB8888 + Z24/S8" : pix15 ? "ARGB1555 + Z16" : "RGB565 + Z16",
         aaSamples == 8 ? ", 8-sample AA" : aaSamples == 4 ? ", 4-sample AA" : aaSamples == 2 ? ", 2-sample AA" : "");
  tbufAll = (1u << (aaSamples > 1 ? aaSamples : 1)) - 1;
  haveStencil = pix32 && X.stencilFunc && X.stencilOp && X.stencilMask;
  { FxI32 nfb = 1; grGet(GR_NUM_FB, 4, &nfb); nChips = (int)nfb; sliUnitsNow = (int)nfb / (aaSamples > 1 ? aaSamples : 1); if (sliUnitsNow < 1) sliUnitsNow = 1;
    /* FX_GLIDE_FORCE_OLD_AA on 2 chips: 2-way SLI with both samples on each chip */
    if (getenv("FX_GLIDE_FORCE_OLD_AA") && nChips == 2 && aaSamples == 2) sliUnitsNow = 2;
    printf("chips %d, SLI units %d\n", (int)nfb, sliUnitsNow); }
  focal = scrW * 0.5f / tanf(30.0f * 3.14159265f / 180.0f);
  printf("MODE personality=%s chips=%d sli_units=%d aa_samples=%d bpp=%d depth=%s stencil=%s fog=%s res=%dx%d env_sli_aa=%d tbuffer=%s\n",
         vsa ? "vsa" : "h3", nChips, sliUnitsNow, aaSamples > 1 ? aaSamples : 1, pix32 ? 32 : pix15 ? 15 : 16,
         ON(F_WBUF) ? "z/w-alternating" : "z", haveStencil ? "8-bit" : "none",
         haveFogCoord ? "table-on-fogcoord(z frames)+table-on-q(w frames)" : "table-on-q", (int)scrW, (int)scrH, envCfg,
         aaSamples > 1 ? (dofReq ? "motion-blur+depth-of-field" : "motion-blur") : aaRefused ? "refused" : "off");

  queryBoard();
  makeLayouts();
#ifdef ISLAND_SCENE_COVERAGE
  if (probeAlpha == 2) {
    if (nChips != 1 || aaSamples > 1) {
      printf("alpha/depth probe requires a single-chip, non-AA framebuffer\n");
      grSstWinClose(ctx);
      grGlideShutdown();
      return 2;
    }
    grCullMode(GR_CULL_DISABLE);
    alphaDepthProbe(0);
    if (ON(F_COMBINE_EXT)) alphaDepthProbe(1);
    grBufferSwap(0);
    grFinish();
    grSstWinClose(ctx);
    grGlideShutdown();
    printf("scene_island: alpha/depth only, %d checks, %d failed -> %s\n",
           nChecks, nFail, nFail ? "FAIL" : "PASS");
    return nFail != 0;
  }
#endif

  {
    FxI32 gsz = 0; void *st;
    grGet(GR_GLIDE_STATE_SIZE, 4, &gsz);
    st = calloc((size_t)gsz + 16, 1);
    grGlideGetState(st);                     /* initial state, restored before shutdown */
    texCur[0] = grTexMinAddress(GR_TMU0); texEnd[0] = grTexMaxAddress(GR_TMU0);
    if (nTmu > 1) { texCur[1] = grTexMinAddress(GR_TMU1); texEnd[1] = grTexMaxAddress(GR_TMU1); }
    printf("texture memory: tmu0 0x%x-0x%x", (unsigned)texCur[0], (unsigned)texEnd[0]);
    if (nTmu > 1) printf(", tmu1 0x%x-0x%x", (unsigned)texCur[1], (unsigned)texEnd[1]);
    printf("\n");
    buildPalettes(); buildNcc();
    if (X.tableExt) {                         /* per-TMU tables: the leaf palette on TMU1 too */
      X.tableExt(GR_TMU0, GR_TEXTABLE_PALETTE, &palSunset);
      if (nTmu > 1) X.tableExt(GR_TMU1, GR_TEXTABLE_PALETTE, &palSunset);
    }
    loadTextures();
    check("texture allocations fit", texAllocFails == 0, "%d failed", texAllocFails);
    printf("large texture: %dx%d RGB565%s\n", bigSize, bigSize, texBig.ok ? (texBig.tmu ? " on TMU1" : " on TMU0") : " (none)");

    /* fog table: exponential in W, generated here */
    for (i = 0; i < fogEntries && i < 128; i++) {
      float w = guFogTableIndexToW(i), fv = 1.0f - expf(-w / 420.0f);
      fogTable[i] = (GrFog_t)clampi((int)(255.0f * fv), 0, 255);
    }
    grFogTable(fogTable);
    grFogColorValue(0xffd8d0c0u);
    gammaSetup();
    placeProps();
    grDitherMode(GR_DITHER_4x4);
    grCullMode(GR_CULL_DISABLE);
    if (haveStencil) X.lfbConstStencil(0);
#ifdef ISLAND_SCENE_COVERAGE
    apiSweep(ctx);                           /* before frame 0: its test draws are covered by the scene */
#endif
    grFogTable(fogTable);

    int sceneFrame = 0, lastAnimatedFrame = -1;
#ifdef ISLAND_SCENE_COVERAGE
    unsigned recordStart = recordDir ? platform->scanout(platform->context) : 0;
#endif
    for (frame = 0; (interactive && frames == -1) || frame < frames; frame++) {
      FxU32 clearDepth;
#ifdef ISLAND_SCENE_COVERAGE
      pollControls(frame, &dofReq);
#endif
      if (quitScene) break;
      grReset(GR_STATS_POINTS); grReset(GR_STATS_LINES);
      statTris = statPoints = statLines = 0;
      tNow = (float)sceneFrame / (float)recordFps;
      useW = ON(F_WBUF) ? (sceneFrame & 1) : 0;
      if (useW) USE(F_WBUF);
      setCamera(sceneFrame);
      useLayouts(!useW);
      clearDepth = useW ? wrange[1] : zrange[1];
      grDepthBufferMode(useW ? GR_DEPTHBUFFER_WBUFFER : GR_DEPTHBUFFER_ZBUFFER);
      grColorMask(FXTRUE, FXFALSE);
      grDepthMask(FXTRUE);
      if (aaSamples > 1 && X.tbufMask) X.tbufMask(tbufAll);
      if (haveStencil) X.bufferClear(0xff8fb8e0u, 0, clearDepth, 0);
      else grBufferClear(0xff8fb8e0u, 0, clearDepth);
#ifdef ISLAND_SCENE_COVERAGE
      /* LFB round trips before the scene: the picture then covers the test patches
         (with -g they run after the scene and stay visible) */
      if (!galleryMode && (frame == 0 || frame == frames - 1)) lfbChecks(frame);
#endif

      if (sceneFrame != lastAnimatedFrame) {
        animTextures(sceneFrame);
        lastAnimatedFrame = sceneFrame;
      }
      if (dofReq && aaSamples > 1 && ON(F_TBUF_DOF) && X.tbufMask) {
        /* T-buffer depth of field: the scene once per sample buffer, the eye moved around
           the focus point (hut) per sample; the video AA filter averages them */
        static const float jit[8][2] = { { 0.35f, 0.0f }, { -0.35f, 0.0f }, { 0.0f, 0.3f }, { 0.0f, -0.3f },
                                         { 0.25f, 0.25f }, { -0.25f, -0.25f }, { 0.25f, -0.25f }, { -0.25f, 0.25f } };
        int smp;
        inDof = 1;
        for (smp = 0; smp < aaSamples; smp++) {
          X.tbufMask(1u << smp);
          camJit[0] = jit[smp][0]; camJit[1] = jit[smp][1];
          setCamera(sceneFrame);
          drawScene(sceneFrame);
        }
        inDof = 0;
        camJit[0] = camJit[1] = 0.0f;
        setCamera(sceneFrame);
        X.tbufMask(tbufAll);
        USE(F_TBUF_DOF); USE(F_TBUFFER);
      } else drawScene(sceneFrame);
      drawGallery();
      if (perfHud && ON(F_HUD)) samplePerfHud();
      drawHud(frame, dofReq);
#ifdef ISLAND_SCENE_COVERAGE
      if (galleryMode && (frame == 0 || frame == frames - 1)) lfbChecks(frame);
      if (probeAlpha && frame == frames - 1) {
        if (nChips == 1 && aaSamples < 2) {
          alphaDepthProbe(0);
          if (ON(F_COMBINE_EXT)) alphaDepthProbe(1);
        } else printf("alpha/depth probe skipped: requires a single-chip, non-AA framebuffer\n");
      }
#endif

      {
        FxI32 gp = 0, gl = 0;
        grGet(GR_STATS_POINTS, 4, &gp); grGet(GR_STATS_LINES, 4, &gl);
        if (frame == 0 || frame == frames - 1) {
          char nm[64];
          snprintf(nm, sizeof nm, "grGet GR_STATS_POINTS/LINES match frame %d", frame);
          check(nm, (unsigned long)gp == statPoints && (unsigned long)gl == statLines,
                "glide %d/%d, scene %lu/%lu", (int)gp, (int)gl, statPoints, statLines);
        }
      }
      if (!quiet) printf("frame %d: %s, tris %lu points %lu lines %lu\n", frame, useW ? "W" : "Z", statTris, statPoints, statLines);
      grBufferSwap((frame % 8) == 7 ? 0 : 1);   /* swap interval 1, every 8th frame 0 */
      if (frame == 0) api("grBufferSwap");
#ifdef ISLAND_SCENE_COVERAGE
      if (recordDir) {
        char path[1024];
        unsigned captured = 0, expected = recordStart + (unsigned)frame + 1;
        int len = snprintf(path, sizeof path, "%s/frame_%05d.ppm", recordDir, frame);
        if (len < 0 || len >= (int)sizeof path || !platform->save_frame(platform->context, path, &captured)) {
          check("record presented frame", 0, "frame %d", frame);
          break;
        }
        if (captured != expected) {
          check("capture follows the swap sequence", 0, "file frame %d: expected scanout %u, captured %u",
                frame, expected, captured);
          break;
        }
        printf("capture frame %d scanout %u\n", frame, captured);
      }
#endif
      if (!paused) sceneFrame++;
    }
    grGlideSetState(st);
    free(st);
  }
  check("frame count reached or quit requested", frame == frames || (interactive && quitScene), "%d of %d", frame, frames);
  check("lfb locks granted", lfbLockFails == 0, "%d refused", lfbLockFails);
  {
    /* every texture format the board takes is drawn by something in the picture */
    static const int base[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe };
    static const int ext[] = { 0x11, 0x12, 0x16, 0x17, 0x18, 0x19, 0x1a };
    int miss = 0, k;
    char lst[128] = "";
    for (k = 0; k < (int)(sizeof base / sizeof base[0]); k++)
      if (!fmtUsed[base[k]]) { miss++; snprintf(lst + strlen(lst), sizeof lst - strlen(lst), " 0x%x", base[k]); }
    if (featDet[F_TEX8888] && !featMask)
      for (k = 0; k < (int)(sizeof ext / sizeof ext[0]); k++)
        if (!fmtUsed[ext[k]]) { miss++; snprintf(lst + strlen(lst), sizeof lst - strlen(lst), " 0x%x", ext[k]); }
    if (!featMask) check("every texture format the board takes is drawn", miss == 0, "missing%s", lst);
    if (miss == 0) USE(F_GALLERY);
    for (i = 0; i < NSHELL; i++) if (shells[i].tex.ok && !fmtUsed[shells[i].fmt]) miss++;
    if (galleryMode) check("every gallery format drawn (-g)", miss == 0, "%d not drawn", miss);
  }
  grSstWinClose(ctx); api("grSstWinClose");
#ifdef ISLAND_SCENE_COVERAGE
  /* AA request: the 16bpp probe would replace the scene as the last presented frame (the
     exit screenshot) and, run first, would fill the shim's SLI programming log ahead of
     the AA window's set-up.  It is left out unless -P asks for it (then after the scene). */
  if (vsa && aaArg >= 2) {
    if (probeAA) lfb16Probe(res);
    else apiWhy("grSstWinOpen", "16bpp probe not run after an AA request (keeps the scene as the exit picture; -P runs it)");
  }
#endif

  /* feature report, stable order */
  printf("--- features (detected / exercised)\n");
  for (f = 0; f < F_COUNT; f++) {
    const char *st = featUsed[f] ? "yes" : (featMask >> f & 1) ? "masked" : "no";
    if (f == F_SHADOW && featUsed[f] && !haveStencil) st = "fallback";
    if (f == F_PIX32 && !featUsed[f] && featDet[f] && aaSamples > 1) st = "yes";
    printf("FEATURE %02d %-26s detected=%-3s exercised=%-8s %s\n", f, featName[f], featDet[f] ? "yes" : "no", st, featNote[f]);
  }
  printf("--- texture formats drawn:");
  for (i = 0; i < 0x20; i++) if (fmtUsed[i]) printf(" 0x%02x", i);
  printf("\n");
  if (vsa) printf("--- downloaded, not drawn: YUYV_422/UYVY_422/AYUV_444 (h5 grTexSource has no textureMode mapping: 0x13-0x15 overflow the 4-bit tformat field)\n");
  api("grGlideShutdown");
#ifdef ISLAND_SCENE_COVERAGE
  {
    int miss = apiReport();
    check("every Linux-applicable entry point called", miss == 0, "%d missing", miss);
  }
#endif

#ifdef ISLAND_SCENE_COVERAGE
  printf("glide error callbacks: %d\n", errCalls);
#endif
  grGlideShutdown();
  printf("scene_island: %d frames, %d checks, %d failed, %d informational (%d differ) -> %s\n",
         frame, nChecks, nFail, nInfo, nInfoBad, nFail ? "FAIL" : "PASS");
  return nFail ? 1 : 0;
}

#ifdef ISLAND_SCENE_COVERAGE
int island_scene_run(int argc, char **argv)
{
  return island_scene_run_with_platform(argc, argv, NULL);
}
#endif

#ifdef ISLAND_SCENE_MAIN
int main(int argc, char **argv)
{
  return island_scene_run(argc, argv);
}
#endif

#endif /* ISLAND_SCENE_IMPLEMENTATION */
