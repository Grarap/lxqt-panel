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


#include "lxqtpanel.h"
#include "lxqtpanellimits.h"
#include "ilxqtpanelplugin.h"
#include "lxqtpanelapplication.h"
#include "lxqtpanellayout.h"
#include "config/configpaneldialog.h"
#include "popupmenu.h"
#include "plugin.h"
#include <LXQt/AddPluginDialog>
#include <LXQt/Settings>
#include <LXQt/PluginInfo>

#include <LXQt/XfitMan>
#include <X11/Xatom.h>
#include <QX11Info>

#include <QDebug>
#include <QDesktopWidget>
#include <QMenu>
#include <XdgIcon>

#include <XdgDirs>

// Config keys and groups
#define CFG_KEY_SCREENNUM   "desktop"
#define CFG_KEY_POSITION    "position"
#define CFG_KEY_PANELSIZE   "panelSize"
#define CFG_KEY_ICONSIZE    "iconSize"
#define CFG_KEY_LINECNT     "lineCount"
#define CFG_KEY_LENGTH      "width"
#define CFG_KEY_PERCENT     "width-percent"
#define CFG_KEY_ALIGNMENT   "alignment"
#define CFG_KEY_PLUGINS "plugins"

#define CFG_KEY_AUTOHIDE "autohideTb"
#define AUTOHIDETB_DEFAULT_HEIGHT 1


using namespace LxQt;

/************************************************
 Returns the Position by the string.
 String is one of "Top", "Left", "Bottom", "Right", string is not case sensitive.
 If the string is not correct, returns defaultValue.
 ************************************************/
ILxQtPanel::Position LxQtPanel::strToPosition(const QString& str, ILxQtPanel::Position defaultValue)
{
    if (str.toUpper() == "TOP")    return LxQtPanel::PositionTop;
    if (str.toUpper() == "LEFT")   return LxQtPanel::PositionLeft;
    if (str.toUpper() == "RIGHT")  return LxQtPanel::PositionRight;
    if (str.toUpper() == "BOTTOM") return LxQtPanel::PositionBottom;
    return defaultValue;
}


/************************************************
 Return  string representation of the position
 ************************************************/
QString LxQtPanel::positionToStr(ILxQtPanel::Position position)
{
    switch (position)
    {
        case LxQtPanel::PositionTop:    return QString("Top");
        case LxQtPanel::PositionLeft:   return QString("Left");
        case LxQtPanel::PositionRight:  return QString("Right");
        case LxQtPanel::PositionBottom: return QString("Bottom");
    }

    return QString();
}


/************************************************

 ************************************************/
LxQtPanel::LxQtPanel(const QString &configGroup, QWidget *parent) :
    QFrame(parent),
    mConfigGroup(configGroup),
    mIconSize(0),
    mLineCount(0),
    crazypad(0)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_X11NetWmWindowTypeDock);
    setAttribute(Qt::WA_AlwaysShowToolTips);
    setAttribute(Qt::WA_TranslucentBackground);

    setWindowTitle("LxQt Panel");
    setObjectName(QString("LxQtPanel %1").arg(configGroup));

    LxQtPanelWidget = new QFrame(this);
    LxQtPanelWidget->setObjectName("BackgroundWidget");
    QGridLayout* lav = new QGridLayout();
    lav->setContentsMargins(QMargins(0,0,0,0));
    setLayout(lav);
    this->layout()->addWidget(LxQtPanelWidget);

    mLayout = new LxQtPanelLayout(LxQtPanelWidget);
    connect(mLayout, SIGNAL(pluginMoved()), this, SLOT(pluginMoved()));
    LxQtPanelWidget->setLayout(mLayout);
    mLayout->setLineCount(mLineCount);

    mDelaySave.setSingleShot(true);
    mDelaySave.setInterval(SETTINGS_SAVE_DELAY);
    connect(&mDelaySave, SIGNAL(timeout()), this, SLOT(saveSettings()));

    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(screensChangeds()));
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(realign()));
    connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), this, SLOT(screensChangeds()));
    connect(LxQt::Settings::globalSettings(), SIGNAL(settingsChanged()), this, SLOT(update()));
    connect(lxqtApp, SIGNAL(themeChanged()), this, SLOT(realign()));

    LxQtPanelApplication *app = reinterpret_cast<LxQtPanelApplication*>(qApp);
    mSettings = app->settings();
    readSettings();
    loadPlugins();

    // startup apply show or hide
    setAutohideActive(true);
    autoHideUnlock();

    prevEventFilter = QAbstractEventDispatcher::instance()->setEventFilter(LxQtPanel::sysEventFilter);

    show();

    realign();
    emit realigned();

    qDebug() << "STARTED ------------------------------------ " << mAutoHideActive << " - root: " << QX11Info::appRootWindow();
}


