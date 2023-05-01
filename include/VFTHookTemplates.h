#pragma once

#include <mutex>
#include <memory>
#include <cstdint>

namespace HookTemplate {
	struct HookData {
		HookData() {}

		const unsigned long long magic = 0x6B6F6F48544656ull; // magic: "VFTHook\0"
		uint64_t : 64;
		std::shared_ptr<std::mutex> mutex = nullptr;
		void* previous = nullptr;
		void* fnNew = nullptr;
		void* fnHooked = nullptr;
		void* extra = nullptr;
	};

	struct Base {
		Base() {}

		//data:
		HookData hookData{};

		// assembly:
		unsigned char asmRaw[114] = {
		0x48, 0x8D, 0x44, 0x24, 0xA0,       // lea    rax,[rsp-0x60]
		0x24, 0xF0,                   		// and    al,0xF0
		0x0F, 0x29, 0x40, 0x50,             // movaps [rax+0x50],xmm0
		0x0F, 0x29, 0x48, 0x40,             // movaps [rax+0x40],xmm1
		0x0F, 0x29, 0x50, 0x30,             // movaps [rax+0x30],xmm2
		0x0F, 0x29, 0x58, 0x20,             // movaps [rax+0x20],xmm3
		0x0F, 0x29, 0x60, 0x10,             // movaps [rax+0x10],xmm4
		0x0F, 0x29, 0x28,                	// movaps [rax],xmm5
		0x48, 0x89, 0x60, 0xF0,             // mov    [rax-0x10],rsp
		0x48, 0x89, 0x48, 0xE8,             // mov    [rax-0x18],rcx
		0x48, 0x89, 0x50, 0xE0,             // mov    [rax-0x20],rdx
		0x4C, 0x89, 0x40, 0xD8,             // mov    [rax-0x28],r8
		0x4C, 0x89, 0x48, 0xD0,             // mov    [rax-0x30],r9
		0x48, 0x8D, 0x60, 0xB0,             // lea    rsp,[rax-0x50]
		0xFF, 0x15, 0xAC, 0xFF, 0xFF, 0xFF, // call   [fnNew]
		0x48, 0x8D, 0x44, 0x24, 0x50,       // lea    rax,[rsp+0x50]
		0x4C, 0x8B, 0x48, 0xD0,             // mov    r9,[rax-0x30]
		0x4C, 0x8B, 0x40, 0xD8,             // mov    r8,[rax-0x28]
		0x48, 0x8B, 0x50, 0xE0,             // mov    rdx,[rax-0x20]
		0x48, 0x8B, 0x48, 0xE8,             // mov    rcx,[rax-0x18]
		0x0F, 0x28, 0x28,                	// movaps xmm5,[rax]
		0x0F, 0x28, 0x60, 0x10,             // movaps xmm4,[rax+0x10]
		0x0F, 0x28, 0x58, 0x20,             // movaps xmm3,[rax+0x20]
		0x0F, 0x28, 0x50, 0x30,             // movaps xmm2,[rax+0x30]
		0x0F, 0x28, 0x48, 0x40,             // movaps xmm1,[rax+0x40]
		0x0F, 0x28, 0x40, 0x50,             // movaps xmm0,[rax+0x50]
		0x48, 0x8B, 0x60, 0xF0,             // mov    rsp,[rax-0x10]
		0xFF, 0x25, 0x7E, 0xFF, 0xFF, 0xFF, // jmp    [fnHooked]
		};
	};

	struct Return {
		Return() {}

		//data:
		HookData hookData{};

		// assembly:
		unsigned char asmRaw[125] = {
		0x51,								// push   rcx
		0x48, 0x83, 0xEC, 0x20,				// sub    rsp,20
		0xFF, 0x15, 0xE5, 0xFF, 0xFF, 0xFF,	// call   [fnHooked]
		0x48, 0x83, 0xC4, 0x20,				// sub    add,20
		0x59,								// pop    rcx
		0x48, 0x8D, 0x44, 0x24, 0xA0,		// lea    rax,[rsp-0x60]
		0x24, 0xF0,                   		// and    al,0xF0
		0x0F, 0x29, 0x40, 0x50,             // movaps [rax+0x50],xmm0
		0x0F, 0x29, 0x48, 0x40,             // movaps [rax+0x40],xmm1
		0x0F, 0x29, 0x50, 0x30,             // movaps [rax+0x30],xmm2
		0x0F, 0x29, 0x58, 0x20,             // movaps [rax+0x20],xmm3
		0x0F, 0x29, 0x60, 0x10,             // movaps [rax+0x10],xmm4
		0x0F, 0x29, 0x28,                	// movaps [rax],xmm5
		0x48, 0x89, 0x60, 0xF0,             // mov    [rax-0x10],rsp
		0x48, 0x89, 0x48, 0xE8,             // mov    [rax-0x18],rcx
		0x48, 0x89, 0x50, 0xE0,             // mov    [rax-0x20],rdx
		0x4C, 0x89, 0x40, 0xD8,             // mov    [rax-0x28],r8
		0x4C, 0x89, 0x48, 0xD0,             // mov    [rax-0x30],r9
		0x48, 0x8D, 0x60, 0xB0,             // lea    rsp,[rax-0x50]
		0xFF, 0x15, 0x9C, 0xFF, 0xFF, 0xFF, // call   [fnNew]
		0x48, 0x8D, 0x44, 0x24, 0x50,       // lea    rax,[rsp+0x50]
		0x4C, 0x8B, 0x48, 0xD0,             // mov    r9,[rax-0x30]
		0x4C, 0x8B, 0x40, 0xD8,             // mov    r8,[rax-0x28]
		0x48, 0x8B, 0x50, 0xE0,             // mov    rdx,[rax-0x20]
		0x48, 0x8B, 0x48, 0xE8,             // mov    rcx,[rax-0x18]
		0x0F, 0x28, 0x28,                	// movaps xmm5,[rax]
		0x0F, 0x28, 0x60, 0x10,             // movaps xmm4,[rax+0x10]
		0x0F, 0x28, 0x58, 0x20,             // movaps xmm3,[rax+0x20]
		0x0F, 0x28, 0x50, 0x30,             // movaps xmm2,[rax+0x30]
		0x0F, 0x28, 0x48, 0x40,             // movaps xmm1,[rax+0x40]
		0x0F, 0x28, 0x40, 0x50,             // movaps xmm0,[rax+0x50]
		0x48, 0x8B, 0x60, 0xF0,             // mov    rsp,[rax-0x10]
		0xC3,								// ret
		};
	};
}