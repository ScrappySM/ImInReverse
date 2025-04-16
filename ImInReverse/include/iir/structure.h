#pragma once

#include "pch.h"

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

	class StructureManager {
	public:
		static StructureManager& GetInstance() {
			static StructureManager instance;
			return instance;
		}

	private:
		StructureManager() = default;
		~StructureManager() = default;
		StructureManager(const StructureManager&) = delete;
		StructureManager& operator=(const StructureManager&) = delete;
	};
}