/************************************************

 ************************************************/
void LxQtPanel::readSettings()
{
    // Read settings ......................................
    mSettings->beginGroup(mConfigGroup);

    // By default we are using size & count from theme.
    setPanelSize(mSettings->value(CFG_KEY_PANELSIZE, PANEL_DEFAULT_SIZE).toInt());
    setIconSize(mSettings->value(CFG_KEY_ICONSIZE, PANEL_DEFAULT_ICON_SIZE).toInt());
    setLineCount(mSettings->value(CFG_KEY_LINECNT, PANEL_DEFAULT_LINE_COUNT).toInt());

    setLength(mSettings->value(CFG_KEY_LENGTH, 100).toInt(),
              mSettings->value(CFG_KEY_PERCENT, true).toBool());

    setPosition(mSettings->value(CFG_KEY_SCREENNUM, QApplication::desktop()->primaryScreen()).toInt(),
                strToPosition(mSettings->value(CFG_KEY_POSITION).toString(), PositionBottom));

    setAlignment(LxQtPanel::Alignment(mSettings->value(CFG_KEY_ALIGNMENT, mAlignment).toInt()));

    setAutohide(mSettings->value(CFG_KEY_AUTOHIDE,PANEL_DEFAULT_AUTOHIDE).toBool());

    mSettings->endGroup();
}


/************************************************

 ************************************************/
void LxQtPanel::saveSettings(bool later)
{
    mDelaySave.stop();
    if (later)
    {
        mDelaySave.start();
        return;
    }

    QStringList pluginsList;

    mSettings->beginGroup(mConfigGroup);

    foreach (const Plugin *plugin, mPlugins)
    {
        pluginsList << plugin->settingsGroup();
    }

    if (pluginsList.isEmpty())
    {
        mSettings->setValue(CFG_KEY_PLUGINS, "");
    }
    else
    {
        mSettings->setValue(CFG_KEY_PLUGINS, pluginsList);
    }

    mSettings->setValue(CFG_KEY_PANELSIZE,  mPanelSize);
    mSettings->setValue(CFG_KEY_ICONSIZE,   mIconSize);
    mSettings->setValue(CFG_KEY_LINECNT,    mLineCount);

    mSettings->setValue(CFG_KEY_LENGTH,   mLength);
    mSettings->setValue(CFG_KEY_PERCENT,  mLengthInPercents);

    mSettings->setValue(CFG_KEY_SCREENNUM, mScreenNum);
    mSettings->setValue(CFG_KEY_POSITION, positionToStr(mPosition));

    mSettings->setValue(CFG_KEY_ALIGNMENT, mAlignment);

    mSettings->setValue(CFG_KEY_AUTOHIDE, mAutoHideTb);

    mSettings->endGroup();
}


/************************************************

 ************************************************/
void LxQtPanel::screensChangeds()
{
    if (! canPlacedOn(mScreenNum, mPosition))
        setPosition(findAvailableScreen(mPosition), mPosition);
}


/************************************************

 ************************************************/
LxQtPanel::~LxQtPanel()
{
    mLayout->setEnabled(false);
    // do not save settings because of "user deleted panel" functionality saveSettings();
    qDeleteAll(mPlugins);
}


/************************************************

 ************************************************/
void LxQtPanel::show()
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // Qt::WA_X11NetWmWindowTypeDock becomes ineffective in Qt 5
    // See QTBUG-39887: https://bugreports.qt-project.org/browse/QTBUG-39887
    // Let's do it manually
    Atom windowTypes[] = {
        xfitMan().atom("_NET_WM_WINDOW_TYPE_DOCK"),
        xfitMan().atom("_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"), // required for Qt::FramelessWindowHint
        xfitMan().atom("_NET_WM_WINDOW_TYPE_NORMAL")
    };
    XChangeProperty(QX11Info::display(), winId(), xfitMan().atom("_NET_WM_WINDOW_TYPE"),
        XA_ATOM, 32, PropModeReplace, (unsigned char*)windowTypes, 3);
#endif
    QWidget::show();
    xfitMan().moveWindowToDesktop(this->effectiveWinId(), -1);
}


