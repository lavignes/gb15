#include <stdio.h>
#include <stdlib.h>

#include <gb15/cpu.h>
#include <gb15/mmu.h>
#include <gb15/gb15.h>

static inline s8 signify8(u8 value) {
    if (value <= (u8)0x7F) {
        return value;
    }
    return -(s8)(((~value) + (u8)1) & (u8)0xFF);
}

static inline u8 read8(GB15Mmu *mmu, u8 *rom, u16 *pc) {
    u8 tmp8 = gb15_mmu_read(mmu, rom, *pc);
    (*pc)++;
    return tmp8;
}

static inline u16 read16(GB15Mmu *mmu, u8 *rom, u16 *pc) {
    GB15LongRegister tmp16;
    tmp16.l = read8(mmu, rom, pc);
    tmp16.h = read8(mmu, rom, pc);
    return tmp16.value;
}

static inline void write16(GB15Mmu *mmu, u16 address, u16 value) {
    GB15LongRegister tmp16;
    tmp16.value = value;
    gb15_mmu_write(mmu, address, tmp16.l);
    gb15_mmu_write(mmu, address + (u16)1, tmp16.h);
}

static inline void set_z(GB15Cpu *cpu, bool value) {
    cpu->f = (cpu->f & (u8)0x7F) | (value << (u8)7);
}

static inline void set_n(GB15Cpu *cpu, bool value) {
    cpu->f = (cpu->f & (u8)0xBF) | (value << (u8)6);
}

static inline void set_h(GB15Cpu *cpu, bool value) {
    cpu->f = (cpu->f & (u8)0xDF) | (value << (u8)5);
}

static inline void set_c(GB15Cpu *cpu, bool value) {
    cpu->f = (cpu->f & (u8)0xEF) | (value << (u8)4);
}

static inline u8 get_z(GB15Cpu *cpu) {
    return (cpu->f & (u8)0x80) >> (u8)7;
}

static inline u8 get_h(GB15Cpu *cpu) {
    return (cpu->f & (u8)0x40) >> (u8)6;
}

static inline u8 get_n(GB15Cpu *cpu) {
    return (cpu->f & (u8)0x20) >> (u8)5;
}

static inline u8 get_c(GB15Cpu *cpu) {
    return (cpu->f & (u8)0x10) >> (u8)4;
}

static inline u8 *reg8(GB15Cpu *cpu, u8 reg) {
    switch (reg) {
        case 0x07:
            return &cpu->a;
        case 0x00:
            return &cpu->b;
        case 0x01:
            return &cpu->c;
        case 0x02:
            return &cpu->d;
        case 0x03:
            return &cpu->e;
        case 0x04:
            return &cpu->h;
        case 0x05:
            return &cpu->l;
        default:
            return NULL;
    }
}

static inline u16 *reg16(GB15Cpu *cpu, u8 reg) {
    switch (reg) {
        case 0x00:
            return &cpu->bc;
        case 0x01:
            return &cpu->de;
        case 0x02:
            return &cpu->hl;
        case 0x03:
            return &cpu->sp;
        default:
            return NULL;
    }
}

static inline u16 *reg16_push_pop(GB15Cpu *cpu, u8 reg) {
    switch (reg) {
        case 0x00:
            return &cpu->bc;
        case 0x01:
            return &cpu->de;
        case 0x02:
            return &cpu->hl;
        case 0x03:
            return &cpu->af;
        default:
            return NULL;
    }
}

static inline void add_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 value, u8 carry) {
    u16 overflow = (u16)cpu->a + (u16)value + (u16)carry;
    set_h(cpu, (((u16)cpu->a & (u16)0xF) + ((u16)value & (u16)0xF) + (u16)carry) > (u16)0xF);
    set_c(cpu, (u16)overflow > (u16)0xFF);
    set_n(cpu, false);
    cpu->a = (u8)overflow & (u8)0xFF;
    set_z(cpu, cpu->a == (u8)0x00);
}

static inline void sub_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 value) {
    s16 overflow = (s16)cpu->a - (s16)value;
    set_h(cpu, (((s16)cpu->a - (s16)value) & (s16)0xF) > ((s16)cpu->a & (s16)0xF));
    set_c(cpu, (s16)overflow < (s16)0x00);
    set_n(cpu, true);
    cpu->a = (u8)overflow & (u8)0xFF;
    set_z(cpu, cpu->a == (u8)0x00);
}

static inline void sbc_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 value) {
    s16 overflow = (s16)cpu->a - ((s16)value + (s16)get_c(cpu));
    set_h(cpu, (((s16)cpu->a & (s16)0xF) - ((s16)cpu->b & (s16)0xF) - (s16)get_c(cpu)) < (s16)0x00);
    set_c(cpu, (s16)overflow < (s16)0x00);
    set_n(cpu, true);
    cpu->a = (u8)overflow & (u8)0xFF;
    set_z(cpu, cpu->a == (u8)0x00);
}

static inline void and_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 value) {
    cpu->a &= value;
    set_z(cpu, cpu->a == (u8)0x00);
    set_h(cpu, true);
    set_n(cpu, false);
    set_c(cpu, false);
}

static inline void xor_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 value) {
    cpu->a ^= value;
    set_z(cpu, cpu->a == (u8)0x00);
    set_h(cpu, false);
    set_n(cpu, false);
    set_c(cpu, false);
}

static inline void or_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 value) {
    cpu->a |= value;
    set_z(cpu, cpu->a == (u8)0x00);
    set_h(cpu, false);
    set_n(cpu, false);
    set_c(cpu, false);
}

