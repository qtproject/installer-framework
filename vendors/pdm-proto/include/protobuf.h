#pragma once

#include "eve_public/app/platform.pb.h"
#include "eve_launcher/pdm.pb.h"

namespace pdm_proto
{
	eve_public::app::platform::Information GetEVEPublicData();
	platform::Information GetEVELauncherData();
}
