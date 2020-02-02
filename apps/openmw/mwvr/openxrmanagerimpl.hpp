#ifndef OPENXR_MANAGER_IMPL_HPP
#define OPENXR_MANAGER_IMPL_HPP

#include "openxrmanager.hpp"
#include "openxrlayer.hpp"
#include "../mwinput/inputmanagerimp.hpp"
#include "../mwrender/vismask.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

// The OpenXR SDK assumes we've included Windows.h
#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <vector>
#include <array>
#include <map>
#include <iostream>
#include <thread>
#include <chrono>

namespace osg {
    Vec3 fromXR(XrVector3f);
    Quat fromXR(XrQuaternionf quat);
    XrVector3f      toXR(Vec3 v);
    XrQuaternionf   toXR(Quat quat);
}

namespace MWVR
{

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr, FILE_AND_LINE);

    XrResult CheckXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr);
    MWVR::Pose fromXR(XrPosef pose);
    XrPosef toXR(MWVR::Pose pose);

    struct OpenXRManagerImpl
    {
        OpenXRManagerImpl(void);
        ~OpenXRManagerImpl(void);

        void LogLayersAndExtensions();
        void LogInstanceInfo();
        void LogReferenceSpaces();

        const XrEventDataBaseHeader* nextEvent();
        void waitFrame();
        void beginFrame();
        void endFrame(int64_t displayTime, class OpenXRLayerStack* layerStack);
        std::array<XrView, 2> getPredictedViews(int64_t predictedDisplayTime, TrackedSpace mSpace);
        MWVR::Pose getPredictedLimbPose(int64_t predictedDisplayTime, TrackedLimb limb, TrackedSpace space);
        int eyes();
        void handleEvents();
        void updateControls();
        void HandleSessionStateChanged(const XrEventDataSessionStateChanged& stateChangedEvent);
        XrFrameState frameState();

        bool initialized = false;
        long long mFrameIndex = 0;
        XrInstance mInstance = XR_NULL_HANDLE;
        XrSession mSession = XR_NULL_HANDLE;
        XrSpace mSpace = XR_NULL_HANDLE;
        XrFormFactor mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrViewConfigurationType mViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrEnvironmentBlendMode mEnvironmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
        XrGraphicsBindingOpenGLWin32KHR mGraphicsBinding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
        XrSystemProperties mSystemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
        std::array<XrViewConfigurationView, 2> mConfigViews{ { {XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW} } };
        XrSpace mReferenceSpaceView = XR_NULL_HANDLE;
        XrSpace mReferenceSpaceStage = XR_NULL_HANDLE;
        XrEventDataBuffer mEventDataBuffer{ XR_TYPE_EVENT_DATA_BUFFER };
        XrFrameState mFrameState{};
        XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
        bool mSessionRunning = false;
        std::mutex mFrameStateMutex{};
        std::mutex mEventMutex{};
    };
}

#endif 
