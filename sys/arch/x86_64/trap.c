#include <stdint.h>

#include "trap.h"

__attribute__((naked)) void trap_common() {
    __asm__ __volatile__("test byte ptr [rsp + 24], 3\n\t"
                         "jz 1f\n\t"
                         "swapgs\n\t"
                         "1:\n\t"
                         "push rax\n\t"
                         "push rcx\n\t"
                         "push rdx\n\t"
                         "push rbx\n\t"
                         "push rbp\n\t"
                         "push rsi\n\t"
                         "push rdi\n\t"
                         "push r8\n\t"
                         "push r9\n\t"
                         "push r10\n\t"
                         "push r11\n\t"
                         "push r12\n\t"
                         "push r13\n\t"
                         "push r14\n\t"
                         "push r15\n\t"

                         "mov rdi, rsp\n\t" // Pass trapframe pointer in RDI
                         "call trap_handler\n\t"

                         ".global trap_return\n\t"
                         "trap_return:\n\t"

                         "pop r15\n\t"
                         "pop r14\n\t"
                         "pop r13\n\t"
                         "pop r12\n\t"
                         "pop r11\n\t"
                         "pop r10\n\t"
                         "pop r9\n\t"
                         "pop r8\n\t"
                         "pop rdi\n\t"
                         "pop rsi\n\t"
                         "pop rbp\n\t"
                         "pop rbx\n\t"
                         "pop rdx\n\t"
                         "pop rcx\n\t"
                         "pop rax\n\t"

                         "add rsp, 16\n\t" // Drop vector and error code
                         "test byte ptr [rsp + 8], 3\n\t"
                         "jz 2f\n\t"
                         "swapgs\n\t"
                         "2:\n\t"
                         "iretq");
}

#define TRAP_STUB(vec)                                                         \
    __attribute__((naked)) static void trap_stub_##vec() {                     \
        __asm__ __volatile__("push 0\n\t"                                      \
                             "push " #vec "\n\t"                               \
                             "jmp trap_common");                               \
    }

#define TRAP_STUB_ERR(vec)                                                     \
    __attribute__((naked)) static void trap_stub_##vec() {                     \
        __asm__ __volatile__("push " #vec "\n\t"                               \
                             "jmp trap_common");                               \
    }

// clang-format off
#define TRAP_VECTOR_LIST(N, E) \
    N(0)   N(1)   N(2)   N(3)   N(4)   N(5)   N(6)   N(7)                  \
    E(8)   N(9)   E(10)  E(11)  E(12)  E(13)  E(14)  N(15)                 \
    N(16)  E(17)  N(18)  N(19)  N(20)  E(21)  N(22)  N(23)                 \
    N(24)  N(25)  N(26)  N(27)  N(28)  E(29)  E(30)  N(31)                 \
    N(32)  N(33)  N(34)  N(35)  N(36)  N(37)  N(38)  N(39)                 \
    N(40)  N(41)  N(42)  N(43)  N(44)  N(45)  N(46)  N(47)                 \
    N(48)  N(49)  N(50)  N(51)  N(52)  N(53)  N(54)  N(55)                 \
    N(56)  N(57)  N(58)  N(59)  N(60)  N(61)  N(62)  N(63)                 \
    N(64)  N(65)  N(66)  N(67)  N(68)  N(69)  N(70)  N(71)                 \
    N(72)  N(73)  N(74)  N(75)  N(76)  N(77)  N(78)  N(79)                 \
    N(80)  N(81)  N(82)  N(83)  N(84)  N(85)  N(86)  N(87)                 \
    N(88)  N(89)  N(90)  N(91)  N(92)  N(93)  N(94)  N(95)                 \
    N(96)  N(97)  N(98)  N(99)  N(100) N(101) N(102) N(103)                \
    N(104) N(105) N(106) N(107) N(108) N(109) N(110) N(111)                \
    N(112) N(113) N(114) N(115) N(116) N(117) N(118) N(119)                \
    N(120) N(121) N(122) N(123) N(124) N(125) N(126) N(127)                \
    N(128) N(129) N(130) N(131) N(132) N(133) N(134) N(135)                \
    N(136) N(137) N(138) N(139) N(140) N(141) N(142) N(143)                \
    N(144) N(145) N(146) N(147) N(148) N(149) N(150) N(151)                \
    N(152) N(153) N(154) N(155) N(156) N(157) N(158) N(159)                \
    N(160) N(161) N(162) N(163) N(164) N(165) N(166) N(167)                \
    N(168) N(169) N(170) N(171) N(172) N(173) N(174) N(175)                \
    N(176) N(177) N(178) N(179) N(180) N(181) N(182) N(183)                \
    N(184) N(185) N(186) N(187) N(188) N(189) N(190) N(191)                \
    N(192) N(193) N(194) N(195) N(196) N(197) N(198) N(199)                \
    N(200) N(201) N(202) N(203) N(204) N(205) N(206) N(207)                \
    N(208) N(209) N(210) N(211) N(212) N(213) N(214) N(215)                \
    N(216) N(217) N(218) N(219) N(220) N(221) N(222) N(223)                \
    N(224) N(225) N(226) N(227) N(228) N(229) N(230) N(231)                \
    N(232) N(233) N(234) N(235) N(236) N(237) N(238) N(239)                \
    N(240) N(241) N(242) N(243) N(244) N(245) N(246) N(247)                \
    N(248) N(249) N(250) N(251) N(252) N(253) N(254) N(255)
// clang-format on

TRAP_VECTOR_LIST(TRAP_STUB, TRAP_STUB_ERR)

#define TRAP_STUB_ENTRY(vec) trap_stub_##vec,

const trap_stub_t trap_stubs[TRAP_VECTORS] = {
    TRAP_VECTOR_LIST(TRAP_STUB_ENTRY, TRAP_STUB_ENTRY)};
