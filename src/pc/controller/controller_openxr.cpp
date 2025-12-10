#ifdef OPENXR_ENABLED

#include "controller_openxr.h"

#include <ultra64.h>
#include <PR/os_cont.h>
#include "pc/openxr/openxr_manager.h"
#include "pc/openxr/openxr_keyboard.h"

extern "C" {

#include "pc/pc_main.h"
#include "pc/djui/djui.h"
}

#include <iostream>
#include <vector>
#include <cmath>

#define OPENXR_CONTROLLER_DEBUG 0

// OpenXR headers are already included via openxr_manager.h -> openxr_instance.h usually, 
// but we might need to include them directly if not.
// Assuming openxr_manager.h provides access or we need to include them.
#include <openxr/openxr.h>

// Helper macros for OpenXR error checking
#define XR_CHECK(x) { \
    XrResult result = (x); \
    if (XR_FAILED(result)) { \
        std::cerr << "OpenXR Error: " << #x << " failed with " << result << std::endl; \
    } \
}

#ifndef XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_LEFT_META
#define XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_LEFT_META (XrVirtualKeyboardInputSourceMETA)1
#endif
#ifndef XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_RIGHT_META
#define XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_RIGHT_META (XrVirtualKeyboardInputSourceMETA)2
#endif

static bool s_initialized = false;
static XrActionSet s_actionSet = XR_NULL_HANDLE;

// Actions
static XrAction s_actionJump = XR_NULL_HANDLE;
static XrAction s_actionAttack = XR_NULL_HANDLE;
static XrAction s_actionCrouch = XR_NULL_HANDLE; // Z-trigger
static XrAction s_actionCamera = XR_NULL_HANDLE; // Right stick / C-buttons
static XrAction s_actionMovement = XR_NULL_HANDLE; // Left stick
static XrAction s_actionStart = XR_NULL_HANDLE;
static XrAction s_actionL = XR_NULL_HANDLE;
static XrAction s_actionR = XR_NULL_HANDLE;
static XrAction s_actionToggleKeyboard = XR_NULL_HANDLE; // Y button

// Paths
static XrPath s_pathHandLeft = XR_NULL_PATH;
static XrPath s_pathHandRight = XR_NULL_PATH;

// Keyboard interaction
static XrAction s_actionPoseLeft = XR_NULL_PATH;
static XrAction s_actionPoseRight = XR_NULL_PATH;
static XrAction s_actionSelectLeft = XR_NULL_PATH;
static XrAction s_actionSelectRight = XR_NULL_PATH;
static XrSpace s_spacePoseLeft = XR_NULL_HANDLE;
static XrSpace s_spacePoseRight = XR_NULL_HANDLE;

// Separate reference space for keyboard (STAGE without rotation/offset)
static XrSpace s_keyboardReferenceSpace = XR_NULL_HANDLE;

