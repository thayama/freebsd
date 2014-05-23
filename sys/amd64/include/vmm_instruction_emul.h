/*-
 * Copyright (c) 2012 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_VMM_INSTRUCTION_EMUL_H_
#define	_VMM_INSTRUCTION_EMUL_H_

enum vm_reg_name;

enum vie_cpu_mode {
	CPU_MODE_COMPATIBILITY,		/* IA-32E mode (CS.L = 0) */
	CPU_MODE_64BIT,			/* IA-32E mode (CS.L = 1) */
};

enum vie_paging_mode {
	PAGING_MODE_FLAT,
	PAGING_MODE_32,
	PAGING_MODE_PAE,
	PAGING_MODE_64,
};

/*
 * The data structures 'vie' and 'vie_op' are meant to be opaque to the
 * consumers of instruction decoding. The only reason why their contents
 * need to be exposed is because they are part of the 'vm_exit' structure.
 */
struct vie_op {
	uint8_t		op_byte;	/* actual opcode byte */
	uint8_t		op_type;	/* type of operation (e.g. MOV) */
	uint16_t	op_flags;
};

#define	VIE_INST_SIZE	15
struct vie {
	uint8_t		inst[VIE_INST_SIZE];	/* instruction bytes */
	uint8_t		num_valid;		/* size of the instruction */
	uint8_t		num_processed;

	uint8_t		rex_w:1,		/* REX prefix */
			rex_r:1,
			rex_x:1,
			rex_b:1,
			rex_present:1;

	uint8_t		mod:2,			/* ModRM byte */
			reg:4,
			rm:4;

	uint8_t		ss:2,			/* SIB byte */
			index:4,
			base:4;

	uint8_t		disp_bytes;
	uint8_t		imm_bytes;

	uint8_t		scale;
	int		base_register;		/* VM_REG_GUEST_xyz */
	int		index_register;		/* VM_REG_GUEST_xyz */

	int64_t		displacement;		/* optional addr displacement */
	int64_t		immediate;		/* optional immediate operand */

	uint8_t		decoded;	/* set to 1 if successfully decoded */

	struct vie_op	op;			/* opcode description */
};

/*
 * Callback functions to read and write memory regions.
 */
typedef int (*mem_region_read_t)(void *vm, int cpuid, uint64_t gpa,
				 uint64_t *rval, int rsize, void *arg);

typedef int (*mem_region_write_t)(void *vm, int cpuid, uint64_t gpa,
				  uint64_t wval, int wsize, void *arg);

/*
 * Emulate the decoded 'vie' instruction.
 *
 * The callbacks 'mrr' and 'mrw' emulate reads and writes to the memory region
 * containing 'gpa'. 'mrarg' is an opaque argument that is passed into the
 * callback functions.
 *
 * 'void *vm' should be 'struct vm *' when called from kernel context and
 * 'struct vmctx *' when called from user context.
 * s
 */
int vmm_emulate_instruction(void *vm, int cpuid, uint64_t gpa, struct vie *vie,
			    mem_region_read_t mrr, mem_region_write_t mrw,
			    void *mrarg);

int vie_update_register(void *vm, int vcpuid, enum vm_reg_name reg,
    uint64_t val, int size);

/*
 * Returns 1 if an alignment check exception should be injected and 0 otherwise.
 */
int vie_alignment_check(int cpl, int operand_size, uint64_t cr0,
    uint64_t rflags, uint64_t gla);

uint64_t vie_size2mask(int size);

#ifdef _KERNEL
/*
 * APIs to fetch and decode the instruction from nested page fault handler.
 *
 * 'vie' must be initialized before calling 'vmm_fetch_instruction()'
 */
int vmm_fetch_instruction(struct vm *vm, int cpuid,
			  uint64_t rip, int inst_length, uint64_t cr3,
			  enum vie_paging_mode paging_mode, int cpl,
			  struct vie *vie);

/*
 * Translate the guest linear address 'gla' to a guest physical address.
 *
 * Returns 0 on success and '*gpa' contains the result of the translation.
 * Returns 1 if a page fault exception was injected into the guest.
 * Returns -1 otherwise.
 */
int vmm_gla2gpa(struct vm *vm, int vcpuid, uint64_t gla, uint64_t cr3,
    uint64_t *gpa, enum vie_paging_mode paging_mode, int cpl, int prot);

void vie_init(struct vie *vie);

uint64_t vie_segbase(enum vm_reg_name segment, enum vie_cpu_mode cpu_mode,
    const struct seg_desc *desc);

/*
 * Decode the instruction fetched into 'vie' so it can be emulated.
 *
 * 'gla' is the guest linear address provided by the hardware assist
 * that caused the nested page table fault. It is used to verify that
 * the software instruction decoding is in agreement with the hardware.
 * 
 * Some hardware assists do not provide the 'gla' to the hypervisor.
 * To skip the 'gla' verification for this or any other reason pass
 * in VIE_INVALID_GLA instead.
 */
#define	VIE_INVALID_GLA		(1UL << 63)	/* a non-canonical address */
int vmm_decode_instruction(struct vm *vm, int cpuid, uint64_t gla,
			   enum vie_cpu_mode cpu_mode, struct vie *vie);
#endif	/* _KERNEL */

#endif	/* _VMM_INSTRUCTION_EMUL_H_ */
