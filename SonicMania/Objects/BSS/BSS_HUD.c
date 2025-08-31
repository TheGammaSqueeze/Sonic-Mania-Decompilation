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

void BSS_HUD_StaticUpdate(void) {}

void BSS_HUD_Draw(void)
{
    RSDK_THIS(BSS_HUD);

    // ---- UI "virtual 320" safe area + snapped scale (0x200 == 1.0) ----
    const int32 baseW = 320;
    const int32 scrW  = ScreenInfo->size.x;
    // prefer true integer scale when scrW is a multiple of 320
    int32 uiScale; // fixed 9.9 format where 0x200 == 1.0
    {
        const int32 mul = scrW / baseW;
        if (mul >= 1 && (scrW % baseW) == 0) {
            // perfect integer upscale (1x, 2x, 3x…)
            uiScale = mul * 0x200;
        } else {
            // fractional case (downscale or non-integer upscale):
            // snap to nearest 1/STEP_DIV step for stable sampling
            #define STEP_DIV 4  /* 2=halves, 4=quarters, 8=eighths */
            const int32 raw  = (scrW * 0x200) / baseW;
            const int32 step = 0x200 / STEP_DIV;
            // round to nearest step
            int32 snapped = (raw + step / 2) / step;
            if (snapped < 1) snapped = 1;               // minimum 0.25x/0.5x/… depending on STEP_DIV
            uiScale = snapped * step;
            #undef STEP_DIV
        }
    }
    // effective UI width after snapping (used to center safely)
    const int32 uiW   = (baseW * uiScale) >> 9;  // >>9 because 0x200 == 512
    const int32 safeL = (scrW > uiW) ? (scrW - uiW) / 2 : 0;
    // helper: scale authored pixels to screen pixels under snapped scale
    #define UI(px)  ((int32)(((px) * uiScale) >> 9))

    // apply scale to this entity so all DrawSprite() calls scale uniformly
    self->drawFX |= FX_SCALE;
    self->scale.x = uiScale;
    self->scale.y = uiScale;

    Vector2 drawPos;
    // Authored positions on a 320-wide canvas:
    //  left icon @ x=19,   y=13
    //  left numbers start @ x=56, y=17 (draws right->left)
    //  right icon @ x=224, y=13
    //  right numbers start @ x=280,y=17 (draws right->left)
    drawPos.x = TO_FIXED(safeL + UI(19));
    drawPos.y = TO_FIXED(UI(13));
    RSDK.DrawSprite(&self->sphereAnimator, &drawPos, true);

    drawPos.x = TO_FIXED(safeL + UI(56));
    drawPos.y = TO_FIXED(UI(17));
    BSS_HUD_DrawNumbers(BSS_Setup->sphereCount, &drawPos);

    drawPos.x = TO_FIXED(safeL + UI(224));
    drawPos.y = TO_FIXED(UI(13));
    RSDK.DrawSprite(&self->ringAnimator, &drawPos, true);

    drawPos.x = TO_FIXED(safeL + UI(280));
    drawPos.y = TO_FIXED(UI(17));
    BSS_HUD_DrawNumbers(BSS_Setup->ringCount, &drawPos);
    
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

void BSS_HUD_DrawNumbers(int32 value, Vector2 *drawPos)
{
    RSDK_THIS(BSS_HUD);

    // Keep digit spacing consistent with the snapped HUD scale
    const int32 baseW = 320;
    const int32 scrW  = ScreenInfo->size.x;
    int32 uiScale;
    {
        const int32 mul = scrW / baseW;
        if (mul >= 1 && (scrW % baseW) == 0) {
            uiScale = mul * 0x200;
        } else {
            #define STEP_DIV 4
            const int32 raw  = (scrW * 0x200) / baseW;
            const int32 step = 0x200 / STEP_DIV;
            int32 snapped = (raw + step / 2) / step;
            if (snapped < 1) snapped = 1;
            uiScale = snapped * step;
            #undef STEP_DIV
        }
    }
    #define UI(px)  ((int32)(((px) * uiScale) >> 9))

    int32 mult = 1;
    for (int32 i = 0; i < 3; ++i) {
        self->numbersAnimator.frameID = value / mult % 10;
        RSDK.DrawSprite(&self->numbersAnimator, drawPos, true);
        drawPos->x -= TO_FIXED(UI(16));
        mult *= 10;
    }
    #undef UI
}

#if GAME_INCLUDE_EDITOR
void BSS_HUD_EditorDraw(void) {}

void BSS_HUD_EditorLoad(void) {}
#endif

void BSS_HUD_Serialize(void) {}
