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
    regfile->f = (regfile->f & (u8)0b01111111) | (value << (u8)7);
}

static inline void set_n(GB15RegFile *regfile, bool value) {
    regfile->f = (regfile->f & (u8)0b10111111) | (value << (u8)6);
}

static inline void set_h(GB15RegFile *regfile, bool value) {
    regfile->f = (regfile->f & (u8)0b11011111) | (value << (u8)5);
}

static inline void set_c(GB15RegFile *regfile, bool value) {
    regfile->f = (regfile->f & (u8)0b11101111) | (value << (u8)4);
}

static inline u8 get_z(GB15RegFile *regfile) {
    return (regfile->f & (u8)0b10000000) >> (u8)7;
}

static inline u8 get_h(GB15RegFile *regfile) {
    return (regfile->f & (u8)0b01000000) >> (u8)6;
}

static inline u8 get_n(GB15RegFile *regfile) {
    return (regfile->f & (u8)0b00100000) >> (u8)5;
}

static inline u8 get_c(GB15RegFile *regfile) {
    return (regfile->f & (u8)0b00010000) >> (u8)4;
}

static inline u8 *reg8(GB15RegFile *regfile, u8 reg) {
    switch (reg) {
        case 0b111:
            return &regfile->a;
        case 0b000:
            return &regfile->b;
        case 0b001:
            return &regfile->c;
        case 0b010:
            return &regfile->d;
        case 0b011:
            return &regfile->e;
        case 0b100:
            return &regfile->h;
        case 0b101:
            return &regfile->l;
        default:
            return NULL;
    }
}

static inline u16 *reg16(GB15RegFile *regfile, u8 reg) {
    switch (reg) {
        case 0b00:
            return &regfile->bc;
        case 0b01:
            return &regfile->de;
        case 0b10:
            return &regfile->hl;
        case 0b11:
            return &regfile->sp;
        default:
            return NULL;
    }
}

static inline u16 *reg16_push_pop(GB15RegFile *regfile, u8 reg) {
    switch (reg) {
        case 0b00:
            return &regfile->bc;
        case 0b01:
            return &regfile->de;
        case 0b10:
            return &regfile->hl;
        case 0b11:
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
        carry = (*dest & (u8)0b10000000) >> (u8)7;
        *dest = (*dest << (u8)1) | carry;
        set_c(regfile, carry);
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        carry = (reg & (u8)0b10000000) >> (u8)7;
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
        set_c(regfile, (*dest & (u8)0b10000000) >> (u8)7);
        *dest = (*dest << (u8)1) | carry;
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        set_c(regfile, (reg & (u8)0b10000000) >> (u8)7);
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
        case 0b00:
            return !get_z(regfile);
        case 0b01:
            return get_z(regfile);
        case 0b10:
            return !get_c(regfile);
        case 0b11:
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
        *dest = (*dest >> (u8)1) | (*dest & (u8)0b10000000);
        set_z(regfile, *dest == (u8)0x00);
    } else { // (hl)
        state->tclocks = 16;
        reg = gb15_memmap_read(memmap, rom, regfile->hl);
        set_c(regfile, reg & (u8)0x01);
        reg = (reg >> (u8)1) | (reg & (u8)0b10000000);
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
    *reg16(regfile, (opcode & (u8)0b00110000) >> (u8)4) = read16(memmap, rom, &regfile->pc);
}

static inline void ld_mem_rr_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, *reg16(regfile, (opcode & (u8)0b00110000) >> (u8)4), 0b111);
}

static inline void inc_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    (*reg16(regfile, (opcode & (u8)0b00110000) >> (u8)4))++;
}

static inline void inc_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    u8 *reg = reg8(regfile, (opcode & (u8)0b00111000) >> (u8)3);
    set_h(regfile, (*reg & 0xF) == (u8)0x0);
    (*reg)++;
    set_z(regfile, *reg == (u8)0x00);
    set_n(regfile, false);
}

static inline void dec_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    u8 *reg = reg8(regfile, (opcode & (u8)0b00111000) >> (u8)3);
    (*reg)--;
    set_z(regfile, *reg == (u8)0x00);
    set_n(regfile, true);
    set_h(regfile, (*reg & 0xF) == (u8)0xF);
}

static inline void ld_r_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    *reg8(regfile, (opcode & (u8)0b00111000) >> (u8)3) = read8(memmap, rom, &regfile->pc);
}

