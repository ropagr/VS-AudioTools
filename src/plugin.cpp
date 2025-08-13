// SPDX-License-Identifier: MIT

#include "VapourSynth4.h"

#include "config.hpp"
#include "convert.hpp"
#include "crossfade.hpp"
#include "delay.hpp"
#include "fadein.hpp"
#include "fadeout.hpp"
#include "findpeak.hpp"
#include "mix.hpp"
#include "normalize.hpp"
#include "sinetone.hpp"
#include "setsamples.hpp"

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin* plugin, const VSPLUGINAPI* vspapi)
{
    vspapi->configPlugin("com.ropagr.atools", "atools", "basic audio functions", VS_MAKE_VERSION(0, 1), VAPOURSYNTH_API_VERSION, 0, plugin);

    convertInit(plugin, vspapi);

    crossfadeInit(plugin, vspapi);

    fadeinInit(plugin, vspapi);

    fadeoutInit(plugin, vspapi);

    findpeakInit(plugin, vspapi);

    delayInit(plugin, vspapi);

    mixInit(plugin, vspapi);

    normalizeInit(plugin, vspapi);

    sinetoneInit(plugin, vspapi);

    // undocumented function, only for debugging
    setsamplesInit(plugin, vspapi);
}
