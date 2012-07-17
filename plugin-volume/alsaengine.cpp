/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Razor - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright: 2012 Razor team
 * Authors:
 *   Johannes Zellner <webmaster@nebulon.de>
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

#include "alsaengine.h"

#include "alsadevice.h"

#include <QtCore/QMetaType>
#include <QtDebug>

AlsaEngine::AlsaEngine(QObject *parent) :
    AudioEngine(parent)
{
    discoverDevices();
}

int AlsaEngine::volumeMax(AudioDevice *device) const
{
    AlsaDevice *dev = qobject_cast<AlsaDevice*>(device);
    if (!dev || !dev->m_elem)
        return 100;

    long vmin;
    long vmax;
    snd_mixer_selem_get_playback_volume_range(dev->m_elem, &vmin, &vmax);

    return vmax;
}

void AlsaEngine::commitDeviceVolume(AudioDevice *device)
{
    AlsaDevice *dev = qobject_cast<AlsaDevice*>(device);
    if (!dev || !dev->m_elem)
        return;

    long value = dev->volume();
    snd_mixer_selem_set_playback_volume_all(dev->m_elem, value);
}

void AlsaEngine::setMute(AudioDevice *device, bool state)
{
    AlsaDevice *dev = qobject_cast<AlsaDevice*>(device);
    if (!dev || !dev->m_elem)
        return;

    if (snd_mixer_selem_has_playback_switch(dev->m_elem))
        snd_mixer_selem_set_playback_switch_all(dev->m_elem, (int)state);
    else if (state)
        dev->setVolume(0);
}

void AlsaEngine::discoverDevices()
{
    int error;
    int cardNum = -1;

    while (1) {
        error = snd_card_next(&cardNum);

        if (cardNum < 0)
            break;

        char str[64];
        sprintf(str, "hw:%i", cardNum);

        snd_ctl_t *cardHandle;
        if ((error = snd_ctl_open(&cardHandle, str, 0)) < 0) {
            qWarning("Can't open card %i: %s\n", cardNum, snd_strerror(error));
            continue;
        }

        snd_ctl_card_info_t *cardInfo;
        snd_ctl_card_info_alloca(&cardInfo);

        QString cardName = QString::fromAscii(snd_ctl_card_info_get_name(cardInfo));
        if (cardName.isEmpty())
            cardName = QString::fromAscii(str);

        if ((error = snd_ctl_card_info(cardHandle, cardInfo)) < 0) {
            qWarning("Can't get info for card %i: %s\n", cardNum, snd_strerror(error));
        } else {
            // setup mixer and iterate over channels
            snd_mixer_t *mixer = 0;
            snd_mixer_open(&mixer, 0);
            snd_mixer_attach(mixer, str);
            snd_mixer_selem_register(mixer, NULL, NULL);
            snd_mixer_load(mixer);

            snd_mixer_elem_t *mixerElem = 0;
            mixerElem = snd_mixer_first_elem(mixer);

            while (mixerElem) {
                // check if we have a Sink or Source
                if (snd_mixer_selem_has_playback_volume(mixerElem)) {
                    AlsaDevice *dev = new AlsaDevice(Sink, this, this);
                    dev->name = QString::fromAscii(snd_mixer_selem_get_name(mixerElem));
                    dev->index = cardNum;
                    dev->description = cardName + " - " + dev->name;

                    // set alsa specific members
                    dev->m_cardName = QString::fromAscii(str);
                    dev->m_mixer = mixer;
                    dev->m_elem = mixerElem;

                    long value;
                    snd_mixer_selem_get_playback_volume(mixerElem, (snd_mixer_selem_channel_id_t)0, &value);
                    dev->setVolumeNoCommit(value);

                    if (snd_mixer_selem_has_playback_switch(mixerElem)) {
                        int mute;
                        snd_mixer_selem_get_playback_switch(mixerElem, (snd_mixer_selem_channel_id_t)0, &mute);
                        dev->setMuteNoCommit(!(bool)mute);
                    }

                    m_sinks.append(dev);
                }

                mixerElem = snd_mixer_elem_next(mixerElem);
            }
        }

        snd_ctl_close(cardHandle);
    }

    snd_config_update_free_global();
}