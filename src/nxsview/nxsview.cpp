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
#include <iostream>
#include <algorithm>
#include <cmath>

#include <QColorDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "nxsview.h"

using namespace std;
using namespace nx;

Nxsview::Nxsview():
    default_trackball(true), button(Trackball::BUTTON_NONE), extracting(true),
    current_viewpoint(static_cast<quint32>(-1)), current_time(-1), playing(false),
    hasToPick(false), pickX(0), pickY(0), fov(60), info(false) {
    colorBottom = Rgba8{64, 64, 128, 1};
    colorTop = Rgba8{0, 0, 0, 1};
    color = Rgba8{200, 200, 200, 1};
}

void Nxsview::checkForUpdate() {
    if(scene.controller.hasCacheChanged())
        requestUpdate();
}

void Nxsview::setTriangles(bool on) { renderer.setMode(Renderer::TRIANGLES, on); requestUpdate(); }
void Nxsview::setNormals(bool on) { renderer.setMode(Renderer::NORMALS, on); requestUpdate(); }
void Nxsview::setColors(bool on) { renderer.setMode(Renderer::COLORS, on); requestUpdate(); }
void Nxsview::setPatches(bool on) { renderer.setMode(Renderer::PATCHES, on); requestUpdate(); }
void Nxsview::setExtraction(bool on) { extracting = on; requestUpdate(); }
void Nxsview::setFov(int value) { fov = value; requestUpdate(); }
void Nxsview::setInfo(bool on) { info = on; requestUpdate(); }
void Nxsview::setPrimitives(double value) { renderer.setMaxPrimitives(static_cast<uint32_t>(value * (1 << 20))); }
void Nxsview::setError(double value) { renderer.setError(value); requestUpdate(); }
void Nxsview::setGpu(double value) { scene.controller.setGpu(value * (1 << 20)); requestUpdate(); }
void Nxsview::setRam(double value) { scene.controller.setRam(value * (1 << 20)); requestUpdate(); }

void Nxsview::setColor(QWidget *parent) {
    QColor selected = QColorDialog::getColor(QColor(color.r, color.g, color.b), parent);
    if(!selected.isValid()) return;
    color = Rgba8{static_cast<uint8_t>(selected.red()), static_cast<uint8_t>(selected.green()), static_cast<uint8_t>(selected.blue()), 0};
    requestUpdate();
}

void Nxsview::setColorTop(QWidget *parent) {
    QColor selected = QColorDialog::getColor(QColor(colorTop.r, colorTop.g, colorTop.b), parent);
    if(!selected.isValid()) return;
    colorTop = Rgba8{static_cast<uint8_t>(selected.red()), static_cast<uint8_t>(selected.green()), static_cast<uint8_t>(selected.blue()), 0};
    requestUpdate();
}

void Nxsview::setColorBottom(QWidget *parent) {
    QColor selected = QColorDialog::getColor(QColor(colorBottom.r, colorBottom.g, colorBottom.b), parent);
    if(!selected.isValid()) return;
    colorBottom = Rgba8{static_cast<uint8_t>(selected.red()), static_cast<uint8_t>(selected.green()), static_cast<uint8_t>(selected.blue()), 0};
    requestUpdate();
}

void Nxsview::zoomIn() { trackball.wheel(1); requestUpdate(); }
void Nxsview::zoomOut() { trackball.wheel(-1); requestUpdate(); }

void Nxsview::advanceViewpointAnimation() {
    if(!playing || current_time < 0) return;

    current_time += time.restart() / 1000.0f;
    int viewpoint = static_cast<int>(floor(current_time));
    if(viewpoint >= static_cast<int>(viewpoints.size()) - 1) {
        playing = false;
        return;
    }

    Trackball::Pose pose;
    pose.scale = 0;
    float totalWeight = 0;
    for(int i = viewpoint - 6; i < viewpoint + 6; ++i) {
        if(i < 0 || i >= static_cast<int>(viewpoints.size())) continue;
        float delta = current_time - i;
        float weight = exp(-delta * delta);
        totalWeight += weight;
        pose += viewpoints[i] * weight;
    }
    if(totalWeight > 0)
        trackball.pose = pose * (1 / totalWeight);
}

static int qtButtons(Qt::MouseButton qtButton, Qt::KeyboardModifiers modifiers) {
    int result = Trackball::BUTTON_NONE;
    if(qtButton & Qt::LeftButton) result |= Trackball::BUTTON_LEFT;
    if(qtButton & Qt::RightButton) result |= Trackball::BUTTON_RIGHT;
    if(qtButton & Qt::MiddleButton) result |= Trackball::BUTTON_MIDDLE;
    if(modifiers & Qt::ControlModifier) result |= Trackball::KEY_CTRL;
    return result;
}

void Nxsview::handleWheelEvent(QWheelEvent *event, int) {
    if(event->angleDelta().y() > 0) trackball.wheel(1);
    else trackball.wheel(-1);
    requestUpdate();
}

void Nxsview::handleMouseMoveEvent(QMouseEvent *event, int height) {
    if(default_trackball) trackball.mouseMove(event->x(), height - event->y());
    else trackball_light.mouseMove(event->x(), height - event->y());
    requestUpdate();
}