static inline void cp_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 value) {
    s16 overflow = (s16)cpu->a - (s16)value;
    set_h(cpu, (((s16)cpu->a - (s16)value) & (s16)0xF) > ((s16)cpu->a & (s16)0xF));
    set_c(cpu, (s16)overflow < (s16)0x00);
    set_n(cpu, true);
    set_z(cpu, overflow == (s16)0x00);
}

static inline u32 rlc_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg) {
    u8 carry;
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) { // a - l
        clocks = 2;
        carry = (*dest & (u8)0x80) >> (u8)7;
        *dest = (*dest << (u8)1) | carry;
        set_c(cpu, carry);
        set_z(cpu, *dest == (u8)0x00);
    } else { // (hl)
        clocks = 4;
        reg = gb15_mmu_read(mmu, rom, cpu->hl);
        carry = (reg & (u8)0x80) >> (u8)7;
        reg = (reg << (u8)1) | carry;
        set_c(cpu, carry);
        set_z(cpu, reg == (u8)0x00);
        gb15_mmu_write(mmu, cpu->hl, reg);
    }
    set_n(cpu, false);
    set_h(cpu, false);
    return clocks;
}

static inline u32 rrc_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg) {
    u8 carry;
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) { // a - l
        clocks = 2;
        carry = *dest & (u8)0x01;
        *dest = (*dest >> (u8)1) | (carry << (u8)7);
        set_c(cpu, carry);
        set_z(cpu, *dest == (u8)0x00);
    } else { // (hl)
        clocks = 4;
        reg = gb15_mmu_read(mmu, rom, cpu->hl);
        carry = reg & (u8)0x01;
        reg = (reg >> (u8)1) | (carry << (u8)7);
        set_c(cpu, carry);
        set_z(cpu, reg  == (u8)0x00);
        gb15_mmu_write(mmu, cpu->hl, reg);
    }
    set_n(cpu, false);
    set_h(cpu, false);
    return clocks;
}

static inline u32 rl_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg, u8 carry) {
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) { // a - l
        clocks = 2;
        set_c(cpu, (*dest & (u8)0x80) >> (u8)7);
        *dest = (*dest << (u8)1) | carry;
        set_z(cpu, *dest == (u8)0x00);
    } else { // (hl)
        clocks = 4;
        reg = gb15_mmu_read(mmu, rom, cpu->hl);
        set_c(cpu, (reg & (u8)0x80) >> (u8)7);
        reg = (reg << (u8)1) | carry;
        set_z(cpu, reg == (u8)0x00);
        gb15_mmu_write(mmu, cpu->hl, reg);
    }
    set_n(cpu, false);
    set_h(cpu, false);
    return clocks;
}

static inline u32 rr_core(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg, u8 carry) {
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) { // a - l
        clocks = 2;
        set_c(cpu, *dest & (u8)0x01);
        *dest = (*dest >> (u8)1) | (carry << (u8)7);
        set_z(cpu, *dest == (u8)0x00);
    } else { // (hl)
        clocks = 4;
        reg = gb15_mmu_read(mmu, rom, cpu->hl);
        set_c(cpu, reg & (u8)0x01);
        reg = (reg >> (u8)1) | (carry << (u8)7);
        set_z(cpu, reg == (u8)0x00);
        gb15_mmu_write(mmu, cpu->hl, reg);
    }
    set_n(cpu, false);
    set_h(cpu, false);
    return clocks;
}

static inline bool test_cond(GB15Cpu *cpu, u8 cond) {
    switch (cond) {
        case 0x00:
            return !get_z(cpu);
        case 0x01:
            return get_z(cpu);
        case 0x02:
            return !get_c(cpu);
        case 0x03:
            return get_c(cpu);
        default:
            break;
    }
    return false;
}

static inline u32 sla(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg) {
    return rl_core(cpu, mmu, rom, reg, 0);
}

static inline u32 sra(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg) {
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) { // a - l
        clocks = 2;
        set_c(cpu, *dest & (u8)0x01);
        *dest = (*dest >> (u8)1) | (*dest & (u8)0x80);
        set_z(cpu, *dest == (u8)0x00);
    } else { // (hl)
        clocks = 4;
        reg = gb15_mmu_read(mmu, rom, cpu->hl);
        set_c(cpu, reg & (u8)0x01);
        reg = (reg >> (u8)1) | (reg & (u8)0x80);
        set_z(cpu, reg == (u8)0x00);
        gb15_mmu_write(mmu, cpu->hl, reg);
    }
    set_n(cpu, false);
    set_h(cpu, false);
    return clocks;
}

static inline u32 swap(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg) {
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) { // a - l
        clocks = 2;
        *dest = ((*dest & (u8)0xF0) >> (u8)4) & ((*dest & (u8)0x0F) << (u8)4);
        set_z(cpu, *dest == (u8)0x00);
    } else { // (hl)
        clocks = 4;
        reg = gb15_mmu_read(mmu, rom, cpu->hl);
        reg = ((reg & (u8)0xF0) >> (u8)4) & ((reg & (u8)0x0F) << (u8)4);
        set_z(cpu, reg == (u8)0x00);
        gb15_mmu_write(mmu, cpu->hl, reg);
    }
    set_c(cpu, false);
    set_n(cpu, false);
    set_h(cpu, false);
    return clocks;
}

static inline u32 bit(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg, u8 mask) {
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) {
        clocks = 2;
        set_z(cpu, (*dest & mask) == (u8)0x00);
    } else {
        clocks = 4;
        set_z(cpu, (gb15_mmu_read(mmu, rom, cpu->hl) & mask) == (u8)0x00);
    }
    set_n(cpu, false);
    set_h(cpu, true);
    return clocks;
}

