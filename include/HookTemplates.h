#pragma once

#include <mutex>
#include <array>
#include <memory>
#include <cstdint>
#include <processthreadsapi.h>

#ifdef UNIHOOK_THREAD_ACCESS_LIMIT
#undef UNIHOOK_THREAD_ACCESS_LIMIT
#endif

// The maximum amount of threads that may access a single hook simultaneosly.
// The default value of 32 is already overkill, but it does not hurt performance either.
// It regulates how many context entries are allocated for each hook instance.
#define UNIHOOK_THREAD_ACCESS_LIMIT 32

// A struct for holding the combined context of (most of) the registers.
// It is allocated automatically by hook templates.
struct alignas(16) HookContext {
	using reg64 = int[2];
	using imm256 = float[8];
	reg64 rax;
	reg64 rbx;
	reg64 rcx;
	reg64 rdx;
	reg64 rsp;
	reg64 rbp;
	reg64 rsi;
	reg64 rdi;
	reg64 r8;
	reg64 r9;
	reg64 r10;
	reg64 r11;
	reg64 r12;
	reg64 r13;
	reg64 r14;
	reg64 r15;
	imm256 imm0;
	imm256 imm1;
	imm256 imm2;
	imm256 imm3;
	imm256 imm4;
	imm256 imm5;
	imm256 imm6;
	imm256 imm7;
	imm256 imm8;
	imm256 imm9;
	imm256 imm10;
	imm256 imm11;
	imm256 imm12;
	imm256 imm13;
	imm256 imm14;
	imm256 imm15;
};

// The struct at the beginning of every hook instance.
// It is entirely managed by the hooking system, 
// setting these values yourself will certainly break things.
struct HookBase {

	// The constructor sets up the context pool to be used by the assembly.
	HookBase() {
		this->poolEntryAllocator = std::make_unique<HookContext[]>(UNIHOOK_THREAD_ACCESS_LIMIT); // Allocate UNIHOOK_THREAD_ACCESS_LIMIT of HookContext structs.
		this->poolArrayAllocator = std::make_unique<HookContext * []>(UNIHOOK_THREAD_ACCESS_LIMIT); // Allocate an array of UNIHOOK_THREAD_ACCESS_LIMIT HookContext pointers.
		// Get the HookContext pointers into the respective struct.
		for (int i = 0; i < UNIHOOK_THREAD_ACCESS_LIMIT; ++i) {
			this->poolArrayAllocator.get()[i] = &this->poolEntryAllocator.get()[i];
		}
		// Get the main pool pointer, accessing the std::unique_ptr pointer directly from assembly would be UB.
		// Set up the offset in a way that simplifies the lookup loop and reduces branching.
		this->pool = this->poolArrayAllocator.get() - 1;
	}

	const unsigned long long magic = 0x6B6F6F48696E55ull; // magic: "UniHook\0".
	std::shared_ptr<std::mutex> mutex = nullptr;
	std::unique_ptr<HookContext[]> poolEntryAllocator; // Using a unique_ptr as a pseudoallocator.
	std::unique_ptr<HookContext* []> poolArrayAllocator; // Storing raw pointers to each context entry in the pool in a single array.
	HookContext** pool; // We don't want to access the unique_ptr pointer directly from assembly, doing so would be UB.
	void* previous = nullptr; // Pointer to the previous hook in a chain of hooks.
	void* fnNew = nullptr; // Pointer to the user-defined hooking function.
	void* fnHooked = nullptr; // Pointer to the hooked function.
	void* extra = nullptr; // An extra pointer field, used mostly to store return addresses.
};

// An assembly stub for retrieving a free context structure to use.
// A pointer to the context pool array is expected in r10.
// The pointer to the context structure is returned in rax and saved in r12.
// The old value of r12 is stored in the context.
// If more than UNIHOOK_THREAD_ACCESS_LIMIT threads try to simultaneosly access the hook, the behavior is undefined.
struct alignas(1) HookBorrowContext {
	uint8_t asmRaw[20] = {
		0x31, 0xC0,                   // xor          eax,eax
		0x4D, 0x8D, 0x52, 0x08,       // lea          r10,[r10+8] <- loop
		0xF0, 0x49, 0x0F, 0xB1, 0x02, // lock cmpxchg [r10],rax
		0x74, 0xF5,                   // je           loop
		0x4C, 0x89, 0x60, 0x60,       // mov          [reg64_r12],r12
		0x49, 0x89, 0xC4,             // mov          r12,rax
	};
};

