#include <string.h>

#include <gb15/gb15.h>

static inline s8 signify8(u8 value) {
    if (value <= (u8)0x7F) {
        return value;
    }
    return -(s8)(((~value) + (u8)1) & (u8)0xFF);
}

static inline s16 signify16(u16 value) {
    if (value <= (u8)0x7FFF) {
        return value;
    }
    return -(s16)(((~value) + (u8)1) & (u8)0xFFFF);
}

static inline u8 add8_carry(u8 lhs, u8 rhs) {
    return (u8)((u16)lhs + (u16)rhs > (u16)0x00FF);
}

static inline u8 sub8_carry(u8 lhs, u8 rhs) {
    return (u8)((s16)lhs - (s16)rhs < (s16)0x0000);
}

static inline u8 add8_half_carry(u8 lhs, u8 rhs) {
    return (u8)((((lhs & (u8)0x0F) + (rhs & (u8)0x0F)) & (u8)0x10) == (u8)0x10);
}

static inline u8 sub8_half_carry(u8 lhs, u8 rhs) {
    return (u8)!add8_half_carry(lhs, ((~rhs) + (u8)1) & (u8)0xFF);
}

static inline u8 add16_carry(u16 lhs, u16 rhs) {
    return (u8)((u32)lhs + (u32)rhs > (u32)0xFFFF);
}

static inline u8 add16_half_carry(u16 lhs, u16 rhs) {
    return (u8)((((lhs & (u16)0x00FF) + (rhs & (u16)0x00FF)) & (u16)0x0800) == (u16)0x0800);
}

static inline u8 sub16_half_carry(u16 lhs, u16 rhs) {
    return (u8)!add16_half_carry(lhs, ((~rhs) + (u16)1) & (u16)0xFFFF);
}

static inline u8 sub16_carry(u16 lhs, u16 rhs) {
    return (u8)((s32)lhs - (s32)rhs < (s32)0x00000000);
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

static inline u16 *reg16_from_opcode(GB15RegFile *regfile, u8 opcode) {
    switch ((opcode & (u8)0b00110000) >> (u8)4) {
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

static inline u16 *reg16_from_opcode_stack(GB15RegFile *regfile, u8 opcode) {
    switch ((opcode & (u8)0b00110000) >> (u8)4) {
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

static inline u8 *dest_from_bits(GB15RegFile *regfile, u8 bits) {
    switch (bits) {
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
        case 0b110:
            return NULL; // (HL) destination is in memory
        case 0b111:
            return &regfile->a;
        default:
            return NULL;
    }
}

static inline bool cond_from_opcode(u8 opcode, GB15RegFile *regfile) {
    switch ((opcode & (u8)0b00011000) >> (u8)3) {
        case 0b00:
            return !regfile->f_fine.z;
        case 0b01:
            return regfile->f_fine.z;
        case 0b10:
            return !regfile->f_fine.c;
        case 0b11:
            return regfile->f_fine.c;
        default:
            return false;
    }
}

static inline u8 bitmask_from_opcode(u8 opcode) {
    return (u8)1 << ((opcode & (u8)0b00111000) >> (u8)3);
}

static void nop(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 4;
}

static void ld_mm_sp(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 20;
    write16(memmap, read16(memmap, rom, &regfile->pc), regfile->sp);
}

static void ld_rr_nn(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 12;
    *reg16_from_opcode(regfile, opcode) = read16(memmap, rom, &regfile->pc);
}

static void add_hl_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u16 rr = *reg16_from_opcode(regfile, opcode);
    state->tclocks = 8;
    regfile->f_fine.n = 0;
    regfile->f_fine.h = add16_half_carry(regfile->hl, rr);
    regfile->f_fine.c = add16_carry(regfile->hl, rr);
    regfile->hl += rr;
}

static void ld_mm_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    gb15_memmap_write(memmap, *reg16_from_opcode(regfile, opcode), regfile->a);
}

static void ld_a_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, *reg16_from_opcode(regfile, opcode));
}

static void inc_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    (*reg16_from_opcode(regfile, opcode))++;
}

static void dec_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    state->tclocks = 8;
    (*reg16_from_opcode(regfile, opcode))--;
}

static void inc_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) {
    u8 *dest = dest_from_bits(regfile, (opcode & (u8)0b00111000) >> (u8)3);
    if (dest) {
        state->tclocks = 4;
        regfile->f_fine.h = add8_half_carry(*dest, 1);
        (*dest)++;
        regfile->f_fine.z = (u8)(*dest == 0);
        regfile->f_fine.n = 0;
        return;
    }
    state->tclocks = 12;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    regfile->f_fine.h = add8_half_carry(tmp8, 1);
    tmp8++;
    gb15_memmap_write(memmap, regfile->hl, tmp8);
    regfile->f_fine.z = (u8)(tmp8 == 0);
    regfile->f_fine.n = 0;
}

static void dec_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *dest = dest_from_bits(regfile, (opcode & (u8)0b00111000) >> (u8)3);
    if (dest) {
        state->tclocks = 4;
        regfile->f_fine.h = sub8_half_carry(*dest, 1);
        (*dest)--;
        regfile->f_fine.z = (u8)(*dest == 0);
        regfile->f_fine.n = 1;
        return;
    }
    state->tclocks = 12;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    regfile->f_fine.h = sub8_half_carry(tmp8, 1);
    tmp8--;
    gb15_memmap_write(memmap, regfile->hl, tmp8);
    regfile->f_fine.z = (u8)(tmp8 == 0);
    regfile->f_fine.n = 1;
}

static void ld_dest_n(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *dest = dest_from_bits(regfile, (opcode & (u8)0b00111000) >> (u8)3);
    u8 tmp8 = read8(memmap, rom, &regfile->pc);
    if (dest) {
        state->tclocks = 8;
        *dest = tmp8;
        return;
    }
    state->tclocks = 12;
    gb15_memmap_write(memmap, regfile->hl, tmp8);
}

