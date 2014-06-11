/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXDE-Qt - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright: 2010-2011 Razor team
 * Authors:
 *   Alexander Sokoloff <sokoloff.a@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */


#ifndef LXQTPANELAPPLICATION_H
#define LXQTPANELAPPLICATION_H

#include <LXQt/Application>
#include "lxqtpanel.h"
#include <LXQt/Settings>
#include <QtDebug>
#include <QUuid>
//#include <LXQt/XfitMan>
#include "lxqtxfitman.h"
#include <QX11Info>
#include <X11/Xlib.h>

class LxQtPanel;
namespace LxQt {
class Settings;
}

class LxQtPanelApplication : public LxQt::Application
{
    Q_OBJECT
public:
    explicit LxQtPanelApplication(int& argc, char** argv, const QString &configFile);
    ~LxQtPanelApplication();
    virtual bool x11EventFilter(XEvent* event);

    int count() { return mPanels.count(); }
    LxQt::Settings *settings() { return mSettings; }

    Display *xdisp;
    Window isMain, r,p, *kids, isRealMain;
    Window xparent;
    Window xrootWindow;
    Window *xchildren;
    //quint32 xNumChildren;
    XMotionEvent xmot;
    XMapEvent xmap;
    XUnmapEvent xunmap;
    char *window_name, *win_name;
    unsigned int numkids;
    unsigned long event_mask;
  //  XfitMan xf;

public slots:
    void addNewPanel();

private:
    QList<LxQtPanel*> mPanels;

    void addPanel(const QString &name);

private slots:
    void removePanel(LxQtPanel* panel);

private:
    LxQt::Settings *mSettings;

    bool isFirst;

};


#endif // LXQTPANELAPPLICATION_H
