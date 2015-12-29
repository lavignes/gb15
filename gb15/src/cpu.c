#include <stdio.h>

#include <gb15/gb15.h>

static inline s8 signify8(u8 value) {
    if (value <= (u8)0x7F) {
        return value;
    }
    return -(s8)(((~value) + (u8)1) & (u8)0xFF);
}

static inline u8 read8(GB15MemMap *memmap, u8 *rom, u16 *pc) {
    u8 tmp8 = gb15_memmap_read(memmap, rom, *pc);
    (*pc)++;
    return tmp8;
}

static inline u16 read16(GB15MemMap *memmap, u8 *rom, u16 *pc) {
    GB15LongRegister tmp16;
    tmp16.l = read8(memmap, rom, pc);
    tmp16.h = read8(memmap, rom, pc);
    return tmp16.value;
}

static inline void write16(GB15MemMap *memmap, u16 address, u16 value) {
    GB15LongRegister tmp16;
    tmp16.value = value;
    gb15_memmap_write(memmap, address, tmp16.l);
    gb15_memmap_write(memmap, address + (u16)1, tmp16.h);
}

static inline void set_z(GB15RegFile *regfile, bool value) {
    regfile->f = (regfile->f & (u8)0x7F) | (value << (u8)7);
}

static inline void set_n(GB15RegFile *regfile, bool value) {
    regfile->f = (regfile->f & (u8)0xBF) | (value << (u8)6);
}

static inline void set_h(GB15RegFile *regfile, bool value) {
    regfile->f = (regfile->f & (u8)0xDF) | (value << (u8)5);
}

static inline void set_c(GB15RegFile *regfile, bool value) {
    regfile->f = (regfile->f & (u8)0xEF) | (value << (u8)4);
}

static inline u8 get_z(GB15RegFile *regfile) {
    return (regfile->f & (u8)0x80) >> (u8)7;
}

static inline u8 get_h(GB15RegFile *regfile) {
    return (regfile->f & (u8)0x40) >> (u8)6;
}

static inline u8 get_n(GB15RegFile *regfile) {
    return (regfile->f & (u8)0x20) >> (u8)5;
}

static inline u8 get_c(GB15RegFile *regfile) {
    return (regfile->f & (u8)0x10) >> (u8)4;
}

static inline u8 *reg8(GB15RegFile *regfile, u8 reg) {
    switch (reg) {
        case 0x07:
            return &regfile->a;
        case 0x00:
            return &regfile->b;
        case 0x01:
            return &regfile->c;
        case 0x02:
            return &regfile->d;
        case 0x03:
            return &regfile->e;
        case 0x04:
            return &regfile->h;
        case 0x05:
            return &regfile->l;
        default:
            return NULL;
    }
}

static inline u16 *reg16(GB15RegFile *regfile, u8 reg) {
    switch (reg) {
        case 0x00:
            return &regfile->bc;
        case 0x01:
            return &regfile->de;
        case 0x02:
            return &regfile->hl;
        case 0x03:
            return &regfile->sp;
        default:
            return NULL;
    }
}

static inline u16 *reg16_push_pop(GB15RegFile *regfile, u8 reg) {
    switch (reg) {
        case 0x00:
            return &regfile->bc;
        case 0x01:
            return &regfile->de;
        case 0x02:
            return &regfile->hl;
        case 0x03:
            return &regfile->af;
        default:
            return NULL;
    }
}

static inline void add_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 value, u8 carry) {
    u16 overflow = (u16)regfile->a + (u16)value + (u16)carry;
    set_h(regfile, (((u16)regfile->a & (u16)0xF) + ((u16)value & (u16)0xF) + (u16)carry) > (u16)0xF);
    set_c(regfile, (u16)overflow > (u16)0xFF);
    set_n(regfile, false);
    regfile->a = (u8)overflow & (u8)0xFF;
    set_z(regfile, regfile->a == (u8)0x00);
}

static inline void sub_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 value) {
    s16 overflow = (s16)regfile->a - (s16)value;
    set_h(regfile, (((s16)regfile->a - (s16)value) & (s16)0xF) > ((s16)regfile->a & (s16)0xF));
    set_c(regfile, (s16)overflow < (s16)0x00);
    set_n(regfile, true);
    regfile->a = (u8)overflow & (u8)0xFF;
    set_z(regfile, regfile->a == (u8)0x00);
}