void Nxsview::handleMousePressEvent(QMouseEvent *event, int height) {
    button = qtButtons(event->button(), event->modifiers());
    if(default_trackball) trackball.mouseDown(event->x(), height - event->y(), button);
    else trackball_light.mouseDown(event->x(), height - event->y(), button);
    requestUpdate();
}

void Nxsview::handleMouseReleaseEvent(QMouseEvent *event, int height) {
    if(default_trackball) trackball.mouseUp(event->x(), height - event->y(), button);
    else trackball_light.mouseUp(event->x(), height - event->y(), button);
}

void Nxsview::handleMouseDoubleClickEvent(QMouseEvent *event, int height) {
    hasToPick = true;
    pickX = event->x();
    pickY = height - event->y();
    requestUpdate();
}

void Nxsview::handleKeyReleaseEvent(QKeyEvent *event) {
    event->ignore();
    if(event->modifiers() == Qt::ControlModifier || event->modifiers() == Qt::ShiftModifier || event->modifiers() == Qt::AltModifier)
        return;
    if(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) return;

    switch(event->key()) {
    case Qt::Key_H:
        cout << "Keyboard shortcuts:\n"
             << "h : print this help sheet\n"
             << "r : reset trackball\n"
             << "p : print trackball\n"
             << "s : save trackball\n"
             << "l : load trackball\n\n"
             << "[ctrl+s] : save camera path\n"
             << "[ctrl+l] : load camera path\n\n"
             << "[enter] : insert viewpoint (after current viewpoint)\n"
             << "[canc] : delete current viewpoint\n"
             << "[home] : go to first viewpoint\n"
             << "[PgDn] : go to next viewpoint\n"
             << "[PgUp] : go to previous viewpoint\n"
             << "[end] : go to last viewpoint\n\n"
             << "[space] : play\n"
             << "[space] : pause\n"
             << "[ctrl+r] : record (starts from beginning)\n";
        break;
    case Qt::Key_S:
        saved_pose = trackball.pose;
        break;
    case Qt::Key_L:
        trackball.pose = saved_pose;
        break;
    case Qt::Key_R:
        trackball.reset();
        break;
    case Qt::Key_P:
        if(!scene.nodes.empty()) {
            Matrix4f matrix = trackball.matrix() * scene.nodes[0].transform;
            for(int i = 0; i < 16; ++i) {
                cout << matrix.m[i];
                if(i != 15) cout << ", ";
            }
            cout << endl;
        }
        break;
    case Qt::Key_Return: {
        size_t insertAt = current_viewpoint == static_cast<quint32>(-1) ? 0 : current_viewpoint + 1;
        insertAt = std::min(insertAt, viewpoints.size());
        viewpoints.insert(viewpoints.begin() + insertAt, trackball.pose);
        current_viewpoint = static_cast<quint32>(insertAt);
        cout << "Viewpoint created: " << current_viewpoint << endl;
        break;
    }
    case Qt::Key_Home:
        if(!viewpoints.empty()) {
            current_time = current_viewpoint = 0;
            trackball.pose = viewpoints.front();
            cout << "Viewpoint: " << current_viewpoint << endl;
        }
        break;
    case Qt::Key_End:
        if(!viewpoints.empty()) {
            current_viewpoint = static_cast<quint32>(viewpoints.size() - 1);
            current_time = current_viewpoint;
            trackball.pose = viewpoints.back();
            cout << "Viewpoint: " << current_viewpoint << endl;
        }
        break;
    case Qt::Key_PageDown:
        if(!viewpoints.empty() && current_viewpoint < viewpoints.size() - 1) {
            ++current_viewpoint;
            current_time = current_viewpoint;
            trackball.pose = viewpoints[current_viewpoint];
            cout << "Viewpoint: " << current_viewpoint << endl;
        }
        break;
    case Qt::Key_PageUp:
        if(current_viewpoint > 0 && current_viewpoint < viewpoints.size()) {
            --current_viewpoint;
            current_time = current_viewpoint;
            trackball.pose = viewpoints[current_viewpoint];
            cout << "Viewpoint: " << current_viewpoint << endl;
        }
        break;
    case Qt::Key_Delete:
        if(!viewpoints.empty() && current_viewpoint < viewpoints.size()) {
            viewpoints.erase(viewpoints.begin() + current_viewpoint);
            if(viewpoints.empty()) {
                current_viewpoint = static_cast<quint32>(-1);
                current_time = -1;
            } else {
                if(current_viewpoint >= viewpoints.size()) current_viewpoint = static_cast<quint32>(viewpoints.size() - 1);
                current_time = current_viewpoint;
                trackball.pose = viewpoints[current_viewpoint];
            }
        }
        cout << "Viewpoint: " << current_viewpoint << endl;
        break;
    case Qt::Key_Space:
        playing = !playing;
        if(playing) time.start();
        break;
    default:
        break;
    }
    requestUpdate();
}

void Nxsview::handleKeyPressEvent(QKeyEvent *event) {
    event->ignore();
}

void Nxsview::handleCloseEvent(QCloseEvent *) {
    // The concrete backend owns controller/context shutdown policy.
}