static void controller_openxr_init(void) {
    if (s_initialized) return;
    
    if (!openxr_is_initialized()) {
        std::cerr << "OpenXR not initialized, skipping controller init" << std::endl;
        return;
    }

    XrInstance instance = openxr_get_instance();
    XrSession session = openxr_get_session();
    
    if (instance == XR_NULL_HANDLE || session == XR_NULL_HANDLE) {
        std::cerr << "OpenXR instance or session not ready" << std::endl;
        return;
    }

    std::cout << "Initializing OpenXR Controller Backend..." << std::endl;

    // Create Action Set
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy(actionSetInfo.actionSetName, "gameplay");
    strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
    actionSetInfo.priority = 0;
    XR_CHECK(xrCreateActionSet(instance, &actionSetInfo, &s_actionSet));

    // Create Actions
    // Jump (A)
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy(actionInfo.actionName, "jump");
    strcpy(actionInfo.localizedActionName, "Jump");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionJump));

    // Attack (B)
    strcpy(actionInfo.actionName, "attack");
    strcpy(actionInfo.localizedActionName, "Attack");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionAttack));

    // Crouch (Z)
    strcpy(actionInfo.actionName, "crouch");
    strcpy(actionInfo.localizedActionName, "Crouch");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionCrouch));

    // Start
    strcpy(actionInfo.actionName, "start");
    strcpy(actionInfo.localizedActionName, "Start");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionStart));

    // L Button
    strcpy(actionInfo.actionName, "l_button");
    strcpy(actionInfo.localizedActionName, "L Button");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionL));

    // R Button
    strcpy(actionInfo.actionName, "r_button");
    strcpy(actionInfo.localizedActionName, "R Button");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionR));

    // Movement (Stick)
    actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    strcpy(actionInfo.actionName, "movement");
    strcpy(actionInfo.localizedActionName, "Movement");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionMovement));

    // Camera (Stick/C-buttons)
    strcpy(actionInfo.actionName, "camera");
    strcpy(actionInfo.localizedActionName, "Camera");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionCamera));

    // Toggle Keyboard (Y button)
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy(actionInfo.actionName, "toggle_keyboard");
    strcpy(actionInfo.localizedActionName, "Toggle Keyboard");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionToggleKeyboard));

    // Aim Poses
    actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy(actionInfo.actionName, "aim_left");
    strcpy(actionInfo.localizedActionName, "Aim Left");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionPoseLeft));

    strcpy(actionInfo.actionName, "aim_right");
    strcpy(actionInfo.localizedActionName, "Aim Right");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionPoseRight));

    // Select (Trigger)
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy(actionInfo.actionName, "select_left");
    strcpy(actionInfo.localizedActionName, "Select Left");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionSelectLeft));

    strcpy(actionInfo.actionName, "select_right");
    strcpy(actionInfo.localizedActionName, "Select Right");
    XR_CHECK(xrCreateAction(s_actionSet, &actionInfo, &s_actionSelectRight));

    // Suggest Bindings
    XrPath pathInteractionProfile = XR_NULL_PATH;
    
    // Oculus Touch
    xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &pathInteractionProfile);
    
    std::vector<XrActionSuggestedBinding> bindings;
    auto addBinding = [&](XrAction action, const char* pathStr) {
        XrPath path;
        xrStringToPath(instance, pathStr, &path);
        bindings.push_back({action, path});
    };

    // Oculus Bindings
    addBinding(s_actionJump, "/user/hand/right/input/a/click");
    addBinding(s_actionAttack, "/user/hand/right/input/b/click"); // Or X/Y on left?
    addBinding(s_actionCrouch, "/user/hand/left/input/trigger/value"); // Left trigger for Z
    addBinding(s_actionCrouch, "/user/hand/right/input/trigger/value"); // Right trigger for Z (alternative)
    addBinding(s_actionStart, "/user/hand/left/input/menu/click"); // Menu button
    addBinding(s_actionMovement, "/user/hand/left/input/thumbstick");
    addBinding(s_actionMovement, "/user/hand/left/input/thumbstick");
    addBinding(s_actionCamera, "/user/hand/right/input/thumbstick");
    addBinding(s_actionL, "/user/hand/left/input/squeeze/value"); // Grab/Grip
    addBinding(s_actionR, "/user/hand/right/input/squeeze/value"); // Grab/Grip
    addBinding(s_actionToggleKeyboard, "/user/hand/left/input/y/click"); // Y button for keyboard toggle

    // Keyboard Bindings
    addBinding(s_actionPoseLeft, "/user/hand/left/input/aim/pose");
    addBinding(s_actionPoseRight, "/user/hand/right/input/aim/pose");
    addBinding(s_actionSelectLeft, "/user/hand/left/input/trigger/value");
    addBinding(s_actionSelectRight, "/user/hand/right/input/trigger/value");

    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = pathInteractionProfile;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
    XR_CHECK(xrSuggestInteractionProfileBindings(instance, &suggestedBindings));

    // Attach Action Set to Session
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &s_actionSet;
    XR_CHECK(xrAttachSessionActionSets(session, &attachInfo));


    // Create Action Spaces
    XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    actionSpaceInfo.action = s_actionPoseLeft;
    actionSpaceInfo.poseInActionSpace = {{0,0,0,1}, {0,0,0}};
    XR_CHECK(xrCreateActionSpace(session, &actionSpaceInfo, &s_spacePoseLeft));

    actionSpaceInfo.action = s_actionPoseRight;
    XR_CHECK(xrCreateActionSpace(session, &actionSpaceInfo, &s_spacePoseRight));

    // Create keyboard reference space (STAGE without rotation/offset)
    XrReferenceSpaceCreateInfo keyboardSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    keyboardSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    keyboardSpaceInfo.poseInReferenceSpace = {{0,0,0,1}, {0,0,0}};
    XR_CHECK(xrCreateReferenceSpace(session, &keyboardSpaceInfo, &s_keyboardReferenceSpace));

    // Get subaction paths
    xrStringToPath(instance, "/user/hand/left", &s_pathHandLeft);
    xrStringToPath(instance, "/user/hand/right", &s_pathHandRight);

    s_initialized = true;
}