static inline u32 set(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom, u8 reg, u8 bit, bool value) {
    u8 *dest = reg8(cpu, reg);
    u32 clocks;
    if (dest) {
        clocks = 2;
        *dest |= (u8)value << bit;
    } else {
        clocks = 4;
        gb15_mmu_write(mmu, cpu->hl, gb15_mmu_read(mmu, rom, cpu->hl) | ((u8)value << bit));
    }
    return clocks;
}

static inline u32 rst_core(GB15Cpu *cpu, GB15Mmu *mmu, u16 dest) {
    cpu->sp -= 2;
    write16(mmu, cpu->sp, cpu->pc);
    cpu->pc = dest;
    return 4;
}

static inline u32 nop(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    return 1;
}

static inline u32 ld_rr_u16(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    *reg16(cpu, (opcode & (u8)0x30) >> (u8)4) = read16(mmu, rom, &cpu->pc);
    return 3;
}

static inline u32 ld_mem_rr_a(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, *reg16(cpu, (opcode & (u8)0x30) >> (u8)4), 0x07);
    return 2;
}

static inline u32 inc_rr(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    (*reg16(cpu, (opcode & (u8)0x30) >> (u8)4))++;
    return 2;
}

static inline u32 inc_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u8 *reg = reg8(cpu, (opcode & (u8)0x38) >> (u8)3);
    set_h(cpu, (*reg & 0xF) == (u8)0x0);
    (*reg)++;
    set_z(cpu, *reg == (u8)0x00);
    set_n(cpu, false);
    return 1;
}

static inline u32 dec_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u8 *reg = reg8(cpu, (opcode & (u8)0x38) >> (u8)3);
    (*reg)--;
    set_z(cpu, *reg == (u8)0x00);
    set_n(cpu, true);
    set_h(cpu, (*reg & 0xF) == (u8)0xF);
    return 1;
}

static inline u32 ld_r_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    *reg8(cpu, (opcode & (u8)0x38) >> (u8)3) = read8(mmu, rom, &cpu->pc);
    return 2;
}

static inline u32 rlca(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    rlc_core(cpu, mmu, rom, 0x07);
    set_z(cpu, false);
    return 1;
}

static inline u32 ld_mem_u16_sp(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    write16(mmu, read16(mmu, rom, &cpu->pc), cpu->sp);
    return 3;
}

static inline u32 ld_a_mem_rr(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->a = gb15_mmu_read(mmu, rom, *reg16(cpu, (opcode & (u8)0x30) >> (u8)4));
    return 2;
}

static inline u32 add_hl_rr(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u32 overflow = (u32)cpu->hl + (u32)*reg16(cpu, (opcode & (u8)0x30) >> (u8)4);
    set_n(cpu, false);
    set_c(cpu, overflow > (u32)0xFFFF);
    set_h(cpu, (overflow & (u32)0x0FFF) < (cpu->hl & (u32)0x0FFF));
    cpu->hl = (u16)(overflow & (u32)0xFFFF);
    return 2;
}

static inline u32 dec_rr(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    (*reg16(cpu, (opcode & (u8)0x30) >> (u8)4))--;
    return 2;
}

static inline u32 rrc(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    return rrc_core(cpu, mmu, rom, opcode & (u8)0x07);
}

static inline u32 stop(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->stopped = true;
    read8(mmu, rom, &cpu->pc);
    return 1;
}

static inline u32 rla(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    rl_core(cpu, mmu, rom, 0x07, get_c(cpu));
    set_z(cpu, false);
    return 1;
}

static inline u32 jr_s8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->pc += signify8(read8(mmu, rom, &cpu->pc));
    return 4;
}

static inline u32 rra(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    rr_core(cpu, mmu, rom, 0x07, get_c(cpu));
    set_z(cpu, false);
    return 4;
}

static inline u32 jr_cond_s8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u8 dest = read8(mmu, rom, &cpu->pc);
    if (test_cond(cpu, (opcode & (u8)0x18) >> (u8)3)) {
        cpu->pc += signify8(dest);
        return 3;
    }
    return 2;
}

static inline u32 ldi_hl_a(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, cpu->hl, cpu->a);
    cpu->hl++;
    return 2;
}

static inline u32 daa(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u16 overflow = cpu->a;
    if (get_n(cpu)) {
        if (get_h(cpu)) {
            overflow = (overflow - (u16)0x06) & (u16)0xFF;
        }
        if (get_c(cpu)) {
            overflow -= (u16)0x60;
        }
    } else {
        if (get_h(cpu) || ((overflow & (u16)0x0F) > (u16)9)) {
            overflow += (u16)0x06;
        }
        if (get_c(cpu) || (overflow > (u16)0x9F)) {
            overflow += (u16)0x60;
        }
    }
    cpu->a = (u8)(overflow & (u16)0xFF);
    set_h(cpu, false);
    set_z(cpu, cpu->a == (u8)0x00);
    if (overflow >= (u16)0x100) {
        set_c(cpu, true);
    }
    return 1;
}

static inline u32 ldi_a_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->a = gb15_mmu_read(mmu, rom, cpu->hl);
    cpu->hl++;
    return 2;
}

static inline u32 cpl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->a = ~cpu->a;
    set_n(cpu, false);
    set_h(cpu, false);
    return 1;
}

static inline u32 ldd_hl_a(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, cpu->hl, cpu->a);
    cpu->hl--;
    return 2;
}

static inline u32 inc_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u32 overflow = gb15_mmu_read(mmu, rom, cpu->hl);
    overflow = (overflow + (u32)1) & (u32)0xFF;
    gb15_mmu_write(mmu, cpu->hl, (u8)overflow);
    set_z(cpu, overflow == (u32)0x00);
    set_n(cpu, false);
    set_h(cpu, (overflow & (u32)0xF) == (u32)0x0);
    return 3;
}

