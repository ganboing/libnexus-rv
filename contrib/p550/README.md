# P550 trace supplementary materials

P550 HW trace encoder uses pre-1.0 spec. Noticeable incompatibilities:
* Timestamp uses XOR scheme
* ResourceFull message RCODE 8/9 for repeated HIST
* Implicit Return Optimization is hard-wired to Simple counting

The return stack depth is 1023. This can be verified by the [test-stack-depth.cpp](./test-stack-depth.cpp)
program. It's meant to be built with `gcc -ftemplate-depth=2000 -Wall -Wextra -Os`

Run it with trace capture (See below for how to capture trace), and it'll produce
something like:
```
addr_func[1029]=0x31de5567a8
...
addr_func[1]=0x31de56181c
addr_func[0]=0x31de556794
addr_ret[0]=0x31de56183e
...
addr_ret[1023]=0x31de5568a6
...
addr_ret[1029]=0x31de556658
```

Trace decoding shows something like:
```
...
[25310090] +4713542 I-CNT 17
[25310209] +4713542 INDIRECT to 0x31de56181c
[25310209] +4713991 I-CNT 17
[25310223] +4713991 INDIRECT to 0x31de556794
[25310223] +4714000 I-CNT 5094
[25323908] +4714000 INDIRECT to 0x31de5568a6
[25323908] +4714006 I-CNT 5
...
```
First we tried to fill up the trace encoder stack, then drain it. The implicit
return optimization covered 1023 returns that would otherwise be emitted as
indirect branches. Thus, we conclude that the encoder return stack depth is 1023.

## End-to-End Demo

### Rebuild the Linux kernel with the following patches:

