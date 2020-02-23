#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrinputmanager.hpp"
#include "openxrswapchain.hpp"
#include "../mwinput/inputmanagerimp.hpp"
#include "../mwbase/environment.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <osg/Camera>

#include <vector>
#include <array>
#include <iostream>
#include "openxrsession.hpp"
#include "openxrmenu.hpp"
#include <time.h>

namespace MWVR
{
    OpenXRSession::OpenXRSession(
        osg::ref_ptr<OpenXRManager> XR, 
        float unitsPerMeter)
        : mXR(XR)
        , mUnitsPerMeter(unitsPerMeter)
    {
    }

    OpenXRSession::~OpenXRSession()
    {
    }

    void OpenXRSession::setLayer(
        OpenXRLayerStack::Layer layerType, 
        OpenXRLayer* layer)
    {
        mLayerStack.setLayer(layerType, layer);
    }

    void OpenXRSession::swapBuffers(osg::GraphicsContext* gc)
    {
        Timer timer("OpenXRSession::SwapBuffers");
        static int wat = 0;
        if (!mXR->sessionRunning())
            return;
        if (!mPredictionsReady)
            return;

        for (auto layer : mLayerStack.layerObjects())
            layer->swapBuffers(gc);

        timer.checkpoint("Rendered");

        mXR->endFrame(mXR->impl().frameState().predictedDisplayTime, &mLayerStack);
    }

    void OpenXRSession::waitFrame()
    {
        mXR->handleEvents();
        if (!mXR->sessionRunning())
            return;

        Timer timer("OpenXRSession::waitFrame");
        mXR->waitFrame();
        timer.checkpoint("waitFrame");
        predictNext(0);

        mPredictionsReady = true;

    }

    void OpenXRSession::showMenu(bool show)
    {
        auto* menuLayer = dynamic_cast<OpenXRMenu*>(mLayerStack.layerObjects()[OpenXRLayerStack::MENU_VIEW_LAYER]);
        if (menuLayer)
        {
            bool change = show != menuLayer->isVisible();
            menuLayer->setVisible(show);

            // Automatically update position of menu whenever the menu opens.
            // This ensures menus are always opened near the player.
            if(change)
                menuLayer->updatePosition();
        }
    }

    void OpenXRSession::updateMenuPosition(void)
    {
        auto* menuLayer = dynamic_cast<OpenXRMenu*>(mLayerStack.layerObjects()[OpenXRLayerStack::MENU_VIEW_LAYER]);
        if (menuLayer)
        {
            menuLayer->updatePosition();
        }
    }


    // OSG doesn't provide API to extract euler angles from a quat, but i need it.
    // Credits goes to Dennis Bunfield, i just copied his formula https://narkive.com/v0re6547.4
    void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll)
    {
        // Now do the computation
        osg::Matrixd m2(osg::Matrixd::rotate(quat));
        double* mat = (double*)m2.ptr();
        double angle_x = 0.0;
        double angle_y = 0.0;
        double angle_z = 0.0;
        double D, C, tr_x, tr_y;
        angle_y = D = asin(mat[2]); /* Calculate Y-axis angle */
        C = cos(angle_y);
        if (fabs(C) > 0.005) /* Test for Gimball lock? */
        {
            tr_x = mat[10] / C; /* No, so get X-axis angle */
            tr_y = -mat[6] / C;
            angle_x = atan2(tr_y, tr_x);
            tr_x = mat[0] / C; /* Get Z-axis angle */
            tr_y = -mat[1] / C;
            angle_z = atan2(tr_y, tr_x);
        }
        else /* Gimball lock has occurred */
        {
            angle_x = 0; /* Set X-axis angle to zero
            */
            tr_x = mat[5]; /* And calculate Z-axis angle
            */
            tr_y = mat[4];
            angle_z = atan2(tr_y, tr_x);
        }

        yaw = angle_z;
        pitch = angle_x;
        roll = angle_y;
    }

    float OpenXRSession::movementYaw(void)
    {
        auto lhandquat = predictedPoses().hands[(int)TrackedSpace::VIEW][(int)MWVR::Side::LEFT_HAND].orientation;
        float yaw = 0.f;
        float pitch = 0.f;
        float roll = 0.f;
        getEulerAngles(lhandquat, yaw, pitch, roll);
        return yaw;
    }

    void OpenXRSession::predictNext(int extraPeriods)
    {
        auto mPredictedDisplayTime = mXR->impl().frameState().predictedDisplayTime;

        auto input = MWBase::Environment::get().getXRInputManager();

        auto previousHeadPose = mPredictedPoses.head[(int)TrackedSpace::STAGE];

        // Update pose predictions
        mPredictedPoses.head[(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::STAGE);
        mPredictedPoses.head[(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::VIEW);
        mPredictedPoses.hands[(int)TrackedSpace::STAGE] = input->getHandPoses(mPredictedDisplayTime, TrackedSpace::STAGE);
        mPredictedPoses.hands[(int)TrackedSpace::VIEW] = input->getHandPoses(mPredictedDisplayTime, TrackedSpace::VIEW);
        auto stageViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::STAGE);
        auto hmdViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::VIEW);
        mPredictedPoses.eye[(int)TrackedSpace::STAGE][(int)Side::LEFT_HAND] = fromXR(stageViews[(int)Side::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)TrackedSpace::VIEW][(int)Side::LEFT_HAND] = fromXR(hmdViews[(int)Side::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)TrackedSpace::STAGE][(int)Side::RIGHT_HAND] = fromXR(stageViews[(int)Side::RIGHT_HAND].pose);
        mPredictedPoses.eye[(int)TrackedSpace::VIEW][(int)Side::RIGHT_HAND] = fromXR(hmdViews[(int)Side::RIGHT_HAND].pose);
    }
}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

