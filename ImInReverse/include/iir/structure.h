#pragma once

#include "pch.h"

#include "iir/process.h"

namespace IIR {
	// union for easily reading memory as a bunch of different types
	union MemoryData {
		uint8_t u8;
		uint16_t u16;
		uint32_t u32;
		uint64_t u64;
		int8_t i8;
		int16_t i16;
		int32_t i32;
		int64_t i64;
		float f32;
		double f64;
		const char* str;
		void* ptr;
		DirectX::XMFLOAT2 vec2;
		DirectX::XMFLOAT3 vec3;
		bool boolean;
	};

	enum class FieldType {
		u8,
		u16,
		u32,
		u64,
		i8,
		i16,
		i32,
		i64,
		f32,
		f64,
		str,
		ptr,
		boolean,
		vec2,
		vec3,
		unk
	};

	/// <summary>
	/// The field type is simply used to index into the main type. It is not useful by itself as it does not contain the data of the memory.
	/// </summary>
	struct Field {
		Field(FieldType fieldType, size_t offset, int size)
			: fieldType(fieldType), offset(offset), size(size) {
			strncpy_s(name, sizeof(name), "unnamed", _TRUNCATE);
		}

		FieldType fieldType = FieldType::unk;
		size_t offset = 0;
		int size = 8; // Size of field in bytes.
		char name[32] = "unnamed";

		//std::string display = ""; TODO: Caching
	};

	class Structure {
	public:
		std::vector<Field> fields = {
			{ FieldType::unk, 0x00, 8 },
			{ FieldType::unk, 0x08, 8 },
			{ FieldType::unk, 0x10, 8 },
			{ FieldType::unk, 0x18, 8 },
			{ FieldType::unk, 0x20, 8 },
			{ FieldType::unk, 0x28, 8 },
			{ FieldType::unk, 0x30, 8 },
			{ FieldType::unk, 0x38, 8 }
		};

		std::vector<uint8_t> mem;
		uintptr_t baseAddr = 0;
		size_t size = 64;

		Structure() {
			this->mem.resize(size);
		}

        MemoryData* GetFieldData(const Field& field) const {  
           if (field.offset + field.size > mem.size()) return nullptr;  
           return reinterpret_cast<MemoryData*>(const_cast<uint8_t*>(mem.data()) + field.offset);  
		}

		void SplitField(size_t index, int targetBytes) {
			while (index < this->fields.size() && this->fields[index].size > targetBytes && this->fields[index].size > 1) {
				this->HalfField(index);
			}
		}

		void JoinField(size_t index, int targetBytes) {
			assert(targetBytes <= 8 && "You cannot join more than 8 bytes.");

			auto& field = this->fields.at(index);
			auto& nextField = this->fields.at(index + 1);

			for (int remainingBytes = targetBytes - field.size; remainingBytes > 0; remainingBytes = targetBytes - field.size) {
				if (remainingBytes >= nextField.size) {
					this->MergeConcurrentFields(index);
				}
				else {
					this->HalfField(index + 1);
				}

					// Update the references
				field = this->fields.at(index);
				nextField = this->fields.at(index + 1);
			}
		}

		void ResizeField(size_t index, int targetBytes) {
			auto& field = this->fields.at(index);

			if (field.size == targetBytes) return;
			if (field.size > targetBytes) this->SplitField(index, targetBytes);
			if (field.size < targetBytes) this->JoinField(index, targetBytes);
		}

		void AddFields(int amount) {
			auto& lastField = this->fields.back();
			size_t firstOffset = lastField.offset + (size_t)lastField.size;

			for (int i = 0; i < amount; ++i) {
				this->fields.emplace_back(FieldType::unk, firstOffset + (i * 8), 8);
			}

			this->size += amount * 8;
			this->mem.resize(this->size);
		}

	private:
		void HalfField(size_t index) {
			auto& field = this->fields.at(index);
			assert(field.size > 1 && "Cannot half a field of size 1");
			field.fieldType = FieldType::unk;
			auto halfSize = field.size / 2;
			field.size = halfSize;
			this->fields.emplace(this->fields.begin() + index + 1, FieldType::unk, field.offset + halfSize, halfSize);
		}

		int MergeConcurrentFields(size_t index) {
			auto& currentField = this->fields.at(index);
			auto& nextField = this->fields.at(index + 1);

			currentField.fieldType = FieldType::unk;
			currentField.size += nextField.size;
			int nextSize = nextField.size;

			this->fields.erase(this->fields.begin() + index + 1);
			return nextSize;
		}
	};

	class StructureManager {
	public:
		static StructureManager& GetInstance() {
			static StructureManager instance;
			return instance;
		}

		void Init() {
			this->running = true;
			this->hUpdateThread = std::thread(&StructureManager::UpdateFunction, this);
		}

		void Lock() { memMutex.lock(); }
		void Unlock() { memMutex.unlock(); }

		std::unordered_map<std::string, Structure> structures = { { "Unnamed", {} } };
		std::pair<std::string, Structure> structure = *structures.begin();

	private:
		StructureManager() = default;
		~StructureManager() {
			this->running = false;
			if (this->hUpdateThread.joinable()) {
				this->hUpdateThread.join();
			}
		}

		StructureManager(const StructureManager&) = delete;
		StructureManager& operator=(const StructureManager&) = delete;

		std::mutex memMutex;

		void UpdateFunction() {
			auto& pm = IIR::ProcessManager::GetInstance();

			auto frequencyStartTime = std::chrono::steady_clock::now();
			uint64_t iterationCount = 0;
			const int reportIntervalSec = 5;

			while (this->running) {
				bool shouldSleep = false;

				iterationCount++;
				auto loopStartTime = std::chrono::steady_clock::now();

				// Get process handle
				auto handle = pm.GetHandle();
				if (handle == nullptr) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				this->Lock();
				for (auto& [k, structure] : this->structures) {
					SIZE_T sizeRead = 0;
					BOOL success = ReadProcessMemory(
						handle,
						reinterpret_cast<LPCVOID>(structure.baseAddr),
						this->structure.second.mem.data(),
						structure.size,
						&sizeRead
					);

					if (!success || sizeRead != structure.size) {
						DWORD errorCode = GetLastError();

						// Format the error code as a string
						LPVOID lpMsgBuf;
						FormatMessage(
							FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
							NULL,
							errorCode,
							MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
							(LPTSTR)&lpMsgBuf,
							0, NULL);
						std::string errorMsg = static_cast<char*>(lpMsgBuf);
						LocalFree(lpMsgBuf);

						spdlog::error("Failed to read memory: {}", errorMsg);
						shouldSleep = true;
						continue;
					}
				}
				this->Unlock();

				if (shouldSleep)
					std::this_thread::sleep_for(std::chrono::seconds(1));

				auto now = std::chrono::steady_clock::now();
				auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - frequencyStartTime).count();

				if (elapsedSec >= reportIntervalSec) {
					double frequency = static_cast<double>(iterationCount) / elapsedSec;
					double avgIterationTimeMs = (elapsedSec * 1000.0) / iterationCount;

					spdlog::info("Updating {:.2f} iterations/sec", frequency);

					// Reset counters
					frequencyStartTime = now;
					iterationCount = 0;
				}

				std::this_thread::sleep_for(std::chrono::nanoseconds(10));
			}
		}

		std::thread hUpdateThread;
		std::atomic<bool> running = false;
	};
}