/************************************************

 ************************************************/
QStringList pluginDesktopDirs()
{
    QStringList dirs;
    dirs << QString(getenv("LXQT_PANEL_PLUGINS_DIR")).split(':', QString::SkipEmptyParts);
    dirs << QString("%1/%2").arg(XdgDirs::dataHome(), "/lxqt/lxqt-panel");
    dirs << PLUGIN_DESKTOPS_DIR;
    return dirs;
}


/************************************************

 ************************************************/
void LxQtPanel::loadPlugins()
{
    QStringList desktopDirs = pluginDesktopDirs();
    mSettings->beginGroup(mConfigGroup);
    QStringList sections = mSettings->value(CFG_KEY_PLUGINS).toStringList();
    mSettings->endGroup();

    foreach (QString sect, sections)
    {
        QString type = mSettings->value(sect+"/type").toString();
        if (type.isEmpty())
        {
            qWarning() << QString("Section \"%1\" not found in %2.").arg(sect, mSettings->fileName());
            continue;
        }

        LxQt::PluginInfoList list = LxQt::PluginInfo::search(desktopDirs, "LxQtPanel/Plugin", QString("%1.desktop").arg(type));
        if( !list.count())
        {
            qWarning() << QString("Plugin \"%1\" not found.").arg(type);
            continue;
        }

        loadPlugin(list.first(), sect);
    }
}


/************************************************

 ************************************************/
Plugin *LxQtPanel::loadPlugin(const LxQt::PluginInfo &desktopFile, const QString &settingsGroup)
{
    Plugin *plugin = new Plugin(desktopFile, mSettings->fileName(), settingsGroup, this);
    if (plugin->isLoaded())
    {
        mPlugins.append(plugin);
        connect(plugin, SIGNAL(startMove()), mLayout, SLOT(startMovePlugin()));
        connect(plugin, SIGNAL(remove()), this, SLOT(removePlugin()));
        connect(this, SIGNAL(realigned()), plugin, SLOT(realign()));
        mLayout->addWidget(plugin);
        return plugin;
    }

    delete plugin;
    return 0;
}


/************************************************

 ************************************************/
