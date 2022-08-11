#ifndef __e2k__
#error Поддерживается только e2k
#endif

.section ".init", "ax", @progbits
    /*
     * Установка флага запрета невыровненных обращений
     */
    rrs %upsr, %r0
    ors %r0, (1<<2), %r0
    {
    rws %r0, %upsr
    nop 4
    }