Patches to fix vendor kernel bugs and reserve memory for trace buffer
 * [0001-riscv-asm-fix-EIC770X-sys-mem-port-conversion.patch](https://github.com/ganboing/riscv-trace-umd/raw/refs/heads/master/patches/0001-riscv-asm-fix-EIC770X-sys-mem-port-conversion.patch)
 * [0001-riscv-dts-hifive-premier-p550-reserve-4g-for-trace-b.patch](https://github.com/ganboing/riscv-trace-umd/raw/refs/heads/master/patches/0001-riscv-dts-hifive-premier-p550-reserve-4g-for-trace-b.patch)

Patches to help capture the correct trace and enable full kcore
 * [0001-riscv-enable-ARCH_PROC_KCORE_TEXT.patch](./kernel-patches/0001-riscv-enable-ARCH_PROC_KCORE_TEXT.patch)
 * [0001-riscv-entry-Use-equivalent-instructions-for-better-t.patch](./kernel-patches/0001-riscv-entry-Use-equivalent-instructions-for-better-t.patch)

### Install the python user-mode trace driver

Instruction is at https://github.com/ganboing/riscv-trace-umd
Ensure you can do `sudo rvtrace config`. Sample output for reference:
```shell
$ sudo rvtrace config
Device hart0-encoder reset, control=81000009 nread=0
Device hart0-encoder timestamp unit reset, nread=0
Device hart1-encoder reset, control=81000009 nread=0
Device hart1-encoder timestamp unit reset, nread=0
Device hart2-encoder reset, control=81000009 nread=0
Device hart2-encoder timestamp unit reset, nread=0
Device hart3-encoder reset, control=81000009 nread=0
Device hart3-encoder timestamp unit reset, nread=0
Device sys-funnel reset, control=70000009 nread=0
Device sys-funnel timestamp unit reset, nread=0
sys-funnel TraceFunnelV0 @18000: configured
hart1-encoder TraceEncoderV0 @101000: configured
hart0-encoder TraceEncoderV0 @100000: configured
hart2-encoder TraceEncoderV0 @102000: configured
hart3-encoder TraceEncoderV0 @103000: configured
```

Without specifying `-c`, rvtrace will use the default config, which is located
at `/usr/local/lib/python3.12/dist-packages/rvtrace/platforms/p550.cfg`. If any
change is required, make a copy of it and use `-c` to specify the modified one.
 In the default configuration, HTM is used with maximum periodic SYNC.

### Install the rvtrace-trigger kernel module to have trace SYNC triggers

Before we start trace capture, there's one extra step. Given that P550 hard-wires
simple counting for implicit return optimization, it doesn't detect function return
address mismatches. Due to this limitation, the return stack will be messed up upon
context switch (task switch) in Linux. The same would happen on KVM world switches
as well. Thus, we need to explicitly clear the encoder's return stack upon context
switch. This is achieved by utilizing the DBTR extension in OpenSBI to install
triggers that fire upon \__switch_to (host task switch), \__kvm_switch_resume
(Host -> VM) and \__kvm_switch_return (VM -> Host). The triggers are set to action
4 (Trace SYNC). When hit, a SYNC message with reason 6 is generated that resets the
encoder return stack.

```shell
$ cd rvtrigger
$ make
$ sudo insmod rvtrace-trigger.ko
$ dmesg
[33446.074983] rvtrace_trigger:rvtrigger_cpu_online: starting cpu 0
...
[33446.075043] rvtrace_trigger:rvtrigger_cpu_online: trig 0: state=0x15 tdata1=0x6000000001004014 tdata2=0xffffffffb7e0a700 tdata3=0x0
[33446.075050] rvtrace_trigger:rvtrigger_cpu_online: trig 1: state=0x115 tdata1=0x6000000001004014 tdata2=0xffffffffb722c7a6 tdata3=0x0
[33446.075056] rvtrace_trigger:rvtrigger_cpu_online: trig 2: state=0x215 tdata1=0x6000000001004014 tdata2=0xffffffffb722c7ac tdata3=0x0

```

### Install other utilities

* Follow instruction [here](https://github.com/ganboing/linuxlib/blob/master/README.md)
  to install *linuxlib*, my collection of linux utilities to facilitate trace capture

### Capture trace

First create an empty directory, and this will be our working directory.
Use the following commands as reference:
```shell
wkdir$ sudo taskset -c 2 ping -q -f <gateway> & sleep 0.1 #Start the workload
wkdir$ sudo rvtrace start && sleep 0.1 && sudo rvtrace stop #Capture trace for 0.1s
wkdir$ sudo gcore `pidof ping` #Capture user space core
wkdir$ kill $! #Stop the workload
wkdir$ sudo linux_bundle > linux-bundle.tar #Capture kernel space core and metadata
wkdir$ sudo rvtrace dump trace.bin
```

### Decode trace

The workload, in this case, flood ping, was started on CPU 2, so we can just
focus on that CPU. There's one caveat -- in Linux, the CPU id is not necessarily
the hart id. Therefore, it's required to get such mapping from `/proc/cpuinfo`
```shell
$ cat /proc/cpuinfo
processor	: 0
hart		: 2
...
processor	: 1
hart		: 0
...
processor	: 2
hart		: 1
...
processor	: 3
hart		: 3
...
```

Now we know that the CPU 2 corresponds to hart 1, so we can just decode trace of hart 1:
Notice the `--filter` switch.
```shell
wkdir$ tar -xvf linux-bundle.tar #Extract the linux bundle
wkdir$ nexusrv-replay --hwcfg model=p550x4,timerfreq=700Mhz --filter 1 --procfs proc --sysfs sys --kcore -u core.<pid> trace.bin
```

Explanation of other flags:
* `--hwcfg`: My particular P550 SoC (EIC7700x) is a 4 Hart P550 HW running at 1.4Ghz,
             and the timer is running at half the frequency, 700Mhz.
* `--filter`: Filter the trace from a given SRC. In our case, we focus on hart 1 as said before.
* `--procfs`: The `proc` directory extracted from linux-bundle.tar
* `--sysfs`: The `sys` directory extracted from linux-bundle.tar
* `--kcore`: Load Linux kcore. The path is derived from `<procfs>/kcore`
* `--ucore`: Load User space core. The path is the core file generated by the `gcore` command

### Observe trace of userspace + kernel + firmware

The lines are in the format of
`[time] +<trace.bin offset> <addr>,+<icnt>  instruction  <function>:<line number>`

* Transitioning from user -> kernel (syscall)
```
[748537] +50286  0x555c8818fc30,+15      bne a1, a3, -8 [taken]               ping4_send_probe; ping.c:1572
[748537] +50286  0x555c8818fc42,+6       bne a1, a3, -8 [taken]                               ; ping.c:1512
[748537] +50286  0x555c8818fc42,+6       bne a1, a3, -8 [taken]                               ; ping.c:1512
[748537] +50286  0x555c8818fc42,+6       bne a1, a3, -8 [taken]                               ; ping.c:1512
[748537] +50286  0x555c8818fc42,+6       bne a1, a3, -8 [taken]                               ; ping.c:1512
[748537] +50286  0x555c8818fc42,+6       bne a1, a3, -8 [taken]                               ; ping.c:1512
[748537] +50286  0x555c8818fc42,+6       bne a1, a3, -8 [taken]                               ; ping.c:1512
[748537] +50286  0x555c8818fc42,+6       bne a1, a3, -8                                       ; ping.c:1512
[748537] +50286  0x555c8818fc4e,+14      c.j -0xba                                            ; ping.c:1523
[748537] +50286  0x555c8818fbae,+12      jal -0x18a2 [stack:2->3]                             ; ping.c:1575
[748537] +50286  0x555c8818e320,+6       jalr t1, t3, 0                        ??; ??:0
[748569] +50286  0x7ff3338165a4,+11      bnez a6, 0x20                         __libc_sendto; sendto.c:25
[748569] +50300  0x7ff3338165ba,+4       ecall                                              ; sendto.c:27 (discriminator 1)
[748580] +50300  0xffffffff80c14094,+4   bnez tp, 0x1e [taken]                 __entry_text_end; entry.S:27
[748580] +50309  0xffffffff80c140b6,+65  bgez s4, 0xa [taken]                  _save_context; entry.S:43
[748580] +50309  0xffffffff80c1413e,+13  bgeu t0, t2, 0xc                                   ; entry.S:97
[748580] +50309  0xffffffff80c14158,+3   c.jalr t1 [stack:3->4]                             ; entry.S:103
[748652] +50309  0xffffffff80c08544,+11  c.bnez a4, 0x54                        do_trap_ecall_u; traps.c:308
[748652] +50319  0xffffffff80c0855a,+10  c.bnez a5, 0xbc                                       ; traps.c:312
[748652] +50319  0xffffffff80c0856e,+3   jal 0x45c [stack:4->5]                                ; traps.c:317
[748652] +50319  0xffffffff80c089cc,+19  c.bnez a5, 0x58                         syscall_enter_from_user_mode; common.c:105
[748652] +50319  0xffffffff80c089f2,+11  c.jr ra [implicit] [stack:5->4]                                     ; common.c:116
[748652] +50319  0xffffffff80c08574,+4   bltu a5, a0, 0xa2                      do_trap_ecall_u; traps.c:319
[748652] +50319  0xffffffff80c0857c,+9   c.jalr a5 [stack:4->5]                 syscall_handler; syscall.h:88
[748705] +50319  0xffffffff809d0b56,+13  jal -0x178 [stack:5->6]                 __riscv_sys_sendto; socket.c:2204
[748705] +50328  0xffffffff809d09f4,+32  jalr ra, ra, -0x628 [stack:6->7]         __sys_sendto; socket.c:2165
```

* Transitioning from kernel -> user (return from syscall)
```
[761297] +52837  0xffffffff809d0afc,+4    c.bnez a5, 0x38                          __sys_sendto; socket.c:2196
[761297] +52837  0xffffffff809d0b04,+7    c.bnez a5, 0x3e                                      ; socket.c:2202
[761297] +52837  0xffffffff809d0b12,+17   c.jr ra [implicit] [stack:6->5]                      ; socket.c:2202
[761297] +52837  0xffffffff809d0b70,+9    c.jr ra [implicit] [stack:5->4]         __riscv_sys_sendto; socket.c:2204
[761297] +52837  0xffffffff80c0858e,+4    jal 0x55e [stack:4->5]                 syscall_handler; syscall.h:90 (discriminator 1)
[761297] +52837  0xffffffff80c08af0,+10   jalr ra, ra, 0x7d2 [stack:5->6]         syscall_exit_to_user_mode; common.c:294
[761309] +52837  0xffffffff800cd2ce,+14   c.bnez a5, 0x28                          syscall_exit_to_user_mode_prepare; common.c:259
[761309] +52859  0xffffffff800cd2ea,+12   c.jr ra [implicit] [stack:6->5]                                           ; common.c:279
[761309] +52859  0xffffffff80c08b04,+7    c.bnez a5, 0x2e                         arch_local_irq_disable; irqflags.h:28
[761309] +52859  0xffffffff80c08b12,+11   c.jr ra [implicit] [stack:5->4]         arch_static_branch; context_tracking_state.h:108 (discriminator 4)
[761309] +52859  0xffffffff80c08596,+11   c.jr ra [implicit] [stack:4->3]        do_trap_ecall_u; traps.c:334
[761309] +52859  0xffffffff80c1415e,+1    c.j 8                                 _save_context; entry.S:105
[761309] +52859  0xffffffff80c14166,+4    c.bnez s0, 0xc                        ret_from_exception; entry.S:118
[761309] +52859  0xffffffff80c1416e,+46   sret                                                    ; entry.S:129
[761438] +52859  0x7ff3338165c2,+4        bltu a5, a0, 0x64                     __libc_sendto; sendto.c:27 (discriminator 1)
[761438] +52874  0x7ff3338165ca,+6        c.jr ra [implicit] [stack:3->2]                    ; sendto.c:33
[761438] +52874  0x555c8818fbc6,+3        bne s4, a0, 6                        ping4_send_probe; ping.c:1575 (discriminator 1)
[761438] +52874  0x555c8818fbcc,+8        c.bnez a5, 0x90                                      ; ping.c:1577 (discriminator 2)                                           ; entry.S:39
...
```

* Transitioning from kernel -> firmware -> kernel (rdtime trap)
```
[758700] +51916  0xffffffff800dbeb4,+1    c.j -0x1ae                                             remove_hrtimer; hrtimer.c:1170
[758700] +51916  0xffffffff800dbd06,+4    c.beqz a4, 0x1a                                        __hrtimer_start_range_ns; hrtimer.c:1247
[758700] +51916  0xffffffff800dbd0e,+3    c.jalr a5 [stack:20->21]                                                       ; hrtimer.c:1248
[758788] +51916  0xffffffff800ddfa4,+14   c.beqz a5, 0xc [taken]                                  ktime_get; timekeeping.c:837
[758788] +51935  0xffffffff800ddfca,+7    c.bnez a4, -0x14                                        __seqprop_raw_spinlock_sequence; seqlock.h:274 (discriminator 2)
[758788] +51935  0xffffffff800ddfd8,+8    c.jalr a5 [stack:21->22]                                ktime_get; timekeeping.c:846 (discriminator 1)
[758858] +51935  0xffffffff809a4b08,+8    [retired 3] rdtime a0
[758875] +51956 INDIRECT interrupt exception to 0x80000400
[758875] +51975 I-CNT 126
[758875] +51975 TNT .!...
[758962] +51975 INDIRECT to 0x80004374
[758962] +51992 I-CNT 52
[758962] +51992 TNT ..
[759030] +51992 INDIRECT to 0x8000c42e
[759030] +52010 I-CNT 89
[759030] +52010 TNT ....
[759038] +52010 INDIRECT to 0x8000f162
[759038] +52028 I-CNT 14
[759038] +52028 TNT !.
[759046] +52028 INDIRECT to 0x8000cf78
[759046] +52043 I-CNT 16
[759046] +52043 TNT .
[759083] +52043 INDIRECT to 0x8000cf2c
[759083] +52060 I-CNT 56
[759083] +52060 TNT ..
[759134] +52060 INDIRECT to 0x8000c524
[759134] +52085 I-CNT 115
[759134] +52085 TNT !.
[759220] +52085 INDIRECT to 0xffffffff809a4b12 riscv_clocksource_rdtime; timer-riscv.c:69
[759220] +52085  0xffffffff809a4b12,+3    c.jr ra [implicit] [stack:22->21]                                                ; timer-riscv.c:69
[759220] +52105  0xffffffff800ddfe8,+16   bne s1, a4, -0x3a                                       timekeeping_get_delta; timekeeping.c:292
[759220] +52105  0xffffffff800de008,+24   c.jr ra [implicit] [stack:21->20]                       clocksource_delta; timekeeping_internal.h:32
[759220] +52105  0xffffffff800dbd14,+4    bltz a5, 0x96                                          ktime_add_safe; hrtimer.c:331
```

* Transitioning from user -> firmware -> user (rdtime trap in VDSO)
```
[762017] +53071  0x555c88192420,+20       bnez a4, 0x206                      pinger; ping_common.c:314
[762017] +53071  0x555c88192448,+8        c.beqz a4, 0xe [taken]                    ; ping_common.c:320 (discriminator 1)
[762017] +53071  0x555c88192464,+7        c.bnez a5, 0xa [taken]                    ; ping_common.c:320 (discriminator 3)
[762017] +53071  0x555c8819247a,+5        jal -0x4070 [stack:1->2]                  ; ping_common.c:331
[762017] +53071  0x555c8818e410,+6        jalr t1, t3, 0                       ??; ??:0
[762124] +53071  0x7ff3337eef0c,+6        c.beqz a5, 0x18                      __GI___clock_gettime; clock_gettime.c:38
[762124] +53086  0x7ff3337eef18,+5        c.jalr a5 [stack:2->3]                                   ; clock_gettime.c:30
[762140] +53086  0x7ff33392580a,+6        bltu a5, a0, 0xc8
[762140] +53107  0x7ff333925816,+13       c.beqz a3, 0xee [taken]
[762140] +53107  0x7ff33392591c,+3        c.bnez a5, 0x12
[762140] +53107  0x7ff333925922,+3        bne a0, a5, -0x4a
[762140] +53107  0x7ff333925928,+5        c.j -0x100
[762140] +53107  0x7ff333925830,+6        c.bnez a2, 0xca
[762140] +53107  0x7ff33392583c,+4        c.beqz a5, 0x98
[762140] +53107  0x7ff333925844,+22       [retired 0] rdtime a5
[762231] +53107 INDIRECT interrupt exception to 0x80000400
[762231] +53123 I-CNT 126
[762231] +53123 TNT .!...
[762305] +53123 INDIRECT to 0x80004374
[762305] +53138 I-CNT 52
[762305] +53138 TNT ..
[762336] +53138 INDIRECT to 0x8000c42e
[762336] +53152 I-CNT 89
[762336] +53152 TNT ....
[762351] +53152 INDIRECT to 0x8000f162
[762351] +53167 I-CNT 14
[762351] +53167 TNT !.
[762363] +53167 INDIRECT to 0x8000cf78
[762363] +53180 I-CNT 16
[762363] +53180 TNT .
[762378] +53180 INDIRECT to 0x8000cf2c
[762378] +53193 I-CNT 56
[762378] +53193 TNT ..
[762398] +53193 INDIRECT to 0x8000c524
[762398] +53213 I-CNT 115
[762398] +53213 TNT !.
[762450] +53213 INDIRECT to 0x7ff333925848
[762450] +53213  0x7ff333925848,+20       bne a3, a2, -0x3c
[762450] +53232  0x7ff333925870,+18       bgeu a2, a5, 0x22 [taken]
[762450] +53232  0x7ff3339258b2,+20       c.jr ra [implicit] [stack:3->2]
[762450] +53232  0x7ff3337eef22,+1        c.bnez a0, 0x1c                                          ; clock_gettime.c:43
[762450] +53232  0x7ff3337eef24,+5        c.jr ra [implicit] [stack:2->1]                          ; clock_gettime.c:44
[762450] +53232  0x555c88192484,+24       beqz a2, 0x160 [taken]              pinger; ping_common.c:332
[762450] +53232  0x555c88192610,+4        blt a1, a5, -0x15e                        ; ping_common.c:337
```
