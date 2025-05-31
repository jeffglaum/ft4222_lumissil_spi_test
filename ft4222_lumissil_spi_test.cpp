
#include <windows.h>
#include <vector>
#include <iostream>
#include <thread>
#include <chrono>

#include "ftd2xx.h"
#include "LibFT4222.h"

using namespace std::chrono;

#define Addr_Write_Page0 0x50
#define Addr_Write_Page1 0x51
#define Addr_Write_Page2 0x52
const int slaveSelectPin = 10;


std::vector< FT_DEVICE_LIST_INFO_NODE > g_FT4222DevList;

uint8_t PWM_Gamma64[64] =
{
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0b,0x0d,0x0f,0x11,0x13,0x16,
  0x1a,0x1c,0x1d,0x1f,0x22,0x25,0x28,0x2e,
  0x34,0x38,0x3c,0x40,0x44,0x48,0x4b,0x4f,
  0x55,0x5a,0x5f,0x64,0x69,0x6d,0x72,0x77,
  0x7d,0x80,0x88,0x8d,0x94,0x9a,0xa0,0xa7,
  0xac,0xb0,0xb9,0xbf,0xc6,0xcb,0xcf,0xd6,
  0xe1,0xe9,0xed,0xf1,0xf6,0xfa,0xfe,0xff
};

FT4222_STATUS SPI_WriteByte(FT_HANDLE ftHandle, uint8_t command, uint8_t address, uint8_t data) {
	uint16_t sizeTransferred;
	std::vector<unsigned char> writebuf;

	writebuf.clear();
	writebuf.push_back(command);
	writebuf.push_back(address);
	writebuf.push_back(data);

	return FT4222_SPIMaster_SingleWrite(ftHandle, &writebuf[0], (uint16_t)writebuf.size(), &sizeTransferred, true);
}

void Init3743B(FT_HANDLE ftHandle)
{
	uint8_t i;

	for (i = 0; i < 0xC7; i++)
	{
		SPI_WriteByte(ftHandle, Addr_Write_Page0, i, 0);      // PWM
	}

	for (i = 1; i < 0xC7; i++)
	{
		SPI_WriteByte(ftHandle, Addr_Write_Page1, i, 0xff);   // Scaling
	}

	SPI_WriteByte(ftHandle, Addr_Write_Page2, 0x02, 0x70);
	SPI_WriteByte(ftHandle, Addr_Write_Page2, 0x01, 0xFF);    // GCC
	SPI_WriteByte(ftHandle, Addr_Write_Page2, 0x00, 0x09);    //
}

inline std::string DeviceFlagToString(DWORD flags)
{
	std::string msg;
	msg += (flags & 0x1) ? "DEVICE_OPEN" : "DEVICE_CLOSED";
	msg += ", ";
	msg += (flags & 0x2) ? "High-speed USB" : "Full-speed USB";
	return msg;
}

void ListFtUsbDevices()
{
	FT_STATUS ftStatus = 0;

	DWORD numOfDevices = 0;
	ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

	for (DWORD iDev = 0; iDev < numOfDevices; ++iDev)
	{
		FT_DEVICE_LIST_INFO_NODE devInfo;
		memset(&devInfo, 0, sizeof(devInfo));

		ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
			devInfo.SerialNumber,
			devInfo.Description,
			&devInfo.ftHandle);

		if (FT_OK == ftStatus)
		{
			std::cout << "Dev: " << iDev << std::endl;
			std::cout << "  Flags = 0x" << devInfo.Flags << ", " << DeviceFlagToString(devInfo.Flags).c_str() << std::endl;
			std::cout << "  Type = 0x" << devInfo.Type << std::endl;
			std::cout << "  ID = 0x" << devInfo.ID << std::endl;
			std::cout << "  LocId = 0x" << devInfo.LocId << std::endl;
			std::cout << "  SerialNumber = " << devInfo.SerialNumber << std::endl;
			std::cout << "  Description = " << devInfo.Description << std::endl;
			std::cout << "  ftHandle = 0x" << devInfo.ftHandle << std::endl;

			const std::string desc = devInfo.Description;
			if (desc == "FT4222" || desc == "FT4222 A")
			{
				g_FT4222DevList.push_back(devInfo);
			}
		}
	}
}

int main()
{
	uint8_t i;
	int8_t j;

	std::cout << "INFO: Locating FT42222...\n";

	ListFtUsbDevices();

	if (g_FT4222DevList.empty()) {
		std::cout << "No FT4222 device is found!" << std::endl;
		return 0;
	}

	FT_HANDLE ftHandle = NULL;
	FT_STATUS ftStatus;
	ftStatus = FT_OpenEx((PVOID)g_FT4222DevList[0].SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
	if (FT_OK != ftStatus)
	{
		std::cout << "Open a FT4222 device failed!" << std::endl;
		return 0;
	}


	ftStatus = FT4222_SPIMaster_Init(ftHandle, SPI_IO_SINGLE, CLK_DIV_2, CLK_IDLE_LOW, CLK_LEADING, 0x01);
	if (FT_OK != ftStatus)
	{
		std::cout << "Init FT4222 as SPI master device failed!" << std::endl;
		return 0;
	}

	ftStatus = FT4222_SPI_SetDrivingStrength(ftHandle, DS_8MA, DS_8MA, DS_8MA);
	if (FT_OK != ftStatus)
	{
		std::cout << "set spi driving strength failed!" << std::endl;
		return 0;
	}

	ftStatus = FT4222_SPIMaster_SetLines(ftHandle, SPI_IO_SINGLE);
	if (FT_OK != ftStatus)
	{
		std::cout << "set spi single line failed!\n";
		return 0;
	}

	Init3743B(ftHandle);

	while (true)
	{
		std::cout << ".";

		for (j = 0; j < 64; j++)	//BLUE
		{
			for (i = 1; i < 0xC7; i = i + 3)
			{
				SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		for (j = 63; j >= 0; j--)
		{
			for (i = 1; i < 0xC7; i = i + 3)
			{
				SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		for (j = 0; j < 64; j++)	//GREEN 
		{
			for (i = 2; i < 0xC7; i = i + 3)
			{
				SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
			}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	for (j = 63; j >= 0; j--)
	{
		for (i = 2; i < 0xC7; i = i + 3)
		{
			SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	for (j = 0; j < 64; j++)	//RED
	{
		for (i = 3; i < 0xC7; i = i + 3)
		{
			SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	for (j = 63; j >= 0; j--)
	{
		for (i = 3; i < 0xC7; i = i + 3)
		{
			SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));


	for (j = 0; j < 64; j++)	//WHITE
	{
		for (i = 1; i < 0xC7; i++)
		{
			SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	for (j = 63; j >= 0; j--)
	{
		for (i = 1; i < 0xC7; i++)
		{
			SPI_WriteByte(ftHandle, Addr_Write_Page0, i, PWM_Gamma64[j]);	//PWM
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

FT4222_UnInitialize(ftHandle);
FT_Close(ftHandle);

return 0;
}
