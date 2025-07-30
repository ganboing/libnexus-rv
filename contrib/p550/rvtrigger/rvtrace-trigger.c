#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/kprobes.h>
#include <asm/sbi.h>

enum sbi_ext_id2 {
	SBI_EXT_DBTR = 0x44425452,
};

enum sbi_ext_dbtr_fid {
	SBI_EXT_DBTR_NUM_TRIGGERS,
	SBI_EXT_DBTR_SET_SHMEM,
	SBI_EXT_DBTR_TRIG_READ,
	SBI_EXT_DBTR_TRIG_INSTALL,
	SBI_EXT_DBTR_TRIG_UPDATE,
	SBI_EXT_DBTR_TRIGSET_UNINSTALL,
	SBI_EXT_DBTR_TRIGSET_ENABLE,
	SBI_EXT_DBTR_TRIGSET_DISABLE,
};

union trig_state {
	unsigned long val;
	struct {
		unsigned char mapped : 1;
		unsigned char u : 1;
		unsigned char s : 1;
		unsigned char vu : 1;
		unsigned char vs : 1;
		unsigned char have_hw_trig : 1;
		unsigned char : 2;
		unsigned long hw_trig_idx : BITS_PER_LONG - 8;
	};
};


struct sbi_dbtr_shmem_msg_v1_6 {
	union trig_state state;
	unsigned long tdata1;
	unsigned long tdata2;
	unsigned long tdata3;
	unsigned long idx;
};

struct sbi_dbtr_shmem_msg_v1_7 {
	union {
		union trig_state state;
		unsigned long idx;
	};
	unsigned long tdata1;
	unsigned long tdata2;
	unsigned long tdata3;
};

#define sbi_dbtr_shmem_msg sbi_dbtr_shmem_msg_v1_6

union csr_mcontrol6 {
	unsigned long val;
	struct {
		unsigned char load : 1;
		unsigned char store : 1;
		unsigned char execute : 1;
		unsigned char u : 1;
		unsigned char s : 1;
		unsigned char uncertainen : 1;
		unsigned char m : 1;
		unsigned short match : 4;
		unsigned char chain : 1;
		unsigned short action : 4;
		unsigned short size : 3;
		unsigned char : 2;
		unsigned char select : 1;
		unsigned char hit0 : 1;
		unsigned char vu : 1;
		unsigned char vs : 1;
		unsigned char hit1 : 1;
		unsigned char uncertain : 1;
		unsigned long : BITS_PER_LONG - 32;
		unsigned char dmode : 1;
		unsigned char type : 4;
	};
};

static int sbi_dbtr_call(unsigned long fid,
			 unsigned long arg0,
			 unsigned long arg1)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_DBTR, fid, arg0, arg1, 0, 0, 0, 0);
	pr_info("(%ld, %ld, %ld) = (%ld, %ld)\n", fid, arg0, arg1, ret.error, ret.value);
	if (ret.error)
		return sbi_err_map_linux_errno(ret.error);
	return ret.value;
}

static char *addrs[4] = {
	"__switch_to",
	"__kvm_switch_resume",
	"__kvm_switch_return",
};
static int addrs_cnt = 3;
module_param_array(addrs, charp, &addrs_cnt, 0444);
MODULE_PARM_DESC(addrs, "Address to tirgger the trace SYNC");

static unsigned long resolved_addrs[ARRAY_SIZE(addrs)];
static int num_trig;
static enum cpuhp_state hp_state;
static DEFINE_PER_CPU(struct sbi_dbtr_shmem_msg*, shmem_msgs);

static struct kprobe kp_kln = {
	.symbol_name = "kallsyms_lookup_name",
};

static void __rvtrigger_cpu_release(unsigned int cpu)
{
	int ret;

	ret = sbi_dbtr_call(SBI_EXT_DBTR_SET_SHMEM, -1, -1);
	if (ret) {
		pr_err("SBI_EXT_DBTR_SET_SHMEM failed: %d\n", ret);
		return;
	}
	kfree(__this_cpu_read(shmem_msgs));
	__this_cpu_write(shmem_msgs, NULL);
}