static inline void sbc_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 value) {
    s16 overflow = (s16)regfile->a - ((s16)value + (s16)get_c(regfile));
    set_h(regfile, (((s16)regfile->a & (s16)0xF) - ((s16)regfile->b & (s16)0xF) - (s16)get_c(regfile)) < (s16)0x00);
    set_c(regfile, (s16)overflow < (s16)0x00);
    set_n(regfile, true);
    regfile->a = (u8)overflow & (u8)0xFF;
    set_z(regfile, regfile->a == (u8)0x00);
}

static inline void and_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 value) {
    regfile->a &= value;
    set_z(regfile, regfile->a == (u8)0x00);
    set_h(regfile, true);
    set_n(regfile, false);
    set_c(regfile, false);
}

static inline void xor_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 value) {
    regfile->a ^= value;
    set_z(regfile, regfile->a == (u8)0x00);
    set_h(regfile, false);
    set_n(regfile, false);
    set_c(regfile, false);
}

static inline void or_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 value) {
    regfile->a |= value;
    set_z(regfile, regfile->a == (u8)0x00);
    set_h(regfile, false);
    set_n(regfile, false);
    set_c(regfile, false);
}

static inline void cp_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 value) {
    s16 overflow = (s16)regfile->a - (s16)value;
    set_h(regfile, (((s16)regfile->a - (s16)value) & (s16)0xF) > ((s16)regfile->a & (s16)0xF));
    set_c(regfile, (s16)overflow < (s16)0x00);
    set_n(regfile, true);
    set_z(regfile, regfile->a == value);
}

static inline void rlc_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg) {
    u8 carry;
    u8 *dest = reg8(regfile, reg);
    if (dest) { // a - l
        state->tclocks = 8;
        carry = (*dest & (u8)0x80) >> (u8)7;
        *dest = (*dest << (u8)1) | carry;
        set_c(regfile, carry);
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        carry = (reg & (u8)0x80) >> (u8)7;
        reg = (reg << (u8)1) | carry;
        set_c(regfile, carry);
        set_z(regfile, reg == (u8)0x00);
        gb15_memmap_write(memmap, regfile->hl, reg);
    }
    set_n(regfile, false);
    set_h(regfile, false);
}

static inline void rrc_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg) {
    u8 carry;
    u8 *dest = reg8(regfile, reg);
    if (dest) { // a - l
        state->tclocks = 8;
        carry = *dest & (u8)0x01;
        *dest = (*dest >> (u8)1) | (carry << (u8)7);
        set_c(regfile, carry);
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        carry = reg & (u8)0x01;
        reg = (reg >> (u8)1) | (carry << (u8)7);
        set_c(regfile, carry);
        set_z(regfile, reg  == (u8)0x00);
        gb15_memmap_write(memmap, regfile->hl, reg);
    }
    set_n(regfile, false);
    set_h(regfile, false);
}

static inline void rl_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg, u8 carry) {
    u8 *dest = reg8(regfile, reg);
    if (dest) { // a - l
        state->tclocks = 8;
        set_c(regfile, (*dest & (u8)0x80) >> (u8)7);
        *dest = (*dest << (u8)1) | carry;
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        set_c(regfile, (reg & (u8)0x80) >> (u8)7);
        reg = (reg << (u8)1) | carry;
        set_z(regfile, reg == (u8)0x00);
        gb15_memmap_write(memmap, regfile->hl, reg);
    }
    set_n(regfile, false);
    set_h(regfile, false);
}

static inline void rr_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg, u8 carry) {
    u8 *dest = reg8(regfile, reg);
    if (dest) { // a - l
        state->tclocks = 8;
        set_c(regfile, *dest & (u8)0x01);
        *dest = (*dest >> (u8)1) | (carry << (u8)7);
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        set_c(regfile, reg & (u8)0x01);
        reg = (reg >> (u8)1) | (carry << (u8)7);
        set_z(regfile, reg == (u8)0x00);
        gb15_memmap_write(memmap, regfile->hl, reg);
    }
    set_n(regfile, false);
    set_h(regfile, false);
}