static inline u32 dec_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u32 overflow = gb15_mmu_read(mmu, rom, cpu->hl);
    overflow = (overflow - (u32)1) & (u32)0xFF;
    gb15_mmu_write(mmu, cpu->hl, (u8)overflow);
    set_z(cpu, overflow == (u32)0x00);
    set_n(cpu, false);
    set_h(cpu, (overflow & (u32)0xF) == (u32)0xF);
    return 3;
}

static inline u32 ld_mem_hl_n(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, cpu->hl, read8(mmu, rom, &cpu->pc));
    return 3;
}

static inline u32 scf(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    set_n(cpu, false);
    set_h(cpu, false);
    set_c(cpu, true);
    return 1;
}

static inline u32 ldd_a_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->a = gb15_mmu_read(mmu, rom, cpu->hl);
    cpu->hl++;
    return 2;
}

static inline u32 ccf(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    set_n(cpu, false);
    set_h(cpu, false);
    set_c(cpu, !get_c(cpu));
    return 1;
}

static inline u32 ld_r_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    *reg8(cpu, (opcode & (u8)0x38) >> (u8)3) = *reg8(cpu, opcode & (u8)0x07);
    return 1;
}

static inline u32 ld_r_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    *reg8(cpu, (opcode & (u8)0x38) >> (u8)3) = gb15_mmu_read(mmu, rom, cpu->hl);
    return 2;
}

static inline u32 ld_mem_hl_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, cpu->hl, *reg8(cpu, opcode & (u8)0x07));
    return 2;
}

static inline u32 halt(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->halted = true;
    cpu->halt_flags = mmu->io[GB15_IO_IF];
    return 1;
}

static inline u32 add_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    add_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07), 0);
    return 1;
}

static inline u32 add_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    add_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl), 0);
    return 2;
}

static inline u32 adc_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    add_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07), get_c(cpu));
    return 1;
}

static inline u32 adc_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    add_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl), get_c(cpu));
    return 2;
}

static inline u32 sub_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    sub_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07));
    return 1;
}

static inline u32 sub_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    sub_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl));
    return 2;
}

static inline u32 sbc_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    sbc_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07));
    return 1;
}

static inline u32 sbc_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    sbc_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl));
    return 2;
}

static inline u32 and_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    and_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07));
    return 1;
}

static inline u32 and_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    and_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl));
    return 2;
}

static inline u32 xor_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    xor_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07));
    return 1;
}

static inline u32 xor_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    xor_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl));
    return 2;
}

static inline u32 or_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    or_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07));
    return 1;
}

static inline u32 or_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    or_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl));
    return 2;
}

static inline u32 cp_r(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cp_core(cpu, mmu, rom, *reg8(cpu, opcode & (u8)0x07));
    return 1;
}

static inline u32 cp_mem_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cp_core(cpu, mmu, rom, gb15_mmu_read(mmu, rom, cpu->hl));
    return 2;
}

static inline u32 ret_cond(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    if (test_cond(cpu, (opcode & (u8)0x18) >> (u8)3)) {
        cpu->pc = read16(mmu, rom, &cpu->sp);
        return 5;
    }
    return 2;
}

static inline u32 pop_rr(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    *reg16_push_pop(cpu, (opcode & (u8)0x30) >> (u8)4) = read16(mmu, rom, &cpu->sp);
    return 3;
}

static inline u32 jp_cond_u16(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u16 dest = read16(mmu, rom, &cpu->pc);
    if (test_cond(cpu, (opcode & (u8)0x18) >> (u8)3)) {
        cpu->pc = dest;
        return 4;
    }
    return 3;
}

static inline u32 jp_u16(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->pc = read16(mmu, rom, &cpu->pc);
    return 4;
}

static inline u32 call_cond_u16(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u16 dest = read16(mmu, rom, &cpu->pc);
    if (test_cond(cpu, (opcode & (u8)0x18) >> (u8)3)) {
        cpu->sp -= 2;
        write16(mmu, cpu->sp, cpu->pc);
        cpu->pc = dest;
        return 6;
    }
    return 3;
}

static inline u32 push_rr(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->sp -= 2;
    write16(mmu, cpu->sp, *reg16_push_pop(cpu, (opcode & (u8)0x30) >> (u8)4));
    return 4;
}

static inline u32 add_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    add_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc), 0);
    return 2;
}

static inline u32 rst(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    rst_core(cpu, mmu, (u16)(opcode & (u8)0x38));
    return 4;
}

static inline u32 ret(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->pc = read16(mmu, rom, &cpu->sp);
    return 4;
}

static inline u32 cb(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    opcode = read8(mmu, rom, &cpu->pc);
    u8 reg = opcode & (u8)0x07;
    switch (opcode >> (u8)3) {
        case 0x00:
            return rlc_core(cpu, mmu, rom, reg);
        case 0x01:
            return rrc_core(cpu, mmu, rom, reg);
        case 0x02:
            return rl_core(cpu, mmu, rom, reg, get_c(cpu));
        case 0x03:
            return rr_core(cpu, mmu, rom, reg, get_c(cpu));
        case 0x04:
            return sla(cpu, mmu, rom, reg);
        case 0x05:
            return sra(cpu, mmu, rom, reg);
        case 0x06:
            return swap(cpu, mmu, rom, reg);
        case 0x07:
            return rr_core(cpu, mmu, rom, reg, 0);
        default:
            switch ((opcode & (u8)0xC0) >> (u8)6) {
                case 0x01:
                    return bit(cpu, mmu, rom, reg, (u8)1 << ((opcode & (u8)0x38) >> (u8)3));
                case 0x02:
                    return set(cpu, mmu, rom, reg, (u8)1 << ((opcode & (u8)0x38) >> (u8)3), false);
                default:
                case 0x03:
                    return set(cpu, mmu, rom, reg, (u8)1 << ((opcode & (u8)0x38) >> (u8)3), true);
            }
    }
}

