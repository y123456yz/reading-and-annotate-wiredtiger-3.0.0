/*-
 * Public Domain 2014-present MongoDB, Inc.
 * Public Domain 2008-2014 WiredTiger, Inc.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *

 [root@localhost build]# ps -ef | grep ex_he
root      99767  83051  1 16:20 pts/4    00:00:00 ./ex_hello -i 1
root      99823  80409  0 16:21 pts/2    00:00:00 grep --color=auto ex_he
[root@localhost build]# pstack -p 99767
Usage: pstack <process-id>
[root@localhost build]# pstack 99767
Thread 10 (Thread 0x7f6b6886f700 (LWP 99772)):
#0  0x00007f6b695f1a35 in pthread_cond_wait@@GLIBC_2.3.2 () from /lib64/libpthread.so.0
#1  0x00007f6b69e202f9 in __wt_cond_wait_signal (session=session@entry=0x266e9b0, cond=0x263e980, usecs=usecs@entry=0, run_func=run_func@entry=0x7f6b69d3f10c <__tiered_server_run_chk>, signalled=signalled@entry=0x7f6b6886edcf) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_posix/os_mtx_cond.c:117
#2  0x00007f6b69d41323 in __tiered_server (arg=0x266e9b0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/conn/conn_tiered.c:402
#3  0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#4  0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 9 (Thread 0x7f6b6806e700 (LWP 99773)):
#0  0x00007f6b68efbaad in write () from /lib64/libc.so.6
#1  0x00007f6b68e862f3 in _IO_new_file_write () from /lib64/libc.so.6
#2  0x00007f6b68e87b0e in __GI__IO_do_write () from /lib64/libc.so.6
#3  0x00007f6b68e86a50 in __GI__IO_file_xsputn () from /lib64/libc.so.6
#4  0x00007f6b68e56bb3 in vfprintf () from /lib64/libc.so.6
#5  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#6  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b6806b8e8, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x266f360) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#7  __wt_fprintf (session=session@entry=0x266f360, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#8  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x266f360, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#9  0x00007f6b69eb34b1 in __eventv (session=0x266f360, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b6806cb28) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#10 0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x266f360, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#11 0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x266f360, ref=0x2642820, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#12 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x266f360, is_server=is_server@entry=true) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#13 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x266f360, is_server=is_server@entry=true) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#14 0x00007f6b69dc4dd4 in __evict_pass (session=session@entry=0x266f360) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:772
#15 0x00007f6b69dc6c29 in __evict_server (session=session@entry=0x266f360, did_work=did_work@entry=0x7f6b6806d9ef) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:414
#16 0x00007f6b69dc705c in __wt_evict_thread_run (session=0x266f360, thread=0x2640230) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:319
#17 0x00007f6b69ecbc66 in __thread_run (arg=0x2640230) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#18 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#19 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 8 (Thread 0x7f6b6786d700 (LWP 99774)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b6786ad98, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x266f838) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x266f838, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x266f838, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x266f838, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b6786bfd8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x266f838, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x266f838, ref=0x26428c0, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#10 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x266f838, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#11 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x266f838, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#12 0x00007f6b69dc7018 in __wt_evict_thread_run (session=0x266f838, thread=0x2640280) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:342
#13 0x00007f6b69ecbc66 in __thread_run (arg=0x2640280) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#14 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#15 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 7 (Thread 0x7f6b6706c700 (LWP 99775)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b67069d98, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x266fd10) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x266fd10, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x266fd10, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x266fd10, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b6706afd8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x266fd10, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x266fd10, ref=0x2642b40, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#10 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x266fd10, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#11 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x266fd10, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#12 0x00007f6b69dc7018 in __wt_evict_thread_run (session=0x266fd10, thread=0x26405a0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:342
#13 0x00007f6b69ecbc66 in __thread_run (arg=0x26405a0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#14 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#15 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 6 (Thread 0x7f6b6686b700 (LWP 99776)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b66868d98, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x26701e8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x26701e8, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x26701e8, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x26701e8, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b66869fd8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x26701e8, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x26701e8, ref=0x2642500, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#10 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x26701e8, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#11 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x26701e8, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#12 0x00007f6b69dc7018 in __wt_evict_thread_run (session=0x26701e8, thread=0x2640550) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:342
#13 0x00007f6b69ecbc66 in __thread_run (arg=0x2640550) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#14 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#15 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 5 (Thread 0x7f6b6606a700 (LWP 99777)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b66067d98, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x26706c0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x26706c0, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x26706c0, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x26706c0, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b66068fd8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x26706c0, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x26706c0, ref=0x2642960, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#10 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x26706c0, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#11 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x26706c0, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#12 0x00007f6b69dc7018 in __wt_evict_thread_run (session=0x26706c0, thread=0x2640500) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:342
#13 0x00007f6b69ecbc66 in __thread_run (arg=0x2640500) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#14 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#15 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 4 (Thread 0x7f6b65869700 (LWP 99778)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b65866d98, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x2670b98) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x2670b98, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x2670b98, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x2670b98, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b65867fd8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x2670b98, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x2670b98, ref=0x2642aa0, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#10 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x2670b98, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#11 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x2670b98, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#12 0x00007f6b69dc7018 in __wt_evict_thread_run (session=0x2670b98, thread=0x26404b0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:342
#13 0x00007f6b69ecbc66 in __thread_run (arg=0x26404b0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#14 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#15 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 3 (Thread 0x7f6b65068700 (LWP 99779)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b65065d98, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x2671070) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x2671070, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x2671070, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x2671070, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b65066fd8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x2671070, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x2671070, ref=0x2642780, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#10 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x2671070, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#11 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x2671070, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#12 0x00007f6b69dc7018 in __wt_evict_thread_run (session=0x2671070, thread=0x2640460) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:342
#13 0x00007f6b69ecbc66 in __thread_run (arg=0x2640460) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#14 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#15 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 2 (Thread 0x7f6b64867700 (LWP 99780)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7f6b64864d98, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x2671548) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x2671548, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x2671548, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x2671548, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_EVICT, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69f209a9 "page %p (%s)", ap=0x7f6b64865fd8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x2671548, category=category@entry=WT_VERB_EVICT, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69f209a9 "page %p (%s)") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69dcafb3 in __wt_evict (session=session@entry=0x2671548, ref=0x2642a00, previous_state=<optimized out>, flags=flags@entry=0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_page.c:199
#10 0x00007f6b69dbe525 in __evict_page (session=session@entry=0x2671548, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:2369
#11 0x00007f6b69dbec55 in __evict_lru_pages (session=session@entry=0x2671548, is_server=is_server@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:1170
#12 0x00007f6b69dc7018 in __wt_evict_thread_run (session=0x2671548, thread=0x2640410) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/evict/evict_lru.c:342
#13 0x00007f6b69ecbc66 in __thread_run (arg=0x2640410) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/thread_group.c:31
#14 0x00007f6b695edea5 in start_thread () from /lib64/libpthread.so.0
#15 0x00007f6b68f0a9fd in clone () from /lib64/libc.so.6
Thread 1 (Thread 0x7f6b6a6bb800 (LWP 99767)):
#0  0x00007f6b68f186ec in __lll_lock_wait_private () from /lib64/libc.so.6
#1  0x00007f6b68e5a00e in _L_lock_1177 () from /lib64/libc.so.6
#2  0x00007f6b68e547f4 in vfprintf () from /lib64/libc.so.6
#3  0x00007f6b69e1c9fa in __stdio_printf (session=<optimized out>, fs=<optimized out>, fmt=<optimized out>, ap=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/os_common/os_fstream_stdio.c:53
#4  0x00007f6b69eb21bd in __wt_vfprintf (ap=0x7ffd1c5cbf78, fmt=0x7f6b69f0fa22 "%s\n", fstr=<optimized out>, session=0x2671a20) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#5  __wt_fprintf (session=session@entry=0x2671a20, fstr=<optimized out>, fmt=fmt@entry=0x7f6b69f0fa22 "%s\n") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/os_fstream_inline.h:66
#6  0x00007f6b69eb21ea in __handle_message_default (handler=<optimized out>, wt_session=0x2671a20, message=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:46
#7  0x00007f6b69eb34b1 in __eventv (session=0x2671a20, is_json=<optimized out>, error=error@entry=0, func=func@entry=0x0, line=line@entry=0, category=category@entry=WT_VERB_BLOCK, level=WT_VERBOSE_DEBUG_1, fmt=0x7f6b69ef0de0 "file extend %ld-%ld", ap=0x7ffd1c5cd1b8) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:446
#8  0x00007f6b69eb3cfb in __wt_verbose_worker (session=session@entry=0x2671a20, category=category@entry=WT_VERB_BLOCK, level=level@entry=WT_VERBOSE_DEBUG_1, fmt=fmt@entry=0x7f6b69ef0de0 "file extend %ld-%ld") at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/support/err.c:637
#9  0x00007f6b69c4af2f in __block_extend (size=28672, offp=0x7ffd1c5cdbe8, block=0x26cc000, session=0x2671a20) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/block/block_ext.c:490
#10 __wt_block_alloc (session=session@entry=0x2671a20, block=block@entry=0x26cc000, offp=offp@entry=0x7ffd1c5cdbe8, size=size@entry=28672) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/block/block_ext.c:542
#11 0x00007f6b69c522fd in __block_write_off (session=session@entry=0x2671a20, block=block@entry=0x26cc000, buf=buf@entry=0x266ae68, offsetp=offsetp@entry=0x7ffd1c5cdc98, sizep=sizep@entry=0x7ffd1c5cdc90, checksump=checksump@entry=0x7ffd1c5cdc94, data_checksum=true, checkpoint_io=true, caller_locked=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/block/block_write.c:273
#12 0x00007f6b69c5389a in __wt_block_write_off (session=session@entry=0x2671a20, block=block@entry=0x26cc000, buf=buf@entry=0x266ae68, offsetp=offsetp@entry=0x7ffd1c5cdc98, sizep=sizep@entry=0x7ffd1c5cdc90, checksump=checksump@entry=0x7ffd1c5cdc94, data_checksum=true, checkpoint_io=true, caller_locked=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/block/block_write.c:389
#13 0x00007f6b69c538d6 in __wt_block_write (session=session@entry=0x2671a20, block=0x26cc000, buf=buf@entry=0x266ae68, addr=addr@entry=0x7ffd1c5ceab0 "\304\035\207?\024\261\200\003\035ik\177", addr_sizep=addr_sizep@entry=0x7ffd1c5cebb8, data_checksum=data_checksum@entry=true, checkpoint_io=true) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/block/block_write.c:198
#14 0x00007f6b69c5c842 in __bm_write (bm=0x26de300, session=0x2671a20, buf=0x266ae68, addr=0x7ffd1c5ceab0 "\304\035\207?\024\261\200\003\035ik\177", addr_sizep=0x7ffd1c5cebb8, data_checksum=<optimized out>, checkpoint_io=true) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/block_cache/block_mgr.c:707
#15 0x00007f6b69c5b8f5 in __wt_blkcache_write (session=session@entry=0x2671a20, buf=buf@entry=0x266ae68, addr=addr@entry=0x7ffd1c5ceab0 "\304\035\207?\024\261\200\003\035ik\177", addr_sizep=addr_sizep@entry=0x7ffd1c5cebb8, compressed_sizep=compressed_sizep@entry=0x7ffd1c5cebb0, checkpoint=checkpoint@entry=false, checkpoint_io=true, compressed=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/include/time_inline.h:111
#16 0x00007f6b69e5a291 in __rec_write (session=session@entry=0x2671a20, buf=buf@entry=0x266ae68, addr=addr@entry=0x7ffd1c5ceab0 "\304\035\207?\024\261\200\003\035ik\177", addr_sizep=addr_sizep@entry=0x7ffd1c5cebb8, compressed_sizep=compressed_sizep@entry=0x7ffd1c5cebb0, checkpoint=checkpoint@entry=false, checkpoint_io=true, compressed=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_write.c:907
#17 0x00007f6b69e5d7d5 in __rec_split_write (session=session@entry=0x2671a20, r=r@entry=0x266ac00, chunk=0x266ad80, compressed_image=compressed_image@entry=0x0, last_block=last_block@entry=false) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_write.c:2133
#18 0x00007f6b69e632c2 in __wt_rec_split (session=session@entry=0x2671a20, r=r@entry=0x266ac00, next_len=231) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_write.c:1499
#19 0x00007f6b69e63745 in __wt_rec_split_crossing_bnd (session=session@entry=0x2671a20, r=r@entry=0x266ac00, next_len=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_write.c:1604
#20 0x00007f6b69e49726 in __rec_row_leaf_insert (session=session@entry=0x2671a20, r=r@entry=0x266ac00, ins=0x289fe40) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_row.c:614
#21 0x00007f6b69e4ffd9 in __wt_rec_row_leaf (session=session@entry=0x2671a20, r=r@entry=0x266ac00, pageref=pageref@entry=0x2b44780, salvage=salvage@entry=0x0) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_row.c:1011
#22 0x00007f6b69e61ca3 in __reconcile (session=session@entry=0x2671a20, ref=ref@entry=0x2b44780, salvage=salvage@entry=0x0, flags=flags@entry=68, page_lockedp=page_lockedp@entry=0x7ffd1c5d085f) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_write.c:291
#23 0x00007f6b69e60f03 in __wt_reconcile (session=session@entry=0x2671a20, ref=0x2b44780, salvage=salvage@entry=0x0, flags=flags@entry=68) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/reconcile/rec_write.c:99
#24 0x00007f6b69cdf312 in __wt_sync_file (session=session@entry=0x2671a20, syncop=syncop@entry=WT_SYNC_CHECKPOINT) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/btree/bt_sync.c:442
#25 0x00007f6b69ee3af7 in __checkpoint_tree (session=session@entry=0x2671a20, is_checkpoint=is_checkpoint@entry=true, cfg=cfg@entry=0x7ffd1c5d1830) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/txn/txn_ckpt.c:2270
#26 0x00007f6b69ee3e3d in __checkpoint_tree_helper (session=0x2671a20, cfg=0x7ffd1c5d1830) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/txn/txn_ckpt.c:2395
#27 0x00007f6b69eddebf in __checkpoint_apply_to_dhandles (session=session@entry=0x2671a20, cfg=cfg@entry=0x7ffd1c5d1830, op=op@entry=0x7f6b69ee3dc2 <__checkpoint_tree_helper>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/txn/txn_ckpt.c:352
#28 0x00007f6b69ee4c55 in __txn_checkpoint (session=session@entry=0x2671a20, cfg=0x7ffd1c5d1830) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/txn/txn_ckpt.c:1155
#29 0x00007f6b69ee5f4a in __txn_checkpoint_wrapper (session=session@entry=0x2671a20, cfg=cfg@entry=0x7ffd1c5d1830) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/txn/txn_ckpt.c:1444
#30 0x00007f6b69ee61d4 in __wt_txn_checkpoint (session=0x2671a20, cfg=cfg@entry=0x7ffd1c5d1830, waiting=waiting@entry=true) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/txn/txn_ckpt.c:1520
#31 0x00007f6b69edd26c in __wt_txn_global_shutdown (session=session@entry=0x266e000, cfg=cfg@entry=0x7ffd1c5d1d00) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/txn/txn.c:2592
#32 0x00007f6b69d1ad25 in __conn_close (wt_conn=0x261a000, config=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/src/conn/conn_api.c:1194
#33 0x00000000004020d4 in access_example (argc=3, argv=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/examples/c/ex_hello.c:164
#34 0x00000000004021b4 in main (argc=<optimized out>, argv=<optimized out>) at /root/yyz/mongodb-guanfang-master/master/wiredtiger-master/wiredtiger/examples/c/ex_hello.c:210
[root@localhost build]# 
 * ex_access.c
 * 	demonstrates how to create and access a simple table, include insert data and load exist table's data.
 */
