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
		FieldType fieldType = FieldType::unk;
		size_t offset = 0;
		int size = 8; // Size of field in bytes.
		char name[32] = "unnamed";

		//std::string display = ""; TODO: Caching
	};

	struct Structure {
		std::vector<Field> fields = {
			{ FieldType::unk, 0, 8 },
			{ FieldType::unk, 8, 8 },
			{ FieldType::unk, 16, 8 },
			{ FieldType::unk, 24, 8 },
			{ FieldType::unk, 32, 8 },
			{ FieldType::unk, 40, 8 },
			{ FieldType::unk, 48, 8 },
			{ FieldType::unk, 56, 8 }
		};
	};

	class StructureManager {
	public:
		static StructureManager& GetInstance() {
			static StructureManager instance;
			return instance;
		}

		void Init() {
			mem.resize(size);

			this->running = true;
			this->hUpdateThread = std::thread(&StructureManager::UpdateFunction, this);
		}

		// Standard mutex lock/unlock functions
		void Lock() {
			memMutex.lock();
		}

		void Unlock() {
			memMutex.unlock();
		}

		// Returns reference to mutex for use with std::lock_guard
		std::mutex& getMutex() {
			return memMutex;
		}

		void SetBase(uintptr_t newBase) { this->baseAddr = newBase; }
		uintptr_t GetBase() { return baseAddr; }
		void SetName(std::string_view newName) { this->name = newName; }
		std::string& GetName() { return this->name; }
		size_t GetSize() { return this->size; }

		MemoryData* GetFieldData(const Field& field) {
			if (field.offset + field.size > mem.size()) return nullptr;
			return reinterpret_cast<MemoryData*>(mem.data() + field.offset);
		}

		std::vector<Field>& GetFields() {
			return this->currentStructure.fields;
		}

		/// Adds a new field at the end of the structure with the specified size and optional type.
		void AddBytes(int byteCount, FieldType type = FieldType::unk, int fieldSize = 8) {
			if (byteCount <= 0 || fieldSize <= 0) return;

			auto& fields = currentStructure.fields;

			// Calculate where to start adding fields
			size_t offset = 0;
			if (!fields.empty()) {
				const auto& last = fields.back();
				offset = last.offset + last.size;
			}

			while (byteCount >= fieldSize) {
				fields.push_back(Field{ type, offset, fieldSize });
				offset += fieldSize;
				byteCount -= fieldSize;
			}

			// Add a final field for any leftover bytes
			if (byteCount > 0) {
				fields.push_back(Field{ type, offset, byteCount });
				offset += byteCount;
			}

			std::lock_guard<std::mutex> Lock(memMutex);
			size = offset;
			mem.resize(size);
		}

		/// Removes the last N bytes from the structure, potentially trimming/removing fields.
		void RemoveBytes(int byteCount) {
			if (byteCount <= 0 || currentStructure.fields.empty()) return;

			int remaining = byteCount;

			auto& fields = currentStructure.fields;
			while (remaining > 0 && !fields.empty()) {
				Field& last = fields.back();

				if (remaining >= last.size) {
					// Remove whole field
					remaining -= last.size;
					fields.pop_back();
				}
				else {
					// Shrink the field
					last.size -= remaining;
					remaining = 0;
				}
			}

			// Resize memory
			std::lock_guard<std::mutex> Lock(memMutex);
			size -= byteCount - remaining; // Subtract what we *actually* removed
			mem.resize(size);
		}

		/// <summary>
		/// Creates a field of the specified size at the position of the selected field,
		/// rearranging all affected fields as needed. Will split existing fields and
		/// create new field boundaries to accommodate the target size.
		/// </summary>
		/// <param name="field">The field to operate from.</param>
		/// <param name="targetSize">The target size for the new field.</param>
		/// <returns>True if the operation was successful, false otherwise.</returns>
		bool JoinOrSplit(const Field& field, int targetSize) {
			if (targetSize <= 0) {
				spdlog::error("JoinOrSplit failed: Target size must be greater than 0");
				return false;
			}

			auto& fields = currentStructure.fields;
			auto it = std::find_if(fields.begin(), fields.end(),
				[&field](const Field& f) { return &f == &field || (f.offset == field.offset && f.size == field.size); });

			if (it == fields.end()) {
				spdlog::error("JoinOrSplit failed: Field not found in current structure");
				return false;
			}

			// The starting offset where we want the new field
			size_t startOffset = field.offset;
			// The ending offset of the new field we want to create
			size_t endOffset = startOffset + targetSize;

			// Step 1: Find all fields that overlap with our target region
			std::vector<Field*> overlappingFields;
			for (auto& f : fields) {
				if (f.offset < endOffset && f.offset + f.size > startOffset) {
					overlappingFields.push_back(&f);
				}
			}

			// Step 2: Create a copy of the fields vector to work with
			std::vector<Field> newFields = fields;

			// Step 3: Remove all overlapping fields from our working copy
			for (auto* f : overlappingFields) {
				auto removeIt = std::find_if(newFields.begin(), newFields.end(),
					[f](const Field& field) { return field.offset == f->offset && field.size == f->size; });
				if (removeIt != newFields.end()) {
					newFields.erase(removeIt);
				}
			}

			// Step 4: Handle any fields that need to be split at the start boundary
			for (auto* f : overlappingFields) {
				if (f->offset < startOffset && f->offset + f->size > startOffset) {
					// Need to create a field for the part before our new field
					int beforeSize = startOffset - f->offset;
					if (beforeSize > 0) {
						newFields.push_back(Field{ f->fieldType, f->offset, beforeSize });
					}
				}
			}

			// Step 5: Add our new field of the target size at the desired position
			newFields.push_back(Field{ FieldType::unk, startOffset, targetSize });

			// Step 6: Handle any fields that need to be split at the end boundary
			for (auto* f : overlappingFields) {
				if (f->offset < endOffset && f->offset + f->size > endOffset) {
					// Need to create a field for the part after our new field
					int afterSize = (f->offset + f->size) - endOffset;
					if (afterSize > 0) {
						newFields.push_back(Field{ f->fieldType, endOffset, afterSize });
					}
				}
			}

			// Step 7: Sort the new fields by offset to maintain the proper order
			std::sort(newFields.begin(), newFields.end(),
				[](const Field& a, const Field& b) { return a.offset < b.offset; });

			// Step 8: Replace the fields in the structure with our modified set
			fields = newFields;

			{
				std::lock_guard<std::mutex> Lock(memMutex);
				size = CalcTotalSize();
				mem.resize(size);
			}

			spdlog::debug("JoinOrSplit success: Created field of size {} at offset {:#x}",
				targetSize, startOffset);
			return true;
		}

		/// <summary>
		/// Splits a field into multiple subfields of the specified size.
		/// The original field is removed and replaced by new fields with unk type.
		/// </summary>
		/// <param name="field">The field to split.</param>
		/// <param name="splitSize">The size of each new subfield (in bytes).</param>
		/// <returns>True if the split was successful, false otherwise.</returns>
		bool SplitField(const Field& field, int splitSize) {
			if (splitSize <= 0) {
				spdlog::error("Split failed: Split size must be greater than 0");
				return false;
			}

			auto& fields = currentStructure.fields;
			auto it = std::find_if(fields.begin(), fields.end(),
				[&field](const Field& f) { return &f == &field || (f.offset == field.offset && f.size == field.size); });

			if (it == fields.end()) {
				spdlog::error("Split failed: Field not found in current structure");
				return false;
			}

			if (field.size <= splitSize) {
				spdlog::error("Split not performed: Field size <= split size");
				return false;
			}

			size_t fieldIndex = std::distance(fields.begin(), it);
			std::vector<Field> newFields;

			size_t numSplits = field.size / splitSize;
			size_t remaining = field.size % splitSize;
			size_t offset = field.offset;

			// Create new fields of unk type
			for (size_t i = 0; i < numSplits; ++i) {
				newFields.push_back(Field{ FieldType::unk, offset, splitSize });
				offset += splitSize;
			}
			if (remaining > 0) {
				newFields.push_back(Field{ FieldType::unk, offset, static_cast<int>(remaining) });
			}

			// Replace the old field with new fields
			fields.erase(it);
			fields.insert(fields.begin() + fieldIndex, newFields.begin(), newFields.end());

			{
				std::lock_guard<std::mutex> Lock(memMutex);
				size = CalcTotalSize();
				mem.resize(size);
			}

			spdlog::debug("Split successful: Field split into {} parts", newFields.size());
			return true;
		}

		/// <summary>
		/// Joins contiguous fields into a single field regardless of type.
		/// The resulting field will have unk type.
		/// </summary>
		/// <param name="field">The field to join with adjacent fields.</param>
		/// <param name="numFields">Number of contiguous fields to join (including the given field).</param>
		/// <returns>True if join was successful, false otherwise.</returns>
		bool JoinFields(const Field& field, size_t numFields = 2) {
			if (numFields < 2) {
				spdlog::error("Join failed: Must join at least 2 fields");
				return false;
			}

			auto& fields = currentStructure.fields;
			auto it = std::find_if(fields.begin(), fields.end(),
				[&field](const Field& f) { return &f == &field || (f.offset == field.offset && f.size == field.size); });

			if (it == fields.end()) {
				spdlog::error("Join failed: Field not found in current structure");
				return false;
			}

			size_t startIdx = std::distance(fields.begin(), it);

			// Ensure there are enough fields after startIdx
			if (startIdx + numFields > fields.size()) {
				spdlog::error("Join failed: Not enough fields to join");
				return false;
			}

			// Check that fields are contiguous - ignore types completely
			size_t expectedOffset = fields[startIdx].offset;
			for (size_t i = 0; i < numFields; ++i) {
				if (fields[startIdx + i].offset != expectedOffset) {
					spdlog::error("Join failed: Fields are not contiguous");
					return false;
				}
				expectedOffset += fields[startIdx + i].size;
			}

			// Compute properties for the new joined field - always unk type
			size_t offset = fields[startIdx].offset;
			int totalSize = 0;
			for (size_t i = 0; i < numFields; ++i)
				totalSize += fields[startIdx + i].size;

			Field joinedField{ FieldType::unk, offset, totalSize };

			// Replace the old fields with the new joined field
			fields.erase(fields.begin() + startIdx, fields.begin() + startIdx + numFields);
			fields.insert(fields.begin() + startIdx, joinedField);

			{
				std::lock_guard<std::mutex> Lock(memMutex);
				size = CalcTotalSize();
				mem.resize(size);
			}

			spdlog::debug("Join successful: {} fields joined", numFields);
			return true;
		}

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

		Structure currentStructure;

		uintptr_t baseAddr = 0;
		std::string name = "unnamed";
		size_t size = 64;

		std::vector<uint8_t> mem;
		std::mutex memMutex;

		void UpdateFunction() {
			auto& pm = IIR::ProcessManager::GetInstance();

			auto frequencyStartTime = std::chrono::steady_clock::now();
			uint64_t iterationCount = 0;
			const int reportIntervalSec = 5;

			while (this->running) {
				iterationCount++;
				auto loopStartTime = std::chrono::steady_clock::now();

				// Get process handle
				auto handle = pm.GetHandle();
				if (handle == nullptr) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				this->Lock();
				SIZE_T sizeRead = 0;
				BOOL success = ReadProcessMemory(
					handle,
					reinterpret_cast<LPCVOID>(baseAddr),
					this->mem.data(),
					size,
					&sizeRead
				);
				this->Unlock();

				if (!success || sizeRead != size) {
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
					std::this_thread::sleep_for(std::chrono::seconds(1));
					continue;
				}

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

		// Helper function to get current time string
		std::string getCurrentTimeString() {
			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);

			// use localtime_s
			tm localTime;
			localtime_s(&localTime, &time);
			std::ostringstream ss;
			ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
			ss << "." << std::setfill('0') << std::setw(3) << (std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);

			return ss.str();
		}

		size_t CalcTotalSize() const {
			if (currentStructure.fields.empty()) return 0;
			const auto& last = currentStructure.fields.back();
			return last.offset + last.size;
		}

		std::thread hUpdateThread;
		std::atomic<bool> running = false;
	};
}