static inline bool test_cond(GB15RegFile *regfile, u8 cond) {
    switch (cond) {
        case 0x00:
            return !get_z(regfile);
        case 0x01:
            return get_z(regfile);
        case 0x02:
            return !get_c(regfile);
        case 0x03:
            return get_c(regfile);
        default:
            break;
    }
    return false;
}

static inline void sla(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg) {
    rl_core(state, regfile, memmap, rom, reg, 0);
}

static inline void sra(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg) {
    u8 *dest = reg8(regfile, reg);
    if (dest) { // a - l
        state->tclocks = 8;
        set_c(regfile, *dest & (u8)0x01);
        *dest = (*dest >> (u8)1) | (*dest & (u8)0x80);
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        set_c(regfile, reg & (u8)0x01);
        reg = (reg >> (u8)1) | (reg & (u8)0x80);
        set_z(regfile, reg == (u8)0x00);
        gb15_memmap_write(memmap, regfile->hl, reg);
    }
    set_n(regfile, false);
    set_h(regfile, false);
}

static inline void swap(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg) {
    u8 *dest = reg8(regfile, reg);
    if (dest) { // a - l
        state->tclocks = 8;
        *dest = ((*dest & (u8)0xF0) >> (u8)4) & ((*dest & (u8)0x0F) << (u8)4);
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        reg = ((reg & (u8)0xF0) >> (u8)4) & ((reg & (u8)0x0F) << (u8)4);
        set_z(regfile, reg == (u8)0x00);
        gb15_memmap_write(memmap, regfile->hl, reg);
    }
    set_c(regfile, false);
    set_n(regfile, false);
    set_h(regfile, false);
}

static inline void bit(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg, u8 mask) {
    u8 *dest = reg8(regfile, reg);
    if (dest) {
        state->tclocks = 8;
        set_z(regfile, (*dest & mask) == (u8)0x00);
    } else {
        state->tclocks = 16;
        set_z(regfile, (gb15_memmap_read(memmap, rom, regfile->hl) & mask) == (u8)0x00);
    }
    set_n(regfile, false);
    set_h(regfile, true);
}

static inline void set(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom, u8 reg, u8 bit, bool value) {
    u8 *dest = reg8(regfile, reg);
    if (dest) {
        state->tclocks = 8;
        *dest |= (u8)value << bit;
    } else {
        state->tclocks = 16;
        gb15_memmap_write(memmap, regfile->hl, gb15_memmap_read(memmap, rom, regfile->hl) | ((u8)value << bit));
    }
}

static inline void rst_core(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u16 dest) {
    state->tclocks = 16;
    regfile->sp -= 2;
    write16(memmap, regfile->sp, regfile->pc);
    regfile->pc = dest;
}

static inline void nop(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
}

static inline void ld_rr_u16(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    *reg16(regfile, (opcode & (u8)0x30) >> (u8)4) = read16(memmap, rom, &regfile->pc);
}

static inline void ld_mem_rr_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, *reg16(regfile, (opcode & (u8)0x30) >> (u8)4), 0x07);
}

static inline void inc_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    (*reg16(regfile, (opcode & (u8)0x30) >> (u8)4))++;
}

static inline void inc_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    u8 *reg = reg8(regfile, (opcode & (u8)0x38) >> (u8)3);
    set_h(regfile, (*reg & 0xF) == (u8)0x0);
    (*reg)++;
    set_z(regfile, *reg == (u8)0x00);
    set_n(regfile, false);
}

static inline void dec_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    u8 *reg = reg8(regfile, (opcode & (u8)0x38) >> (u8)3);
    (*reg)--;
    set_z(regfile, *reg == (u8)0x00);
    set_n(regfile, true);
    set_h(regfile, (*reg & 0xF) == (u8)0xF);
}

static inline void ld_r_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    *reg8(regfile, (opcode & (u8)0x38) >> (u8)3) = read8(memmap, rom, &regfile->pc);
}

static inline void rlca(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rlc_core(state, regfile, memmap, rom, 0x07);
    state->tclocks = 4;
    set_z(regfile, false);
}

