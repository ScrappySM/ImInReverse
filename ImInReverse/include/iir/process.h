// process.h
#pragma once

#include <Windows.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <optional>

#include "driver.h"

#pragma comment(lib, "wbemuuid.lib")

namespace IIR {
	struct Process {
		std::string name;
		DWORD pid;
	};

	class ProcessManager {
	public:
		static ProcessManager& GetInstance();

		const std::vector<Process>& GetProcesses();

		bool OpenProcess(DWORD pid);
		bool OpenProcess(const Process& process) { return OpenProcess(process.pid); }
		std::optional<Process>& GetSelectedProcess() { return selectedProcess; }
		DriverManager* GetDriver() { return driver.get(); }
		uintptr_t GetBaseAddress() const { return baseAddress; }

		void CloseProcess();

		void Init() {
			InitCOM();
			StartListening();
		}

	private:
		ProcessManager();
		~ProcessManager();
		ProcessManager(const ProcessManager&) = delete;
		ProcessManager& operator=(const ProcessManager&) = delete;

		void InitCOM();
		void StartListening();
		void PerformInitialScan();

		std::unique_ptr<DriverManager> driver = nullptr;
		uintptr_t baseAddress = 0;
		std::optional<Process> selectedProcess = std::nullopt;

		IWbemLocator* pLoc = nullptr;
		IWbemServices* pSvc = nullptr;
		IWbemObjectSink* sink = nullptr;

		std::mutex processMtx;
		std::vector<Process> processes;

		class EventSink;
	};
}