#include <test_util.h>

static const char *home = "WT_TEST";

/*
 * usage --
 *     wtperf usage print, no error.
 */
static void
usage(void)
{
    printf("ex_access [-i] [-l]\n");
    printf("\t-i insert data and scan data\n");
    printf("\t-l load exist data and scan data\n");
    printf("\n");
}
#define MAX_TEST_KV_NUM 20000
//  clear && rm -rf WT_HOME && ./ex_hello -i 1   
/*
[1698325054:332413][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:332484][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:334638][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:334728][71578:0x7f9d57e49700], file:access.wt, evict pass: [WT_VERB_EVICTSERVER][DEBUG_2]: file:access.wt walk: seen 55, queued 0
[1698325054:334735][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:336340][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:336370][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:338519][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:338543][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:340420][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:340463][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:342356][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:342411][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:344355][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:344413][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:346369][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:346394][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:348437][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:348477][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:350411][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:350451][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:352267][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:352318][71578:0x7f9d57e49700], file:access.wt, evict pass: [WT_VERB_EVICTSERVER][DEBUG_2]: file:access.wt walk: seen 55, queued 0
[1698325054:352324][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server
[1698325054:354308][71578:0x7f9d57e49700], eviction-server: [WT_VERB_EVICTSERVER][DEBUG_2]: Eviction pass with: Max: 104857600 In use: 6172764 Dirty: 4978115
[1698325054:354340][71578:0x7f9d57e49700], eviction-server: [WT_VERB_MUTEX][DEBUG_2]: wait cache eviction server  
*/  //感觉有死锁问题

