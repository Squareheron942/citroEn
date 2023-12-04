#include "camera.h"
#include "gameobject.h"
#include "entt.hpp"
#include "transform.h"
#include "config.h"
#include "mesh.h"
#include "componentmanager.h"
#include "renderer.h"
#include "meshrenderer.h"

Camera* Camera::mainTop = NULL;
Camera* Camera::mainBottom = NULL;

namespace {
    /** 
     * @brief The default used is just iod/5 (probably to reduce just how much it splits apart which causes headaches). 
     * Having a map function allows the user to basically do whatever they want though.
     * You can have it completely ignore the input and use your own variable, make it exponential, log, alternate between 0 and 100 etc
     * @param iod The inputted slider value
    */
    float defaultIODMap(float iod) { return iod * 0.2f; }

    struct cam_args {
        // general camera properties
        bool wide = true; // whether or not to use the 240x800 mode on supported models
        RenderType type = RENDER_TYPE_TOPSCREEN; // what to render to
        float nearClip = 0.1f, farClip = 1000.f;
        bool ortho = false;
        unsigned int bgcolor = 0x3477ebFF; // defaults to dark blue
        unsigned short cull = 0xFFFF; // shows everything


        // ortho camera properties
        float height = 24.f;
        float width = 40.f;

        // perspective camera properties
        bool stereo = true; // whether to use 3D
        float fovY = 55.f; // default for splatoon human form
        /**
         * @brief Dictates how to map the iod input from the slider to the rendering iod
         * @param iod The inputted slider value
         */
        float(*iodMapFunc)(float) = NULL;

        // texture render properties
        unsigned short resolution; // min 8x8, max 1024x1024 (must be square)
    };
}

Camera::Camera(GameObject& parent, void* args) {
    cam_args c;
    if (args) 
        c = *(cam_args*)args;

    // set standard args
    nearClip = c.nearClip;
    farClip = c.farClip;
    orthographic = c.ortho;
    type = c.type;
    cullingMask = c.cull;
    bgcolor = c.bgcolor;

    if (c.ortho) {
        fovY = 0.f;
        height = c.height;
        width = c.width;
    } else { // perspective
        fovY = c.fovY;
    }

    switch (c.type) {
        case RENDER_TYPE_TOPSCREEN:
            if (c.iodMapFunc) iodMapFunc = c.iodMapFunc;
            else iodMapFunc = defaultIODMap;
            highRes = c.wide && !config::wideIsUnsupported; // disable if not supported
            if (config::wideIsUnsupported) Console::warn("wide mode not supported");
            stereo = c.stereo && !c.ortho;
            target[0] = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8); // only type to be used for display
            if (!target[0]) Console::warn("Could not create render target");
            if (stereo) target[1] = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8); // only type to be used for display
            if (highRes) target[2] = C3D_RenderTargetCreate(240, 800, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8); // only type to be used for display
            aspectRatio = C3D_AspectRatioTop;
            break;
        case RENDER_TYPE_BOTTOMSCREEN:
            aspectRatio = C3D_AspectRatioBot;
            stereo = false;
            highRes = false;
            target[0] = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
            break;
        case RENDER_TYPE_TEXTURE:
            aspectRatio = 1.f;
            stereo = false;
            highRes = false;
            //TODO I still need to figure out render textures
            break;
        default:
            Console::warn("Invalid render type");
    }
    
    this->parent = &parent;
}

void Camera::Render() {
    
    C3D_Mtx projection, view;
    Mtx_Identity(&projection);
    Mtx_Identity(&view);
    float iod = 0;
    if (stereo && !orthographic) iod = iodMapFunc(osGet3DSliderState()); // only calculate if actually necessary
    bool useWide = highRes && !(iod > 0.f);

    switch (type) {
        case RENDER_TYPE_TOPSCREEN:
            if (useWide) {
                C3D_RenderTargetSetOutput(target[2], GFX_TOP, GFX_LEFT, CAM_DISPLAY_TRANSFER_FLAGS);
                C3D_RenderTargetClear(target[2], C3D_CLEAR_ALL, bgcolor, 0);
                C3D_FrameDrawOn(target[2]);
            } else {
                C3D_RenderTargetSetOutput(target[0], GFX_TOP, GFX_LEFT, CAM_DISPLAY_TRANSFER_FLAGS);
                C3D_RenderTargetClear(target[0], C3D_CLEAR_ALL, bgcolor, 0);
                C3D_FrameDrawOn(target[0]);
            }
            break;
        case RENDER_TYPE_BOTTOMSCREEN:
            C3D_RenderTargetSetOutput(target[0], GFX_BOTTOM, GFX_LEFT, CAM_DISPLAY_TRANSFER_FLAGS);
            C3D_RenderTargetClear(target[0], C3D_CLEAR_ALL, bgcolor, 0);
            C3D_FrameDrawOn(target[0]);
            break;
        case RENDER_TYPE_TEXTURE:
            break; 
    }
    
    if (orthographic) Mtx_OrthoTilt(&projection, -width / 2, width / 2, -height / 2, height / 2, nearClip, farClip, false); // no perspective
    else if (stereo) // both perspective and 3D
        Mtx_PerspStereoTilt(
            &projection, 
            C3D_AngleFromDegrees(fovY), 
            aspectRatio, 
            nearClip, farClip, 
            -iod, focalLength, 
            false
        );
    else // perspective but no 3D
        Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(fovY), aspectRatio, nearClip, farClip, false);

    Mtx_PerspStereoTilt(
        &projection, 
        C3D_AngleFromDegrees(fovY), 
        aspectRatio, 
        nearClip, farClip,
        -iod, focalLength,
        false
    );

    
    
    gfxSetWide(useWide); // Enable wide mode if wanted and if not rendering in stereo

    // actually render stuff for left eye

    transform* trans = parent->getComponent<transform>();
    
    if (trans) {
        C3D_Mtx view = *trans;
        for (GameObject* obj : objects) {
            if (!obj) continue;
            if (!(obj->layer & cullingMask)) continue; // skip culled objects

            Renderer* renderer = NULL;

            if (!(renderer = (Renderer*)obj->getComponent<MeshRenderer>())) continue;
            renderer->render(view, projection);
        }
    }


    if (stereo && iod > 0.0f) {
        gfxSet3D(true);
        C3D_RenderTargetSetOutput(target[1], GFX_TOP, GFX_RIGHT, CAM_DISPLAY_TRANSFER_FLAGS); 
        Mtx_PerspStereoTilt(
            &projection, 
            C3D_AngleFromDegrees(fovY), 
            C3D_AspectRatioTop, 
            nearClip, farClip, 
            iod, focalLength, 
            false
        );
        C3D_RenderTargetClear(target[1], C3D_CLEAR_ALL, bgcolor, 0);
        C3D_FrameDrawOn(target[1]);

        // actually render stuff here for 2nd eye
        if (trans) {
            C3D_Mtx view = *trans;
            for (GameObject* obj : objects) {
                if (!obj) continue;
                if (!(obj->layer & cullingMask)) continue; // skip culled objects

                Renderer* renderer = NULL;

                if (!(renderer = (Renderer*)obj->getComponent<MeshRenderer>())) continue; 

                renderer->render(view, projection);       
            }
        }
    }
}

COMPONENT_REGISTER(Camera)