static inline u32 call_u16(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    u16 dest = read16(mmu, rom, &cpu->pc);
    cpu->sp -= 2;
    write16(mmu, cpu->sp, cpu->pc);
    cpu->pc = dest;
    return 6;
}

static inline u32 adc_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    add_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc), get_c(cpu));
    return 2;
}

static inline u32 sub_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    sub_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc));
    return 2;
}

static inline u32 reti(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->pc = read16(mmu, rom, &cpu->sp);
    cpu->ime = true;
    return 4;
}

static inline u32 sbc_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    sbc_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc));
    return 2;
}

static inline u32 ldh_mem_u8_a(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, (u16)0xFF00 + (u16)read8(mmu, rom, &cpu->pc), cpu->a);
    return 3;
}

static inline u32 ldh_mem_c_a(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, (u16)0xFF00 + (u16)cpu->c, cpu->a);
    return 2;
}

static inline u32 and_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    and_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc));
    return 2;
}

static inline u32 add_sp_s8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    s32 overflow = (s32)cpu->sp + (u32)signify8(read8(mmu, rom, &cpu->pc));
    set_c(cpu, overflow > (s32)0xFFFF);
    set_h(cpu, (overflow & (s32)0x0FFF) < (cpu->sp & (u32)0x0FFF));
    cpu->sp = (u16)(overflow & (s32)0xFFFF);
    set_n(cpu, false);
    set_z(cpu, false);
    return 4;
}

static inline u32 jp_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->pc = cpu->hl;
    return 1;
}

static inline u32 ld_mm_u16_a(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    gb15_mmu_write(mmu, read16(mmu, rom, &cpu->pc), cpu->a);
    return 4;
}

static inline u32 xor_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    xor_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc));
    return 2;
}

static inline u32 ldh_a_mem_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->a = gb15_mmu_read(mmu, rom, (u16)0xFF00 + (u16)read8(mmu, rom, &cpu->pc));
    return 3;
}

static inline u32 ldh_a_mem_c(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->a = gb15_mmu_read(mmu, rom, (u16)0xFF00 + (u16)cpu->c);
    return 2;
}

static inline u32 di(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->ime = false;
    return 1;
}

static inline u32 or_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    or_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc));
    return 2;
}

static inline u32 ld_hl_sp_s8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    s32 overflow = (s32)cpu->sp + (u32)signify8(read8(mmu, rom, &cpu->pc));
    set_c(cpu, overflow > (s32)0xFFFF);
    set_h(cpu, (overflow & (s32)0x0FFF) < (cpu->hl & (u32)0x0FFF));
    cpu->hl = (u16)(overflow & (s32)0xFFFF);
    set_n(cpu, false);
    set_z(cpu, false);
    return 3;
}

static inline u32 ld_sp_hl(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->sp = cpu->hl;
    return 2;
}

static inline u32 ld_a_mem_u16(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->a = gb15_mmu_read(mmu, rom, read16(mmu, rom, &cpu->pc));
    return 4;
}

static inline u32 ei(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cpu->ime = true;
    return 1;
}

static inline u32 cp_u8(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    cp_core(cpu, mmu, rom, read8(mmu, rom, &cpu->pc));
    return 2;
}

typedef struct InstructionBundle {
    u8 opcode;
    u8 num_operands;
    const char *name;
    u32 (*function)(u8 opcode, GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom);
} InstructionBundle;