static inline void ld_mem_u16_sp(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    write16(memmap, read16(memmap, rom, &regfile->pc), regfile->sp);
}

static inline void ld_a_mem_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, *reg16(regfile, (opcode & (u8)0x30) >> (u8)4));
}

static inline void add_hl_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    u32 overflow = (u32)regfile->hl + (u32)*reg16(regfile, (opcode & (u8)0x30) >> (u8)4);
    set_n(regfile, false);
    set_c(regfile, overflow > (u32)0xFFFF);
    set_h(regfile, (overflow & (u32)0x0FFF) < (regfile->hl & (u32)0x0FFF));
    regfile->hl = (u16)(overflow & (u32)0xFFFF);
}

static inline void dec_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    (*reg16(regfile, (opcode & (u8)0x30) >> (u8)4))--;
}

static inline void rrc(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rrc_core(state, regfile, memmap, rom, opcode & (u8)0x07);
}

static inline void stop(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    state->stopped = true;
    read8(memmap, rom, &regfile->pc);
}

static inline void rla(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rl_core(state, regfile, memmap, rom, 0x07, get_c(regfile));
    state->tclocks = 4;
    set_z(regfile, false);
}

static inline void jr_s8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    regfile->pc += signify8(read8(memmap, rom, &regfile->pc));
}

static inline void rra(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rr_core(state, regfile, memmap, rom, 0x07, get_c(regfile));
    state->tclocks = 4;
    set_z(regfile, false);
}

static inline void jr_cond_s8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u8 dest = read8(memmap, rom, &regfile->pc);
    if (test_cond(regfile, (opcode & (u8)0x18) >> (u8)3)) {
        state->tclocks = 12;
        regfile->pc += signify8(dest);
    } else {
        state->tclocks = 8;
    }
}

static inline void ldi_hl_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, regfile->hl, regfile->a);
    regfile->hl++;
}

static inline void daa(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    u16 overflow = regfile->a;
    if (get_n(regfile)) {
        if (get_h(regfile)) {
            overflow = (overflow - (u16)0x06) & (u16)0xFF;
        }
        if (get_c(regfile)) {
            overflow -= (u16)0x60;
        }
    } else {
        if (get_h(regfile) || ((overflow & (u16)0x0F) > (u16)9)) {
            overflow += (u16)0x06;
        }
        if (get_c(regfile) || (overflow > (u16)0x9F)) {
            overflow += (u16)0x60;
        }
    }
    regfile->a = (u8)(overflow & (u16)0xFF);
    set_h(regfile, false);
    set_z(regfile, regfile->a == (u8)0x00);
    if (overflow >= (u16)0x100) {
        set_c(regfile, true);
    }
}

static inline void ldi_a_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, regfile->hl);
    regfile->hl++;
}

static inline void cpl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    regfile->a = ~regfile->a;
    set_n(regfile, false);
    set_h(regfile, false);
}

static inline void ldd_hl_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, regfile->hl, regfile->a);
    regfile->hl--;
}

static inline void inc_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    u32 overflow = gb15_memmap_read(memmap, rom, regfile->hl);
    overflow = (overflow + (u32)1) & (u32)0xFF;
    gb15_memmap_write(memmap, regfile->hl, (u8)overflow);
    set_z(regfile, overflow == (u32)0x00);
    set_n(regfile, false);
    set_h(regfile, (overflow & (u32)0xF) == (u32)0x0);
}

static inline void dec_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    u32 overflow = gb15_memmap_read(memmap, rom, regfile->hl);
    overflow = (overflow - (u32)1) & (u32)0xFF;
    gb15_memmap_write(memmap, regfile->hl, (u8)overflow);
    set_z(regfile, overflow == (u32)0x00);
    set_n(regfile, false);
    set_h(regfile, (overflow & (u32)0xF) == (u32)0xF);
}

static inline void ld_mem_hl_n(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    gb15_memmap_write(memmap, regfile->hl, read8(memmap, rom, &regfile->pc));
}

static inline void scf(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    set_n(regfile, false);
    set_h(regfile, false);
    set_c(regfile, true);
}

