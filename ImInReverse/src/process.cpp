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
				std::lock_guard<std::mutex> Lock(manager->processMtx);
				if (wcscmp(vtClass.bstrVal, L"__InstanceCreationEvent") == 0) {
					manager->processes.push_back(p);
					spdlog::debug("Process created: {} (PID: {})", p.name, p.pid);
				} else if (wcscmp(vtClass.bstrVal, L"__InstanceDeletionEvent") == 0) {
					manager->processes.erase(std::remove_if(manager->processes.begin(), manager->processes.end(), [&](const Process& proc) {
						return proc.pid == p.pid;
					}), manager->processes.end());
					spdlog::debug("Process exited: {} (PID: {})", p.name, p.pid);

					// if it was the selected process, close it and clear the selection
					if (manager->selectedProcess && manager->selectedProcess->pid == p.pid) {
						spdlog::info("Selected process exited, closing driver");

						manager->CloseProcess();
						manager->selectedProcess = std::nullopt;
					}
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
            std::lock_guard<std::mutex> Lock(processMtx);
            processes.push_back(p);
			spdlog::debug("Process created: {} (PID: {})", p.name, p.pid);
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
	
	try {
		driver = std::make_unique<DriverManager>("colonelLink");
		
		if (!driver->isValid()) {
			spdlog::error("Failed to initialize driver");
			driver.reset();
			return false;
		}
		
		// Store the selected process first
		{
			std::lock_guard<std::mutex> Lock(processMtx);
			auto it = std::find_if(processes.begin(), processes.end(), [pid](const Process& p) {
				return p.pid == pid;
			});
			if (it != processes.end()) {
				selectedProcess = *it;
			}
			else {
				selectedProcess = std::nullopt;
			}
		}
		
		if (!selectedProcess) {
			spdlog::error("Process with PID {} not found", pid);
			driver.reset();
			return false;
		}
		
		// Attach to process using driver (kernel-mode base address)
		uintptr_t kmBase = 0;
		auto status = driver->attachToProcess(selectedProcess->name, false, &kmBase);
		if (status != DriverStatus::Success) {
			spdlog::error("Failed to attach to process '{}': {}", 
				selectedProcess->name, DriverException::getStatusMessage(status));
			driver.reset();
			baseAddress = 0;
			selectedProcess = std::nullopt;
			return false;
		}

		baseAddress = kmBase;
		spdlog::info("Kernel base for '{}' = 0x{:X}", selectedProcess->name, baseAddress);
		
		spdlog::debug("Attached to process '{}' with PID: {}", selectedProcess->name, pid);
		return true;
	}
	catch (const DriverException& e) {
		spdlog::error("Driver exception: {}", e.what());
		driver.reset();
		selectedProcess = std::nullopt;
		return false;
	}
}

void ProcessManager::CloseProcess() {
	if (driver) {
		driver.reset();
		baseAddress = 0;
		selectedProcess = std::nullopt;
	}
}

const std::vector<Process>& ProcessManager::GetProcesses() {
	std::lock_guard<std::mutex> Lock(processMtx);
	return processes;
}


