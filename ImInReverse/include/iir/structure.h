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
		unk
	};

	/// <summary>
	/// The field type is simply used to index into the main type. It is not useful by itself as it does not contain the data of the memory.
	/// </summary>
	struct Field {
		FieldType fieldType = FieldType::unk;
		size_t offset = 0;
		int size = 8; // Size of field in bytes.
		//std::string display = ""; TODO: Caching
	};

	struct Structure {
		std::vector<Field> fields = {
			{ FieldType::unk, 0, 8 },
			{ FieldType::unk, 8, 8 },
			{ FieldType::unk, 16, 8 },
			{ FieldType::unk, 24, 8 }
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

		void SetBase(uintptr_t newBase) { this->baseAddr = newBase; }
		uintptr_t GetBase() { return baseAddr; }
		void SetName(std::string_view newName) { this->name = newName; }
		std::string& GetName() { return this->name; }
		size_t GetSize() { return this->size; }

		MemoryData* GetFieldData(const Field& field) {
			if (field.offset + sizeof(MemoryData) > mem.size()) return nullptr;
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
			size -= byteCount - remaining; // Subtract what we *actually* removed
			mem.resize(size);
		}

		/// <summary>
/// Splits a field into multiple subfields of the specified size.
/// The original field is removed and replaced by new fields.
/// </summary>
/// <param name="field">The field to split.</param>
/// <param name="splitSize">The size of each new subfield (in bytes).</param>
/// <returns>True if the split was successful, false otherwise.</returns>
		bool SplitField(const Field& field, int splitSize) {
			if (splitSize <= 0) return false;

			auto& fields = currentStructure.fields;
			auto it = std::find_if(fields.begin(), fields.end(),
				[&field](const Field& f) { return &f == &field || (f.offset == field.offset && f.size == field.size); });

			if (it == fields.end()) return false;

			size_t fieldIndex = std::distance(fields.begin(), it);
			FieldType typeToUse = field.fieldType;
			std::vector<Field> newFields;

			size_t numSplits = field.size / splitSize;
			size_t remaining = field.size % splitSize;
			size_t offset = field.offset;

			for (size_t i = 0; i < numSplits; ++i) {
				newFields.push_back(Field{ typeToUse, offset, splitSize });
				offset += splitSize;
			}
			if (remaining > 0) {
				newFields.push_back(Field{ typeToUse, offset, static_cast<int>(remaining) });
			}

			// Replace the old field with new fields
			fields.erase(it);
			fields.insert(fields.begin() + fieldIndex, newFields.begin(), newFields.end());

			size = CalcTotalSize();
			mem.resize(size);

			return true;
		}

		/// <summary>
		/// Joins a field with its immediately adjacent fields into a single field.
		/// The fields to join are determined by their offsets and must be contiguous.
		/// </summary>
		/// <param name="field">The field to join with adjacent fields.</param>
		/// <param name="numFields">Number of contiguous fields to join (including the given field).</param>
		/// <returns>True if join was successful, false otherwise.</returns>
		bool JoinFields(const Field& field, size_t numFields = 2) {
			if (numFields < 2) return false;

			auto& fields = currentStructure.fields;
			auto it = std::find_if(fields.begin(), fields.end(),
				[&field](const Field& f) { return &f == &field || (f.offset == field.offset && f.size == field.size); });

			if (it == fields.end()) return false;

			size_t startIdx = std::distance(fields.begin(), it);

			// Ensure there are enough fields after startIdx
			if (startIdx + numFields > fields.size()) return false;

			// Check that fields are contiguous
			size_t expectedOffset = fields[startIdx].offset;
			FieldType typeToUse = fields[startIdx].fieldType;
			for (size_t i = 0; i < numFields; ++i) {
				if (fields[startIdx + i].offset != expectedOffset || fields[startIdx + i].fieldType != typeToUse)
					return false;
				expectedOffset += fields[startIdx + i].size;
			}

			// Compute properties for the new joined field
			size_t offset = fields[startIdx].offset;
			int totalSize = 0;
			for (size_t i = 0; i < numFields; ++i)
				totalSize += fields[startIdx + i].size;

			Field joinedField{ typeToUse, offset, totalSize };

			// Replace the old fields with the new joined field
			fields.erase(fields.begin() + startIdx, fields.begin() + startIdx + numFields);
			fields.insert(fields.begin() + startIdx, joinedField);

			size = CalcTotalSize();
			mem.resize(size);

			return true;
		}

		/// <summary>
		/// If the field is smaller than targetSize, attempts to join with adjacent fields until the size matches targetSize exactly.
		/// If the field is larger than targetSize, splits into multiple fields of targetSize.
		/// Does not change the type.
		/// </summary>
		/// <param name="field">The field to operate on.</param>
		/// <param name="targetSize">The target size to join/split to.</param>
		/// <returns>True if a join or split was performed, false otherwise.</returns>
		bool JoinOrSplit(const Field& field, int targetSize) {
			if (targetSize <= 0) return false;

			auto& fields = currentStructure.fields;
			// Find the selected field
			auto it = std::find_if(fields.begin(), fields.end(),
				[&field](const Field& f) { return &f == &field || (f.offset == field.offset && f.size == field.size); });

			if (it == fields.end())
				return false;

			// If field is bigger than target, just split it
			if (field.size > targetSize)
				return SplitField(field, targetSize);

			// If already target size, do nothing
			if (field.size == targetSize)
				return false;

			// Define the target range exactly where the selected field is
			size_t rangeStart = field.offset;
			size_t rangeEnd = rangeStart + targetSize;

			// Step 1: Split any field that overlaps with our target range but isn't entirely inside it
			std::vector<size_t> fieldsToSplit;
			for (size_t i = 0; i < fields.size(); i++) {
				auto& f = fields[i];
				// Skip the selected field itself
				if (&f == &field) continue;

				// If field partially overlaps our target range, mark it for splitting
				if (f.offset < rangeEnd && (f.offset + f.size) > rangeStart &&
					(f.offset < rangeStart || (f.offset + f.size) > rangeEnd)) {
					fieldsToSplit.push_back(i);
				}
			}

			// Split the overlapping fields - starting from the end to avoid index shifting
			for (auto it = fieldsToSplit.rbegin(); it != fieldsToSplit.rend(); ++it) {
				size_t idx = *it;
				if (idx >= fields.size()) continue; // Safety check

				auto& f = fields[idx];
				// If field starts before range and extends into it
				if (f.offset < rangeStart && f.offset + f.size > rangeStart) {
					SplitField(f, rangeStart - f.offset);
				}
				// Re-fetch fields after potential modification
				// If field starts inside range and extends past it
				for (auto& f : fields) {
					if (f.offset < rangeEnd && f.offset >= rangeStart && f.offset + f.size > rangeEnd) {
						SplitField(f, rangeEnd - f.offset);
						break;
					}
				}
			}

			// Step 2: Find all fields inside the target range
			std::vector<Field*> rangeFields;
			for (auto& f : fields) {
				if (f.offset >= rangeStart && f.offset + f.size <= rangeEnd) {
					rangeFields.push_back(&f);
				}
			}

			// Sort by offset
			std::sort(rangeFields.begin(), rangeFields.end(),
				[](const Field* a, const Field* b) { return a->offset < b->offset; });

			// Step 3: Check they are contiguous and join them
			if (rangeFields.size() > 0) {
				// Check the first field starts at rangeStart
				if (rangeFields[0]->offset != rangeStart)
					return false;

				// Join them
				return JoinFields(*rangeFields[0], rangeFields.size());
			}

			return false;
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

		void UpdateFunction() {
			auto& pm = IIR::ProcessManager::GetInstance();

			while (this->running) {
				auto handle = pm.GetHandle();
				if (handle == nullptr) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				SIZE_T sizeRead = 0;
				BOOL success = ReadProcessMemory(
					handle,
					reinterpret_cast<LPCVOID>(baseAddr),
					this->mem.data(),
					size,
					&sizeRead
				);

				if (!success || sizeRead != size) {
					DWORD errorCode = GetLastError();
					spdlog::error("ReadProcessMemory failed (err: {}) — read {}/{} bytes", errorCode, sizeRead, size);
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
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