static inline void ldd_a_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, regfile->hl);
    regfile->hl++;
}

static inline void ccf(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    set_n(regfile, false);
    set_h(regfile, false);
    set_c(regfile, !get_c(regfile));
}

static inline void ld_r_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    *reg8(regfile, (opcode & (u8)0x38) >> (u8)3) = *reg8(regfile, opcode & (u8)0x07);
}

static inline void ld_r_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    *reg8(regfile, (opcode & (u8)0x38) >> (u8)3) = gb15_memmap_read(memmap, rom, regfile->hl);
}

static inline void ld_mem_hl_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, regfile->hl, *reg8(regfile, opcode & (u8)0x07));
}

static inline void halt(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    state->halted = true;
}

static inline void add_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    add_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07), 0);
}

static inline void add_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    add_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl), 0);
}

static inline void adc_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    add_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07), get_c(regfile));
}

static inline void adc_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    add_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl), get_c(regfile));
}

static inline void sub_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    sub_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07));
}

static inline void sub_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    sub_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void sbc_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    sbc_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07));
}

static inline void sbc_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    sbc_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void and_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    and_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07));
}

static inline void and_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    and_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void xor_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    xor_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07));
}

static inline void xor_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    xor_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void or_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    or_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07));
}

static inline void or_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    or_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void cp_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    cp_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0x07));
}

static inline void cp_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    cp_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void ret_cond(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u8 dest = read8(memmap, rom, &regfile->sp);
    if (test_cond(regfile, (opcode & (u8)0x18) >> (u8)3)) {
        state->tclocks = 20;
        regfile->pc += signify8(dest);
    } else {
        state->tclocks = 8;
    }
}

static inline void pop_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    *reg16_push_pop(regfile, (opcode & (u8)0x30) >> (u8)4) = read16(memmap, rom, &regfile->sp);
}

static inline void jp_cond_u16(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u16 dest = read16(memmap, rom, &regfile->pc);
    if (test_cond(regfile, (opcode & (u8)0x18) >> (u8)3)) {
        state->tclocks = 16;
        regfile->pc = dest;
    } else {
        state->tclocks = 12;
    }
}

static inline void jp_u16(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    regfile->pc = read16(memmap, rom, &regfile->pc);
}

static inline void call_cond_u16(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u16 dest = read16(memmap, rom, &regfile->pc);
    if (test_cond(regfile, (opcode & (u8)0x18) >> (u8)3)) {
        state->tclocks = 24;
        regfile->sp -= 2;
        write16(memmap, regfile->sp, regfile->pc);
        regfile->pc = dest;
    } else {
        state->tclocks = 12;
    }
}

static inline void push_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    regfile->sp -= 2;
    write16(memmap, regfile->sp, *reg16_push_pop(regfile, (opcode & (u8)0x30) >> (u8)4));
}

static inline void add_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    add_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc), 0);
}

static inline void rst(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rst_core(state, regfile, memmap, (u16)(opcode & (u8)0x38));
}

static inline void ret(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    regfile->pc = read16(memmap, rom, &regfile->sp);
}

static inline void cb(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    opcode = read8(memmap, rom, &regfile->pc);
    u8 reg = opcode & (u8)0x07;
    switch (opcode >> (u8)3) {
        case 0x00:
            rlc_core(state, regfile, memmap, rom, reg);
            break;
        case 0x01:
            rrc_core(state, regfile, memmap, rom, reg);
            break;
        case 0x02:
            rl_core(state, regfile, memmap, rom, reg, get_c(regfile));
            break;
        case 0x03:
            rr_core(state, regfile, memmap, rom, reg, get_c(regfile));
            break;
        case 0x04:
            sla(state, regfile, memmap, rom, reg);
            break;
        case 0x05:
            sra(state, regfile, memmap, rom, reg);
            break;
        case 0x06:
            swap(state, regfile, memmap, rom, reg);
            break;
        case 0x07:
            rr_core(state, regfile, memmap, rom, reg, 0);
            break;
        default:
            switch ((opcode & (u8)0xC0) >> (u8)6) {
                case 0x01:
                    bit(state, regfile, memmap, rom, reg, (u8)1 << ((opcode & (u8)0x38) >> (u8)3));
                    break;
                case 0x02:
                    set(state, regfile, memmap, rom, reg, (u8)1 << ((opcode & (u8)0x38) >> (u8)3), false);
                    break;
                default:
                case 0x03:
                    set(state, regfile, memmap, rom, reg, (u8)1 << ((opcode & (u8)0x38) >> (u8)3), true);
                    break;
            }
    }
}

