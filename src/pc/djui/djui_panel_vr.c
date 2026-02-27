#include "djui.h"
#include "djui_panel.h"
#include "djui_panel_menu.h"
#include "djui_panel_vr.h"
#include "djui_slider.h"
#include "pc/utils/misc.h"
#include "pc/configfile.h"

static struct DjuiSelectionbox* sSnapAngleSelection = NULL;
static float sRenderScaleValues[10] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f, 2.75f, 3.0f };
static unsigned int sRenderScaleIndex = 2;

static void djui_panel_vr_render_scale_change(UNUSED struct DjuiBase* caller) {
    configVrRenderScale = sRenderScaleValues[sRenderScaleIndex];
}

static void djui_panel_vr_turn_mode_change(UNUSED struct DjuiBase* caller) {
    if (sSnapAngleSelection) {
        djui_base_set_enabled(&sSnapAngleSelection->base, configVrTurnMode != 0);
    }
}

void djui_panel_vr_create(struct DjuiBase* caller) {
    struct DjuiThreePanel* panel = djui_panel_menu_create(DLANG(VR, VR), false);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    {
        djui_checkbox_create(body, DLANG(VR, FIRST_PERSON_CAMERA), &configVrFirstPersonCamera, NULL);
        djui_checkbox_create(body, DLANG(VR, ASPECT_RATIO_CORRECTION), &configVrAspectRatioCorrection, NULL);

        sRenderScaleIndex = 2;
        for (int i = 0; i < 10; i++) {
            if (configVrRenderScale == sRenderScaleValues[i]) {
                sRenderScaleIndex = i;
                break;
            }
        }

        char* renderScaleChoices[10] = { "0.5x","0.75x", "1.0x", "1.25x", "1.5x", "1.75x", "2.0x", "2.25x", "2.5x", "2.75x", "3.0x" };
        djui_selectionbox_create(body, DLANG(VR, RENDER_SCALE), renderScaleChoices, 10, &sRenderScaleIndex, djui_panel_vr_render_scale_change);
        
        char* hudPositionChoices[2] = { DLANG(VR, HUD_POSITION_HEAD_LOCKED), DLANG(VR, HUD_POSITION_LEFT_HAND) };
        djui_selectionbox_create(body, DLANG(VR, HUD_POSITION), hudPositionChoices, 2, &configVrHudPosition, NULL);
        djui_slider_create(body, DLANG(VR, HUD_DISTANCE), &configVrHudDistance, 1, 25, NULL);
        djui_slider_create(body, DLANG(VR, HUD_YAW), &configVrHudYaw, 0, 100, NULL);
        djui_slider_create(body, DLANG(VR, HUD_PITCH), &configVrHudPitch, 0, 100, NULL);
        djui_slider_create(body, DLANG(VR, HUD_X), &configVrHudX, 0, 100, NULL);
        djui_slider_create(body, DLANG(VR, HUD_Y), &configVrHudY, 0, 100, NULL);

        char* skyboxChoices[3] = { DLANG(VR, SKYBOX_OFF), DLANG(VR, SKYBOX_ORIGINAL), DLANG(VR, SKYBOX_3D) };
        djui_selectionbox_create(body, DLANG(VR, SKYBOX), skyboxChoices, 3, &configVrSkybox, NULL);

        char* turnModeChoices[2] = { DLANG(VR, TURN_MODE_CONTINUOUS), DLANG(VR, TURN_MODE_SNAP) };
        djui_selectionbox_create(body, DLANG(VR, TURN_MODE), turnModeChoices, 2, &configVrTurnMode, djui_panel_vr_turn_mode_change);

        char* snapAngleChoices[12] = { "15", "30", "45", "60", "75", "90", "105", "120", "135", "150", "165", "180" };
        sSnapAngleSelection = djui_selectionbox_create(body, DLANG(VR, SNAP_ANGLE), snapAngleChoices, 6, &configVrSnapAngle, NULL);
        
        djui_panel_vr_turn_mode_change(NULL); // Apply initial state

        djui_button_create(body, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    }
    
    djui_panel_add(caller, panel, NULL);
}