static void controller_openxr_read(OSContPad *pad) {
    if (!s_initialized) return;

    XrSession session = openxr_get_session();
    if (session == XR_NULL_HANDLE) return;

    // Sync Actions
    XrActiveActionSet activeActionSet{s_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    
    if (XR_FAILED(xrSyncActions(session, &syncInfo))) {
        return;
    }

    // Helper to get boolean state
    auto getBool = [&](XrAction action) -> bool {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &state))) {
            return state.currentState && state.isActive;
        }
        return false;
    };

    // Helper to get vector2 state
    auto getVec2 = [&](XrAction action) -> XrVector2f {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &getInfo, &state))) {
            if (state.isActive) return state.currentState;
        }
        return {0.0f, 0.0f};
    };

    // Map Inputs
    if (getBool(s_actionJump)) pad->button |= A_BUTTON;
    if (getBool(s_actionAttack)) pad->button |= B_BUTTON;
    if (getBool(s_actionCrouch)) pad->button |= Z_TRIG;
    if (getBool(s_actionStart)) pad->button |= START_BUTTON;
    if (getBool(s_actionL)) pad->button |= L_TRIG;
    if (getBool(s_actionR)) pad->button |= R_TRIG;

    // Movement Stick
    XrVector2f movement = getVec2(s_actionMovement);
    // Map -1.0..1.0 to -80..80 (approx)
    pad->stick_x = (s8)(movement.x * 80.0f);
    pad->stick_y = (s8)(movement.y * 80.0f);

    // Camera Stick -> C-Buttons
    XrVector2f camera = getVec2(s_actionCamera);
    float threshold = 0.5f;
    if (camera.x > threshold) pad->button |= R_CBUTTONS;
    if (camera.x < -threshold) pad->button |= L_CBUTTONS;
    if (camera.y > threshold) pad->button |= U_CBUTTONS;
    if (camera.y < -threshold) pad->button |= D_CBUTTONS;

    // Handle Y button for keyboard toggle
    static bool s_lastToggleState = false;
    bool currentToggleState = getBool(s_actionToggleKeyboard);
    
    // Toggle chat box on button press (rising edge detection)
    if (currentToggleState && !s_lastToggleState) {
        djui_chat_box_toggle();
        if (gDjuiChatBoxFocus) {
            openxr_show_keyboard();
        } else {
            openxr_hide_keyboard();
        }
    }
    s_lastToggleState = currentToggleState;


    // Send keyboard input
    if (openxr_is_keyboard_visible()) {
        XrTime time = openxr_get_predicted_display_time();

        auto sendInput = [&](XrSpace space, XrAction selectAction, XrVirtualKeyboardInputSourceMETA source) {
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
            // Use keyboard reference space (STAGE without rotation/offset) instead of game's reference space
            if (XR_SUCCEEDED(xrLocateSpace(space, s_keyboardReferenceSpace, time, &location))) {
                if (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {                    
                    float selectValue = 0.0f;
                    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
                    getInfo.action = selectAction;
                    XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};

                    if (XR_SUCCEEDED(xrGetActionStateFloat(session, &getInfo, &state))) {
                        if (state.isActive) selectValue = state.currentState;
                    }

                    bool pressed = selectValue > 0.5f;
                    XrPosef interactorRootPose = location.pose;

                    OpenXRKeyboard::GetInstance().SendInput(s_keyboardReferenceSpace, source, location.pose, pressed, &interactorRootPose);
                }
            }
        };

        sendInput(s_spacePoseLeft, s_actionSelectLeft, XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_LEFT_META);
        sendInput(s_spacePoseRight, s_actionSelectRight, XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_RIGHT_META);
    }
}

static u32 controller_openxr_rawkey(void) {
    return VK_INVALID;
}

static void controller_openxr_rumble_play(float str, float time) {
    // TODO: Implement haptics
}

static void controller_openxr_rumble_stop(void) {
    // TODO: Implement haptics
}

static void controller_openxr_bind(void) {
    // No-op for now
}

static void controller_openxr_shutdown(void) {
    if (!s_initialized) return;
    
    // OpenXR resources are generally cleaned up by destroying the instance/session
    // But we could destroy actions here if we wanted to be pedantic.
    s_initialized = false;
}

struct ControllerAPI controller_openxr = {
    0, // vkbase (not used for this backend really)
    controller_openxr_init,
    controller_openxr_read,
    controller_openxr_rawkey,
    controller_openxr_rumble_play,
    controller_openxr_rumble_stop,
    controller_openxr_bind,
    controller_openxr_shutdown
};

XrSpace controller_openxr_get_keyboard_space(void) {
    return s_keyboardReferenceSpace;
}

#endif // OPENXR_ENABLED