void LxQtPanel::realign()
{
    if (!isVisible())
        return;
#if 0
    qDebug() << "** Realign *********************";
    qDebug() << "PanelSize:   " << mPanelSize;
    qDebug() << "IconSize:      " << mIconSize;
    qDebug() << "LineCount:     " << mLineCount;
    qDebug() << "Length:        " << mLength << (mLengthInPercents ? "%" : "px");
    qDebug() << "Alignment:     " << (mAlignment == 0 ? "center" : (mAlignment < 0 ? "left" : "right"));
    qDebug() << "Position:      " << positionToStr(mPosition) << "on" << mScreenNum;
    qDebug() << "Plugins count: " << mPlugins.count();
#endif

    const QRect screen = QApplication::desktop()->screenGeometry(mScreenNum);
    QSize size = sizeHint();
    QRect rect;

    qDebug() << "REALIGN STATE: CFG/ACTIVE/CONFIG" << mAutoHideTb << " / " << mAutoHideActive << " / " << mAutoHideConfigLock;

   // qDebug() << "realign, states: (active,configlock) " << mAutoHideActive << " -- " << mAutoHideConfigLock;

    if (isHorizontal())
    {
        // Horiz panel ***************************
        size.setHeight(mPanelSize);

        // Size .......................        

        if (mAutoHideTb && mAutoHideActive && !mAutoHideConfigLock) {


            rect.setHeight(qMax(1, AUTOHIDETB_DEFAULT_HEIGHT));
        }
        else
            rect.setHeight(qMax(PANEL_MINIMUM_SIZE, size.height()));

        if (mLengthInPercents)
            rect.setWidth(screen.width() * mLength / 100.0);
        else
        {
          if (mLength <= 0)
            rect.setWidth(screen.width() + mLength);
          else
            rect.setWidth(mLength);
        }

        rect.setWidth(qMax(rect.size().width(), mLayout->minimumSize().width()));

        // Horiz ......................
        switch (mAlignment)
        {
        case LxQtPanel::AlignmentLeft:
            rect.moveLeft(screen.left());
            break;

        case LxQtPanel::AlignmentCenter:
            rect.moveCenter(screen.center());
            break;

        case LxQtPanel::AlignmentRight:
            rect.moveRight(screen.right());
            break;
        }

        // Vert .......................
        if (mPosition == ILxQtPanel::PositionTop)
            rect.moveTop(screen.top());
        else
            rect.moveBottom(screen.bottom());
    }
    else
    {
        // Vert panel ***************************
        size.setWidth(mPanelSize);

        // Size .......................        
        if (mAutoHideTb && mAutoHideActive && !mAutoHideConfigLock)
            rect.setWidth(qMax(1, AUTOHIDETB_DEFAULT_HEIGHT));
        else
            rect.setWidth(qMax(PANEL_MINIMUM_SIZE, size.width()));


        if (mLengthInPercents)
            rect.setHeight(screen.height() * mLength / 100.0);
        else
        {
          if (mLength <= 0)
            rect.setHeight(screen.height() + mLength);
          else
            rect.setHeight(mLength);
        }

        rect.setHeight(qMax(rect.size().height(), mLayout->minimumSize().height()));

        // Vert .......................
        switch (mAlignment)
        {
        case LxQtPanel::AlignmentLeft:
            rect.moveTop(screen.top());
            break;

        case LxQtPanel::AlignmentCenter:
            rect.moveCenter(screen.center());
            break;

        case LxQtPanel::AlignmentRight:
            rect.moveBottom(screen.bottom());
            break;
        }

        // Horiz ......................
        if (mPosition == ILxQtPanel::PositionLeft)
            rect.moveLeft(screen.left());
        else
            rect.moveRight(screen.right());
    }

    // due to autohide commented
//    if (rect == geometry())
//        return;

    setGeometry(rect);
    setFixedSize(rect.size());


    // Reserve our space on the screen ..........
    XfitMan xf = xfitMan();
    Window wid = effectiveWinId();


    // ... depeding on autohide taskbar on or off
    if (mAutoHideTb)
    {

        switch (mPosition)
        {
            case LxQtPanel::PositionTop:
                xf.setStrut(wid, 0, 0, AUTOHIDETB_DEFAULT_HEIGHT, 0,
                   /* Left   */  0, 0,
                   /* Right  */  0, 0,
                   /* Top    */  rect.left(), rect.right(),
                   /* Bottom */  0, 0
                             );
            break;

            case LxQtPanel::PositionBottom:

            xf.setStrut(wid, 0, 0, 0, AUTOHIDETB_DEFAULT_HEIGHT,
                   /* Left   */  0, 0,
                   /* Right  */  0, 0,
                   /* Top    */  0, 0,
                   /* Bottom */  rect.left(), rect.right()
                             );
                break;

            case LxQtPanel::PositionLeft:
                xf.setStrut(wid, AUTOHIDETB_DEFAULT_HEIGHT, 0, 0, 0,
                   /* Left   */  rect.top(), rect.bottom(),
                   /* Right  */  0, 0,
                   /* Top    */  0, 0,
                   /* Bottom */  0, 0
                             );

                break;

            case LxQtPanel::PositionRight:
                xf.setStrut(wid, 0, screen.width() - rect.x(), 0, 0,
                   /* Left   */  0, 0,
                   /* Right  */  rect.top(), rect.bottom(),
                   /* Top    */  0, 0,
                   /* Bottom */  0, 0
                             );
                break;
        }
    }
    else
    {
        switch (mPosition)
        {
            case LxQtPanel::PositionTop:
                xf.setStrut(wid, 0, 0, height(), 0,
                   /* Left   */  0, 0,
                   /* Right  */  0, 0,
                   /* Top    */  rect.left(), rect.right(),
                   /* Bottom */  0, 0
                             );
            break;

            case LxQtPanel::PositionBottom:
               xf.setStrut(wid, 0, 0, 0, screen.height() - rect.y(),
                   /* Left   */  0, 0,
                   /* Right  */  0, 0,
                   /* Top    */  0, 0,
                   /* Bottom */  rect.left(), rect.right()
                             );
                break;

            case LxQtPanel::PositionLeft:
                xf.setStrut(wid, width(), 0, 0, 0,
                   /* Left   */  rect.top(), rect.bottom(),
                   /* Right  */  0, 0,
                   /* Top    */  0, 0,
                   /* Bottom */  0, 0
                             );

                break;

            case LxQtPanel::PositionRight:
                xf.setStrut(wid, 0, screen.width() - rect.x(), 0, 0,
                   /* Left   */  0, 0,
                   /* Right  */  rect.top(), rect.bottom(),
                   /* Top    */  0, 0,
                   /* Bottom */  0, 0
                             );
                break;
        }
    }


}


