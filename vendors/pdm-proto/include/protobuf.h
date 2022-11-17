#pragma once

#include <ostream>

namespace pdm_proto
{
  // Return information from the eve_public domain
  bool GetData(std::ostream* stream);
  bool GetEVEPublicData(std::ostream* stream);
  // Return information from the deprecated eve_launcher domain
  [[deprecated("The eve_launcher domain is deprecated, use eve_public instead")]]
  bool GetEVELauncherData(std::ostream* stream);
}