static void rlca(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    u8 tmp8 = (regfile->a & (u8)0b10000000) >> (u8)7;
    regfile->a = (regfile->a << (u8)1) | tmp8;
    regfile->f_fine.c = tmp8;
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void rrca(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    u8 tmp8 = regfile->a & (u8)0x01;
    regfile->a = (regfile->a >> (u8)1) | (tmp8 << (u8)7);
    regfile->f_fine.c = tmp8;
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void rla(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    u8 tmp8 = (regfile->a & (u8)0b10000000) >> (u8)7;
    regfile->a = (regfile->a << (u8)1) | (regfile->f_fine.c);
    regfile->f_fine.c = tmp8;
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void rra(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    u8 tmp8 = regfile->a & (u8)0x01;
    regfile->a = (regfile->a >> (u8)1) | (regfile->f_fine.c << (u8)7);
    regfile->f_fine.c = tmp8;
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void stop(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    state->stopped = true;
}

static void jr_n(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    regfile->pc += signify8(read8(memmap, rom, &regfile->pc));
}

static void jr_cond_n(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    s8 tmp8 = signify8(read8(memmap, rom, &regfile->pc));
    if (cond_from_opcode(opcode, regfile)) {
        state->tclocks = 12;
        regfile->pc += tmp8;
    } else {
        state->tclocks = 8;
    }
}

static void ldi_hl_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 8;
    gb15_memmap_write(memmap, regfile->hl, regfile->a);
    regfile->hl++;
}

static void ldi_a_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, regfile->hl);
    regfile->hl++;
}

static void ldd_hl_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 8;
    gb15_memmap_write(memmap, regfile->hl, regfile->a);
    regfile->hl--;
}

static void ldd_a_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, regfile->hl);
    regfile->hl--;
}

static void daa(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    if ((regfile->a & (u8)0x0F) > (u8)9 || regfile->f_fine.h) {
        regfile->a += (u8)0x06;
    }
    if (((regfile->a & (u8)0xF0) >> (u8)4) > (u8)9) {
        regfile->f_fine.c = 1;
        regfile->a += (u8)0x60;
    }
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.h = 0;
}

static void cpl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    regfile->a = ~regfile->a;
    regfile->f_fine.n = 1;
    regfile->f_fine.h = 1;
}

static void scf(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
    regfile->f_fine.c = 1;
}

static void ccf(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
    regfile->f_fine.c = ~regfile->f_fine.c;
}

static void ld_dest_src(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *dest = dest_from_bits(regfile, (opcode & (u8)0b00111000) >> (u8)3);
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    state->tclocks = (!src || !dest)? (u8)8 : (u8)4;
    u8 src_value = src? *src : gb15_memmap_read(memmap, rom, regfile->hl);
    if (dest) {
        *dest = src_value;
        return;
    }
    gb15_memmap_write(memmap, regfile->hl, src_value);
}

static void halt(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->halted = true;
}

static void alu_op_src(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 src_value = 0;
    if (opcode & (u8)0b01000000) { // ALU A, N
        state->tclocks = 8;
        src_value = read8(memmap, rom, &regfile->pc);
    } else {             // ALU A, src
        u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
        state->tclocks = src? (u8)4 : (u8)8;
        src_value = src? *src : gb15_memmap_read(memmap, rom, regfile->hl);
    }
    switch ((opcode & (u8)0b00111000) >> (u8)3) {
        case 0b001: // ADC
            src_value += regfile->f_fine.c;
        case 0b000: // ADD
            regfile->f_fine.h = add8_half_carry(regfile->a, src_value);
            regfile->f_fine.c = add8_carry(regfile->a, src_value);
            regfile->a += src_value;
            regfile->f_fine.z = (u8)(regfile->a == 0);
            regfile->f_fine.n = 0;
            break;
        case 0b011: // SBC
            src_value += regfile->f_fine.c;
        case 0b010: // SUB
            regfile->f_fine.h = sub8_half_carry(regfile->a, src_value);
            regfile->f_fine.c = sub8_carry(regfile->a, src_value);
            regfile->a -= src_value;
            regfile->f_fine.z = (u8)(regfile->a == 0);
            regfile->f_fine.n = 1;
            break;
        case 0b100: // AND
            regfile->a &= src_value;
            regfile->f_fine.h = 1;
            regfile->f_fine.c = 0;
            regfile->f_fine.z = (u8)(regfile->a == 0);
            regfile->f_fine.n = 0;
            break;
        case 0b101: // XOR
            regfile->a ^= src_value;
            regfile->f_fine.h = 0;
            regfile->f_fine.c = 0;
            regfile->f_fine.z = (u8)(regfile->a == 0);
            regfile->f_fine.n = 0;
            break;
        case 0b110: // OR
            regfile->a |= src_value;
            regfile->f_fine.h = 0;
            regfile->f_fine.c = 0;
            regfile->f_fine.z = (u8)(regfile->a == 0);
            regfile->f_fine.n = 0;
            break;
        case 0b111: // CP
            regfile->f_fine.c = sub8_carry(regfile->a, src_value);
            regfile->f_fine.h = sub8_half_carry(regfile->a, src_value);
            regfile->a -= src_value;
            regfile->f_fine.z = (u8)(regfile->a == 0);
            regfile->f_fine.n = 1;
            break;
        default:
            break;
    }
}

static void pop_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 16;
    *reg16_from_opcode_stack(regfile, opcode) = read16(memmap, rom, &regfile->sp);
}

static void push_rr(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 16;
    regfile->sp -= 2;
    write16(memmap, regfile->sp, *reg16_from_opcode_stack(regfile, opcode));
}

