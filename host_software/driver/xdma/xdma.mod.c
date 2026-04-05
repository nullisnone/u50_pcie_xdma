#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xa569808b, "module_layout" },
	{ 0x2d3385d3, "system_wq" },
	{ 0x1848b8a7, "dma_map_sg_attrs" },
	{ 0xe50a0773, "cdev_del" },
	{ 0x16e180ec, "kmalloc_caches" },
	{ 0xe2c17b5d, "__SCT__might_resched" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x2b8cf2d9, "cdev_init" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0x18e8a83, "pci_write_config_word" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0x494ae084, "pci_read_config_byte" },
	{ 0x46cf10eb, "cachemode2protval" },
	{ 0x5bbdd589, "kmalloc_trace" },
	{ 0x3f84cfd, "dma_unmap_sg_attrs" },
	{ 0x85bc4c9f, "dma_set_mask" },
	{ 0x48396c8e, "pcie_set_readrq" },
	{ 0x4649a7a8, "boot_cpu_data" },
	{ 0xf49db96, "pci_disable_device" },
	{ 0x5a673ba8, "pci_disable_msix" },
	{ 0x6e6a08a, "set_page_dirty_lock" },
	{ 0x837b7b09, "__dynamic_pr_debug" },
	{ 0xd7695b93, "device_destroy" },
	{ 0x76308f9f, "kobject_set_name" },
	{ 0x6729d3df, "__get_user_4" },
	{ 0x533065e8, "__folio_put" },
	{ 0xfbe215e4, "sg_next" },
	{ 0x83e26644, "alloc_pages" },
	{ 0x3834afb7, "pci_release_regions" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x999e8297, "vfree" },
	{ 0xafc4df40, "dma_free_attrs" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0x142a0e27, "dma_set_coherent_mask" },
	{ 0x15ba50a6, "jiffies" },
	{ 0xbcd47cfe, "pcie_capability_clear_and_set_word_unlocked" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x85a8deac, "pci_set_master" },
	{ 0xfb578fc5, "memset" },
	{ 0x980d5862, "pci_restore_state" },
	{ 0x53648d9f, "pci_iounmap" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0xe1537255, "__list_del_entry_valid" },
	{ 0x54a2f082, "pci_read_config_word" },
	{ 0xadc56e64, "dma_alloc_attrs" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0xc52db6bb, "class_create" },
	{ 0x9cbfb38f, "device_create" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0x6091797f, "synchronize_rcu" },
	{ 0x3c393330, "pci_enable_msi" },
	{ 0x68f31cbd, "__list_add_valid" },
	{ 0x98378a1d, "cc_mkdec" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0xee124168, "pci_find_capability" },
	{ 0xbcb36fe4, "hugetlb_optimize_vmemmap_key" },
	{ 0x2be5967a, "cdev_add" },
	{ 0xb3f985a8, "sg_alloc_table" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0xc81d3d47, "__free_pages" },
	{ 0xb2fd5ceb, "__put_user_4" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x9cb986f2, "vmalloc_base" },
	{ 0x8ddd8aad, "schedule_timeout" },
	{ 0x1000e51, "schedule" },
	{ 0x92997ed8, "_printk" },
	{ 0x7aa5b3b9, "dma_map_page_attrs" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x159fc17e, "pci_unregister_driver" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0x53e9a1d3, "pci_irq_vector" },
	{ 0xe2964344, "__wake_up" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0xd0008929, "__put_devmap_managed_page_refs" },
	{ 0xbe9f85eb, "pcpu_hot" },
	{ 0x37a0cba, "kfree" },
	{ 0xe748bd1e, "remap_pfn_range" },
	{ 0x4e2b8df8, "pci_request_regions" },
	{ 0x3a4b5047, "pci_disable_msi" },
	{ 0x6128b5fc, "__printk_ratelimit" },
	{ 0x659e1c5e, "__pci_register_driver" },
	{ 0xa22f07f3, "class_destroy" },
	{ 0xf07cb09c, "dma_unmap_page_attrs" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x48d88a2c, "__SCT__preempt_schedule" },
	{ 0xc8c85086, "sg_free_table" },
	{ 0xc5b6f236, "queue_work_on" },
	{ 0x8dd7de1a, "pci_alloc_irq_vectors" },
	{ 0x3b736c2b, "pci_iomap" },
	{ 0xd9cb1131, "pci_enable_device_mem" },
	{ 0x7f02188f, "__msecs_to_jiffies" },
	{ 0x4a453f53, "iowrite32" },
	{ 0xa2fe031, "pci_enable_device" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x915f355c, "param_ops_uint" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xa78af5f3, "ioread32" },
	{ 0x7f9002ec, "get_user_pages_fast" },
	{ 0x3864566a, "pcie_capability_read_word" },
	{ 0xc1514a3b, "free_irq" },
	{ 0x7d1bf2bf, "pci_save_state" },
	{ 0xe914e41e, "strcpy" },
	{ 0x587f22d7, "devmap_managed_key" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("pci:v000010EEd0000903Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009038sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009028sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009018sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009034sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009024sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009014sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009032sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009022sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009012sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009031sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009021sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00009011sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008011sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008012sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008014sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008018sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008021sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008022sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008024sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008028sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008031sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008032sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008034sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00008038sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007011sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007012sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007014sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007018sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007021sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007022sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007024sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007028sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007031sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007032sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007034sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00007038sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006828sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006830sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006928sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006930sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A28sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006A30sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00006D30sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004808sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004828sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004908sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004A28sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00004B28sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v000010EEd00002808sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "14E15F68A3EE39F20320FF6");
MODULE_INFO(rhelversion, "9.4");
