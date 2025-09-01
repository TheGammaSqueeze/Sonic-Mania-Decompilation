// ---------------------------------------------------------------------
// RSDK Project: Sonic Mania
// Object Description: BSS_HUD Object
// Object Author: Christian Whitehead/Simon Thomley/Hunter Bridges
// Decompiled by: Rubberduckycooly & RMGRich
// ---------------------------------------------------------------------

#include "Game.h"

ObjectBSS_HUD *BSS_HUD;

void BSS_HUD_Update(void) {}

void BSS_HUD_LateUpdate(void) {}

// Remember the final 9.9 HUD scale chosen during BSS_HUD_Draw so external
// callers using the legacy API can render digits with identical spacing.
static int32 s_uiScale9_9 = 0x200; // default 1.0x

static void BSS_HUD_DrawNumbersScaled(int32 value, Vector2 *drawPos, int32 uiScale);

void BSS_HUD_StaticUpdate(void) {}

void BSS_HUD_Draw(void)
{
    RSDK_THIS(BSS_HUD);

    // ---- UI "virtual 320" safe area + snapped scale (0x200 == 1.0) ----
    const int32 baseW = 320;
    const int32 scrW  = ScreenInfo->size.x;           // in-scene width
    const int32 leftG = ScreenInfo->position.x;       // left gutter (outside scene)
    const int32 fullW = scrW + (leftG << 1);          // total window width (scene + gutters)
    const int32 baseStep = 0x80;                      // 0x200/4 -> quarter-step snapping for stable sampling
    // prefer true integer scale when scrW is a multiple of 320
    int32 uiScale; // fixed 9.9 format where 0x200 == 1.0
    {
        // Use full window width for HUD scale so it fills the letterbox space on portrait devices.
        const int32 mul = fullW / baseW;
        if (mul >= 1 && (fullW % baseW) == 0) {
            // perfect integer upscale (1x, 2x, 3x…)
            uiScale = mul * 0x200;
        } else {
            // fractional case (downscale or non-integer upscale): use the true (smooth) fraction
            const int32 raw  = (fullW * 0x200) / baseW;
            if (raw < 1) {
                uiScale = 1; // safety
            } else {
                uiScale = raw;
            }
        }
    }

    // --- clamp scale so HUD always fits with side padding ---
    // padding is authored pixels in the 320-wide space (applied after scale)
    const int32 padL_px = 0;
    const int32 padR_px = 0;
    {
        // -------- minimum readable size (configurable) --------
        // e.g. 2.0x -> MIN_NUM=2, MIN_DEN=1  |  1.5x -> 3/2
        #define MIN_NUM 2
        #define MIN_DEN 1
        const int32 minScaleDesired = (0x200 * MIN_NUM) / MIN_DEN; // 9.9 fixed
        if (uiScale < minScaleDesired) uiScale = minScaleDesired;

        // First cap to the full window (lets HUD grow on portrait)
        const int32 maxScaleWindow = ((fullW - (padL_px + padR_px)) << 9) / baseW; // 9.9 that still fits in window
        if (uiScale > maxScaleWindow) uiScale = maxScaleWindow;

        // IMPORTANT: the BSS HUD draws in the scene and is clipped by the viewport.
        // To avoid disappearing, ensure the final HUD width can be placed inside the scene.
        const int32 maxScaleScene  = ((scrW - (padL_px + padR_px)) << 9) / baseW;
        if (maxScaleScene > 0 && uiScale > maxScaleScene)
            uiScale = maxScaleScene;

        #undef MIN_NUM
        #undef MIN_DEN

        // Snap to a stable step (quarters) to reduce shimmering at non-integer scales.
        // This is a "sharp bilinear"-style compromise: still fractional, but phase-stable.
        {
            const int32 step = baseStep;                      // 0x80 = 1/4
            uiScale = ((uiScale + (step >> 1)) / step) * step; // round to nearest step
        }

        // OPTIONAL (commented): force a readable floor (e.g., 1.0x) even if it clips.
        // Uncomment to test, knowing it will crop at scene edges:
        if (uiScale < 0x200) uiScale = 0x180;
    }
    // effective UI width after final scale (used to place safely)
    const int32 uiW   = (baseW * uiScale) >> 9;  // >>9 because 0x200 == 512

    // expose the exact final scale for external digit draws
    s_uiScale9_9 = uiScale;

    // helper: scale authored pixels to screen pixels under snapped scale
    // NOTE: UIR() rounds to nearest pixel to reduce shimmering at non-integer scales (“sharp bilinear”-like presentation).
    #define UI(px)   ((int32)(((int64_t)(px) * (int64_t)uiScale) >> 9))
    #define UIR(px)  ((int32)((((int64_t)(px) * (int64_t)uiScale) + 0x100) >> 9)) /* +0x100 = round-half-up in 9.9 */

    // Mirror the right HUD’s behavior: anchor from the left edge with the same padding,
    // rather than centering the whole HUD when there are side gutters.
    // This keeps the left HUD flush with the left margin (like the right HUD on the right).
    int32 safeL_scene;
    {
        // Anchor from scene-left (x=0) + left padding, matching right HUD
        // which anchors from scene-right (x=scrW) - right padding.
        int32 tmp = UIR(padL_px);
        const int32 maxL = (scrW > uiW) ? (scrW - uiW) : UIR(padL_px);
        if (tmp < UIR(padL_px)) tmp = UIR(padL_px);
        if (tmp > maxL)         tmp = maxL;
        safeL_scene = tmp; // no extra snapping to avoid bias
    }

    // also compute a right anchor that guarantees at least padR on the right edge (scene-mapped)
    int32 rightAnchor_scene = (fullW - UIR(padR_px)) - leftG;
    if (rightAnchor_scene < UIR(padR_px)) rightAnchor_scene = UIR(padR_px);
    if (rightAnchor_scene > scrW) rightAnchor_scene = scrW;

    // apply scale to this entity so all DrawSprite() calls scale uniformly
    self->drawFX |= FX_SCALE;
    self->scale.x = uiScale;
    self->scale.y = uiScale;

    // When using non-integer scaling, snap sprite positions to integer pixels to minimize sampling aliasing.
    // (Acts like a “sharp bilinear” presentation without engine-level sampler toggles.)
    #define SNAP_INT_PIX(px)   ((px))  // UI()/UIR() already produce integer pixels; keep for clarity.

    Vector2 drawPos;
    // Authored positions on a 320-wide canvas:
    //  left icon @ x=19,   y=13
    //  left numbers start @ x=56, y=17 (draws right->left)
    //  right icon @ x=224, y=13
    //  right numbers start @ x=280,y=17 (draws right->left)
    drawPos.x = TO_FIXED(SNAP_INT_PIX(safeL_scene + UIR(19)));
    drawPos.y = TO_FIXED(SNAP_INT_PIX(UIR(13)));
    RSDK.DrawSprite(&self->sphereAnimator, &drawPos, true);

    drawPos.x = TO_FIXED(SNAP_INT_PIX(safeL_scene + UIR(56)));
    drawPos.y = TO_FIXED(SNAP_INT_PIX(UIR(17)));
    BSS_HUD_DrawNumbers(BSS_Setup->sphereCount, &drawPos);

    // Distance-from-right authored pixels (scene-mapped):
    //   320 - 224 = 96 (right icon), 320 - 280 = 40 (right numbers start)
    drawPos.x = TO_FIXED(SNAP_INT_PIX(rightAnchor_scene - UIR(96)));
    drawPos.y = TO_FIXED(SNAP_INT_PIX(UIR(13)));
    RSDK.DrawSprite(&self->ringAnimator, &drawPos, true);

    drawPos.x = TO_FIXED(SNAP_INT_PIX(rightAnchor_scene - UIR(40)));
    drawPos.y = TO_FIXED(SNAP_INT_PIX(UIR(17)));
    BSS_HUD_DrawNumbers(BSS_Setup->ringCount, &drawPos);
    
    #undef SNAP_INT_PIX
    #undef ALIGN_TO_SCALE_PIX
    #undef UIR
    #undef UI
}