static void rst(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 16;
    regfile->sp -= 2;
    write16(memmap, regfile->sp, regfile->pc);
    regfile->pc = opcode & (u8)0b00111000;
}

static void ret_cond(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    if (cond_from_opcode(opcode, regfile)) {
        state->tclocks = 20;
        regfile->pc = read16(memmap, rom, &regfile->sp);
    } else {
        state->tclocks = 8;
    }
}

static void ret(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    regfile->pc = read16(memmap, rom, &regfile->sp);
}

static void reti(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    state->ime = true;
    regfile->pc = read16(memmap, rom, &regfile->sp);
}

static void jp_cond_nn(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    if (cond_from_opcode(opcode, regfile)) {
        state->tclocks = 16;
        regfile->pc = read16(memmap, rom, &regfile->pc);
    } else {
        state->tclocks = 12;
    }
}

static void jp_nn(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 16;
    regfile->pc = read16(memmap, rom, &regfile->pc);
}

static void call_cond_nn(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    if (cond_from_opcode(opcode, regfile)) {
        state->tclocks = 24;
        regfile->sp -= 2;
        write16(memmap, regfile->sp, regfile->pc);
        regfile->pc = read16(memmap, rom, &regfile->pc);
    } else {
        state->tclocks = 12;
    }
}

static void call_nn(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 24;
    regfile->sp -= 2;
    write16(memmap, regfile->sp, regfile->pc);
    regfile->pc = read16(memmap, rom, &regfile->pc);
}

static void add_sp_n(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 16;
    u8 tmp8 = read8(memmap, rom, &regfile->pc);
    regfile->f_fine.h = add16_half_carry(regfile->sp, tmp8);
    regfile->f_fine.c = add16_carry(regfile->sp, tmp8);
    regfile->f_fine.n = 0;
    regfile->f_fine.z = 0;
    regfile->sp += tmp8;
}

static void ldhl_sp_n(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    u8 tmp8 = read8(memmap, rom, &regfile->pc);
    regfile->f_fine.h = add16_half_carry(regfile->sp, tmp8);
    regfile->f_fine.c = add16_carry(regfile->sp, tmp8);
    regfile->f_fine.n = 0;
    regfile->f_fine.z = 0;
    regfile->hl = regfile->sp + tmp8;
}

static void ldh_n_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    gb15_memmap_write(memmap, (u16)0xFF00 + read8(memmap, rom, &regfile->pc), regfile->a);
}

static void ldh_a_n(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    regfile->a = gb15_memmap_read(memmap, rom, (u16)0xFF00 + read8(memmap, rom, &regfile->pc));
}

static void ldh_c_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 8;
    gb15_memmap_write(memmap, (u16)0xFF00 + regfile->c, regfile->a);
}

static void ldh_a_c(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 8;
    regfile->a = gb15_memmap_read(memmap, rom, (u16)0xFF00 + regfile->c);
}

static void ld_nn_a(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    gb15_memmap_write(memmap, read16(memmap, rom, &regfile->pc), regfile->a);
}

static void ld_a_nn(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 12;
    regfile->a = gb15_memmap_read(memmap, rom, read16(memmap, rom, &regfile->pc));
}

static void jp_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    regfile->pc = regfile->hl;
}

static void ld_sp_hl(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 8;
    regfile->sp = regfile->hl;
}

static void di(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    state->di_mclocks = 2;
}

static void ei(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    state->tclocks = 4;
    state->ei_mclocks = 2;
}

