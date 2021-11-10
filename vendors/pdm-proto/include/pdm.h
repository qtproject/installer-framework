#pragma once

#include "pdm_data.h"

namespace PDM
{
	PDMDllExport const PDMData& RetrievePDMData(std::string applicationName, std::string applicationVersion);
	PDMDllExport std::string PDMDataToFormattedString(const PDMData& pdmData);

	PDMDllExport std::string GetPDMVersion();
	PDMDllExport OS GetOSType();
	PDMDllExport std::string GetOSName();
	PDMDllExport std::string GetOSMajorVersion();
	PDMDllExport std::string GetOSMinorVersion();
	PDMDllExport std::string GetOSBuildNumber();
	PDMDllExport std::string GetOSKernelVersion();
	PDMDllExport std::string GetHardwareModel();
	PDMDllExport std::string GetMachineName();
	PDMDllExport std::string GetUsername();
	PDMDllExport std::string GetUserLocale();
	PDMDllExport uint32_t GetMonitorCount();
	PDMDllExport uint64_t GetTotalMemory();
	PDMDllExport StreamingService GetStreamingService();
	PDMDllExport bool IsRemoteSession();
	PDMDllExport size_t GetTimingCycles();
	PDMDllExport BatteryStatus GetBatteryStatus();
	PDMDllExport std::string GetMachineUuidString();
	PDMDllExport std::vector<uint8_t> GetMachineUuid();
	PDMDllExport std::vector<MonitorInfo> GetMonitorsInfo();
	PDMDllExport std::vector<GPUInfo> GetGPUInfo();
	PDMDllExport std::vector<NetworkAdapterInfo> GetNetworkAdapterInfo();
	PDMDllExport std::vector<HardDriveInfo> GetHardDriveInfo();
	PDMDllExport bool GetMetalSupported();
	PDMDllExport VulkanProperties GetVulkanProperties();
	PDMDllExport std::string GetD3DHighestSupport();
	PDMDllExport bool IsWine();
	PDMDllExport const char* GetWineVersion();
	PDMDllExport const char* GetWineHostOs();
	PDMDllExport Bitness GetProcessBitness();
	PDMDllExport Bitness GetOSBitness();
	PDMDllExport CPUInfo GetCPUInfo();
	PDMDllExport bool HasVMExecutionTiming();
	PDMDllExport bool HasHypervisorBit();
	PDMDllExport std::string GetHypervisorName();
	PDMDllExport bool IsHyperVGuestOS();
	PDMDllExport bool IsSuspectedVM();
	PDMDllExport bool IsRosetta();
}
