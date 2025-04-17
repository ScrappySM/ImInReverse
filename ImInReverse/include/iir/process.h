// process.h
#pragma once

#include <Windows.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

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
		bool IsProcessSuspended();
		HANDLE GetHandle() { return this->processHandle; }

		void SuspendProcess();
		void ResumeProcess();

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

		HANDLE processHandle = nullptr;
		std::optional<Process> selectedProcess = std::nullopt;

		IWbemLocator* pLoc = nullptr;
		IWbemServices* pSvc = nullptr;
		IWbemObjectSink* sink = nullptr;

		std::mutex processMtx;
		std::vector<Process> processes;

		class EventSink;
	};
}
