/* Сюда пока в виде макросов
 * Файл взял без изменений от такой же версии для gcc-3.4.6, поэтому
 * комментарии соответствующие */

/* gnu'шный номер для регистра %sp */
#define __builtin_dwarf_sp_column() 14

/* Инициализация массива char'ов размерами регистров. Нумерация идёт в gnu'той
 * терминологии (см. файл mdes_sparc_reg.c, только там количество регистров
 * не полное) */
#ifdef __LP64__
#define __builtin_init_dwarf_reg_size_table(buff) \
{ \
  int i; \
  char *ptr = (char*)(buff); \
  for (i = 0; i < 32; i++) \
    ptr[i] = 8; \
  for (i = 33; i < 102; i++) \
    ptr[i] = 4; \
}
#else
#define __builtin_init_dwarf_reg_size_table(buff) \
{ \
  int i; \
  char *ptr = (char*)(buff); \
  for (i = 0; i < 102; i++) \
    ptr[i] = 4; \
}
#endif

/* Судя по всему, для sparc'а все регистры принудительно откачиваются в стек,
 * потому как вся unwind-info описана в терминах того, что регистры
 * от предыдущего окна хранятся в стеке
 *
 * Старый комментарий:
 *
 * Точно не заю что
 * См. gcc-3.4.6/gcc/except.c процедура expand_builtin_unwind_init
 * Далее на sparc'е вызывается gen_flush_register_windows, что соответсвует
 * define_insn "flush_register_windows". При этом в процедуре
 * expand_builtin_unwind_init взводится флажок current_function_has_nonlocal_label,
 * по которому хз что делается, но, вроде бы как для -O0 некритично */
#ifdef __LP64__
#define __builtin_unwind_init() \
  asm ("flushw");
#else
#define __builtin_unwind_init() \
  asm ("ta 3");
#endif

/* См. bug #82489
 *
 * Старый комментарий:
 *
 * См. gcc-3.4.6/gcc/except.c процедура expand_builtin_eh_return_data_regno
 * На sparc'е реализации такие:
 *
 * #define EH_RETURN_DATA_REGNO(N) ((N) < 4 ? (N) + 24 : INVALID_REGNUM)
 * DWARF_FRAME_REGNUM - не нашёл, но, судя по тому, что генерит sparc'овский gcc,
 * он ничего не делает
 *
 * Параметр должен быть строго константный, но здесь нормально это не отсечём
 * (чтобы была именно ошибка компиляции). Так что полагаем, что в исходниках
 * у нас всё в порядке */
#define __builtin_eh_return_data_regno(val) \
  (((val) >= 0 && (val) < 4) ? (val) + 24 : -1)

/* Судя по всему, это вычисление CFA (Call Frame Adress). По сути описания
 * в стандарте dwarf2 это есть значение %sp в предыдущем фрэйме (т.е. значение
 * %fp в текущем фрэйме)
 *
 * Старый комментарий:
 *
 * Непонятно, что это, но на v9, судя по тому, что генерится, это
 * результат '%fp + 2047', а на v8 просто '%fp'.
 * Соответсвует return virtual_cfa_rtx; а откудо оно берётся - не осилил */
#ifdef __LP64__
#define __builtin_dwarf_cfa() \
  ({ void *p; \
     asm volatile ("add %%fp, 2047, %0" : "=r"(p)); \
     p; })
#else
#define __builtin_dwarf_cfa() \
  ({ void *p; \
     asm volatile ("mov %%fp, %0" : "=r"(p)); \
     p; })
#endif

/* См. gcc-3.4.6/gcc/except.c процедура expand_builtin_frob_return_addr
 * Фактически dst = src - RETURN_ADDR_OFFSET в терминах void*
 * Для sparc'а
 * #define RETURN_ADDR_OFFSET \
 *  (8 + 4 * (! TARGET_ARCH64 && current_function_returns_struct))
 * Так что для режима 64 можно сделать и в виде макроса */
#ifdef __LP64__
#define __builtin_frob_return_addr(src) \
  ((void*)(src) - 8)
#else
/* В нашем случае нигде нет использования builtin'а в процедурах, возвращающих
 * структуру, а потому реализуем вариант со скалярным результатом и нам пока
 * этого достаточно. Полноценный вариант можно сделать только при поддержке
 * builtin'а компилятором */
#define __builtin_frob_return_addr(src) \
  ((void*)(src) - 8)
#endif

/* См. gcc-3.4.6/gcc/except.c процедура expand_builtin_eh_return
 * Мало чего понял, а потому сделал то, что делает gcc
 * При этом есть подозрение, что использование %g1 и %g2 критично
 * (по крайней мере про %g1 где-то что-то встречал) */
#if defined __sparc__
#define __builtin_eh_return(ival,ptr) \
  ({ \
    asm volatile ("mov %0, %%g1\n\t" \
                  "mov %1, %%g2\n\t" \
                  "mov %%g2, %%i7\n\t" \
                  "restore\n\t" \
                  "retl\n\t" \
                  " add %%sp, %%g1, %%sp" \
                  : : "r" ((long)(ival)), "r" ((void*)(ptr)) : "g1", "g2", "i7"); \
  });
#elif defined __e2k__
#define __builtin_eh_return(ival,ptr) \
  ({ \
    asm volatile ("nop" : : "r" ((long)(ival)), "r" ((void*)(ptr))); \
  });

#endif
