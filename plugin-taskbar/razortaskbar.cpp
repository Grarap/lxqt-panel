/* BEGIN_COMMON_COPYRIGHT_HEADER
 *
 * Razor - a lightweight, Qt based, desktop toolset
 * https://sourceforge.net/projects/razor-qt/
 *
 * Copyright: 2011 Razor team
 * Authors:
 *   Alexander Sokoloff <sokoloff.a@gmail.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation;  only version 2 of
 * the License is valid for this program.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * END_COMMON_COPYRIGHT_HEADER */


#include <QApplication>

#include <QtCore/QDebug>
#include <QToolButton>

#include "razortaskbar.h"
#include <razorqt/xdgicon.h>
#include <razorqt/xfitman.h>
#include <QtCore/QList>


#include <QDesktopWidget>

#include <X11/Xlib.h>
#include "razortaskbutton.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <QX11Info>

EXPORT_RAZOR_PANEL_PLUGIN_CPP(RazorTaskBar)

/************************************************

************************************************/
RazorTaskBar::RazorTaskBar(const RazorPanelPluginStartInfo* startInfo, QWidget* parent) :
    RazorPanelPlugin(startInfo, parent)
{
    setObjectName("TaskBar");

    QSizePolicy sp(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    sp.setHorizontalStretch(1);
    sp.setVerticalStretch(1);
    setSizePolicy(sp);
    layout()->addStretch();

    mRootWindow = QX11Info::appRootWindow();
    refreshTaskList();
}


/************************************************

 ************************************************/
RazorTaskBar::~RazorTaskBar()
{
}


/************************************************

 ************************************************/
RazorTaskButton* RazorTaskBar::buttonByWindow(Window window) const
{
    if (mButtonsHash.contains(window))
        return mButtonsHash.value(window);
    return 0;
}


/************************************************

 ************************************************/
void RazorTaskBar::refreshTaskList()
{
    XfitMan xf = xfitMan();
    QList<Window> tmp = xf.getClientList();

    //qDebug() << "** Fill ********************************";
    //foreach (Window wnd, tmp)
    //    if (xf->acceptWindow(wnd)) qDebug() << XfitMan::debugWindow(wnd);
    //qDebug() << "****************************************";


    QMutableHashIterator<Window, RazorTaskButton*> i(mButtonsHash);
    while (i.hasNext())
    {
        i.next();
        int n = tmp.removeAll(i.key());

        if (!n)
        {
            delete i.value();
            i.remove();
        }
    }

    foreach (Window wnd, tmp)
    {
        if (xf.acceptWindow(wnd))
        {
            RazorTaskButton* btn = new RazorTaskButton(wnd, this);
            mButtonsHash.insert(wnd, btn);
            // -1 is here due the last stretchable item
            layout()->insertWidget(layout()->count()-1, btn);
            // now I want to set higher stretchable priority for buttons
            // to suppress stretchItem (last item) default value which
            // will remove that anoying aggresive space at the end -- petr
            layout()->setStretch(layout()->count()-2, 1);
        }
    }

    activeWindowChanged();

}


/************************************************

 ************************************************/
void RazorTaskBar::activeWindowChanged()
{
    Window window = xfitMan().getActiveAppWindow();

    RazorTaskButton* btn = buttonByWindow(window);

    if (btn)
        btn->setChecked(true);
    else
        RazorTaskButton::unCheckAll();
}


/************************************************

 ************************************************/
void RazorTaskBar::x11EventFilter(XEvent* event)
{

    switch (event->type)
    {
        case PropertyNotify:
            handlePropertyNotify(&event->xproperty);
            break;

//        default:
//        {
//            qDebug() << "** XEvent ************************";
//            qDebug() << "Type:   " << xEventTypeToStr(event);
//        }


    }
}


/************************************************

 ************************************************/
void RazorTaskBar::handlePropertyNotify(XPropertyEvent* event)
{

    if (event->window == mRootWindow)
    {
        // Windows list changed ...............................
        if (event->atom == XfitMan::atom("_NET_CLIENT_LIST"))
        {
            refreshTaskList();
            return;
        }

        // Activate window ....................................
        if (event->atom == XfitMan::atom("_NET_ACTIVE_WINDOW"))
        {
            activeWindowChanged();
            return;
        }
    }
    else
    {
        RazorTaskButton* btn = buttonByWindow(event->window);
        if (btn)
            btn->handlePropertyNotify(event);
    }
//    char* aname = XGetAtomName(QX11Info::display(), event->atom);
//    qDebug() << "** XPropertyEvent ********************";
//    qDebug() << "  atom:       0x" << hex << event->atom
//            << " (" << (aname ? aname : "Unknown") << ')';
//    qDebug() << "  window:    " << XfitMan::debugWindow(event->window);
//    qDebug() << "  display:   " << event->display;
//    qDebug() << "  send_event:" << event->send_event;
//    qDebug() << "  serial:    " << event->serial;
//    qDebug() << "  state:     " << event->state;
//    qDebug() << "  time:      " << event->time;
//    qDebug();

}