/************************************************
  The panel can't be placed on boundary of two displays.
  This function checks, is the panel can be placed on the display
  @displayNum on @position.
 ************************************************/
bool LxQtPanel::canPlacedOn(int screenNum, LxQtPanel::Position position)
{
    QDesktopWidget* dw = QApplication::desktop();

    switch (position)
    {
        case LxQtPanel::PositionTop:
            for (int i=0; i < dw->screenCount(); ++i)
            {
                if (dw->screenGeometry(i).bottom() < dw->screenGeometry(screenNum).top())
                    return false;
            }
            return true;

        case LxQtPanel::PositionBottom:
            for (int i=0; i < dw->screenCount(); ++i)
            {
                if (dw->screenGeometry(i).top() > dw->screenGeometry(screenNum).bottom())
                    return false;
            }
            return true;

        case LxQtPanel::PositionLeft:
            for (int i=0; i < dw->screenCount(); ++i)
            {
                if (dw->screenGeometry(i).right() < dw->screenGeometry(screenNum).left())
                    return false;
            }
            return true;

        case LxQtPanel::PositionRight:
            for (int i=0; i < dw->screenCount(); ++i)
            {
                if (dw->screenGeometry(i).left() > dw->screenGeometry(screenNum).right())
                    return false;
            }
            return true;
    }

    return false;
}


/************************************************

 ************************************************/
int LxQtPanel::findAvailableScreen(LxQtPanel::Position position)
{
    int current = mScreenNum;
    for (int i = current; i < QApplication::desktop()->screenCount(); ++i)
    {
        if (canPlacedOn(i, position))
            return i;
    }

    for (int i = 0; i < current; ++i)
    {
        if (canPlacedOn(i, position))
            return i;
    }

    return 0;
}


/************************************************

 ************************************************/
void LxQtPanel::showConfigDialog()
{
    ConfigPanelDialog::exec(this);
}


void LxQtPanel::autoHideUnlock()
{
    qDebug() << " ----------------------------------- Unlock called";
    if (isAutoHideConfigLock())
    {
        qDebug() << " -- Autohide omitted ";
        unsetAutoHideConfigLock();
    }
    //mAutoHideLock = false;
    mAutoHideActive = true;
    emit realigned();
    realign();
}


void LxQtPanel::autoHideLock()
{
    qDebug() << " ----------------------------------- Lock called";
//    mAutoHideLock = true;
    setAutoHideConfigLock();
    emit realigned();
    realign();
}


/************************************************

 ************************************************/