static const InstructionBundle INSTRUCTIONS[256] = {
        {0x00, 0, "nop", nop},                    {0x01, 0, "ld bc, u16", ld_rr_u16},
        {0x02, 0, "ld (bc), a", ld_mem_rr_a},     {0x03, 0, "inc bc", inc_rr},
        {0x04, 0, "inc b", inc_r},                {0x05, 0, "dec b", dec_r},
        {0x06, 1, "ld b, %.2X", ld_r_u8},         {0x07, 0, "rlca", rlca},
        {0x08, 1, "ld (%.2X), sp", ld_mem_u16_sp},{0x09, 0, "add hl, bc", add_hl_rr},
        {0x0A, 0, "ld a, (bc)", ld_a_mem_rr},     {0x0B, 0, "dec bc", dec_rr},
        {0x0C, 0, "inc c", inc_r},                {0x0D, 0, "dec c", dec_r},
        {0x0E, 1, "ld c, %.2X", ld_r_u8},         {0x0F, 0, "rrc", rrc},

        {0x10, 1, "stop %.2X", stop},             {0x11, 0, "ld de, u16", ld_rr_u16},
        {0x12, 0, "ld (de), a", ld_mem_rr_a},     {0x13, 0, "inc de", inc_rr},
        {0x14, 0, "inc d", inc_r},                {0x15, 0, "dec d", dec_r},
        {0x16, 1, "ld d, %.2X", ld_r_u8},         {0x17, 0, "rla", rla},
        {0x18, 1, "jr %.2X", jr_s8},              {0x19, 0, "add hl, de", add_hl_rr},
        {0x1A, 0, "ld a, (de)", ld_a_mem_rr},     {0x1B, 0, "dec de", dec_rr},
        {0x1C, 0, "inc e", inc_r},                {0x1D, 0, "dec e", dec_r},
        {0x1E, 1, "ld e, %.2X", ld_r_u8},         {0x1F, 0, "rra", rra},

        {0x20, 1, "jr nz, %.2X", jr_cond_s8},     {0x21, 2, "ld hl, %.4X", ld_rr_u16},
        {0x22, 0, "ld (hl+), a", ldi_hl_a},       {0x23, 0, "inc hl", inc_rr},
        {0x24, 0, "inc h", inc_r},                {0x25, 0, "dec h", dec_r},
        {0x26, 1, "ld h, %.2X", ld_r_u8},         {0x27, 0, "daa", daa},
        {0x28, 1, "jr z, %.2X", jr_cond_s8},      {0x29, 0, "add hl, hl", add_hl_rr},
        {0x2A, 0, "ld a, (hl+)", ldi_a_hl},       {0x2B, 0, "dec hl", dec_rr},
        {0x2C, 0, "inc l", inc_r},                {0x2D, 0, "dec l", dec_r},
        {0x2E, 1, "ld l, %.2X", ld_r_u8},         {0x2F, 0, "cpl", cpl},

        {0x30, 1, "jr nc, %.2X", jr_cond_s8},     {0x31, 2, "ld sp, %.4X", ld_rr_u16},
        {0x32, 0, "ld (hl-), a", ldd_hl_a},       {0x33, 0, "inc sp", inc_rr},
        {0x34, 0, "inc (hl)", inc_mem_hl},        {0x35, 0, "dec (hl)", dec_mem_hl},
        {0x36, 1, "ld (hl), %.2X", ld_mem_hl_n},  {0x37, 0, "scf", scf},
        {0x38, 1, "jr c, %.2X", jr_cond_s8},      {0x39, 0, "add hl, sp", add_hl_rr},
        {0x3A, 0, "ld a, (hl-)", ldd_a_hl},       {0x3B, 0, "dec sp", dec_rr},
        {0x3C, 0, "inc a", inc_r},                {0x3D, 0, "dec a", dec_r},
        {0x3E, 1, "ld a, %.2X", ld_r_u8},         {0x3F, 0, "ccf", ccf},

        {0x40, 0, "ld b, b", ld_r_r},             {0x41, 0, "ld b, c", ld_r_r},
        {0x42, 0, "ld b, d", ld_r_r},             {0x43, 0, "ld b, e", ld_r_r},
        {0x44, 0, "ld b, h", ld_r_r},             {0x45, 0, "ld b, l", ld_r_r},
        {0x46, 0, "ld b, (hl)", ld_r_mem_hl},     {0x47, 0, "ld b, a", ld_r_r},
        {0x48, 0, "ld c, b", ld_r_r},             {0x49, 0, "ld c, c", ld_r_r},
        {0x4A, 0, "ld c, d", ld_r_r},             {0x4B, 0, "ld c, e", ld_r_r},
        {0x4C, 0, "ld c, h", ld_r_r},             {0x4D, 0, "ld c, l", ld_r_r},
        {0x4E, 0, "ld c, (hl)", ld_r_mem_hl},     {0x4F, 0, "ld c, a", ld_r_r},

        {0x50, 0, "ld d, b", ld_r_r},             {0x51, 0, "ld d, c", ld_r_r},
        {0x52, 0, "ld d, d", ld_r_r},             {0x53, 0, "ld d, e", ld_r_r},
        {0x54, 0, "ld d, h", ld_r_r},             {0x55, 0, "ld d, l", ld_r_r},
        {0x56, 0, "ld d, (hl)", ld_r_mem_hl},     {0x57, 0, "ld d, a", ld_r_r},
        {0x58, 0, "ld e, b", ld_r_r},             {0x59, 0, "ld e, c", ld_r_r},
        {0x5A, 0, "ld e, d", ld_r_r},             {0x5B, 0, "ld e, e", ld_r_r},
        {0x5C, 0, "ld e, h", ld_r_r},             {0x5D, 0, "ld e, l", ld_r_r},
        {0x5E, 0, "ld e, (hl)", ld_r_mem_hl},     {0x5F, 0, "ld e, a", ld_r_r},

        {0x60, 0, "ld h, b", ld_r_r},             {0x61, 0, "ld h, c", ld_r_r},
        {0x62, 0, "ld h, d", ld_r_r},             {0x63, 0, "ld h, e", ld_r_r},
        {0x64, 0, "ld h, h", ld_r_r},             {0x65, 0, "ld h, l", ld_r_r},
        {0x66, 0, "ld h, (hl)", ld_r_mem_hl},     {0x67, 0, "ld h, a", ld_r_r},
        {0x68, 0, "ld l, b", ld_r_r},             {0x69, 0, "ld l, c", ld_r_r},
        {0x6A, 0, "ld l, d", ld_r_r},             {0x6B, 0, "ld l, e", ld_r_r},
        {0x6C, 0, "ld l, h", ld_r_r},             {0x6D, 0, "ld l, l", ld_r_r},
        {0x6E, 0, "ld l, (hl)", ld_r_mem_hl},     {0x6F, 0, "ld l, a", ld_r_r},

        {0x70, 0, "ld (hl), b", ld_mem_hl_r},     {0x71, 0, "ld (hl), c", ld_mem_hl_r},
        {0x72, 0, "ld (hl), d", ld_mem_hl_r},     {0x73, 0, "ld (hl), e", ld_mem_hl_r},
        {0x74, 0, "ld (hl), h", ld_mem_hl_r},     {0x75, 0, "ld (hl), l", ld_mem_hl_r},
        {0x76, 0, "halt", halt},                  {0x77, 0, "ld (hl), a", ld_mem_hl_r},
        {0x78, 0, "ld a, b", ld_r_r},             {0x79, 0, "ld a, c", ld_r_r},
        {0x7A, 0, "ld a, d", ld_r_r},             {0x7B, 0, "ld a, e", ld_r_r},
        {0x7C, 0, "ld a, h", ld_r_r},             {0x7D, 0, "ld a, l", ld_r_r},
        {0x7E, 0, "ld a, (hl)", ld_r_mem_hl},     {0x7F, 0, "ld a, a", ld_r_r},

        {0x80, 0, "add b", add_r},                {0x81, 0, "add c", add_r},
        {0x82, 0, "add d", add_r},                {0x83, 0, "add e", add_r},
        {0x84, 0, "add h", add_r},                {0x85, 0, "add l", add_r},
        {0x86, 0, "add (hl)", add_mem_hl},        {0x87, 0, "add a", add_r},
        {0x88, 0, "adc b", adc_r},                {0x89, 0, "adc c", adc_r},
        {0x8A, 0, "adc d", adc_r},                {0x8B, 0, "adc e", adc_r},
        {0x8C, 0, "adc h", adc_r},                {0x8D, 0, "adc l", adc_r},
        {0x8E, 0, "adc (hl)", adc_mem_hl},        {0x8F, 0, "adc a", adc_r},

        {0x90, 0, "sub b", sub_r},                {0x91, 0, "sub c", sub_r},
        {0x92, 0, "sub d", sub_r},                {0x93, 0, "sub e", sub_r},
        {0x94, 0, "sub h", sub_r},                {0x95, 0, "sub l", sub_r},
        {0x96, 0, "sub (hl)", sub_mem_hl},        {0x97, 0, "sub a", sub_r},
        {0x98, 0, "sbc b", sbc_r},                {0x99, 0, "sbc c", sbc_r},
        {0x9A, 0, "sbc d", sbc_r},                {0x9B, 0, "sbc e", sbc_r},
        {0x9C, 0, "sbc h", sbc_r},                {0x9D, 0, "sbc l", sbc_r},
        {0x9E, 0, "sbc (hl)", sbc_mem_hl},        {0x9F, 0, "sbc a", sbc_r},

        {0xA0, 0, "and b", and_r},                {0xA1, 0, "and c", and_r},
        {0xA2, 0, "and d", and_r},                {0xA3, 0, "and e", and_r},
        {0xA4, 0, "and h", and_r},                {0xA5, 0, "and l", and_r},
        {0xA6, 0, "and (hl)", and_mem_hl},        {0xA7, 0, "and a", and_r},
        {0xA8, 0, "xor b", xor_r},                {0xA9, 0, "xor c", xor_r},
        {0xAA, 0, "xor d", xor_r},                {0xAB, 0, "xor e", xor_r},
        {0xAC, 0, "xor h", xor_r},                {0xAD, 0, "xor l", xor_r},
        {0xAE, 0, "xor (hl)", xor_mem_hl},        {0xAF, 0, "xor a", xor_r},

        {0xB0, 0, "or b", or_r},                  {0xB1, 0, "or c", or_r},
        {0xB2, 0, "or d", or_r},                  {0xB3, 0, "or e", or_r},
        {0xB4, 0, "or h", or_r},                  {0xB5, 0, "or l", or_r},
        {0xB6, 0, "or (hl)", or_mem_hl},          {0xB7, 0, "or a", or_r},
        {0xB8, 0, "cp b", cp_r},                  {0xB9, 0, "cp c", cp_r},
        {0xBA, 0, "cp d", cp_r},                  {0xBB, 0, "cp e", cp_r},
        {0xBC, 0, "cp h", cp_r},                  {0xBD, 0, "cp l", cp_r},
        {0xBE, 0, "cp (hl)", cp_mem_hl},          {0xBF, 0, "cp a", cp_r},

        {0xC0, 0, "ret nz", ret_cond},            {0xC1, 0, "pop bc", pop_rr},
        {0xC2, 2, "jp nz, %.4X", jp_cond_u16},    {0xC3, 2, "jp %.4X", jp_u16},
        {0xC4, 2, "call nz, %.4X", call_cond_u16},{0xC5, 0, "push bc", push_rr},
        {0xC6, 1, "add a, %.2X", add_u8},         {0xC7, 0, "rst 00", rst},
        {0xC8, 0, "ret z", ret_cond},             {0xC9, 0, "ret", ret},
        {0xCA, 2, "jp z, %.4X", jp_cond_u16},     {0xCB, 0, "CB", cb},
        {0xCC, 2, "call z, %.4X", call_cond_u16}, {0xCD, 2, "call %.4X", call_u16},
        {0xCE, 1, "adc %.2X", adc_u8},            {0xCF, 0, "rst 08", rst},

        {0xD0, 0, "ret nc", ret_cond},            {0xD1, 0, "pop de", pop_rr},
        {0xD2, 2, "jp nc, %.4X", jp_cond_u16},    {0xD3, 0, "invalid", nop},
        {0xD4, 2, "call nc, %.4X", call_cond_u16},{0xD5, 0, "push de", push_rr},
        {0xD6, 1, "sub %.2X", sub_u8},            {0xD7, 0, "rst 10", rst},
        {0xD8, 0, "ret c", ret_cond},             {0xD9, 0, "reti", reti},
        {0xDA, 2, "jp c, %.4X", jp_cond_u16},     {0xDB, 0, "invalid", nop},
        {0xDC, 2, "call c, %.4X", call_cond_u16}, {0xDD, 0, "invalid", nop},
        {0xDE, 1, "sbc %.2X", sbc_u8},            {0xDF, 0, "rst 18", nop},

        {0xE0, 1, "ldh (%.2X), a", ldh_mem_u8_a}, {0xE1, 0, "pop hl", pop_rr},
        {0xE2, 0, "ldh (c), a", ldh_mem_c_a},     {0xE3, 0, "invalid", nop},
        {0xE4, 0, "invalid", nop},                {0xE5, 0, "push hl", push_rr},
        {0xE6, 1, "and %.2X", and_u8},            {0xE7, 0, "rst 20", rst},
        {0xE8, 1, "add sp, %.2X", add_sp_s8},     {0xE9, 0, "jp hl", jp_hl},
        {0xEA, 2, "ld (%.4X), a", ld_mm_u16_a},   {0xEB, 0, "invalid", nop},
        {0xEC, 0, "invalid", nop},                {0xED, 0, "invalid", nop},
        {0xEE, 1, "xor %.2X", xor_u8},            {0xEF, 0, "rst 28", rst},

        {0xF0, 1, "ldh a, (%.2X)", ldh_a_mem_u8}, {0xF1, 0, "pop af", pop_rr},
        {0xF2, 0, "ldh a, (c)", ldh_a_mem_c},     {0xF3, 0, "di", di},
        {0xF4, 0, "invalid", nop},                {0xF5, 0, "push af", push_rr},
        {0xF6, 1, "or %.2X", or_u8},              {0xF7, 0, "rst 30", rst},
        {0xF8, 1, "ld hl, sp+%.2X", ld_hl_sp_s8}, {0xF9, 0, "ld sp, hl", ld_sp_hl},
        {0xFA, 2, "ld a, (%.4X)", ld_a_mem_u16},  {0xFB, 0, "ei", ei},
        {0xFC, 0, "invalid", nop},                {0xFD, 0, "invalid", nop},
        {0xFE, 1, "cp %.2X", cp_u8},              {0xFF, 0, "rst 38", rst}
};