static inline void rlca(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rlc_core(state, regfile, memmap, rom, 0b111);
    state->tclocks = 4;
    set_z(regfile, false);
}

static inline void ld_mem_u16_sp(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    write16(memmap, read16(memmap, rom, &regfile->pc), regfile->sp);
}

static inline void ld_a_mem_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, *reg16(regfile, (opcode & (u8)0b00110000) >> (u8)4));
}

static inline void add_hl_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    u32 overflow = (u32)regfile->hl + (u32)*reg16(regfile, (opcode & (u8)0b00110000) >> (u8)4);
    set_n(regfile, false);
    set_c(regfile, overflow > (u32)0xFFFF);
    set_h(regfile, (overflow & (u32)0x0FFF) < (regfile->hl & (u32)0x0FFF));
    regfile->hl = (u16)(overflow & (u32)0xFFFF);
}

static inline void dec_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    (*reg16(regfile, (opcode & (u8)0b00110000) >> (u8)4))--;
}

static inline void rrc(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rrc_core(state, regfile, memmap, rom, opcode & (u8)0b111);
}

static inline void stop(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    state->stopped = true;
    read8(memmap, rom, &regfile->pc);
}

static inline void rla(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rl_core(state, regfile, memmap, rom, 0b111, get_c(regfile));
    state->tclocks = 4;
    set_z(regfile, false);
}

static inline void jr_s8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    regfile->pc += signify8(read8(memmap, rom, &regfile->pc));
}

static inline void rra(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rr_core(state, regfile, memmap, rom, 0b111, get_c(regfile));
    state->tclocks = 4;
    set_z(regfile, false);
}

static inline void jr_cond_s8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u8 dest = read8(memmap, rom, &regfile->pc);
    if (test_cond(regfile, (opcode & (u8)0b00011000) >> (u8)3)) {
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
    *reg8(regfile, (opcode & (u8)0b00111000) >> (u8)3) = *reg8(regfile, opcode & (u8)0b111);
}

static inline void ld_r_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    *reg8(regfile, (opcode & (u8)0b00111000) >> (u8)3) = gb15_memmap_read(memmap, rom, regfile->hl);
}

static inline void ld_mem_hl_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, regfile->hl, *reg8(regfile, opcode & (u8)0b111));
}

static inline void halt(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    state->halted = true;
}

static inline void add_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    add_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111), 0);
}

static inline void add_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    add_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl), 0);
}

static inline void adc_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    add_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111), get_c(regfile));
}

static inline void adc_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    add_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl), get_c(regfile));
}

static inline void sub_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    sub_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111));
}

static inline void sub_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    sub_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void sbc_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    sbc_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111));
}

static inline void sbc_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    sbc_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void and_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    and_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111));
}

static inline void and_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    and_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void xor_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    xor_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111));
}

static inline void xor_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    xor_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void or_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    or_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111));
}

static inline void or_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    or_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void cp_r(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
    cp_core(state, regfile, memmap, rom, *reg8(regfile, opcode & (u8)0b111));
}

static inline void cp_mem_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    cp_core(state, regfile, memmap, rom, gb15_memmap_read(memmap, rom, regfile->hl));
}

static inline void ret_cond(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u8 dest = read8(memmap, rom, &regfile->sp);
    if (test_cond(regfile, (opcode & (u8)0b00011000) >> (u8)3)) {
        state->tclocks = 20;
        regfile->pc += signify8(dest);
    } else {
        state->tclocks = 8;
    }
}

static inline void pop_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    *reg16_push_pop(regfile, (opcode & (u8)0b00110000) >> (u8)4) = read16(memmap, rom, &regfile->sp);
}