static int rvtrigger_cpu_online(unsigned int cpu)
{
	struct sbi_dbtr_shmem_msg *msgs;
	unsigned long trig_set = 0;
	union csr_mcontrol6 tdata1 = { 0 };
	int ret, trig_idx, i;

	tdata1.execute = 1;
	tdata1.s = 1;
	tdata1.vs = 1;
	tdata1.type = 6;
	tdata1.action = 4;

	msgs = kzalloc_node(sizeof(*msgs) * num_trig, GFP_KERNEL,
			cpu_to_node(cpu));
	if (!msgs)
		return -ENOMEM;

	__this_cpu_write(shmem_msgs, msgs);
	pr_info("starting cpu %u\n", cpu);

	ret = sbi_dbtr_call(SBI_EXT_DBTR_SET_SHMEM, __pa(msgs), 0);
	if (ret) {
		pr_err("SBI_EXT_DBTR_SET_SHMEM failed: %d\n", ret);
		goto free;
	}
	/* Uninstall all */
	for (i = 0; i < num_trig; ++i)
		sbi_dbtr_call(SBI_EXT_DBTR_TRIGSET_UNINSTALL, i, 1);

	for (i = 0, trig_idx = 0; i < addrs_cnt; ++i) {
		if (!resolved_addrs[i])
			continue;
		msgs[trig_idx].tdata1 = tdata1.val;
		msgs[trig_idx].tdata2 = resolved_addrs[i];
		msgs[trig_idx].tdata3 = 0;
		trig_set |= 1UL << trig_idx;
		++trig_idx;
	}
	ret = sbi_dbtr_call(SBI_EXT_DBTR_TRIG_INSTALL, trig_idx, 0);
	if (ret) {
		pr_err("SBI_EXT_DBTR_TRIG_INSTALL failed: %d\n", ret);
		goto free;
	}
	ret = sbi_dbtr_call(SBI_EXT_DBTR_TRIGSET_ENABLE, 0, trig_set);
	if (ret) {
		pr_err("SBI_EXT_DBTR_TRIGSET_ENABLE failed: %d\n", ret);
		sbi_dbtr_call(SBI_EXT_DBTR_TRIGSET_DISABLE, 0, trig_set);
		goto free;
	}
	/* Remember OpenSBI's affected by the -1 bug, thus can't read all triggers  */
	ret = sbi_dbtr_call(SBI_EXT_DBTR_TRIG_READ, 0, trig_idx);
	if (ret) {
		pr_err("SBI_EXT_DBTR_TRIG_READ failed: %d\n", ret);
		goto free;
	}
	for (i = 0; i < trig_idx; ++i) {
		pr_info("trig %d: state=0x%lx tdata1=0x%lx tdata2=0x%lx tdata3=0x%lx\n",
			i, msgs[i].state.val, msgs[i].tdata1, msgs[i].tdata2, msgs[i].tdata3);
	}
	return 0;
free:
	__rvtrigger_cpu_release(cpu);
	return ret;
}

static int rvtrigger_cpu_offline(unsigned int cpu)
{
	unsigned long trig_set = 0;
	int trig_idx, i;

	pr_info("stopping cpu %u\n", cpu);
	for (i = 0, trig_idx = 0; i < addrs_cnt; ++i) {
		if (!resolved_addrs[i])
			continue;
		trig_set |= 1UL << trig_idx;
		++trig_idx;
	}
	sbi_dbtr_call(SBI_EXT_DBTR_TRIGSET_DISABLE, 0, trig_set);
	sbi_dbtr_call(SBI_EXT_DBTR_TRIGSET_UNINSTALL, 0, trig_set);
	__rvtrigger_cpu_release(cpu);
	return 0;
}

static int rvtrigger_init(void)
{
	typeof(&kallsyms_lookup_name) kln;
	union csr_mcontrol6 tdata1 = { 0 };
	int ret, i;

	if (!sbi_probe_extension(SBI_EXT_DBTR)) {
		pr_err("SBI DBTR extension is not available\n");
		return -ENODEV;
	}
	tdata1.type = 6;
	ret = sbi_dbtr_call(SBI_EXT_DBTR_NUM_TRIGGERS, tdata1.val, 0);
	if (ret < 0) {
		pr_err("SBI_EXT_DBTR_NUM_TRIGGERS failed: %d\n", ret);
		return ret;
	}
	num_trig = ret;
	pr_info("%u triggers available\n", num_trig);
	if (num_trig < addrs_cnt) {
		pr_err("running short of triggers %u vs %u\n", num_trig, addrs_cnt);
		return -E2BIG;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	register_kprobe(&kp_kln);
	kln = (void *)kp_kln.addr;
	unregister_kprobe(&kp_kln);
#else
	kln = &kallsyms_lookup_name;
#endif
	for (i = 0 ; i < addrs_cnt; ++i) {
		resolved_addrs[i] = kstrtoul(addrs[i], NULL, 0);
		if (resolved_addrs[i])
			goto resolved;
		resolved_addrs[i] = kln(addrs[i]);
		if (resolved_addrs[i])
			goto resolved;
		pr_err("unable to resolve %s (ignored)\n", addrs[i]);
		continue;
resolved:
		pr_info("resolved %s to 0x%lx\n", addrs[i], resolved_addrs[i]);
	}

	hp_state = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "rv-trigger",
				rvtrigger_cpu_online, rvtrigger_cpu_offline);
	if (hp_state < 0)
		return hp_state;

	return 0;
}

static void rvtrigger_exit(void)
{
	pr_info("exiting\n");
	cpuhp_remove_state(hp_state);
}

module_init(rvtrigger_init);
module_exit(rvtrigger_exit);
MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Bo Gan");