static inline void service_interrupts(GB15Cpu *cpu, GB15Mmu *mmu, u8 *rom) {
    if (!cpu->ime) {
        return;
    }
    u8 ints = mmu->io[GB15_IO_IF] & mmu->io[GB15_IO_IE];
    if (ints == 0x00) {
        return;
    }
    if (ints & 0x01) { // vblank
        rst_core(cpu, mmu, 0x0040);
        mmu->io[GB15_IO_IF] ^= 0x01;
        cpu->ime = false;
        return;
    }
    if (ints & 0x02) { // lcd stat
        rst_core(cpu, mmu, 0x0048);
        mmu->io[GB15_IO_IF] ^= 0x02;
        cpu->ime = false;
        return;
    }
    if (ints & 0x04) { // timer
        rst_core(cpu, mmu, 0x0050);
        mmu->io[GB15_IO_IF] ^= 0x04;
        cpu->ime = false;
        return;
    }
    if (ints & 0x08) { // serial
        rst_core(cpu, mmu, 0x0058);
        mmu->io[GB15_IO_IF] ^= 0x08;
        cpu->ime = false;
        return;
    }
    if (ints & 0x10) { // joypad
        rst_core(cpu, mmu, 0x0060);
        mmu->io[GB15_IO_IF] ^= 0x10;
        cpu->ime = false;
        return;
    }
}

