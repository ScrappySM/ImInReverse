#include "pch.h"
#include "iir/process.h"

using namespace IIR;

class ProcessManager::EventSink : public IWbemObjectSink {
	LONG m_lRef;
	ProcessManager* manager;

public:
	EventSink(ProcessManager* mgr) : m_lRef(0), manager(mgr) {}
	virtual ~EventSink() {}

	ULONG STDMETHODCALLTYPE AddRef() override {
		return InterlockedIncrement(&m_lRef);
	}
	ULONG STDMETHODCALLTYPE Release() override {
		LONG lRef = InterlockedDecrement(&m_lRef);
		if (lRef == 0) delete this;
		return lRef;
	}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
		if (riid == IID_IUnknown || riid == IID_IWbemObjectSink) {
			*ppv = static_cast<IWbemObjectSink*>(this);
			AddRef();
			return WBEM_S_NO_ERROR;
		} else {
			*ppv = nullptr;
			return E_NOINTERFACE;
		}
	}
	HRESULT STDMETHODCALLTYPE Indicate(LONG lObjectCount, IWbemClassObject** apObjArray) override {
		for (int i = 0; i < lObjectCount; ++i) {
			VARIANT vtClass;
			apObjArray[i]->Get(L"__Class", 0, &vtClass, nullptr, nullptr);

			VARIANT vtInst;
			apObjArray[i]->Get(L"TargetInstance", 0, &vtInst, nullptr, nullptr);
			IWbemClassObject* pInst = (IWbemClassObject*)vtInst.pdispVal;

			VARIANT vtName, vtPid;
			pInst->Get(L"Name", 0, &vtName, nullptr, nullptr);
			pInst->Get(L"ProcessId", 0, &vtPid, nullptr, nullptr);

			std::wstring wname(vtName.bstrVal);
			std::string name(wname.begin(), wname.end());

			Process p{ name, (DWORD)vtPid.uintVal };

			{
				std::lock_guard<std::mutex> lock(manager->processMtx);
				if (wcscmp(vtClass.bstrVal, L"__InstanceCreationEvent") == 0) {
					manager->processes.push_back(p);
					spdlog::info("[+] Process created: {} (PID: {})", p.name, p.pid);
				} else if (wcscmp(vtClass.bstrVal, L"__InstanceDeletionEvent") == 0) {
					manager->processes.erase(std::remove_if(manager->processes.begin(), manager->processes.end(), [&](const Process& proc) {
						return proc.pid == p.pid;
					}), manager->processes.end());
					spdlog::info("[-] Process exited: {} (PID: {})", p.name, p.pid);

					// if it was the selected process, close it and clear the selection
					if (manager->selectedProcess && manager->selectedProcess->pid == p.pid) {
						manager->CloseProcess();
						manager->selectedProcess = std::nullopt;
						spdlog::info("[-] Closed selected process: {} (PID: {})", p.name, p.pid);
					}

					// TODO: in-app notification about this.
				}
			}

			VariantClear(&vtName);
			VariantClear(&vtPid);
			VariantClear(&vtClass);
			VariantClear(&vtInst);
		}
		return WBEM_S_NO_ERROR;
	}
	HRESULT STDMETHODCALLTYPE SetStatus(LONG, HRESULT, BSTR, IWbemClassObject*) override {
		return WBEM_S_NO_ERROR;
	}
};

ProcessManager::ProcessManager() {
}

ProcessManager::~ProcessManager() {
	if (pSvc) pSvc->Release();
	if (pLoc) pLoc->Release();
	if (sink) sink->Release();
	CoUninitialize();
	CloseProcess();
}

void ProcessManager::InitCOM() {
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) throw std::runtime_error("CoInitializeEx failed");

	hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
		RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
		nullptr, EOAC_NONE, nullptr);
	if (FAILED(hr)) throw std::runtime_error("CoInitializeSecurity failed");

	hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
	if (FAILED(hr)) throw std::runtime_error("CoCreateInstance failed");

	hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, NULL, 0, 0, &pSvc);
	if (FAILED(hr)) throw std::runtime_error("ConnectServer failed");

	hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
		RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
	if (FAILED(hr)) throw std::runtime_error("CoSetProxyBlanket failed");
}

void ProcessManager::StartListening() {
    // Perform initial scan of all processes
    PerformInitialScan();

    // Start listening for events
    sink = new EventSink(this);
    sink->AddRef();

    HRESULT hr = pSvc->ExecNotificationQueryAsync(
        _bstr_t("WQL"),
        _bstr_t("SELECT * FROM __InstanceOperationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'"),
        WBEM_FLAG_SEND_STATUS, nullptr, sink);

    if (FAILED(hr)) throw std::runtime_error("ExecNotificationQueryAsync failed");
}