static inline void call_u16(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 24;
    regfile->sp -= 2;
    u16 dest = read16(memmap, rom, &regfile->pc);
    write16(memmap, regfile->sp, regfile->pc);
    regfile->pc = dest;
}

static inline void adc_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    add_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc), get_c(regfile));
}

static inline void sub_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    sub_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc));
}

static inline void reti(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    state->ei_mclocks = 2;
    memmap->io[GB15_IO_IE] = 0x01;
    regfile->pc = read16(memmap, rom, &regfile->sp);
}

static inline void sbc_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    sbc_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc));
}

static inline void ldh_mem_u8_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    gb15_memmap_write(memmap, (u16)0xFF00 + (u16)read8(memmap, rom, &regfile->pc), regfile->a);
}

static inline void ldh_mem_c_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, (u16)0xFF00 + (u16)regfile->c, regfile->a);
}

static inline void and_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    and_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc));
}

static inline void add_sp_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    s32 overflow = (s32)regfile->sp + (u32)signify8(read8(memmap, rom, &regfile->pc));
    set_c(regfile, overflow > (s32)0xFFFF);
    set_h(regfile, (overflow & (s32)0x0FFF) < (regfile->sp & (u32)0x0FFF));
    regfile->sp = (u16)(overflow & (s32)0xFFFF);
    set_n(regfile, false);
    set_z(regfile, false);
}

static inline void jp_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    regfile->pc = regfile->hl;
}

static inline void ld_mm_u16_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    gb15_memmap_write(memmap, read16(memmap, rom, &regfile->pc), regfile->a);
}

static inline void xor_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    xor_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc));
}

static inline void ldh_a_mem_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    regfile->a = gb15_memmap_read(memmap, rom, (u16)0xFF00 + (u16)read8(memmap, rom, &regfile->pc));
}

static inline void ldh_a_mem_c(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, (u16)0xFF00 + (u16)regfile->c);
}

static inline void di(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    state->di_mclocks = 2;
}

static inline void or_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    or_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc));
}

static inline void ld_hl_sp_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    s32 overflow = (s32)regfile->sp + (u32)signify8(read8(memmap, rom, &regfile->pc));
    set_c(regfile, overflow > (s32)0xFFFF);
    set_h(regfile, (overflow & (s32)0x0FFF) < (regfile->hl & (u32)0x0FFF));
    regfile->hl = (u16)(overflow & (s32)0xFFFF);
    set_n(regfile, false);
    set_z(regfile, false);
}

static inline void ld_sp_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    regfile->sp = regfile->hl;
}

static inline void ld_a_mem_u16(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    regfile->a = gb15_memmap_read(memmap, rom, read16(memmap, rom, &regfile->pc));
}

static inline void ei(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    state->ei_mclocks = 2;
}

static inline void cp_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    cp_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc));
}

typedef struct InstructionBundle {
    u8 opcode;
    u8 num_operands;
    const char *name;
    void (*function)(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom);
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

        {0x10, 0, "stop", stop},                  {0x11, 0, "ld de, u16", ld_rr_u16},
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
        {0xE8, 1, "add sp, %.2X", add_sp_u8},     {0xE9, 0, "jp hl", jp_hl},
        {0xEA, 2, "ld (%.4X), a", ld_mm_u16_a},   {0xEB, 0, "invalid", nop},
        {0xEC, 0, "invalid", nop},                {0xED, 0, "invalid", nop},
        {0xEE, 1, "xor %.2X", xor_u8},            {0xEF, 0, "rst 28", rst},

        {0xF0, 1, "ldh a, (%.2X)", ldh_a_mem_u8}, {0xF1, 0, "pop af", pop_rr},
        {0xF2, 0, "ldh a, (c)", ldh_a_mem_c},     {0xF3, 0, "di", di},
        {0xF4, 0, "invalid", nop},                {0xF5, 0, "push af", push_rr},
        {0xF6, 1, "or %.2X", or_u8},              {0xF7, 0, "rst 30", rst},
        {0xF8, 1, "ld hl, sp+%.2X", ld_hl_sp_u8}, {0xF9, 0, "ld sp, hl", ld_sp_hl},
        {0xFA, 2, "ld a, (%.4X)", ld_a_mem_u16},  {0xFB, 0, "ei", ei},
        {0xFC, 0, "invalid", nop},                {0xFD, 0, "invalid", nop},
        {0xFE, 1, "cp %.2X", cp_u8},              {0xFF, 0, "rst 38", rst}
};

