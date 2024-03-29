// PlainAdapterLogger.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <iterator>
#include <vector>
#include <array>
using namespace std::literals;

#pragma comment(lib, "legacy_stdio_definitions.lib")
#ifdef __cplusplus
FILE iob[] = { *stdin, *stdout, *stderr };
extern "C" {
	FILE* __cdecl _iob(void) { return iob; }
}
#endif

#include "libusb.h"

libusb_device_handle* handle = NULL;
std::ofstream output_file;

#define MAKE_RUMBLE 0
#define TRIGGER_RUMBLES 1
#define SHOW_CONNECTIONS 0
#define LOG_INPUTS 1

void INCallback(struct libusb_transfer* transfer) {
#if LOG_INPUTS
	std::ostringstream oss;
	oss << std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1) << " ";
	for (int i = 0; i < transfer->actual_length; i++) {
		oss << (int)transfer->buffer[i] << (i == transfer->actual_length - 1 ? "\n" : " ");
	}

	output_file << oss.str();
#endif

#if SHOW_CONNECTIONS
	static std::array<bool, 4> persistent = { false , false, false, false };
	std::array<bool, 4> connected = { transfer->buffer[1] != 0, transfer->buffer[1 + 9] != 0, transfer->buffer[1 + 18] != 0, transfer->buffer[1 + 27] != 0 };
	if (connected != persistent) {
		for (int i = 0; i < 4; i++) {
			std::cout << (connected[i] ? "CONN" : "DISC") << " ";
		}
		std::cout << "\n";
		persistent = connected;
	}
#endif
	free(transfer->buffer);
	libusb_free_transfer(transfer);

	uint8_t* newTransferBuffer = (uint8_t*)malloc(37);
	libusb_transfer* newTransfer = libusb_alloc_transfer(0); // freed in callback
	libusb_fill_interrupt_transfer(newTransfer, handle, 129, newTransferBuffer, 37, INCallback, 0, 0);
	libusb_submit_transfer(newTransfer);
};

const bool makeRumble = false;

int main()
{
#pragma region init
	SetThreadPriority(GetCurrentThread(), 15);

	libusb_init(NULL);

	libusb_device** list;
	libusb_device* found = NULL;
	ssize_t cnt = libusb_get_device_list(NULL, &list);

	for (size_t idx = 0; idx < cnt; ++idx)
	{
		libusb_device* device = list[idx];
		libusb_device_descriptor desc = { 0 };
		//libusb_device_handle* handle = NULL;

		libusb_get_device_descriptor(device, &desc);

		std::cout << "idVendor  " << desc.idVendor << "\t";
		std::cout << "idProduct " << desc.idProduct << std::endl;

		if (desc.idVendor == 0x057E && desc.idProduct == 0x0337) {
			std::cout << "Attempt to get WUP-028 handle" << std::endl;
			int errorHandle = libusb_open(device, &handle);
			std::cout << errorHandle << " - " << libusb_error_name(errorHandle) << std::endl;
			if (errorHandle) return 0;
			libusb_claim_interface(handle, 0);
		}

	}

	if (!handle) return 1;
#pragma endregion

	__time64_t long_time;
	_time64(&long_time);
	struct tm newtime;
	_localtime64_s(&newtime, &long_time);

	std::ostringstream fileNameStream;
	fileNameStream << "Adapter log ";
	fileNameStream << std::put_time(&newtime, "%Y-%m-%d %H-%M-%S");
	fileNameStream << ".txt";

	std::string fileName = fileNameStream.str();

	output_file.open(fileName, std::ios_base::out);

	#if MAKE_RUMBLE
	std::array<uint8_t, 5> rumble = { 0x11, 2, 2, 2, 2 };
	int al;
	libusb_interrupt_transfer(handle, 2, rumble.data(), 5, &al, 0);
	#endif

	// Setup IN packet reads
	{
		uint8_t* newTransferBuffer = (uint8_t*)malloc(37);
		libusb_transfer* newTransfer = libusb_alloc_transfer(0); // freed in callback
		libusb_fill_interrupt_transfer(newTransfer, handle, 129, newTransferBuffer, 37, INCallback, 0, 0);
		libusb_submit_transfer(newTransfer);
	}

	std::array<uint8_t, 5> makeRumble = { 0x11, 2, 2, 2, 2 };
	std::array<uint8_t, 5> makeNotRumble = { 0x11, 0, 0, 0, 0 };
	std::chrono::time_point tp = std::chrono::steady_clock::now();
	bool rumble = false;

	while (true) {
		libusb_handle_events(NULL);
		if (std::chrono::steady_clock::now() > tp + 400ms) {
			rumble = !rumble;
			tp = std::chrono::steady_clock::now();
			int al;
			libusb_interrupt_transfer(handle, 2, rumble ? makeRumble.data() : makeNotRumble.data(), 5, &al, 0);
		}
	}
}