void ProcessManager::PerformInitialScan() {
    // Query for all processes
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hr = pSvc->ExecQuery(
        _bstr_t("WQL"),
        _bstr_t("SELECT * FROM Win32_Process"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator
    );

    if (FAILED(hr)) {
        throw std::runtime_error("ExecQuery failed for initial scan");
    }

    IWbemClassObject* pClassObject = nullptr;
    ULONG uReturn = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pClassObject, &uReturn);
        if (FAILED(hr) || uReturn == 0) break;

        VARIANT vtName, vtPid;
        pClassObject->Get(L"Name", 0, &vtName, nullptr, nullptr);
        pClassObject->Get(L"ProcessId", 0, &vtPid, nullptr, nullptr);

        std::wstring wname(vtName.bstrVal);
        std::string name(wname.begin(), wname.end());

        Process p{ name, (DWORD)vtPid.uintVal };

        {
            std::lock_guard<std::mutex> lock(processMtx);
            processes.push_back(p);
			spdlog::info("[+] Process created: {} (PID: {})", p.name, p.pid);
        }

        VariantClear(&vtName);
        VariantClear(&vtPid);
        pClassObject->Release();
    }

    if (pEnumerator) pEnumerator->Release();
}


ProcessManager& ProcessManager::GetInstance() {
	static ProcessManager instance;
	return instance;
}

bool ProcessManager::OpenProcess(DWORD pid) {
	CloseProcess();
	processHandle = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (!processHandle) {
		spdlog::error("Failed to open process with PID: {}", pid);
		return false;
	}

	// Store the selected process
	std::lock_guard<std::mutex> lock(processMtx);
	auto it = std::find_if(processes.begin(), processes.end(), [pid](const Process& p) {
		return p.pid == pid;
		});
	if (it != processes.end()) {
		selectedProcess = *it;
	}
	else {
		selectedProcess = std::nullopt;
	}
	spdlog::info("Opened process with PID: {}", pid);

	return processHandle != nullptr;
}

void ProcessManager::CloseProcess() {
	if (processHandle) {
		CloseHandle(processHandle);
		processHandle = nullptr;
		selectedProcess = std::nullopt;
	}
}

const std::vector<Process>& ProcessManager::GetProcesses() {
	std::lock_guard<std::mutex> lock(processMtx);
	return processes;
}

bool ProcessManager::IsProcessSuspended() {
    if (!processHandle) {
        spdlog::error("Invalid process handle");
        return false;
    }

    // Iterate through all threads of the process
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to create snapshot of process threads");
        return false;
    }

    BOOL foundSuspended = false;

    // Loop through the threads
    if (Thread32First(hSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == GetProcessId(processHandle)) {
                HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                if (hThread != NULL) {
                    DWORD suspendCount = SuspendThread(hThread);
                    if (suspendCount == -1) {
                        spdlog::error("Failed to suspend thread {}", te32.th32ThreadID);
                    } else if (suspendCount > 0) {
                        // The thread was suspended
                        foundSuspended = true;
                    }
                    // Resume the thread after checking
                    ResumeThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te32));
    }

    CloseHandle(hSnapshot);

    return foundSuspended;
}

void ProcessManager::SuspendProcess() {
	if (!processHandle) return;

	// Suspend the entire process (processHandle is a handle to the process)
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE) {
		spdlog::error("Failed to create thread snapshot");
		return;
	}

	THREADENTRY32 te;
	te.dwSize = sizeof(THREADENTRY32);
	if (Thread32First(hThreadSnap, &te)) {
		do {
			if (te.th32OwnerProcessID == GetProcessId(processHandle)) {
				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
				if (hThread) {
					SuspendThread(hThread);
					CloseHandle(hThread);
				}
			}
		} while (Thread32Next(hThreadSnap, &te));
	}
	CloseHandle(hThreadSnap);
	spdlog::info("Suspended process with PID: {}", selectedProcess->pid);
}

void ProcessManager::ResumeProcess() {
	if (!processHandle) return;
	// Resume the entire process (processHandle is a handle to the process)
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE) {
		spdlog::error("Failed to create thread snapshot");
		return;
	}
	THREADENTRY32 te;
	te.dwSize = sizeof(THREADENTRY32);
	if (Thread32First(hThreadSnap, &te)) {
		do {
			if (te.th32OwnerProcessID == GetProcessId(processHandle)) {
				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
				if (hThread) {
					ResumeThread(hThread);
					CloseHandle(hThread);
				}
			}
		} while (Thread32Next(hThreadSnap, &te));
	}
	CloseHandle(hThreadSnap);
	spdlog::info("Resumed process with PID: {}", selectedProcess->pid);
}
