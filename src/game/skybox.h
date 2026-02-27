#ifndef SKYBOX_H
#define SKYBOX_H

#include <PR/ultratypes.h>
#include <PR/gbi.h>


extern s8 gReadOnlyBackground;
extern s8 gOverrideBackground;
extern Color gSkyboxColor;

extern Texture* gCustomSkyboxPtrList[];

Gfx *create_skybox_facing_camera(s8 player, s8 background, f32 fov,
                                 f32 posX, f32 posY, f32 posZ,
                                 f32 focX, f32 focY, f32 focZ);

Gfx *create_vr_skybox(s8 player, s8 background, s8 colorIndex);

#endif // SKYBOX_H
