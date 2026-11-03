#ifndef GUARD_CONSTANTS_MART_TMS_H
#define GUARD_CONSTANTS_MART_TMS_H

// Fantasy Retold: every Hoenn town mart also sells 5 TMs, fixed per save (design.md §2, Mart TMs).

// VAR_0x8004 for special PokeMart_SetTMStock, in story order (earlier marts get disjoint TMs)
#define MART_TMS_NONE       0
#define MART_TMS_OLDALE     1
#define MART_TMS_PETALBURG  2
#define MART_TMS_RUSTBORO   3
#define MART_TMS_SLATEPORT  4
#define MART_TMS_MAUVILLE   5
#define MART_TMS_VERDANTURF 6
#define MART_TMS_FALLARBOR  7
#define MART_TMS_LAVARIDGE  8
#define MART_TMS_FORTREE    9
#define MART_TMS_MOSSDEEP   10
#define MART_TMS_SOOTOPOLIS 11
#define MART_TMS_COUNT      11

#define MART_TMS_PER_MART 5
#define MART_TM_PRICE     5000

#endif // GUARD_CONSTANTS_MART_TMS_H