static void cb_rlc_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *dest = dest_from_bits(regfile, opcode & (u8)0b111);
    if (dest) {
        state->tclocks = 8;
        u8 tmp8 = (*dest & (u8)0b10000000) >> (u8)7;
        *dest = (*dest << (u8)1) | tmp8;
        regfile->f_fine.c = tmp8;
        regfile->f_fine.z = (u8)(*dest == 0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    u8 carry = (tmp8 & (u8)0b10000000) >> (u8)7;
    u8 result = (tmp8 << (u8)1) | carry;
    gb15_memmap_write(memmap, regfile->hl, result);
    regfile->f_fine.c = carry;
    regfile->f_fine.z = (u8)(result == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_rrc_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *dest = dest_from_bits(regfile, opcode & (u8)0b111);
    if (dest) {
        state->tclocks = 8;
        u8 tmp8 = *dest & (u8)0x01;
        *dest = (*dest >> (u8)1) | (tmp8 << (u8)7);
        regfile->f_fine.c = tmp8;
        regfile->f_fine.z = (u8)(*dest == 0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    u8 result = (tmp8 >> (u8)1) | (tmp8 & (u8)0x01);
    gb15_memmap_write(memmap, regfile->hl, result);
    regfile->f_fine.c = (tmp8 & (u8)0x01);
    regfile->f_fine.z = (u8)(result == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_rl_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *dest = dest_from_bits(regfile, opcode & (u8)0b111);
    if (dest) {
        state->tclocks = 8;
        u8 tmp8 = (*dest & (u8)0b10000000) >> (u8)7;
        *dest = (*dest << (u8)1) | (regfile->f_fine.c);
        regfile->f_fine.c = tmp8;
        regfile->f_fine.z = (u8)(*dest == 0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    u8 result = (tmp8 << (u8)1) | (regfile->f_fine.c);
    gb15_memmap_write(memmap, regfile->hl, result);
    regfile->f_fine.c = (tmp8 & (u8)0b10000000) >> (u8)7;
    regfile->f_fine.z = (u8)(result == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_rr_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *dest = dest_from_bits(regfile, opcode & (u8)0b111);
    if (dest) {
        state->tclocks = 8;
        u8 tmp8 = *dest & (u8)0x01;
        *dest = (*dest >> (u8)1) | (regfile->f_fine.c << (u8)7);
        regfile->f_fine.c = tmp8;
        regfile->f_fine.z = (u8)(*dest == 0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    u8 result = (tmp8 >> (u8)1) | (regfile->f_fine.c << (u8)7);
    gb15_memmap_write(memmap, regfile->hl, result);
    regfile->f_fine.c = (tmp8 & (u8)0x01);
    regfile->f_fine.z = (u8)(result == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_sla_src(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    if (src) {
        state->tclocks = 8;
        u8 tmp8 = (*src & (u8)0b10000000) >> (u8)7;
        regfile->a = *src << (u8)1;
        regfile->f_fine.c = tmp8;
        regfile->f_fine.z = (u8)(regfile->a == 0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    u8 carry = (tmp8 & (u8)0b10000000) >> (u8)7;
    regfile->a = tmp8 << (u8)1;
    regfile->f_fine.c = carry;
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_sra_src(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    if (src) {
        state->tclocks = 8;
        u8 tmp8 = *src & (u8)0x01;
        regfile->a = (*src >> (u8)1) | (*src & (u8)0b10000000);
        regfile->f_fine.c = tmp8;
        regfile->f_fine.z = (u8)(regfile->a == 0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    u8 carry = tmp8 & (u8)0x01;
    regfile->a = (tmp8 >> (u8)1) | (tmp8 & (u8)0b10000000);
    regfile->f_fine.c = carry;
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_swap_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    if (src) {
        state->tclocks = 8;
        *src = ((*src & (u8)0x0F) << (u8)4) & (*src & (u8)0xF0 >> (u8)4);
        regfile->f_fine.z = (u8)(*src == 0);
        regfile->f_fine.c = 0;
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    tmp8 = ((tmp8 & (u8)0x0F) << (u8)4) & (tmp8 & (u8)0xF0 >> (u8)4);
    gb15_memmap_write(memmap, tmp8, regfile->a);
    regfile->f_fine.z = (u8)(tmp8 == 0);
    regfile->f_fine.c = 0;
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_srl_dest(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    if (src) {
        state->tclocks = 8;
        u8 tmp8 = *src & (u8)0x01;
        regfile->a = (*src >> (u8)1);
        regfile->f_fine.c = tmp8;
        regfile->f_fine.z = (u8)(regfile->a == 0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 0;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    u8 carry = tmp8 & (u8)0x01;
    regfile->a = (tmp8 >> (u8)1);
    regfile->f_fine.c = carry;
    regfile->f_fine.z = (u8)(regfile->a == 0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 0;
}

static void cb_bit_n_src(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    if (src) {
        state->tclocks = 8;
        regfile->f_fine.z = (u8)((*src & bitmask_from_opcode(opcode)) == (u8)0);
        regfile->f_fine.n = 0;
        regfile->f_fine.h = 1;
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    regfile->f_fine.z = (u8)((tmp8 & bitmask_from_opcode(opcode)) == (u8)0);
    regfile->f_fine.n = 0;
    regfile->f_fine.h = 1;
}

static void cb_res_n_src(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    if (src) {
        state->tclocks = 8;
        *src &= ~bitmask_from_opcode(opcode);
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    tmp8 &= ~bitmask_from_opcode(opcode);
    gb15_memmap_write(memmap, regfile->hl, tmp8);
}

static void cb_set_n_src(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom)  {
    u8 *src = dest_from_bits(regfile, opcode & (u8)0b111);
    if (src) {
        state->tclocks = 8;
        *src |= bitmask_from_opcode(opcode);
        return;
    }
    state->tclocks = 16;
    u8 tmp8 = gb15_memmap_read(memmap, rom, regfile->hl);
    tmp8 |= bitmask_from_opcode(opcode);
    gb15_memmap_write(memmap, regfile->hl, tmp8);
}

typedef void(*GB15Instruction)(u8 opcode, GB15State *state, GB15RegFile *regfile, GB15MemMap *memmap, u8 *rom) ;

static GB15Instruction GB15_INSTRUCTIONS[256] = {
    [0x00] = nop,
    [0x01] = ld_rr_nn,
    [0x02] = ld_mm_a,
    [0x03] = inc_rr,
    [0x04] = inc_dest,
    [0x05] = dec_dest,
    [0x06] = ld_dest_n,
    [0x07] = rlca,
    [0x08] = ld_mm_sp,
    [0x09] = add_hl_rr,
    [0x0A] = ld_a_rr,
    [0x0B] = dec_rr,
    [0x0C] = inc_dest,
    [0x0D] = dec_dest,
    [0x0E] = ld_dest_n,
    [0x0F] = rrca,

    [0x10] = stop,
    [0x11] = ld_rr_nn,
    [0x12] = ld_mm_a,
    [0x13] = inc_rr,
    [0x14] = inc_dest,
    [0x15] = dec_dest,
    [0x16] = ld_dest_n,
    [0x17] = rla,
    [0x18] = jr_n,
    [0x19] = add_hl_rr,
    [0x1A] = ld_a_rr,
    [0x1B] = dec_rr,
    [0x1C] = inc_dest,
    [0x1D] = dec_dest,
    [0x1E] = ld_dest_n,
    [0x1F] = rra,

    [0x20] = jr_cond_n,
    [0x21] = ld_rr_nn,
    [0x22] = ldi_hl_a,
    [0x23] = inc_rr,
    [0x24] = inc_dest,
    [0x25] = dec_dest,
    [0x26] = ld_dest_n,
    [0x27] = daa,
    [0x28] = jr_cond_n,
    [0x29] = add_hl_rr,
    [0x2A] = ldi_a_hl,
    [0x2B] = dec_rr,
    [0x2C] = inc_dest,
    [0x2D] = dec_dest,
    [0x2E] = ld_dest_n,
    [0x2F] = cpl,

    [0x30] = jr_cond_n,
    [0x31] = ld_rr_nn,
    [0x32] = ldd_hl_a,
    [0x33] = inc_rr,
    [0x34] = inc_dest,
    [0x35] = dec_dest,
    [0x36] = ld_dest_n,
    [0x37] = scf,
    [0x38] = jr_cond_n,
    [0x39] = add_hl_rr,
    [0x3A] = ldd_a_hl,
    [0x3B] = dec_rr,
    [0x3C] = inc_dest,
    [0x3D] = dec_dest,
    [0x3E] = ld_dest_n,
    [0x3F] = ccf,

    [0x40] = ld_dest_src,
    [0x41] = ld_dest_src,
    [0x42] = ld_dest_src,
    [0x43] = ld_dest_src,
    [0x44] = ld_dest_src,
    [0x45] = ld_dest_src,
    [0x46] = ld_dest_src,
    [0x47] = ld_dest_src,
    [0x48] = ld_dest_src,
    [0x49] = ld_dest_src,
    [0x4A] = ld_dest_src,
    [0x4B] = ld_dest_src,
    [0x4C] = ld_dest_src,
    [0x4D] = ld_dest_src,
    [0x4E] = ld_dest_src,
    [0x4F] = ld_dest_src,

    [0x50] = ld_dest_src,
    [0x51] = ld_dest_src,
    [0x52] = ld_dest_src,
    [0x53] = ld_dest_src,
    [0x54] = ld_dest_src,
    [0x55] = ld_dest_src,
    [0x56] = ld_dest_src,
    [0x57] = ld_dest_src,
    [0x58] = ld_dest_src,
    [0x59] = ld_dest_src,
    [0x5A] = ld_dest_src,
    [0x5B] = ld_dest_src,
    [0x5C] = ld_dest_src,
    [0x5D] = ld_dest_src,
    [0x5E] = ld_dest_src,
    [0x5F] = ld_dest_src,

    [0x60] = ld_dest_src,
    [0x61] = ld_dest_src,
    [0x62] = ld_dest_src,
    [0x63] = ld_dest_src,
    [0x64] = ld_dest_src,
    [0x65] = ld_dest_src,
    [0x66] = ld_dest_src,
    [0x67] = ld_dest_src,
    [0x68] = ld_dest_src,
    [0x69] = ld_dest_src,
    [0x6A] = ld_dest_src,
    [0x6B] = ld_dest_src,
    [0x6C] = ld_dest_src,
    [0x6D] = ld_dest_src,
    [0x6E] = ld_dest_src,
    [0x6F] = ld_dest_src,

    [0x70] = ld_dest_src,
    [0x71] = ld_dest_src,
    [0x72] = ld_dest_src,
    [0x73] = ld_dest_src,
    [0x74] = ld_dest_src,
    [0x75] = ld_dest_src,
    [0x76] = halt,
    [0x77] = ld_dest_src,
    [0x78] = ld_dest_src,
    [0x79] = ld_dest_src,
    [0x7A] = ld_dest_src,
    [0x7B] = ld_dest_src,
    [0x7C] = ld_dest_src,
    [0x7D] = ld_dest_src,
    [0x7E] = ld_dest_src,
    [0x7F] = ld_dest_src,

    [0x80] = alu_op_src,
    [0x81] = alu_op_src,
    [0x82] = alu_op_src,
    [0x83] = alu_op_src,
    [0x84] = alu_op_src,
    [0x85] = alu_op_src,
    [0x86] = alu_op_src,
    [0x87] = alu_op_src,
    [0x88] = alu_op_src,
    [0x89] = alu_op_src,
    [0x8A] = alu_op_src,
    [0x8B] = alu_op_src,
    [0x8C] = alu_op_src,
    [0x8D] = alu_op_src,
    [0x8E] = alu_op_src,
    [0x8F] = alu_op_src,

    [0x90] = alu_op_src,
    [0x91] = alu_op_src,
    [0x92] = alu_op_src,
    [0x93] = alu_op_src,
    [0x94] = alu_op_src,
    [0x95] = alu_op_src,
    [0x96] = alu_op_src,
    [0x97] = alu_op_src,
    [0x98] = alu_op_src,
    [0x99] = alu_op_src,
    [0x9A] = alu_op_src,
    [0x9B] = alu_op_src,
    [0x9C] = alu_op_src,
    [0x9D] = alu_op_src,
    [0x9E] = alu_op_src,
    [0x9F] = alu_op_src,

    [0xA0] = alu_op_src,
    [0xA1] = alu_op_src,
    [0xA2] = alu_op_src,
    [0xA3] = alu_op_src,
    [0xA4] = alu_op_src,
    [0xA5] = alu_op_src,
    [0xA6] = alu_op_src,
    [0xA7] = alu_op_src,
    [0xA8] = alu_op_src,
    [0xA9] = alu_op_src,
    [0xAA] = alu_op_src,
    [0xAB] = alu_op_src,
    [0xAC] = alu_op_src,
    [0xAD] = alu_op_src,
    [0xAE] = alu_op_src,
    [0xAF] = alu_op_src,

    [0xB0] = alu_op_src,
    [0xB1] = alu_op_src,
    [0xB2] = alu_op_src,
    [0xB3] = alu_op_src,
    [0xB4] = alu_op_src,
    [0xB5] = alu_op_src,
    [0xB6] = alu_op_src,
    [0xB7] = alu_op_src,
    [0xB8] = alu_op_src,
    [0xB9] = alu_op_src,
    [0xBA] = alu_op_src,
    [0xBB] = alu_op_src,
    [0xBC] = alu_op_src,
    [0xBD] = alu_op_src,
    [0xBE] = alu_op_src,
    [0xBF] = alu_op_src,

    [0xC0] = ret_cond,
    [0xC1] = pop_rr,
    [0xC2] = jp_cond_nn,
    [0xC3] = jp_nn,
    [0xC4] = call_cond_nn,
    [0xC5] = push_rr,
    [0xC6] = alu_op_src,
    [0xC7] = rst,
    [0xC8] = ret_cond,
    [0xC9] = ret,
    [0xCA] = jp_cond_nn,
    [0xCB] = halt, // Invalid
    [0xCC] = call_cond_nn,
    [0xCD] = call_nn,
    [0xCE] = alu_op_src,
    [0xCF] = rst,

    [0xD0] = ret_cond,
    [0xD1] = pop_rr,
    [0xD2] = jp_cond_nn,
    [0xD3] = halt, // Invalid
    [0xD4] = call_cond_nn,
    [0xD5] = push_rr,
    [0xD6] = alu_op_src,
    [0xD7] = rst,
    [0xD8] = ret_cond,
    [0xD9] = reti,
    [0xDA] = jp_cond_nn,
    [0xDB] = halt, // Invalid
    [0xDC] = call_cond_nn,
    [0xDD] = halt, // Invalid
    [0xDE] = alu_op_src,
    [0xDF] = rst,

    [0xE0] = ldh_n_a,
    [0xE1] = pop_rr,
    [0xE2] = ldh_c_a,
    [0xE3] = halt, // Invalid
    [0xE4] = halt, // Invalid
    [0xE5] = push_rr,
    [0xE6] = alu_op_src,
    [0xE7] = rst,
    [0xE8] = add_sp_n,
    [0xE9] = jp_hl,
    [0xEA] = ld_nn_a,
    [0xEB] = halt, // Invalid
    [0xEC] = halt, // Invalid
    [0xED] = halt, // Invalid
    [0xEE] = alu_op_src,
    [0xEF] = rst,

    [0xF0] = ldh_a_n,
    [0xF1] = pop_rr,
    [0xF2] = ldh_a_c,
    [0xF3] = di,
    [0xF4] = halt, // Invalid
    [0xF5] = push_rr,
    [0xF6] = alu_op_src,
    [0xF7] = rst,
    [0xF8] = ldhl_sp_n,
    [0xF9] = ld_sp_hl,
    [0xFA] = ld_a_nn,
    [0xFB] = ei,
    [0xFC] = halt, // Invalid
    [0xFD] = halt, // Invalid
    [0xFE] = alu_op_src,
    [0xFF] = rst,
};

static GB15Instruction GB15_CBINSTRUCTIONS[256] = {
    [0x00] = cb_rlc_dest,
    [0x01] = cb_rlc_dest,
    [0x02] = cb_rlc_dest,
    [0x03] = cb_rlc_dest,
    [0x04] = cb_rlc_dest,
    [0x05] = cb_rlc_dest,
    [0x06] = cb_rlc_dest,
    [0x07] = cb_rlc_dest,
    [0x08] = cb_rrc_dest,
    [0x09] = cb_rrc_dest,
    [0x0A] = cb_rrc_dest,
    [0x0B] = cb_rrc_dest,
    [0x0C] = cb_rrc_dest,
    [0x0D] = cb_rrc_dest,
    [0x0E] = cb_rrc_dest,
    [0x0F] = cb_rrc_dest,

    [0x10] = cb_rl_dest,
    [0x11] = cb_rl_dest,
    [0x12] = cb_rl_dest,
    [0x13] = cb_rl_dest,
    [0x14] = cb_rl_dest,
    [0x15] = cb_rl_dest,
    [0x16] = cb_rl_dest,
    [0x17] = cb_rl_dest,
    [0x18] = cb_rr_dest,
    [0x19] = cb_rr_dest,
    [0x1A] = cb_rr_dest,
    [0x1B] = cb_rr_dest,
    [0x1C] = cb_rr_dest,
    [0x1D] = cb_rr_dest,
    [0x1E] = cb_rr_dest,
    [0x1F] = cb_rr_dest,

    [0x20] = cb_sla_src,
    [0x21] = cb_sla_src,
    [0x22] = cb_sla_src,
    [0x23] = cb_sla_src,
    [0x24] = cb_sla_src,
    [0x25] = cb_sla_src,
    [0x26] = cb_sla_src,
    [0x27] = cb_sla_src,
    [0x28] = cb_sra_src,
    [0x29] = cb_sra_src,
    [0x2A] = cb_sra_src,
    [0x2B] = cb_sra_src,
    [0x2C] = cb_sra_src,
    [0x2D] = cb_sra_src,
    [0x2E] = cb_sra_src,
    [0x2F] = cb_sra_src,

    [0x30] = cb_swap_dest,
    [0x31] = cb_swap_dest,
    [0x32] = cb_swap_dest,
    [0x33] = cb_swap_dest,
    [0x34] = cb_swap_dest,
    [0x35] = cb_swap_dest,
    [0x36] = cb_swap_dest,
    [0x37] = cb_swap_dest,
    [0x38] = cb_srl_dest,
    [0x39] = cb_srl_dest,
    [0x3A] = cb_srl_dest,
    [0x3B] = cb_srl_dest,
    [0x3C] = cb_srl_dest,
    [0x3D] = cb_srl_dest,
    [0x3E] = cb_srl_dest,
    [0x3F] = cb_srl_dest,

    [0x40] = cb_bit_n_src,
    [0x41] = cb_bit_n_src,
    [0x42] = cb_bit_n_src,
    [0x43] = cb_bit_n_src,
    [0x44] = cb_bit_n_src,
    [0x45] = cb_bit_n_src,
    [0x46] = cb_bit_n_src,
    [0x47] = cb_bit_n_src,
    [0x48] = cb_bit_n_src,
    [0x49] = cb_bit_n_src,
    [0x4A] = cb_bit_n_src,
    [0x4B] = cb_bit_n_src,
    [0x4C] = cb_bit_n_src,
    [0x4D] = cb_bit_n_src,
    [0x4E] = cb_bit_n_src,
    [0x4F] = cb_bit_n_src,

    [0x50] = cb_bit_n_src,
    [0x51] = cb_bit_n_src,
    [0x52] = cb_bit_n_src,
    [0x53] = cb_bit_n_src,
    [0x54] = cb_bit_n_src,
    [0x55] = cb_bit_n_src,
    [0x56] = cb_bit_n_src,
    [0x57] = cb_bit_n_src,
    [0x58] = cb_bit_n_src,
    [0x59] = cb_bit_n_src,
    [0x5A] = cb_bit_n_src,
    [0x5B] = cb_bit_n_src,
    [0x5C] = cb_bit_n_src,
    [0x5D] = cb_bit_n_src,
    [0x5E] = cb_bit_n_src,
    [0x5F] = cb_bit_n_src,

    [0x60] = cb_bit_n_src,
    [0x61] = cb_bit_n_src,
    [0x62] = cb_bit_n_src,
    [0x63] = cb_bit_n_src,
    [0x64] = cb_bit_n_src,
    [0x65] = cb_bit_n_src,
    [0x66] = cb_bit_n_src,
    [0x67] = cb_bit_n_src,
    [0x68] = cb_bit_n_src,
    [0x69] = cb_bit_n_src,
    [0x6A] = cb_bit_n_src,
    [0x6B] = cb_bit_n_src,
    [0x6C] = cb_bit_n_src,
    [0x6D] = cb_bit_n_src,
    [0x6E] = cb_bit_n_src,
    [0x6F] = cb_bit_n_src,

    [0x70] = cb_bit_n_src,
    [0x71] = cb_bit_n_src,
    [0x72] = cb_bit_n_src,
    [0x73] = cb_bit_n_src,
    [0x74] = cb_bit_n_src,
    [0x75] = cb_bit_n_src,
    [0x76] = cb_bit_n_src,
    [0x77] = cb_bit_n_src,
    [0x78] = cb_bit_n_src,
    [0x79] = cb_bit_n_src,
    [0x7A] = cb_bit_n_src,
    [0x7B] = cb_bit_n_src,
    [0x7C] = cb_bit_n_src,
    [0x7D] = cb_bit_n_src,
    [0x7E] = cb_bit_n_src,
    [0x7F] = cb_bit_n_src,

    [0x80] = cb_res_n_src,
    [0x81] = cb_res_n_src,
    [0x82] = cb_res_n_src,
    [0x83] = cb_res_n_src,
    [0x84] = cb_res_n_src,
    [0x85] = cb_res_n_src,
    [0x86] = cb_res_n_src,
    [0x87] = cb_res_n_src,
    [0x88] = cb_res_n_src,
    [0x89] = cb_res_n_src,
    [0x8A] = cb_res_n_src,
    [0x8B] = cb_res_n_src,
    [0x8C] = cb_res_n_src,
    [0x8D] = cb_res_n_src,
    [0x8E] = cb_res_n_src,
    [0x8F] = cb_res_n_src,

    [0x90] = cb_res_n_src,
    [0x91] = cb_res_n_src,
    [0x92] = cb_res_n_src,
    [0x93] = cb_res_n_src,
    [0x94] = cb_res_n_src,
    [0x95] = cb_res_n_src,
    [0x96] = cb_res_n_src,
    [0x97] = cb_res_n_src,
    [0x98] = cb_res_n_src,
    [0x99] = cb_res_n_src,
    [0x9A] = cb_res_n_src,
    [0x9B] = cb_res_n_src,
    [0x9C] = cb_res_n_src,
    [0x9D] = cb_res_n_src,
    [0x9E] = cb_res_n_src,
    [0x9F] = cb_res_n_src,

    [0xA0] = cb_res_n_src,
    [0xA1] = cb_res_n_src,
    [0xA2] = cb_res_n_src,
    [0xA3] = cb_res_n_src,
    [0xA4] = cb_res_n_src,
    [0xA5] = cb_res_n_src,
    [0xA6] = cb_res_n_src,
    [0xA7] = cb_res_n_src,
    [0xA8] = cb_res_n_src,
    [0xA9] = cb_res_n_src,
    [0xAA] = cb_res_n_src,
    [0xAB] = cb_res_n_src,
    [0xAC] = cb_res_n_src,
    [0xAD] = cb_res_n_src,
    [0xAE] = cb_res_n_src,
    [0xAF] = cb_res_n_src,

    [0xB0] = cb_res_n_src,
    [0xB1] = cb_res_n_src,
    [0xB2] = cb_res_n_src,
    [0xB3] = cb_res_n_src,
    [0xB4] = cb_res_n_src,
    [0xB5] = cb_res_n_src,
    [0xB6] = cb_res_n_src,
    [0xB7] = cb_res_n_src,
    [0xB8] = cb_res_n_src,
    [0xB9] = cb_res_n_src,
    [0xBA] = cb_res_n_src,
    [0xBB] = cb_res_n_src,
    [0xBC] = cb_res_n_src,
    [0xBD] = cb_res_n_src,
    [0xBE] = cb_res_n_src,
    [0xBF] = cb_res_n_src,

    [0xC0] = cb_set_n_src,
    [0xC1] = cb_set_n_src,
    [0xC2] = cb_set_n_src,
    [0xC3] = cb_set_n_src,
    [0xC4] = cb_set_n_src,
    [0xC5] = cb_set_n_src,
    [0xC6] = cb_set_n_src,
    [0xC7] = cb_set_n_src,
    [0xC8] = cb_set_n_src,
    [0xC9] = cb_set_n_src,
    [0xCA] = cb_set_n_src,
    [0xCB] = cb_set_n_src,
    [0xCC] = cb_set_n_src,
    [0xCD] = cb_set_n_src,
    [0xCE] = cb_set_n_src,
    [0xCF] = cb_set_n_src,

    [0xD0] = cb_set_n_src,
    [0xD1] = cb_set_n_src,
    [0xD2] = cb_set_n_src,
    [0xD3] = cb_set_n_src,
    [0xD4] = cb_set_n_src,
    [0xD5] = cb_set_n_src,
    [0xD6] = cb_set_n_src,
    [0xD7] = cb_set_n_src,
    [0xD8] = cb_set_n_src,
    [0xD9] = cb_set_n_src,
    [0xDA] = cb_set_n_src,
    [0xDB] = cb_set_n_src,
    [0xDC] = cb_set_n_src,
    [0xDD] = cb_set_n_src,
    [0xDE] = cb_set_n_src,
    [0xDF] = cb_set_n_src,

    [0xE0] = cb_set_n_src,
    [0xE1] = cb_set_n_src,
    [0xE2] = cb_set_n_src,
    [0xE3] = cb_set_n_src,
    [0xE4] = cb_set_n_src,
    [0xE5] = cb_set_n_src,
    [0xE6] = cb_set_n_src,
    [0xE7] = cb_set_n_src,
    [0xE8] = cb_set_n_src,
    [0xE9] = cb_set_n_src,
    [0xEA] = cb_set_n_src,
    [0xEB] = cb_set_n_src,
    [0xEC] = cb_set_n_src,
    [0xED] = cb_set_n_src,
    [0xEE] = cb_set_n_src,
    [0xEF] = cb_set_n_src,

    [0xF0] = cb_set_n_src,
    [0xF1] = cb_set_n_src,
    [0xF2] = cb_set_n_src,
    [0xF3] = cb_set_n_src,
    [0xF4] = cb_set_n_src,
    [0xF5] = cb_set_n_src,
    [0xF6] = cb_set_n_src,
    [0xF7] = cb_set_n_src,
    [0xF8] = cb_set_n_src,
    [0xF9] = cb_set_n_src,
    [0xFA] = cb_set_n_src,
    [0xFB] = cb_set_n_src,
    [0xFC] = cb_set_n_src,
    [0xFD] = cb_set_n_src,
    [0xFE] = cb_set_n_src,
    [0xFF] = cb_set_n_src,
};

void gb15_tick(GB15State *state, u8 *rom, GB15VBlankCallback vblank, void *userdata) {
    if (state->tclocks == 0) {
        u8 opcode = read8(&state->memmap, rom, &state->regfile.pc);
        if (opcode == (u8)0xCB) {
            opcode = read8(&state->memmap, rom, &state->regfile.pc);
            GB15_CBINSTRUCTIONS[opcode](opcode, state, &state->regfile, &state->memmap, rom);
        } else {
            GB15_INSTRUCTIONS[opcode](opcode, state, &state->regfile, &state->memmap, rom);
        }
        // Enable / Disable interrupts
        if (state->di_mclocks) {
            state->di_mclocks--;
            if (state->di_mclocks == 0) {
                state->ime = false;
            }
        }
        if (state->ei_mclocks) {
            state->ei_mclocks--;
            if (state->ei_mclocks == 0) {
                state->ime = true;
            }
        }
    }
    gb15_gpu_tick(state, rom, vblank, userdata);
    state->tclocks--;
}

void gb15_boot(GB15State *state)
{
    state->ime = true;
    gb15_gpu_init(state);
//    gb15_memmap_write(&state->memmap, GB15_REG_BIOS, 0b00010000);
//    GB15RegFile *regfile = &state->regfile;
//    regfile->a = 0x11;
//    regfile->f = 0xB0;
//    regfile->bc = 0x0013;
//    regfile->de = 0x00D8;
//    regfile->hl = 0x014D;
//    regfile->sp = 0xFFFE;
//    regfile->pc = 0x0100;
//    const struct {GB15VirtualRegister address; u8 value; } initializer[31] = {
//            {GB15_REG_BIOS,  0x10},
//            {GB15_REG_TIMA,  0x00},
//            {GB15_REG_TMA,   0x00},
//            {GB15_REG_TAC,   0x00},
//            {GB15_REG_NR10,  0x80},
//            {GB15_REG_NR11,  0xBF},
//            {GB15_REG_NR12,  0xF3},
//            {GB15_REG_NR14,  0xBF},
//            {GB15_REG_NR21,  0x3F},
//            {GB15_REG_NR22,  0x00},
//            {GB15_REG_NR24,  0xBF},
//            {GB15_REG_NR30,  0x7F},
//            {GB15_REG_NR31,  0xFF},
//            {GB15_REG_NR32,  0x9F},
//            {GB15_REG_NR33,  0xBF},
//            {GB15_REG_NR41,  0xFF},
//            {GB15_REG_NR42,  0x00},
//            {GB15_REG_NR43,  0x00},
//            {GB15_REG_NR30,  0xBF},
//            {GB15_REG_NR50,  0x77},
//            {GB15_REG_NR51,  0xF3},
//            {GB15_REG_NR52,  0xF1},
//            {GB15_REG_SCX,   0x00},
//            {GB15_REG_SCY,   0x00},
//            {GB15_REG_LYC,   0x00},
//            {GB15_REG_BGP,   0xFC},
//            {GB15_REG_OBP0,  0xFF},
//            {GB15_REG_OBP1,  0xFF},
//            {GB15_REG_WX,    0x00},
//            {GB15_REG_WY,    0x00},
//            {GB15_REG_IE,    0x00},
//    };
//    for (u32 i = 0; i < 31; i++) {
//        gb15_memmap_write(memmap, initializer[i].address, initializer[i].value);
//    }
}