static inline void jp_cond_u16(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u16 dest = read16(memmap, rom, &regfile->pc);
    if (test_cond(regfile, (opcode & (u8)0b00011000) >> (u8)3)) {
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
    if (test_cond(regfile, (opcode & (u8)0b00011000) >> (u8)3)) {
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
    write16(memmap, regfile->sp, *reg16_push_pop(regfile, (opcode & (u8)0b00110000) >> (u8)4));
}

static inline void add_u8(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    add_core(state, regfile, memmap, rom, read8(memmap, rom, &regfile->pc), 0);
}

static inline void rst(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    rst_core(state, regfile, memmap, (u16)(opcode & (u8)0b00111000));
}

static inline void ret(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 16;
    regfile->pc = read16(memmap, rom, &regfile->sp);
}

static inline void cb(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    opcode = read8(memmap, rom, &regfile->pc);
    u8 reg = opcode & (u8)0b111;
    switch (opcode >> (u8)3) {
        case 0b000:
            rlc_core(state, regfile, memmap, rom, reg);
            break;
        case 0b001:
            rrc_core(state, regfile, memmap, rom, reg);
            break;
        case 0b010:
            rl_core(state, regfile, memmap, rom, reg, get_c(regfile));
            break;
        case 0b011:
            rr_core(state, regfile, memmap, rom, reg, get_c(regfile));
            break;
        case 0b100:
            sla(state, regfile, memmap, rom, reg);
            break;
        case 0b101:
            sra(state, regfile, memmap, rom, reg);
            break;
        case 0b110:
            swap(state, regfile, memmap, rom, reg);
            break;
        case 0b111:
            rr_core(state, regfile, memmap, rom, reg, 0);
            break;
        default:
            switch ((opcode & (u8)0b11000000) >> (u8)6) {
                case 0b01:
                    bit(state, regfile, memmap, rom, reg, (u8)1 << ((opcode & (u8)0b00111000) >> (u8)3));
                    break;
                case 0b10:
                    set(state, regfile, memmap, rom, reg, (u8)1 << ((opcode & (u8)0b00111000) >> (u8)3), false);
                    break;
                default:
                case 0b11:
                    set(state, regfile, memmap, rom, reg, (u8)1 << ((opcode & (u8)0b00111000) >> (u8)3), true);
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
    const char *name;
    void (*function)(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom);
} InstructionBundle;

static const InstructionBundle INSTRUCTIONS[256] = {
        {0x00, "nop", nop},                    {0x01, "ld bc, u16", ld_rr_u16},
        {0x02, "ld (bc), a", ld_mem_rr_a},     {0x03, "inc bc", inc_rr},
        {0x04, "inc b", inc_r},                {0x05, "dec b", dec_r},
        {0x06, "ld b, u8", ld_r_u8},           {0x07, "rlca", rlca},
        {0x08, "ld (u8), sp", ld_mem_u16_sp},  {0x09, "add hl, bc", add_hl_rr},
        {0x0A, "ld a, (bc)", ld_a_mem_rr},     {0x0B, "dec bc", dec_rr},
        {0x0C, "inc c", inc_r},                {0x0D, "dec c", dec_r},
        {0x0E, "ld c, u8", ld_r_u8},           {0x0F, "rrc", rrc},

        {0x10, "stop", stop},                  {0x11, "ld de, u16", ld_rr_u16},
        {0x12, "ld (de), a", ld_mem_rr_a},     {0x13, "inc de", inc_rr},
        {0x14, "inc d", inc_r},                {0x15, "dec d", dec_r},
        {0x16, "ld d, u8", ld_r_u8},           {0x17, "rla", rla},
        {0x18, "jr s8", jr_s8},                {0x19, "add hl, de", add_hl_rr},
        {0x1A, "ld a, (de)", ld_a_mem_rr},     {0x1B, "dec de", dec_rr},
        {0x1C, "inc e", inc_r},                {0x1D, "dec e", dec_r},
        {0x1E, "ld e, u8", ld_r_u8},           {0x1F, "rra", rra},

        {0x20, "jr nz, s8", jr_cond_s8},       {0x21, "ld hl, u16", ld_rr_u16},
        {0x22, "ld (hl+), a", ldi_hl_a},       {0x23, "inc hl", inc_rr},
        {0x24, "inc h", inc_r},                {0x25, "dec h", dec_r},
        {0x26, "ld h, u8", ld_r_u8},           {0x27, "daa", daa},
        {0x28, "jr z, s8", jr_cond_s8},        {0x29, "add hl, hl", add_hl_rr},
        {0x2A, "ld a, (hl+)", ldi_a_hl},       {0x2B, "dec hl", dec_rr},
        {0x2C, "inc l", inc_r},                {0x2D, "dec l", dec_r},
        {0x2E, "ld l, u8", ld_r_u8},           {0x2F, "cpl", cpl},

        {0x30, "jr nc, s8", jr_cond_s8},       {0x31, "ld sp, u16", ld_rr_u16},
        {0x32, "ld (hl-), a", ldd_hl_a},       {0x33, "inc sp", inc_rr},
        {0x34, "inc (hl)", inc_mem_hl},        {0x35, "dec (hl)", dec_mem_hl},
        {0x36, "ld (hl), u8", ld_mem_hl_n},    {0x37, "scf", scf},
        {0x38, "jr c, s8", jr_cond_s8},        {0x39, "add hl, sp", add_hl_rr},
        {0x3A, "ld a, (hl-)", ldd_a_hl},       {0x3B, "dec sp", dec_rr},
        {0x3C, "inc a", inc_r},                {0x3D, "dec a", dec_r},
        {0x3E, "ld a, u8", ld_r_u8},           {0x3F, "ccf", ccf},

        {0x40, "ld b, b", ld_r_r},             {0x41, "ld b, c", ld_r_r},
        {0x42, "ld b, d", ld_r_r},             {0x43, "ld b, e", ld_r_r},
        {0x44, "ld b, h", ld_r_r},             {0x45, "ld b, l", ld_r_r},
        {0x46, "ld b, (hl)", ld_r_mem_hl},     {0x47, "ld b, a", ld_r_r},
        {0x48, "ld c, b", ld_r_r},             {0x49, "ld c, c", ld_r_r},
        {0x4A, "ld c, d", ld_r_r},             {0x4B, "ld c, e", ld_r_r},
        {0x4C, "ld c, h", ld_r_r},             {0x4D, "ld c, l", ld_r_r},
        {0x4E, "ld c, (hl)", ld_r_mem_hl},     {0x4F, "ld c, a", ld_r_r},

        {0x50, "ld d, b", ld_r_r},             {0x51, "ld d, c", ld_r_r},
        {0x52, "ld d, d", ld_r_r},             {0x53, "ld d, e", ld_r_r},
        {0x54, "ld d, h", ld_r_r},             {0x55, "ld d, l", ld_r_r},
        {0x56, "ld d, (hl)", ld_r_mem_hl},     {0x57, "ld d, a", ld_r_r},
        {0x58, "ld e, b", ld_r_r},             {0x59, "ld e, c", ld_r_r},
        {0x5A, "ld e, d", ld_r_r},             {0x5B, "ld e, e", ld_r_r},
        {0x5C, "ld e, h", ld_r_r},             {0x5D, "ld e, l", ld_r_r},
        {0x5E, "ld e, (hl)", ld_r_mem_hl},     {0x5F, "ld e, a", ld_r_r},

        {0x60, "ld h, b", ld_r_r},             {0x61, "ld h, c", ld_r_r},
        {0x62, "ld h, d", ld_r_r},             {0x63, "ld h, e", ld_r_r},
        {0x64, "ld h, h", ld_r_r},             {0x65, "ld h, l", ld_r_r},
        {0x66, "ld h, (hl)", ld_r_mem_hl},     {0x67, "ld h, a", ld_r_r},
        {0x68, "ld l, b", ld_r_r},             {0x69, "ld l, c", ld_r_r},
        {0x6A, "ld l, d", ld_r_r},             {0x6B, "ld l, e", ld_r_r},
        {0x6C, "ld l, h", ld_r_r},             {0x6D, "ld l, l", ld_r_r},
        {0x6E, "ld l, (hl)", ld_r_mem_hl},     {0x6F, "ld l, a", ld_r_r},

        {0x70, "ld (hl), b", ld_mem_hl_r},     {0x71, "ld (hl), c", ld_mem_hl_r},
        {0x72, "ld (hl), d", ld_mem_hl_r},     {0x73, "ld (hl), e", ld_mem_hl_r},
        {0x74, "ld (hl), h", ld_mem_hl_r},     {0x75, "ld (hl), l", ld_mem_hl_r},
        {0x76, "halt", halt},                  {0x77, "ld (hl), a", ld_mem_hl_r},
        {0x78, "ld a, b", ld_r_r},             {0x79, "ld a, c", ld_r_r},
        {0x7A, "ld a, d", ld_r_r},             {0x7B, "ld a, e", ld_r_r},
        {0x7C, "ld a, h", ld_r_r},             {0x7D, "ld a, l", ld_r_r},
        {0x7E, "ld a, (hl)", ld_r_mem_hl},     {0x7F, "ld a, a", ld_r_r},

        {0x80, "add b", add_r},                {0x81, "add c", add_r},
        {0x82, "add d", add_r},                {0x83, "add e", add_r},
        {0x84, "add h", add_r},                {0x85, "add l", add_r},
        {0x86, "add (hl)", add_mem_hl},        {0x87, "add a", add_r},
        {0x88, "adc b", adc_r},                {0x89, "adc c", adc_r},
        {0x8A, "adc d", adc_r},                {0x8B, "adc e", adc_r},
        {0x8C, "adc h", adc_r},                {0x8D, "adc l", adc_r},
        {0x8E, "adc (hl)", adc_mem_hl},        {0x8F, "adc a", adc_r},

        {0x90, "sub b", sub_r},                {0x91, "sub c", sub_r},
        {0x92, "sub d", sub_r},                {0x93, "sub e", sub_r},
        {0x94, "sub h", sub_r},                {0x95, "sub l", sub_r},
        {0x96, "sub (hl)", sub_mem_hl},        {0x97, "sub a", sub_r},
        {0x98, "sbc b", sbc_r},                {0x99, "sbc c", sbc_r},
        {0x9A, "sbc d", sbc_r},                {0x9B, "sbc e", sbc_r},
        {0x9C, "sbc h", sbc_r},                {0x9D, "sbc l", sbc_r},
        {0x9E, "sbc (hl)", sbc_mem_hl},        {0x9F, "sbc a", sbc_r},

        {0xA0, "and b", and_r},                {0xA1, "and c", and_r},
        {0xA2, "and d", and_r},                {0xA3, "and e", and_r},
        {0xA4, "and h", and_r},                {0xA5, "and l", and_r},
        {0xA6, "and (hl)", and_mem_hl},        {0xA7, "and a", and_r},
        {0xA8, "xor b", xor_r},                {0xA9, "xor c", xor_r},
        {0xAA, "xor d", xor_r},                {0xAB, "xor e", xor_r},
        {0xAC, "xor h", xor_r},                {0xAD, "xor l", xor_r},
        {0xAE, "xor (hl)", xor_mem_hl},        {0xAF, "xor a", xor_r},

        {0xB0, "or b", or_r},                  {0xB1, "or c", or_r},
        {0xB2, "or d", or_r},                  {0xB3, "or e", or_r},
        {0xB4, "or h", or_r},                  {0xB5, "or l", or_r},
        {0xB6, "or (hl)", or_mem_hl},          {0xB7, "or a", or_r},
        {0xB8, "cp b", cp_r},                  {0xB9, "cp c", cp_r},
        {0xBA, "cp d", cp_r},                  {0xBB, "cp e", cp_r},
        {0xBC, "cp h", cp_r},                  {0xBD, "cp l", cp_r},
        {0xBE, "cp (hl)", cp_mem_hl},          {0xBF, "cp a", cp_r},

        {0xC0, "ret nz", ret_cond},            {0xC1, "pop bc", pop_rr},
        {0xC2, "jp nz, u16", jp_cond_u16},     {0xC3, "jp u16", jp_u16},
        {0xC4, "call nz, u16", call_cond_u16}, {0xC5, "push bc", push_rr},
        {0xC6, "add a, u8", add_u8},           {0xC7, "rst $00", rst},
        {0xC8, "ret z", ret_cond},             {0xC9, "ret", ret},
        {0xCA, "jp z, u16", jp_cond_u16},      {0xCB, "CB prefix", cb},
        {0xCC, "call z, u16", call_cond_u16},  {0xCD, "call u16", call_u16},
        {0xCE, "adc u8", adc_u8},              {0xCF, "rst $08", rst},

        {0xD0, "ret nc", ret_cond},            {0xD1, "pop de", pop_rr},
        {0xD2, "jp nc, u16", jp_cond_u16},     {0xD3, "invalid", nop},
        {0xD4, "call nc, u16", call_cond_u16}, {0xD5, "push de", push_rr},
        {0xD6, "sub u8", sub_u8},              {0xD7, "rst $10", rst},
        {0xD8, "ret c", ret_cond},             {0xD9, "reti", reti},
        {0xDA, "jp c, u16", jp_cond_u16},      {0xDB, "invalid", nop},
        {0xDC, "call c, u16", call_cond_u16},  {0xDD, "invalid", nop},
        {0xDE, "sbc u8", sbc_u8},              {0xDF, "rst $18", nop},

        {0xE0, "ldh (u8), a", ldh_mem_u8_a},   {0xE1, "pop hl", pop_rr},
        {0xE2, "ldh (c), a", ldh_mem_c_a},     {0xE3, "invalid", nop},
        {0xE4, "invalid", nop},                {0xE5, "push hl", push_rr},
        {0xE6, "and u8", and_u8},              {0xE7, "rst $20", rst},
        {0xE8, "add sp, u8", add_sp_u8},       {0xE9, "jp hl", jp_hl},
        {0xEA, "ld (u16), a", ld_mm_u16_a},    {0xEB, "invalid", nop},
        {0xEC, "invalid", nop},                {0xED, "invalid", nop},
        {0xEE, "xor u8", xor_u8},              {0xEF, "rst $28", rst},

        {0xF0, "ldh a, (u8)", ldh_a_mem_u8},   {0xF1, "pop af", pop_rr},
        {0xF2, "ldh a, (c)", ldh_a_mem_c},     {0xF3, "di", di},
        {0xF4, "invalid", nop},                {0xF5, "push af", push_rr},
        {0xF6, "or u8", or_u8},                {0xF7, "rst $30", rst},
        {0xF8, "ld hl, sp+u8", ld_hl_sp_u8},   {0xF9, "ld sp, hl", ld_sp_hl},
        {0xFA, "ld a, (u16)", ld_a_mem_u16},   {0xFB, "ei", ei},
        {0xFC, "invalid", nop},                {0xFD, "invalid", nop},
        {0xFE, "cp u8", cp_u8},                {0xFF, "rst $38", rst}
};

static inline bool handle_interrupt(GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    if (memmap->io[GB15_IO_IE] == 0x00) {
        return true;
    }
    u8 flags = memmap->io[GB15_IO_IF];
    if (flags == 0x00) {
        return true;
    }
    if (flags & 0b00001) { // vblank
        rst_core(state, regfile, memmap, 0x0040);
        memmap->io[GB15_IO_IF] ^= 0b00001;
        return false;
    }
    if (flags & 0b00010) { // lcd stat
        rst_core(state, regfile, memmap, 0x0048);
        memmap->io[GB15_IO_IF] ^= 0b00010;
        return false;
    }
    if (flags & 0b00100) { // timer
        rst_core(state, regfile, memmap, 0x0050);
        memmap->io[GB15_IO_IF] ^= 0b00100;
        return false;
    }
    if (flags & 0b01000) { // serial
        rst_core(state, regfile, memmap, 0x0058);
        memmap->io[GB15_IO_IF] ^= 0b01000;
        return false;
    }
    // joypad
    rst_core(state, regfile, memmap, 0x0060);
    memmap->io[GB15_IO_IF] ^= 0b10000;
    return false;
}

void gb15_tick(GB15State *state, u8 *rom, GB15VBlankCallback vblank, void *userdata) {
    if (state->tclocks == 0) {
        if (state->regfile.pc == 0x0100) {
            state = (void *)state;
        }
        GB15MemMap *memmap = &state->memmap;
        GB15RegFile *regfile = &state->regfile;
        if (handle_interrupt(state, regfile, memmap, rom)) {
            u8 opcode = read8(memmap, rom, &regfile->pc);
            const InstructionBundle *bundle = INSTRUCTIONS + opcode;
//            printf("pc=%.4x|sp=%.4x|af=%.4x|bc=%.4x|de=%.4x|hl=%.4x|%.4x :: %s\n",
//                   regfile->pc,
//                   regfile->sp,
//                   regfile->af,
//                   regfile->bc,
//                   regfile->de,
//                   regfile->hl,
//                   regfile->pc - 1,
//                   bundle->name);
            bundle->function(opcode, state, &state->regfile, &state->memmap, rom);
        }
        // Enable / Disable interrupts
        if (state->di_mclocks) {
            state->di_mclocks--;
            if (state->di_mclocks == 0) {
                memmap->io[GB15_IO_IE] = 0x00;
            }
        }
        if (state->ei_mclocks) {
            state->ei_mclocks--;
            if (state->ei_mclocks == 0) {
                memmap->io[GB15_IO_IE] = 0x01;
            }
        }
    }
    gb15_gpu_tick(state, rom, vblank, userdata);
    state->tclocks--;
}

void gb15_boot(GB15State *state)
{
    gb15_gpu_init(state);
}