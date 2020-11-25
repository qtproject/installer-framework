#pragma once

#include "pdm_data.h"

namespace PDM
{
	DllExport const PDMData& RetrievePDMData(std::string applicationName, std::string applicationVersion);

	DllExport std::string GetPDMVersion();
	DllExport OS GetOSType();
	DllExport std::string GetOSName();
	DllExport std::string GetOSMajorVersion();
	DllExport std::string GetOSMinorVersion();
	DllExport std::string GetOSBuildNumber();
	DllExport std::string GetOSKernelVersion();
	DllExport std::string GetHardwareModel();
	DllExport std::string GetMachineName();
	DllExport std::string GetUsername();
	DllExport std::string GetUserLocale();
	DllExport unsigned GetMonitorCount();
	DllExport uint64_t GetTotalMemory();
	DllExport bool IsRemoteSession();
	DllExport size_t GetTimingCycles();
	DllExport std::string GetMachineUuidString();
	DllExport std::vector<uint8_t> GetMachineUuid();
	DllExport std::vector<MonitorInfo> GetMonitorsInfo();
	DllExport std::vector<GPUInfo> GetGPUInfo();
	DllExport std::vector<NetworkAdapterInfo> GetNetworkAdapterInfo();
	DllExport bool GetMetalSupported();
	DllExport VulkanProperties GetVulkanProperties();
	DllExport std::string GetD3DHighestSupport();
	DllExport bool IsWine();
	DllExport const char* GetWineVersion();
	DllExport const char* GetWineHostOs();
	DllExport Bitness GetProcessBitness();
	DllExport Bitness GetOSBitness();
	DllExport CPUInfo GetCPUInfo();
	DllExport bool HasVMExecutionTiming();
	DllExport bool HasHypervisorBit();
	DllExport std::string GetHypervisorName();
	DllExport bool IsHyperVGuestOS();
	DllExport bool IsSuspectedVM();
}
