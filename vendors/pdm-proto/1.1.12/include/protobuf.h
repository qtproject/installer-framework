#pragma once

#if USE_EVE_PUBLIC_DOMAIN
#include "eve_public/app/platform.pb.h"
namespace platform = eve_public::app::platform;
#else
#include "eve_launcher/pdm.pb.h"
#endif

namespace pdm_proto
{
    DllExport platform::Information GetData();
}