static void
access_example(int argc, char *argv[])
{
    /*! [access example connection] */
    WT_CONNECTION *conn;
    WT_CURSOR *cursor;
    WT_SESSION *session;
    const char  *value;
    int ch;
    bool insertConfig = false;
    bool loadDataConfig = false;
    char cmd_buf[512];
    char buf[512];
    int i;
    
    const char *cmdflags = "i:l:";
    /* Do a basic validation of options */
    while ((ch = __wt_getopt("ex_access", argc, argv, cmdflags)) != EOF) {
        switch (ch) {
        /* insert and scan data */
        case 'i':
            insertConfig = true;
            break;
        /* load and scan data */
        case 'l':
            loadDataConfig = true;
            break;
        case '?':
        default:
            usage();
            return;
        }
    }

    if (!insertConfig && !loadDataConfig) {
        usage();
        return;
    }

    /* prepare data */
    if (insertConfig) {
        (void)snprintf(cmd_buf, sizeof(cmd_buf), "rm -rf %s && mkdir %s", home, home);
        error_check(system(cmd_buf));

        /* Open a connection to the database, creating it if necessary. */
        //error_check(wiredtiger_open(home, NULL, "create,statistics=(all),verbose=[config_all_verbos:0, metadata:0, api:0]", &conn));
        error_check(wiredtiger_open(home, NULL, "create,statistics=(all), io_capacity=(total=1M),verbose=[all:5, metadata:0, api:0]", &conn));

        /* Open a session handle for the database. */
        error_check(conn->open_session(conn, NULL, NULL, &session));
        /*! [access example connection] */

        /*! [access example table create] */
        error_check(session->create(session, "table:access", "key_format=S,value_format=S"));
        /*! [access example table create] */

        /*! [access example cursor open] */
        error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
        /*! [access example cursor open] */

         //insert
        for (i = 0; i < MAX_TEST_KV_NUM; i++) {
            snprintf(buf, sizeof(buf), "key%d", i);
            cursor->set_key(cursor, buf);

            //value_item.data = "old value  ###############################################################################################################################################################################################################\0";
            //value_item.size = strlen(value_item.data) + 1;
            
            cursor->set_value(cursor, "old value  ###############################################################################################################################################################################################################\0");
            error_check(cursor->insert(cursor));
        }

        /*! [access example cursor insert] */

        /*! [access example cursor list] */
        /*error_check(cursor->reset(cursor));  
        while ((ret = cursor->next(cursor)) == 0) {
            error_check(cursor->get_key(cursor, &key));
            error_check(cursor->get_value(cursor, &value));

            printf("Got record: %s : %s\n", key, value);
        }
        scan_end_check(ret == WT_NOTFOUND); */ /* Check for end-of-table. */
        /*! [access example cursor list] */

        /*! [access example close] */
        error_check(conn->close(conn, NULL)); /* Close all handles. */
                                              /*! [access example close] */
    }

    /* load exist data, for example: when process restart, wo should warmup and load exist data*/
    if (loadDataConfig) {
        /* Open a connection to the database, creating it if necessary. */
        error_check(wiredtiger_open(home, NULL, "statistics=(all),verbose=[all:0, metadata:0, api:0]", &conn));

        /* Open a session handle for the database. */
        error_check(conn->open_session(conn, NULL, NULL, &session));
        /*! [access example connection] */

        /*! [access example cursor open] */
        error_check(session->open_cursor(session, "table:access", NULL, NULL, &cursor));
        /*! [access example cursor open] */

        cursor->set_key(cursor, "key111");
        error_check(cursor->search(cursor));
        error_check(cursor->get_value(cursor, &value));
        printf("Load search record: %s : %s\n", "key111", value);

        //error_check(cursor->reset(cursor)); /* Restart the scan. */
        /*while ((ret = cursor->next(cursor)) == 0) {
            error_check(cursor->get_key(cursor, &key));
            error_check(cursor->get_value(cursor, &value));

            printf("Load record: %s : %s\n", key, value);
        }
        scan_end_check(ret == WT_NOTFOUND); */
        /*! [access example cursor list] */

        /*! [access example close] */
        error_check(conn->close(conn, NULL)); /* Close all handles. */
                                              /*! [access example close] */
    }
}

/*
run step:
  step 1(prepare data):                ./ex_access -i 1
  step 2(warmup and load exist data):  ./ex_access -l 1
*/
int
main(int argc, char *argv[])
{
    access_example(argc, argv);

    return (EXIT_SUCCESS);
}



