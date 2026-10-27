/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#ifndef NX_NXSVIEW_H
#define NX_NXSVIEW_H

#include <QElapsedTimer>
#include <QWidget>
#include <vector>

#include "../common/renderer.h"
#include "scene.h"
#include "trackball.h"

class QCloseEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

// Renderer-independent view state and interaction. Concrete widgets own their
// native rendering surface and implement requestUpdate().
class Nxsview {
public:
    Nxsview();
    virtual ~Nxsview() = default;

    nx::Renderer renderer;
    Scene scene;
    nx::Rgba8 colorBottom;
    nx::Rgba8 colorTop;
    nx::Rgba8 color;

    void checkForUpdate();
    void lightTrackball(bool on) { default_trackball = !on; }
    void setTriangles(bool on);
    void setNormals(bool on);
    void setColors(bool on);
    void setPatches(bool on);
    void setExtraction(bool on);
    void setFov(int f);
    void setInfo(bool on);
    void setPrimitives(double e);
    void setError(double e);
    void setGpu(double e);
    void setRam(double d);
    void setColor(QWidget *parent);
    void setColorTop(QWidget *parent);
    void setColorBottom(QWidget *parent);
    void zoomIn();
    void zoomOut();
    void stopController() {}

    // Called by the concrete backend once per rendered frame.
    void advanceViewpointAnimation();

protected:
    virtual void requestUpdate() = 0;

    Trackball trackball;
    Trackball::Pose saved_pose;
    Trackball trackball_light;
    bool default_trackball;
    int button;
    bool extracting;

    std::vector<Trackball::Pose> viewpoints;
    quint32 current_viewpoint;
    float current_time;
    QElapsedTimer time;
    bool playing;

    bool hasToPick;
    int pickX;
    int pickY;
    float fov;
    bool info;

    void handleWheelEvent(QWheelEvent *event, int height);
    void handleMouseMoveEvent(QMouseEvent *event, int height);
    void handleMousePressEvent(QMouseEvent *event, int height);
    void handleMouseReleaseEvent(QMouseEvent *event, int height);
    void handleMouseDoubleClickEvent(QMouseEvent *event, int height);
    void handleKeyReleaseEvent(QKeyEvent *event);
    void handleKeyPressEvent(QKeyEvent *event);
    void handleCloseEvent(QCloseEvent *event);
};

#endif // NX_NXSVIEW_H