void LxQtPanel::showAddPluginDialog()
{
    AddPluginDialog* dialog = findChild<AddPluginDialog*>();

    if (!dialog)
    {
        dialog = new AddPluginDialog(pluginDesktopDirs(), "LxQtPanel/Plugin", "*", this);
        dialog->setWindowTitle(tr("Add Panel Widgets"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, SIGNAL(pluginSelected(const LxQt::PluginInfo&)), this, SLOT(addPlugin(const LxQt::PluginInfo&)));
    }

    LxQt::PluginInfoList pluginsInUse;
    foreach (Plugin *i, mPlugins)
        pluginsInUse << i->desktopFile();
    dialog->setPluginsInUse(pluginsInUse);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    xfitMan().raiseWindow(dialog->effectiveWinId());
    xfitMan().moveWindowToDesktop(dialog->effectiveWinId(), qMax(xfitMan().getActiveDesktop(), 0));

    dialog->show();
}


/************************************************

 ************************************************/
void LxQtPanel::addPlugin(const LxQt::PluginInfo &desktopFile)
{
    QString settingsGroup = findNewPluginSettingsGroup(desktopFile.id());
    loadPlugin(desktopFile, settingsGroup);
    saveSettings(true);

    realign();
    emit realigned();
}


/************************************************

 ************************************************/
void LxQtPanel::updateStyleSheet()
{
    QStringList sheet;
    sheet << QString("Plugin > * { qproperty-iconSize: %1px %1px; }").arg(mIconSize);
    sheet << QString("Plugin > * > * { qproperty-iconSize: %1px %1px; }").arg(mIconSize);

    setStyleSheet(sheet.join("\n"));
}



/************************************************

 ************************************************/
void LxQtPanel::setPanelSize(int value)
{
    if (mPanelSize != value)
    {
        mPanelSize = value;
        realign();
        emit realigned();
        saveSettings(true);
    }
}



/************************************************

 ************************************************/
void LxQtPanel::setIconSize(int value)
{
    if (mIconSize != value)
    {
        mIconSize = value;
        updateStyleSheet();
        mLayout->setLineSize(mIconSize);
        saveSettings(true);

        realign();
        emit realigned();
    }
}


/************************************************
 *
 * *********************************************/
void LxQtPanel::setAutohide(bool value)
{

    if (mAutoHideTb != value)
    {
        mAutoHideTb = value;
        updateStyleSheet();        
        saveSettings(true);

        // for showing the result immediately
        mAutoHideActive = value;
    }

    realign();
    emit realigned();
}


/************************************************

 ************************************************/
void LxQtPanel::setLineCount(int value)
{
    if (mLineCount != value)
    {
        mLineCount = value;
        mLayout->setEnabled(false);
        mLayout->setLineCount(mLineCount);        
        mLayout->setEnabled(true);        
        saveSettings(true);

        realign();
        emit realigned();
    }
}


/************************************************

 ************************************************/
void LxQtPanel::setLength(int length, bool inPercents)
{
    if (mLength == length &&
        mLengthInPercents == inPercents)
        return;

    mLength = length;
    mLengthInPercents = inPercents;    
    saveSettings(true);

    realign();
    emit realigned();
}


/************************************************

 ************************************************/
void LxQtPanel::setPosition(int screen, ILxQtPanel::Position position)
{
    if (mScreenNum == screen &&
        mPosition == position)
        return;

    mScreenNum = screen;
    mPosition = position;
    mLayout->setPosition(mPosition);
    saveSettings(true);    

    realign();
    emit realigned();
}


/************************************************

 ************************************************/
void LxQtPanel::setAlignment(LxQtPanel::Alignment value)
{
    if (mAlignment == value)
        return;

    mAlignment = value;    
    saveSettings(true);

    realign();
    emit realigned();
}


/************************************************

 ************************************************/

void LxQtPanel::setAutohideActive(bool value)
{
    if (!mAutoHideTb)
        return;

    mAutoHideActive = value;

    qDebug () << "setAutohideActive: " << mAutoHideActive << " - " << mAutoHideConfigLock;
    //emit realigned();
    realign();
}

void LxQtPanel::setAutohideLeaveWorkaround(bool value)
{

    mAutoHideLeaveWorkaround = value;
}

bool LxQtPanel::isAutoHide()
{
    return mAutoHideActive && !mAutoHideConfigLock;
}


//bool LxQtPanel::eventFilter (QObject *obj, QEvent *event)
//{
// qDebug() << "EVENT FILTER CALLED";
//}


bool LxQtPanel::sysEventFilter(void* message)
{
 qDebug("Event!");
 return true;
}

/************************************************

 ************************************************/
void LxQtPanel::x11EventFilter(XEventType* event)
{

    //qDebug() << "General: has Childern: " << this->frameRect();

    switch (event->type)
    {

//        case Expose:
////qDebug() << " --- EXPOSE" << event->type << " -- " << event->pad;
//        break;
//        case NoExpose:
////qDebug() << " --- NO EXPOSE" << event->type << " -- " << event->pad;
//        break;

        // No test
        case MapNotify:
            qDebug() << " --- MAP" << event->type << " -- " << event->pad;
          //  autoHideLock();
        break;
        case UnmapNotify:
            qDebug() << " --- UNMAP" << event->type << " -- " << event->pad;
            if (crazypad == event->pad)
            {
                qDebug () << "##### SHOULD UNLOCK FROM UNMAP ###########################################";
                //setAutohide(true);

            }
          //  autoHideLock();
        break;

        case EnterNotify:
            //autoHideLock();
            qDebug() << " --- ENTER" << event->type << " -- " << event->pad;
            if(crazypad == 0)
            {
               crazypad = event->pad;
               qDebug () << "CRAZY PAD ASSIGNED: " << crazypad;
            }
//            if (isAutoHide())
//            {
//                setAutohide(false);
//                setAutohideLeaveWorkaround(true);
//            }
        break;

 //       case LeaveNotify:
//        if(_mouseHandler)
//            _mouseHandler->HandleInput(lDisplay, &XEvent);
         qDebug() << " --- LEAVE" << event->type << " -- " << event->pad;
         if(crazypad == 0) {
            crazypad = event->pad;
            qDebug () << "CRAZY PAD ASSIGNED: " << crazypad;
         }

         if (!mAutoHideLeaveWorkaround)
         {
            //setAutohide(true);
         }
         setAutohideLeaveWorkaround(false);

         break;

//        case DestroyNotify:
            qDebug() << " --- DESTROY" << event->type << " -- " << event->pad;
            //autoHideUnlock();

            if (crazypad == event->pad)
            {
                qDebug () << "##### SHOULD UNLOCK FROM DESTROY ###########################################";
                //setAutohide(true);

            }
        break;


//    case MotionNotify:
      //  qDebug() << "Unwanted Event! --- !!"  << event->type << " -- " << event->pad;
    break;






    case ConfigureNotify:
  //       qDebug() << "Configure Event! --- !!";
        break;
    case VisibilityNotify:
         qDebug() << "Visibility Event! --- !!";
        break;
    case CreateNotify:
        qDebug() << "create Event! --- !!";
        break;


// unwanted events
    case PropertyNotify:

        break;

    default:
//        if(_keyboardHandler)
//            _keyboardHandler->HandleInput(lDisplay, &XEvent);
     //    qDebug() << "unknown Event! --- !!" << event->type;
        break;
    }

    QList<Plugin*>::iterator i;
    for (i = mPlugins.begin(); i != mPlugins.end(); ++i)
        (*i)->x11EventFilter(event);
}


/************************************************

 ************************************************/
QRect LxQtPanel::globalGometry() const
{
    return QRect(mapToGlobal(QPoint(0, 0)), this->size());
}


void LxQtPanel:: mouseMoveEvent(QMouseEvent* event)
{

}

/************************************************

 ************************************************/
bool LxQtPanel::event(QEvent *event)
{
//return QFrame::event(event);
    qDebug() << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!There was an Qevent!!!" << event->type();

    switch (event->type())
    {
    case QEvent::ContextMenu:
        qDebug() << "CONTEXT MENU";
        showPopupMenu();
        break;

    case QEvent::LayoutRequest:
        realign();
        emit realigned();
        break;

    case QEvent::Leave:
           // qDebug() << "Mouse left...";
            break;

    case QEvent::Enter:
      //  qDebug() << "QEvent ENTER";
        //autoHideLock();
   //     setAutohideActive(true);
        break;

    case QEvent::ChildAdded:
        qDebug() << "            CHILD ADDED...";
        break;

    case QEvent::ChildPolished:
        qDebug() << "            CHILD Polished...";
        break;

    case QEvent::ChildRemoved:
        qDebug() << "            CHILD removed...";
        break;

    default:
        //qDebug() << "!!!There was an Qevent!!!" << event->type();
        break;
    }
    return QFrame::event(event);
}


/************************************************

 ************************************************/
void LxQtPanel::showEvent(QShowEvent *event)
{
    realign();
    emit realigned();
}

//void LxQtPanel::mouseMoveEvent(QMouseEvent *event) {
void LxQtPanel::enterEvent(QEvent *event)
{

    //setAutohideActive(false);
}


void LxQtPanel::dragEnterEvent(QDragEnterEvent *event)
{
    qDebug() << "dragEnterEvent should bring all panels to unhide";

}

void LxQtPanel::leaveEvent(QEvent *event)
{
 //   if (!mAutoHideConfigLock)
   //     setAutohideActive(true);
}


void LxQtPanel::theTestFunc ()
{
    qDebug() << " OH BABY!!!!!!!!!!!!!!!!!!!1!";
}

/************************************************

 ************************************************/
void LxQtPanel::showPopupMenu(Plugin *plugin)
{
    QList<QMenu*> pluginsMenus;
    PopupMenu menu(tr("Panel"));

    menu.setIcon(XdgIcon::fromTheme("configure-toolbars"));

    // Plugin Menu ..............................
    if (plugin)
    {
        QMenu *m = plugin->popupMenu();

        if (m)
        {
            menu.addTitle(plugin->windowTitle().replace("&", "&&"));

            menu.addActions(m->actions());
            pluginsMenus << m;
        }
    }

    // Panel menu ...............................

    menu.addTitle(QIcon(), tr("Panel"));

    menu.addAction(tr("Configure Panel..."),
                   this, SLOT(showConfigDialog())
                  );

    //connect(menu, SIGNAL(destroyed()), this, SLOT(autoHideUnlock()));
    //connect(&menu,SIGNAL(hovered()),this, SLOT(theTestFunc()));
//    connect(this,SLOT(theTestFunc()),&menu, SIGNAL(hovered()));
    //connect(this, SLOT(theTestFunc()),&menu,SIGNAL(menuGehidet()));

    qDebug() << " MAIN FUNCTION MENU TRIGGERED!" << pluginsMenus;

    // autohide lock
    //autoHideLock();


    menu.addAction(XdgIcon::fromTheme("preferences-plugin"),
                   tr("Add Panel Widgets..."),
                   this, SLOT(showAddPluginDialog())
                  );

    LxQtPanelApplication *a = reinterpret_cast<LxQtPanelApplication*>(qApp);
    menu.addAction(tr("Add Panel"),
                   a, SLOT(addNewPanel())
                  );

    if (a->count() > 1)
    {
        menu.addAction(tr("Remove Panel"),
                       this, SLOT(userRequestForDeletion())
                      );
    }

#ifdef DEBUG
    menu.addSeparator();
    menu.addAction("Exit (debug only)", qApp, SLOT(quit()));
#endif

    menu.exec(QCursor::pos());
    qDeleteAll(pluginsMenus);
}


/************************************************

 ************************************************/
Plugin *LxQtPanel::findPlugin(const ILxQtPanelPlugin *iPlugin) const
{
    foreach(Plugin *plugin, mPlugins)
    {
        if (plugin->iPlugin() == iPlugin)
            return plugin;
    }

    return 0;
}


/************************************************

 ************************************************/
QRect LxQtPanel::calculatePopupWindowPos(const ILxQtPanelPlugin *plugin, const QSize &windowSize) const
{
    Plugin *panelPlugin = findPlugin(plugin);
    if (!plugin)
        return QRect();

    int x=0, y=0;

    switch (position())
    {
        case ILxQtPanel::PositionTop:
            x = panelPlugin->mapToGlobal(QPoint(0, 0)).x();
            y = globalGometry().bottom();
            break;

        case ILxQtPanel::PositionBottom:
            x = panelPlugin->mapToGlobal(QPoint(0, 0)).x();
            y = globalGometry().top() - windowSize.height();
            break;

        case ILxQtPanel::PositionLeft:
            x = globalGometry().right();
            y = panelPlugin->mapToGlobal(QPoint(0, 0)).y();
            break;

        case ILxQtPanel::PositionRight:
            x = globalGometry().left() - windowSize.width();
            y = panelPlugin->mapToGlobal(QPoint(0, 0)).y();
            break;
    }

    QRect res(QPoint(x, y), windowSize);

    QRect screen = QApplication::desktop()->availableGeometry(this);

    if (res.right() > screen.right())
        res.moveRight(screen.right());

    if (res.bottom() > screen.bottom())
        res.moveBottom(screen.bottom());

    if (res.left() < screen.left())
        res.moveLeft(screen.left());

    if (res.top() < screen.top())
        res.moveTop(screen.top());

    return res;
}


/************************************************

 ************************************************/
QString LxQtPanel::qssPosition() const
{
    return positionToStr(position());
}


/************************************************

 ************************************************/
QString LxQtPanel::findNewPluginSettingsGroup(const QString &pluginType) const
{
    QStringList groups = mSettings->childGroups();
    groups.sort();

    // Generate new section name ................
    for (int i=2; true; ++i)
    {
        if (!groups.contains(QString("%1%2").arg(pluginType).arg(i)))
        {
            return QString("%1%2").arg(pluginType).arg(i);
        }
    }
}


/************************************************

 ************************************************/
void LxQtPanel::removePlugin()
{
    Plugin *plugin = qobject_cast<Plugin*>(sender());
    if (plugin)
    {
        mPlugins.removeAll(plugin);
    }
    saveSettings();
}


/************************************************

 ************************************************/
void LxQtPanel::pluginMoved()
{
    mPlugins.clear();
    for (int i=0; i<mLayout->count(); ++i)
    {
        Plugin *plugin = qobject_cast<Plugin*>(mLayout->itemAt(i)->widget());
        if (plugin)
            mPlugins << plugin;
    }
    saveSettings();
}

void LxQtPanel::userRequestForDeletion()
{
    mSettings->beginGroup(mConfigGroup);
    QStringList plugins = mSettings->value("plugins").toStringList();
    mSettings->endGroup();

    Q_FOREACH(QString i, plugins)
        mSettings->remove(i);

    mSettings->remove(mConfigGroup);

    emit deletedByUser(this);
}