// An assembly stub for returning the context structure to the pool.
// A pointer to the context pool array is expected in r10.
// The pointer to the context structure is expected in r12 as put there by the stub above.
// If more than UNIHOOK_THREAD_ACCESS_LIMIT threads try to simultaneosly access the hook, the behavior is undefined.
struct alignas(1) HookReturnContext {
	uint8_t asmRaw[21] = {
		0x4D, 0x89, 0xE3,             // mov          r11,r12
		0x4D, 0x8B, 0x64, 0x24, 0x60, // mov          r12,[reg64_r12]
		0x4D, 0x8D, 0x52, 0x08,       // lea          r10,[r10+8] <- loop
		0x31, 0xC0,                   // xor          eax,eax
		0xF0, 0x4D, 0x0F, 0xB1, 0x1A, // lock cmpxchg [r10],r11
		0x75, 0xF3,                   // jne          continue
	};
};

// The default hook type.
// The hooking function is executed before the hooked function.
// The default Microsoft x86-64 calling convention is assumed: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
// Use EntryHookV for vectorcall functions.
struct EntryHook {
	EntryHook() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[7] = {
	0x4C, 0x8B, 0x15, 0xD1, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[65] = {
	0x48, 0x89, 0x48, 0x10,                   // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                   // mov    [reg64_rdx],rdx
	0x4C, 0x89, 0x40, 0x40,                   // mov    [reg64_r8],r8 
	0x4C, 0x89, 0x48, 0x48,                   // mov    [reg64_r9],r9
	0x48, 0x8D, 0x05, 0x0E, 0x00, 0x00, 0x00, // lea    rax,[new_return]
	0x48, 0x87, 0x04, 0x24,                   // xchg   [old_return],rax
	0x49, 0x89, 0x04, 0x24,                   // mov    [reg64_rax],rax
	0xFF, 0x25, 0xA8, 0xFF, 0xFF, 0xFF,       // jmp    [fnNew]
	0x4C, 0x89, 0xE0,                         // mov    rax,r12 <- new_return
	0x48, 0x8B, 0x48, 0x10,                   // mov    rcx,[reg64_rcx]
	0x48, 0x8B, 0x50, 0x18,                   // mov    rdx,[reg64_rdx]
	0x4C, 0x8B, 0x40, 0x40,                   // mov    r8,[reg64_r8] 
	0x4C, 0x8B, 0x48, 0x48,                   // mov    r9,[reg64_r9]
	0xFF, 0x30,                               // push   [reg64_rax]
	0x4C, 0x8B, 0x15, 0x7C, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[6] = {
	0xFF, 0x25, 0x79, 0xFF, 0xFF, 0xFF,       // jmp    [fnHooked]
	};
};

// A hook that executes the hooked function before the hooking one.
// The return value of the hooked function is preserved, use ReturnHook to override it.
// The default Microsoft x86-64 calling convention is assumed: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
// Use ExitHookV for vectorcall functions.
struct ExitHook {
	ExitHook() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[7] = {
	0x4C, 0x8B, 0x15, 0xD1, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[80] = {
	0x48, 0x89, 0x48, 0x10,                   // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                   // mov    [reg64_rdx],rdx
	0x4C, 0x89, 0x40, 0x40,                   // mov    [reg64_r8],r8 
	0x4C, 0x89, 0x48, 0x48,                   // mov    [reg64_r9],r9
	0x4C, 0x8D, 0x15, 0x0E, 0x00, 0x00, 0x00, // lea    r10,[new_return]
	0x4C, 0x87, 0x14, 0x24,                   // xchg   [old_return],r10
	0x4C, 0x89, 0x50, 0x50,                   // mov    [reg64_r10],r10
	0xFF, 0x25, 0xB0, 0xFF, 0xFF, 0xFF,       // jmp    [fnHooked]
	0x4D, 0x89, 0xE2,                         // mov    r10,r12 <- new_return
	0x49, 0x89, 0x02,                         // mov    [reg64_rax],rax
	0x49, 0x8B, 0x4A, 0x10,                   // mov    rcx,[reg64_rcx]
	0x49, 0x8B, 0x52, 0x18,                   // mov    rdx,[reg64_rdx]
	0x4D, 0x8B, 0x42, 0x40,                   // mov    r8,[reg64_r8] 
	0x4D, 0x8B, 0x4A, 0x48,                   // mov    r9,[reg64_r9]
	0xFF, 0x15, 0x8C, 0xFF, 0xFF, 0xFF,       // call   [fnNew]
	0x4C, 0x89, 0xE0,                         // mov    rax,r12
	0xFF, 0x70, 0x50,                         // push   [reg64_r10]
	0xFF, 0x30,                               // push   [rax]
	0x4C, 0x8B, 0x15, 0x6D, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[2] = {
	0x58,                                     // pop rax
	0xC3,                                     // ret
	};
};

// A hook that executes the hooked function before the hooking one.
// The return value of the hooked function is overriden, make your hooking function has a fitting return value.
// The default Microsoft x86-64 calling convention is assumed: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
// Use ReturnHookV for vectorcall functions.
struct ReturnHook {
	ReturnHook() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[7] = {
	0x4C, 0x8B, 0x15, 0xD1, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[65] = {
	0x48, 0x89, 0x48, 0x10,                   // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                   // mov    [reg64_rdx],rdx
	0x4C, 0x89, 0x40, 0x40,                   // mov    [reg64_r8],r8 
	0x4C, 0x89, 0x48, 0x48,                   // mov    [reg64_r9],r9
	0x48, 0x8D, 0x05, 0x0E, 0x00, 0x00, 0x00, // lea    rax,[new_return]
	0x48, 0x87, 0x04, 0x24,                   // xchg   [old_return],rax
	0x49, 0x89, 0x04, 0x24,                   // mov    [reg64_rax],rax
	0xFF, 0x25, 0xB0, 0xFF, 0xFF, 0xFF,       // jmp    [fnHooked]
	0x4C, 0x89, 0xE0,                         // mov    rax,r12 <- new_return
	0x48, 0x8B, 0x48, 0x10,                   // mov    rcx,[reg64_rcx]
	0x48, 0x8B, 0x50, 0x18,                   // mov    rdx,[reg64_rdx]
	0x4C, 0x8B, 0x40, 0x40,                   // mov    r8,[reg64_r8] 
	0x4C, 0x8B, 0x48, 0x48,                   // mov    r9,[reg64_r9]
	0xFF, 0x30,                               // push   [reg64_rax]
	0x4C, 0x8B, 0x15, 0x7C, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[6] = {
	0xFF, 0x25, 0x71, 0xFF, 0xFF, 0xFF,       // jmp    [fnNew]
	};
};

// A special hook template that loads all integer registers into the context structure,
// then passes a pointer to it to the hooking function as its first parameter.
// This allows modifying any register by accessing them inside the struct.
// Hooking function signature:
// void (*)(HookContext*)
// Does not include SIMD registers, use ContextHookV for that.
struct ContextHook {
	ContextHook() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[9] = {
	0x41, 0x52,                                     // push   r10
	0x4C, 0x8B, 0x15, 0xCF, 0xFF, 0xFF, 0xFF,       // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[136] = {
	0x48, 0x89, 0x58, 0x08,                         // mov    [reg64_rbx],rbx
	0x48, 0x89, 0x48, 0x10,                         // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                         // mov    [reg64_rdx],rdx
	0x48, 0x89, 0x60, 0x20,                         // mov    [reg64_rsp],rsp
	0x48, 0x89, 0x68, 0x28,                         // mov    [reg64_rbp],rbp
	0x48, 0x89, 0x70, 0x30,                         // mov    [reg64_rsi],rsi
	0x48, 0x89, 0x78, 0x38,                         // mov    [reg64_rdi],rdi
	0x4C, 0x89, 0x40, 0x40,                         // mov    [reg64_r8],r8
	0x4C, 0x89, 0x48, 0x48,                         // mov    [reg64_r9],r9
	0x8F, 0x40, 0x50,                               // pop    [reg64_r10]
	0x4C, 0x89, 0x58, 0x58,                         // mov    [reg64_r11],r11
	0x4C, 0x89, 0x60, 0x60,                         // mov    [reg64_r12],r12
	0x4C, 0x89, 0x68, 0x68,                         // mov    [reg64_r13],r13
	0x4C, 0x89, 0x70, 0x70,                         // mov    [reg64_r14],r14
	0x4C, 0x89, 0x78, 0x78,                         // mov    [reg64_r15],r15
	0x48, 0x8D, 0x05, 0x0E, 0x00, 0x00, 0x00,       // lea    rax,[new_return]
	0x48, 0x87, 0x04, 0x24,                         // xchg   [old_return],rax
	0x49, 0x89, 0x04, 0x24,                         // mov    [reg64_rax],rax
	0xFF, 0x25, 0x7B, 0xFF, 0xFF, 0xFF,             // jmp    [fnNew]
	0x4C, 0x89, 0xE0,                               // mov    rax,r12 <- new_return
	0x48, 0x8B, 0x58, 0x08,                         // mov    rbx,[reg64_rbx]
	0x48, 0x8B, 0x48, 0x10,                         // mov    rcx,[reg64_rcx]
	0x48, 0x8B, 0x50, 0x18,                         // mov    rdx,[reg64_rdx]
	0x48, 0x8B, 0x68, 0x28,                         // mov    rbp,[reg64_rbp]
	0x48, 0x8B, 0x70, 0x30,                         // mov    rsi,[reg64_rsi]
	0x48, 0x8B, 0x78, 0x38,                         // mov    rdi,[reg64_rdi]
	0x4C, 0x8B, 0x40, 0x40,                         // mov    r8,[reg64_r8]
	0x4C, 0x8B, 0x48, 0x48,                         // mov    r9,[reg64_r9]
	0x4C, 0x8B, 0x68, 0x68,                         // mov    r13,[reg64_r13]
	0x4C, 0x8B, 0x70, 0x70,                         // mov    r14,[reg64_r14]
	0x4C, 0x8B, 0x78, 0x78,                         // mov    r15,[reg64_r15]
	0xFF, 0x30,                                     // push   [reg64_rax]
	0x4C, 0x8B, 0x15, 0x33, 0xFF, 0xFF, 0xFF,       // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[6] = {
	0xFF, 0x25, 0x30, 0xFF, 0xFF, 0xFF,             // jmp    [fnHooked]
	};
};

// The hooking function is executed before the hooked function.
// The vectorcall Microsoft calling convention is assumed: https://learn.microsoft.com/en-us/cpp/cpp/vectorcall?view=msvc-170
// Can be used for non-vectorcall functions, however it would be unnecessary and slower than normal EntryHook.
struct EntryHookV {
	EntryHookV() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[7] = {
	0x4C, 0x8B, 0x15, 0xD1, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[149] = {
	0x48, 0x89, 0x48, 0x10,                   // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                   // mov    [reg64_rdx],rdx
	0x4C, 0x89, 0x40, 0x40,                   // mov    [reg64_r8],r8 
	0x4C, 0x89, 0x48, 0x48,                   // mov    [reg64_r9],r9
	0x0F, 0x29, 0x80, 0x80, 0x00, 0x00, 0x00, // movaps [imm256_xmm0],xmm0
	0x0F, 0x29, 0x88, 0xA0, 0x00, 0x00, 0x00, // movaps [imm256_xmm1],xmm1
	0x0F, 0x29, 0x90, 0xC0, 0x00, 0x00, 0x00, // movaps [imm256_xmm2],xmm2
	0x0F, 0x29, 0x98, 0xE0, 0x00, 0x00, 0x00, // movaps [imm256_xmm3],xmm3
	0x0F, 0x29, 0xA0, 0x00, 0x01, 0x00, 0x00, // movaps [imm256_xmm4],xmm4
	0x0F, 0x29, 0xA8, 0x20, 0x01, 0x00, 0x00, // movaps [imm256_xmm5],xmm5
	0x48, 0x8D, 0x05, 0x0E, 0x00, 0x00, 0x00, // lea    rax,[new_return]
	0x48, 0x87, 0x04, 0x24,                   // xchg   [old_return],rax
	0x49, 0x89, 0x04, 0x24,                   // mov    [reg64_rax],rax
	0xFF, 0x25, 0x7E, 0xFF, 0xFF, 0xFF,       // jmp    [fnNew]
	0x4C, 0x89, 0xE0,                         // mov    rax,r12 <- new_return
	0x48, 0x8B, 0x48, 0x10,                   // mov    rcx,[reg64_rcx]
	0x48, 0x8B, 0x50, 0x18,                   // mov    rdx,[reg64_rdx]
	0x4C, 0x8B, 0x40, 0x40,                   // mov    r8,[reg64_r8]
	0x4C, 0x8B, 0x48, 0x48,                   // mov    r9,[reg64_r9]
	0x0F, 0x28, 0x80, 0x80, 0x00, 0x00, 0x00, // movaps xmm0,[imm256_xmm0]
	0x0F, 0x28, 0x88, 0xA0, 0x00, 0x00, 0x00, // movaps xmm1,[imm256_xmm1]
	0x0F, 0x28, 0x90, 0xC0, 0x00, 0x00, 0x00, // movaps xmm2,[imm256_xmm2]
	0x0F, 0x28, 0x98, 0xE0, 0x00, 0x00, 0x00, // movaps xmm3,[imm256_xmm3]
	0x0F, 0x28, 0xA0, 0x00, 0x01, 0x00, 0x00, // movaps xmm4,[imm256_xmm4]
	0x0F, 0x28, 0xA8, 0x20, 0x01, 0x00, 0x00, // movaps xmm5,[imm256_xmm5]
	0xFF, 0x30,                               // push   [reg64_rax]
	0x4C, 0x8B, 0x15, 0x28, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[6] = {
	0xFF, 0x25, 0x25, 0xFF, 0xFF, 0xFF,       // jmp    [fnHooked]
	};
};

// A hook that executes the hooked function before the hooking one.
// The return value of the hooked function is preserved, use ReturnHookV to override it.
// The vectorcall Microsoft calling convention is assumed: https://learn.microsoft.com/en-us/cpp/cpp/vectorcall?view=msvc-170
// Can be used for non-vectorcall functions, however it would be unnecessary and slower than normal ExitHook.
struct ExitHookV {
	ExitHookV() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[7] = {
	0x4C, 0x8B, 0x15, 0xD1, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[221] = {
	0x48, 0x89, 0x48, 0x10,                   // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                   // mov    [reg64_rdx],rdx
	0x4C, 0x89, 0x40, 0x40,                   // mov    [reg64_r8],r8 
	0x4C, 0x89, 0x48, 0x48,                   // mov    [reg64_r9],r9
	0x0F, 0x29, 0x80, 0x80, 0x00, 0x00, 0x00, // movaps [imm256_xmm0],xmm0
	0x0F, 0x29, 0x88, 0xA0, 0x00, 0x00, 0x00, // movaps [imm256_xmm1],xmm1
	0x0F, 0x29, 0x90, 0xC0, 0x00, 0x00, 0x00, // movaps [imm256_xmm2],xmm2
	0x0F, 0x29, 0x98, 0xE0, 0x00, 0x00, 0x00, // movaps [imm256_xmm3],xmm3
	0x0F, 0x29, 0xA0, 0x00, 0x01, 0x00, 0x00, // movaps [imm256_xmm4],xmm4
	0x0F, 0x29, 0xA8, 0x20, 0x01, 0x00, 0x00, // movaps [imm256_xmm5],xmm5
	0x4C, 0x8D, 0x15, 0x0E, 0x00, 0x00, 0x00, // lea    r10,[new_return]
	0x4C, 0x87, 0x14, 0x24,                   // xchg   [old_return],r10
	0x4C, 0x89, 0x50, 0x50,                   // mov    [reg64_r10],r10
	0xFF, 0x25, 0x86, 0xFF, 0xFF, 0xFF,       // jmp    [fnHooked]
	0x49, 0x89, 0x04, 0x24,                   // mov    [reg64_rax],rax <- new_return
	0x4C, 0x89, 0xE0,                         // mov    rax,r12
	0x48, 0x8B, 0x48, 0x10,                   // mov    rcx,[reg64_rcx]
	0x48, 0x8B, 0x50, 0x18,                   // mov    rdx,[reg64_rdx]
	0x4C, 0x8B, 0x40, 0x40,                   // mov    r8,[reg64_r8]
	0x4C, 0x8B, 0x48, 0x48,                   // mov    r9,[reg64_r9]
	0x0F, 0x29, 0x80, 0x40, 0x01, 0x00, 0x00, // movaps [imm256_xmm6],xmm0
	0x0F, 0x29, 0x88, 0x60, 0x01, 0x00, 0x00, // movaps [imm256_xmm7],xmm1
	0x0F, 0x29, 0x90, 0x80, 0x01, 0x00, 0x00, // movaps [imm256_xmm8],xmm2
	0x0F, 0x29, 0x98, 0xA0, 0x01, 0x00, 0x00, // movaps [imm256_xmm9],xmm3
	0x0F, 0x28, 0x80, 0x80, 0x00, 0x00, 0x00, // movaps xmm0,[imm256_xmm0]
	0x0F, 0x28, 0x88, 0xA0, 0x00, 0x00, 0x00, // movaps xmm1,[imm256_xmm1]
	0x0F, 0x28, 0x90, 0xC0, 0x00, 0x00, 0x00, // movaps xmm2,[imm256_xmm2]
	0x0F, 0x28, 0x98, 0xE0, 0x00, 0x00, 0x00, // movaps xmm3,[imm256_xmm3]
	0x0F, 0x28, 0xA0, 0x00, 0x01, 0x00, 0x00, // movaps xmm4,[imm256_xmm4]
	0x0F, 0x28, 0xA8, 0x20, 0x01, 0x00, 0x00, // movaps xmm5,[imm256_xmm5]
	0xFF, 0x15, 0x1B, 0xFF, 0xFF, 0xFF,       // call   [fnNew]
	0x4C, 0x89, 0xE0,                         // mov    rax,r12
	0x0F, 0x28, 0x80, 0x40, 0x01, 0x00, 0x00, // movaps xmm0,[imm256_xmm6]
	0x0F, 0x28, 0x88, 0x60, 0x01, 0x00, 0x00, // movaps xmm1,[imm256_xmm7]
	0x0F, 0x28, 0x90, 0x80, 0x01, 0x00, 0x00, // movaps xmm2,[imm256_xmm8]
	0x0F, 0x28, 0x98, 0xA0, 0x01, 0x00, 0x00, // movaps xmm3,[imm256_xmm9]
	0xFF, 0x70, 0x50,                         // push   [reg64_r10]
	0xFF, 0x30,                               // push   [rax]
	0x4C, 0x8B, 0x15, 0xE0, 0xFE, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[2] = {
	0x58,                                     // pop rax
	0xC3,                                     // ret
	};
};

// A hook that executes the hooked function before the hooking one.
// The return value of the hooked function is overriden, make your hooking function has a fitting return value.
// The vectorcall Microsoft calling convention is assumed: https://learn.microsoft.com/en-us/cpp/cpp/vectorcall?view=msvc-170
// Can be used for non-vectorcall functions, however it would be unnecessary and slower than normal ReturnHook.
struct ReturnHookV {
	ReturnHookV() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[7] = {
	0x4C, 0x8B, 0x15, 0xD1, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[149] = {
	0x48, 0x89, 0x48, 0x10,                   // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                   // mov    [reg64_rdx],rdx
	0x4C, 0x89, 0x40, 0x40,                   // mov    [reg64_r8],r8 
	0x4C, 0x89, 0x48, 0x48,                   // mov    [reg64_r9],r9
	0x0F, 0x29, 0x80, 0x80, 0x00, 0x00, 0x00, // movaps [imm256_xmm0],xmm0
	0x0F, 0x29, 0x88, 0xA0, 0x00, 0x00, 0x00, // movaps [imm256_xmm1],xmm1
	0x0F, 0x29, 0x90, 0xC0, 0x00, 0x00, 0x00, // movaps [imm256_xmm2],xmm2
	0x0F, 0x29, 0x98, 0xE0, 0x00, 0x00, 0x00, // movaps [imm256_xmm3],xmm3
	0x0F, 0x29, 0xA0, 0x00, 0x01, 0x00, 0x00, // movaps [imm256_xmm4],xmm4
	0x0F, 0x29, 0xA8, 0x20, 0x01, 0x00, 0x00, // movaps [imm256_xmm5],xmm5
	0x48, 0x8D, 0x05, 0x0E, 0x00, 0x00, 0x00, // lea    rax,[new_return]
	0x48, 0x87, 0x04, 0x24,                   // xchg   [old_return],rax
	0x49, 0x89, 0x04, 0x24,                   // mov    [reg64_rax],rax
	0xFF, 0x25, 0x86, 0xFF, 0xFF, 0xFF,       // jmp    [fnHooked]
	0x4C, 0x89, 0xE0,                         // mov    rax,r12 <- new_return
	0x48, 0x8B, 0x48, 0x10,                   // mov    rcx,[reg64_rcx]
	0x48, 0x8B, 0x50, 0x18,                   // mov    rdx,[reg64_rdx]
	0x4C, 0x8B, 0x40, 0x40,                   // mov    r8,[reg64_r8] 
	0x4C, 0x8B, 0x48, 0x48,                   // mov    r9,[reg64_r9]
	0x0F, 0x28, 0x80, 0x80, 0x00, 0x00, 0x00, // movaps xmm0,[imm256_xmm0]
	0x0F, 0x28, 0x88, 0xA0, 0x00, 0x00, 0x00, // movaps xmm1,[imm256_xmm1]
	0x0F, 0x28, 0x90, 0xC0, 0x00, 0x00, 0x00, // movaps xmm2,[imm256_xmm2]
	0x0F, 0x28, 0x98, 0xE0, 0x00, 0x00, 0x00, // movaps xmm3,[imm256_xmm3]
	0x0F, 0x28, 0xA0, 0x00, 0x01, 0x00, 0x00, // movaps xmm4,[imm256_xmm4]
	0x0F, 0x28, 0xA8, 0x20, 0x01, 0x00, 0x00, // movaps xmm5,[imm256_xmm5]
	0xFF, 0x30,                               // push   [reg64_rax]
	0x4C, 0x8B, 0x15, 0x28, 0xFF, 0xFF, 0xFF, // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[6] = {
	0xFF, 0x25, 0x1D, 0xFF, 0xFF, 0xFF,       // jmp    [fnNew]
	};
};

// A special hook template that loads all integer registers into the context structure,
// then passes a pointer to it to the hooking function as its first parameter.
// This allows modifying any register by accessing them inside the struct.
// Hooking function signature:
// void (*)(HookContext*)
// Includes SIMD registers (and is therefore quite large). 
// Use ContextHook if you only.
struct ContextHookV {
	ContextHookV() {}

	//data:
	HookBase hookData{};

	// assembly:
	uint8_t asmRaw1[9] = {
	0x41, 0x52,                                     // push   r10
	0x4C, 0x8B, 0x15, 0xCF, 0xFF, 0xFF, 0xFF,       // mov    r10,[pool]
	};
	HookBorrowContext asmBorrow{};
	uint8_t asmRaw2[376] = {
	0x48, 0x89, 0x58, 0x08,                         // mov    [reg64_rbx],rbx
	0x48, 0x89, 0x48, 0x10,                         // mov    [reg64_rcx],rcx
	0x48, 0x89, 0x50, 0x18,                         // mov    [reg64_rdx],rdx
	0x48, 0x89, 0x60, 0x20,                         // mov    [reg64_rsp],rsp
	0x48, 0x89, 0x68, 0x28,                         // mov    [reg64_rbp],rbp
	0x48, 0x89, 0x70, 0x30,                         // mov    [reg64_rsi],rsi
	0x48, 0x89, 0x78, 0x38,                         // mov    [reg64_rdi],rdi
	0x4C, 0x89, 0x40, 0x40,                         // mov    [reg64_r8],r8
	0x4C, 0x89, 0x48, 0x48,                         // mov    [reg64_r9],r9
	0x8F, 0x40, 0x50,                               // pop    [reg64_r10]
	0x4C, 0x89, 0x58, 0x58,                         // mov    [reg64_r11],r11
	0x4C, 0x89, 0x60, 0x60,                         // mov    [reg64_r12],r12
	0x4C, 0x89, 0x68, 0x68,                         // mov    [reg64_r13],r13
	0x4C, 0x89, 0x70, 0x70,                         // mov    [reg64_r14],r14
	0x4C, 0x89, 0x78, 0x78,                         // mov    [reg64_r15],r15
	0x0F, 0x29, 0x80, 0x80, 0x00, 0x00, 0x00,       // movaps [imm256_xmm0],xmm0
	0x0F, 0x29, 0x88, 0xA0, 0x00, 0x00, 0x00,       // movaps [imm256_xmm1],xmm1
	0x0F, 0x29, 0x90, 0xC0, 0x00, 0x00, 0x00,       // movaps [imm256_xmm2],xmm2
	0x0F, 0x29, 0x98, 0xE0, 0x00, 0x00, 0x00,       // movaps [imm256_xmm3],xmm3
	0x0F, 0x29, 0xA0, 0x00, 0x01, 0x00, 0x00,       // movaps [imm256_xmm4],xmm4
	0x0F, 0x29, 0xA8, 0x20, 0x01, 0x00, 0x00,       // movaps [imm256_xmm5],xmm5
	0x0F, 0x29, 0xB0, 0x40, 0x01, 0x00, 0x00,       // movaps [imm256_xmm6],xmm6
	0x0F, 0x29, 0xB8, 0x60, 0x01, 0x00, 0x00,       // movaps [imm256_xmm7],xmm7
	0x44, 0x0F, 0x29, 0x80, 0x80, 0x01, 0x00, 0x00, // movaps [imm256_xmm8],xmm8
	0x44, 0x0F, 0x29, 0x88, 0xA0, 0x01, 0x00, 0x00, // movaps [imm256_xmm9],xmm9
	0x44, 0x0F, 0x29, 0x90, 0xC0, 0x01, 0x00, 0x00, // movaps [imm256_xmm10],xmm10
	0x44, 0x0F, 0x29, 0x98, 0xE0, 0x01, 0x00, 0x00, // movaps [imm256_xmm11],xmm11
	0x44, 0x0F, 0x29, 0xA0, 0x00, 0x02, 0x00, 0x00, // movaps [imm256_xmm12],xmm12
	0x44, 0x0F, 0x29, 0xA8, 0x20, 0x02, 0x00, 0x00, // movaps [imm256_xmm13],xmm13
	0x44, 0x0F, 0x29, 0xB0, 0x40, 0x02, 0x00, 0x00, // movaps [imm256_xmm14],xmm14
	0x44, 0x0F, 0x29, 0xB8, 0x60, 0x02, 0x00, 0x00, // movaps [imm256_xmm15],xmm15
	0x48, 0x8D, 0x05, 0x0E, 0x00, 0x00, 0x00,       // lea    rax,[new_return]
	0x48, 0x87, 0x04, 0x24,                         // xchg   [old_return],rax
	0x49, 0x89, 0x04, 0x24,                         // mov    [reg64_rax],rax
	0xFF, 0x25, 0x03, 0xFF, 0xFF, 0xFF,             // jmp    [fnNew]
	0x4C, 0x89, 0xE0,                               // mov    rax,r12 <- new_return
	0x48, 0x8B, 0x58, 0x08,                         // mov    rbx,[reg64_rbx]
	0x48, 0x8B, 0x48, 0x10,                         // mov    rcx,[reg64_rcx]
	0x48, 0x8B, 0x50, 0x18,                         // mov    rdx,[reg64_rdx]
	0x48, 0x8B, 0x68, 0x28,                         // mov    rbp,[reg64_rbp]
	0x48, 0x8B, 0x70, 0x30,                         // mov    rsi,[reg64_rsi]
	0x48, 0x8B, 0x78, 0x38,                         // mov    rdi,[reg64_rdi]
	0x4C, 0x8B, 0x40, 0x40,                         // mov    r8,[reg64_r8]
	0x4C, 0x8B, 0x48, 0x48,                         // mov    r9,[reg64_r9]
	0x4C, 0x8B, 0x68, 0x68,                         // mov    r13,[reg64_r13]
	0x4C, 0x8B, 0x70, 0x70,                         // mov    r14,[reg64_r14]
	0x4C, 0x8B, 0x78, 0x78,                         // mov    r15,[reg64_r15]
	0x0F, 0x28, 0x80, 0x80, 0x00, 0x00, 0x00,       // movaps xmm0,[imm256_xmm0]
	0x0F, 0x28, 0x88, 0xA0, 0x00, 0x00, 0x00,       // movaps xmm1,[imm256_xmm1]
	0x0F, 0x28, 0x90, 0xC0, 0x00, 0x00, 0x00,       // movaps xmm2,[imm256_xmm2]
	0x0F, 0x28, 0x98, 0xE0, 0x00, 0x00, 0x00,       // movaps xmm3,[imm256_xmm3]
	0x0F, 0x28, 0xA0, 0x00, 0x01, 0x00, 0x00,       // movaps xmm4,[imm256_xmm4]
	0x0F, 0x28, 0xA8, 0x20, 0x01, 0x00, 0x00,       // movaps xmm5,[imm256_xmm5]
	0x0F, 0x28, 0xB0, 0x40, 0x01, 0x00, 0x00,       // movaps xmm6,[imm256_xmm6]
	0x0F, 0x28, 0xB8, 0x60, 0x01, 0x00, 0x00,       // movaps xmm7,[imm256_xmm7]
	0x44, 0x0F, 0x28, 0x80, 0x80, 0x01, 0x00, 0x00, // movaps xmm8,[imm256_xmm8]
	0x44, 0x0F, 0x28, 0x88, 0xA0, 0x01, 0x00, 0x00, // movaps xmm9,[imm256_xmm9]
	0x44, 0x0F, 0x28, 0x90, 0xC0, 0x01, 0x00, 0x00, // movaps xmm10,[imm256_xmm10]
	0x44, 0x0F, 0x28, 0x98, 0xE0, 0x01, 0x00, 0x00, // movaps xmm11,[imm256_xmm11]
	0x44, 0x0F, 0x28, 0xA0, 0x00, 0x02, 0x00, 0x00, // movaps xmm12,[imm256_xmm12]
	0x44, 0x0F, 0x28, 0xA8, 0x20, 0x02, 0x00, 0x00, // movaps xmm13,[imm256_xmm13]
	0x44, 0x0F, 0x28, 0xB0, 0x40, 0x02, 0x00, 0x00, // movaps xmm14,[imm256_xmm14]
	0x44, 0x0F, 0x28, 0xB8, 0x60, 0x02, 0x00, 0x00, // movaps xmm15,[imm256_xmm15]
	0xFF, 0x30,                                     // push   [reg64_rax]
	0x4C, 0x8B, 0x15, 0x43, 0xFE, 0xFF, 0xFF,       // mov    r10,[pool]
	};
	HookReturnContext asmReturn{};
	uint8_t asmRaw3[6] = {
	0xFF, 0x25, 0x40, 0xFE, 0xFF, 0xFF,             // jmp    [fnHooked]
	};
};

#undef UNIHOOK_THREAD_ACCESS_LIMIT