void BSS_HUD_Create(void *data)
{
    RSDK_THIS(BSS_HUD);

    if (!SceneInfo->inEditor) {
        self->active        = ACTIVE_NORMAL;
        self->visible       = true;
        self->drawGroup     = DRAWGROUP_COUNT - 1;
        self->updateRange.x = TO_FIXED(128);
        self->updateRange.y = TO_FIXED(128);

        RSDK.SetSpriteAnimation(BSS_HUD->aniFrames, 0, &self->sphereAnimator, true, 0);
        RSDK.SetSpriteAnimation(BSS_HUD->aniFrames, 0, &self->ringAnimator, true, 1);
        RSDK.SetSpriteAnimation(BSS_HUD->aniFrames, 1, &self->numbersAnimator, true, 0);
    }
}

void BSS_HUD_StageLoad(void)
{
    BSS_HUD->aniFrames = RSDK.LoadSpriteAnimation("SpecialBS/HUD.bin", SCOPE_STAGE);

    RSDK.ResetEntitySlot(SLOT_BSS_HUD, BSS_HUD->classID, NULL);
}

static void BSS_HUD_DrawNumbersScaled(int32 value, Vector2 *drawPos, int32 uiScale)
{
    RSDK_THIS(BSS_HUD);

    #define UI(px)   ((int32)(((int64_t)(px) * (int64_t)uiScale) >> 9))
    #define UIR(px)  ((int32)((((int64_t)(px) * (int64_t)uiScale) + 0x100) >> 9)) /* rounded */
 

    // tighten spacing so digits stay inside HUD box
    const int32 advBase = UIR(14);
    const int32 kernOne = UIR(2);

    // Snap starting position to integer pixels to reduce shimmer at non-integer scales.
    drawPos->x = TO_FIXED((drawPos->x >> 16)); // already fixed; quantize to integer pixel
    drawPos->y = TO_FIXED((drawPos->y >> 16));

    int32 mult = 1;
    for (int32 i = 0; i < 3; ++i) {
        int32 digit = (value / mult) % 10;
        self->numbersAnimator.frameID = digit;
        RSDK.DrawSprite(&self->numbersAnimator, drawPos, true);

        int32 adv = advBase;
        if (digit == 1)
            adv -= kernOne;

        // integer pixel advance
        drawPos->x -= TO_FIXED(adv);
        mult *= 10;
    }
    #undef UIR
    #undef UI
}

void BSS_HUD_DrawNumbers(int32 value, Vector2 *drawPos)
{
    RSDK_THIS(BSS_HUD);

    // Keep digit spacing consistent with the snapped HUD scale
    BSS_HUD_DrawNumbersScaled(value, drawPos, s_uiScale9_9);
}

#if GAME_INCLUDE_EDITOR
void BSS_HUD_EditorDraw(void) {}

void BSS_HUD_EditorLoad(void) {}
#endif

void BSS_HUD_Serialize(void) {}