static inline u32 cpu_tick(GB15State *state, u8 *rom) {
    GB15Mmu *mmu = &state->mmu;
    GB15Cpu *cpu = &state->cpu;
    if (cpu->halted) {
        if (cpu->halt_flags != mmu->io[GB15_IO_IF]) {
            cpu->halted = false;
        }
        return 1;
    }
    service_interrupts(cpu, mmu, rom);
    u8 opcode = read8(mmu, rom, &cpu->pc);
    const InstructionBundle *bundle = INSTRUCTIONS + opcode;
//    printf("af=%.4X|bc=%.4X|de=%.4X|hl=%.4X|pc=%.4X|sp=%.4X :: ",
//           cpu->af,
//           cpu->bc,
//           cpu->de,
//           cpu->hl,
//           cpu->pc - 1,
//           cpu->sp
//    );
//    u16 tmp_pc = cpu->pc;
//    if (bundle->num_operands == 1) {
//        printf(bundle->name, read8(mmu, rom, &tmp_pc));
//    } else if (bundle->num_operands == 2) {
//        printf(bundle->name, read16(mmu, rom, &tmp_pc));
//    } else {
//        printf(bundle->name);
//    }
//    printf("\n\tlcdc=%.2X|stat=%.2X|ly=%.2X|ie=%.2X|if=%.2X|*sp=%.2X%.2X\n",
//           mmu->io[GB15_IO_LCDC],
//           mmu->io[GB15_IO_STAT],
//           mmu->io[GB15_IO_LY],
//           mmu->io[GB15_IO_IE],
//           mmu->io[GB15_IO_IF],
//           gb15_mmu_read(mmu, rom, cpu->sp + (u16)1),
//           gb15_mmu_read(mmu, rom, cpu->sp)
//    );
    return bundle->function(opcode, cpu, mmu, rom);
}

void gb15_tick(GB15State *state, u8 *rom, GB15VBlankCallback vblank, void *userdata) {
    u32 cycles = cpu_tick(state, rom);
    gb15_gpu_tick(state, rom, vblank, userdata, cycles);
}

void gb15_boot(GB15State *state)
{
    gb15_gpu_init(state);
    state->cpu.ime  = true;
    GB15Mmu *mmu = &state->mmu;
    mmu->io[GB15_IO_STAT] = 0x84;
    mmu->io[GB15_IO_IF] = 0xE1;
}