static inline bool handle_interrupt(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    if (!memmap->ime) {
        return true;
    }
    u8 flags = memmap->io[GB15_IO_IF];
    if (flags == 0x00) {
        return true;
    }
    if (flags & 0x01) { // vblank
        rst_core(state, regfile, memmap, 0x0040);
        memmap->io[GB15_IO_IF] ^= 0x01;
        return false;
    }
    if (flags & 0x02) { // lcd stat
        rst_core(state, regfile, memmap, 0x0048);
        memmap->io[GB15_IO_IF] ^= 0x02;
        return false;
    }
    if (flags & 0x04) { // timer
        rst_core(state, regfile, memmap, 0x0050);
        memmap->io[GB15_IO_IF] ^= 0x04;
        return false;
    }
    if (flags & 0x08) { // serial
        rst_core(state, regfile, memmap, 0x0058);
        memmap->io[GB15_IO_IF] ^= 0x08;
        return false;
    }
    // joypad
    rst_core(state, regfile, memmap, 0x0060);
    memmap->io[GB15_IO_IF] ^= 0x10;
    return false;
}

void gb15_tick(GB15State *state, u8 *rom, GB15VBlankCallback vblank, void *userdata) {
    if (state->tclocks == 0) {
        if (state->regfile.pc == 0x0100) {
            state = (void *)state;
        }
        if (state->regfile.pc == 0x0064) {
            state = (void *)state;
        }
        GB15MemMap *memmap = &state->memmap;
        GB15RegFile *regfile = &state->regfile;
        if (handle_interrupt(state, regfile, memmap, rom)) {
            u8 opcode = read8(memmap, rom, &regfile->pc);
            const InstructionBundle *bundle = INSTRUCTIONS + opcode;
            printf("af=%.4X|bc=%.4X|de=%.4X|hl=%.4X|pc=%.4X|sp=%.4X :: ",
                   regfile->af,
                   regfile->bc,
                   regfile->de,
                   regfile->hl,
                   regfile->pc - 1,
                   regfile->sp
            );
            u16 tmp_pc = regfile->pc;
            if (bundle->num_operands == 1) {
                printf(bundle->name, read8(memmap, rom, &tmp_pc));
            } else if (bundle->num_operands == 2) {
                printf(bundle->name, read16(memmap, rom, &tmp_pc));
            } else {
                printf(bundle->name);
            }
            printf("\n\tlcdc=%.2X|stat=%.2X|ly=%.2X|ie=%.2X|if=%.2X\n",
                   memmap->io[GB15_IO_LCDC],
                   memmap->io[GB15_IO_STAT],
                   memmap->io[GB15_IO_LY],
                   memmap->io[GB15_IO_IE],
                   memmap->io[GB15_IO_IF]
            );
            bundle->function(opcode, state, &state->regfile, &state->memmap, rom);
        }
        // Enable / Disable interrupts
        if (state->di_mclocks) {
            state->di_mclocks--;
            if (state->di_mclocks == 0) {
                state->memmap.ime = false;
            }
        }
        if (state->ei_mclocks) {
            state->ei_mclocks--;
            if (state->ei_mclocks == 0) {
                state->memmap.ime = false;
            }
        }
    }
    gb15_lcd_tick(state, rom, vblank, userdata);
    state->tclocks--;
}

void gb15_boot(GB15State *state)
{
    gb15_gpu_init(state);
}