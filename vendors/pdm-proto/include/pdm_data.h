#pragma once

#ifdef PDM_DLL_EXPORT

#if _WIN32
#define PDMDllExport __declspec( dllexport )
#else
#define PDMDllExport __attribute__(( visibility( "default" ) ))
#endif

#else

#define PDMDllExport

#endif

#include <vector>
#include <string>
#include <ctime>

namespace PDM
{
	enum class Bitness
	{
		BITNESS_UNKNOWN =  0,
		BITNESS_32      = 32,
		BITNESS_64      = 64
	};

	enum class CPUArchitecture
	{
		UNKNOWN = 0,
		X86,
		X86_64,
		ARM,
		ARM64,
	};

	enum class OS
	{
		UNKNOWN = 0,
		WINDOWS,
		MACOS,
		WINE,
	};

	enum class StreamingService
	{
		NONE = 0,
		UNKNOWN,
		INTEL_STREAM,
	};

	enum class VulkanSupport
	{
		UNKNOWN = 0,
		SUPPORTED,
		UNSUPPORTED,
	};

	struct PDMDllExport CPUInfo
	{
		int32_t model{ 0 };
		int32_t stepping{ 0 };
		std::string vendor;
		std::string brand;
		Bitness bitness;
		uint32_t logicalCoreCount{ 0 };
		CPUArchitecture architecture{ CPUArchitecture::UNKNOWN };
		std::vector<std::string> extensions;
	};

	struct PDMDllExport MonitorInfo
	{
		std::string name;
		uint32_t width{};
		uint32_t height{};
		uint32_t bitsPerColor{};
		uint32_t refreshRate{};
		uint32_t dpiScaling{};
	};

	struct PDMDllExport GPUInfo
	{
		std::string description;
		uint32_t vendorID{};
		uint32_t deviceID{};
		uint32_t revision{};
		uint64_t memory{};
		std::string driverVersionString;
		std::string driverDate;
		std::string driverVendor;

		bool operator==(const GPUInfo& rhs)
		{
			return
				description == rhs.description &&
				vendorID == rhs.vendorID &&
				deviceID == rhs.deviceID &&
				revision == rhs.revision &&
				memory == rhs.memory &&
				driverVersionString == rhs.driverVersionString &&
				driverDate == rhs.driverDate &&
				driverVendor == rhs.driverVendor
				;
		}
	};

	struct PDMDllExport NetworkAdapterInfo
	{
		std::string name;
		std::string macAddressString;
		std::string uuidString;
		std::vector<uint8_t> macAddress;
		std::vector<uint8_t> uuid;
	};

	struct PDMDllExport VulkanProperties
	{
		VulkanSupport support{ VulkanSupport::UNKNOWN };
		std::string version;
	};

	struct PDMDllExport TimeStamp : tm
	{
		bool operator ==(const TimeStamp& other) const
		{
			return
				tm_sec   == other.tm_sec  &&
				tm_min   == other.tm_min  &&
				tm_hour  == other.tm_hour &&
				tm_mday  == other.tm_mday &&
				tm_mon   == other.tm_mon  &&
				tm_year  == other.tm_year &&
				tm_yday  == other.tm_yday &&
				tm_isdst == other.tm_isdst;
		}
	};

	struct PDMDllExport DataField
	{
		bool operator ==(const DataField& other) const
		{
			return name == other.name && value == other.value;
		}

		std::string name;
		std::string value;
	};

	struct PDMDllExport SubItem
	{
		bool operator ==(const SubItem& other) const
		{
			return name == other.name &&
				subitems == other.subitems &&
				items == other.items;
		}

		std::string name;
		std::vector<SubItem> subitems;
		std::vector<DataField> items;
	};

	struct PDMDllExport PDMData
	{
		bool operator ==(const PDMData& other) const
		{
			return data == other.data && timestamp == other.timestamp;
		}

		SubItem data;
		TimeStamp timestamp;
	};
}
