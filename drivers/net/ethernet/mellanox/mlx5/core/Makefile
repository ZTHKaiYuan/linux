# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Mellanox 5th generation network adapters
# (ConnectX series) core & netdev driver
#

subdir-ccflags-y += -I$(src)

obj-$(CONFIG_MLX5_CORE) += mlx5_core.o

#
# mlx5 core basic
#
mlx5_core-y :=	main.o cmd.o debugfs.o fw.o eq.o uar.o pagealloc.o \
		health.o mcg.o cq.o alloc.o port.o mr.o pd.o \
		transobj.o vport.o sriov.o fs_cmd.o fs_core.o pci_irq.o \
		fs_counters.o fs_ft_pool.o rl.o lag/debugfs.o lag/lag.o dev.o events.o wq.o lib/gid.o \
		lib/devcom.o lib/pci_vsc.o lib/dm.o lib/fs_ttc.o diag/fs_tracepoint.o \
		diag/fw_tracer.o diag/crdump.o devlink.o diag/rsc_dump.o diag/reporter_vnic.o \
		fw_reset.o qos.o lib/tout.o lib/aso.o wc.o fs_pool.o

#
# Netdev basic
#
mlx5_core-$(CONFIG_MLX5_CORE_EN) += en/rqt.o en/tir.o en/rss.o en/rx_res.o \
		en/channels.o en_main.o en_common.o en_fs.o en_ethtool.o \
		en_tx.o en_rx.o en_dim.o en_txrx.o en/xdp.o en_stats.o \
		en_selftest.o en/port.o en/monitor_stats.o en/health.o \
		en/reporter_tx.o en/reporter_rx.o en/params.o en/xsk/pool.o \
		en/xsk/setup.o en/xsk/rx.o en/xsk/tx.o en/devlink.o en/ptp.o \
		en/qos.o en/htb.o en/trap.o en/fs_tt_redirect.o en/selq.o \
		lib/crypto.o lib/sd.o en/pcie_cong_event.o

#
# Netdev extra
#
mlx5_core-$(CONFIG_MLX5_EN_ARFS)     += en_arfs.o
mlx5_core-$(CONFIG_MLX5_EN_RXNFC)    += en_fs_ethtool.o
mlx5_core-$(CONFIG_MLX5_CORE_EN_DCB) += en_dcbnl.o en/port_buffer.o
mlx5_core-$(CONFIG_PCI_HYPERV_INTERFACE) += en/hv_vhca_stats.o
mlx5_core-$(CONFIG_MLX5_ESWITCH)     += lag/mp.o lag/port_sel.o lib/geneve.o lib/port_tun.o \
					en_rep.o en/rep/bond.o en/mod_hdr.o \
					en/mapping.o lag/mpesw.o
mlx5_core-$(CONFIG_MLX5_CLS_ACT)     += en_tc.o en/rep/tc.o en/rep/neigh.o \
					lib/fs_chains.o en/tc_tun.o \
					esw/indir_table.o en/tc_tun_encap.o \
					en/tc_tun_vxlan.o en/tc_tun_gre.o en/tc_tun_geneve.o \
					en/tc_tun_mplsoudp.o diag/en_tc_tracepoint.o \
					en/tc/post_act.o en/tc/int_port.o en/tc/meter.o \
					en/tc/post_meter.o en/tc/act_stats.o

mlx5_core-$(CONFIG_MLX5_CLS_ACT)     += en/tc/act/act.o en/tc/act/drop.o en/tc/act/trap.o \
					en/tc/act/accept.o en/tc/act/mark.o en/tc/act/goto.o \
					en/tc/act/tun.o en/tc/act/csum.o en/tc/act/pedit.o \
					en/tc/act/vlan.o en/tc/act/vlan_mangle.o en/tc/act/mpls.o \
					en/tc/act/mirred.o en/tc/act/mirred_nic.o \
					en/tc/act/ct.o en/tc/act/sample.o en/tc/act/ptype.o \
					en/tc/act/redirect_ingress.o en/tc/act/police.o

ifneq ($(CONFIG_MLX5_TC_CT),)
	mlx5_core-y			     += en/tc_ct.o en/tc/ct_fs_dmfs.o
	mlx5_core-$(CONFIG_MLX5_SW_STEERING) += en/tc/ct_fs_smfs.o
	mlx5_core-$(CONFIG_MLX5_HW_STEERING) += en/tc/ct_fs_hmfs.o
endif

mlx5_core-$(CONFIG_MLX5_TC_SAMPLE)   += en/tc/sample.o

#
# Core extra
#
mlx5_core-$(CONFIG_MLX5_ESWITCH)   += eswitch.o eswitch_offloads.o eswitch_offloads_termtbl.o \
				      ecpf.o rdma.o esw/legacy.o \
				      esw/devlink_port.o esw/vporttbl.o esw/qos.o esw/ipsec.o

mlx5_core-$(CONFIG_MLX5_ESWITCH)   += esw/acl/helper.o \
				      esw/acl/egress_lgcy.o esw/acl/egress_ofld.o \
				      esw/acl/ingress_lgcy.o esw/acl/ingress_ofld.o

ifneq ($(CONFIG_MLX5_EN_IPSEC),)
	mlx5_core-$(CONFIG_MLX5_ESWITCH)   += esw/ipsec_fs.o
endif

mlx5_core-$(CONFIG_MLX5_BRIDGE)    += esw/bridge.o esw/bridge_mcast.o esw/bridge_debugfs.o \
				      en/rep/bridge.o

mlx5_core-$(CONFIG_HWMON)          += hwmon.o
mlx5_core-$(CONFIG_MLX5_MPFS)      += lib/mpfs.o
mlx5_core-$(CONFIG_VXLAN)          += lib/vxlan.o
mlx5_core-$(CONFIG_PTP_1588_CLOCK) += lib/clock.o
mlx5_core-$(CONFIG_PCI_HYPERV_INTERFACE) += lib/hv.o lib/hv_vhca.o

#
# Ipoib netdev
#
mlx5_core-$(CONFIG_MLX5_CORE_IPOIB) += ipoib/ipoib.o ipoib/ethtool.o ipoib/ipoib_vlan.o

#
# Accelerations & FPGA
#
mlx5_core-$(CONFIG_MLX5_FPGA) += fpga/cmd.o fpga/core.o fpga/conn.o fpga/sdk.o

mlx5_core-$(CONFIG_MLX5_MACSEC) += en_accel/macsec.o lib/macsec_fs.o \
				      en_accel/macsec_stats.o

mlx5_core-$(CONFIG_MLX5_EN_IPSEC) += en_accel/ipsec.o en_accel/ipsec_rxtx.o \
				     en_accel/ipsec_stats.o en_accel/ipsec_fs.o \
				     en_accel/ipsec_offload.o lib/ipsec_fs_roce.o

mlx5_core-$(CONFIG_MLX5_EN_TLS) += en_accel/ktls_stats.o \
				   en_accel/fs_tcp.o en_accel/ktls.o en_accel/ktls_txrx.o \
				   en_accel/ktls_tx.o en_accel/ktls_rx.o

#
# SW Steering
#
mlx5_core-$(CONFIG_MLX5_SW_STEERING) += steering/sws/dr_domain.o \
					steering/sws/dr_table.o \
					steering/sws/dr_matcher.o \
					steering/sws/dr_rule.o \
					steering/sws/dr_icm_pool.o \
					steering/sws/dr_buddy.o \
					steering/sws/dr_ste.o \
					steering/sws/dr_send.o \
					steering/sws/dr_ste_v0.o \
					steering/sws/dr_ste_v1.o \
					steering/sws/dr_ste_v2.o \
					steering/sws/dr_ste_v3.o \
					steering/sws/dr_cmd.o \
					steering/sws/dr_fw.o \
					steering/sws/dr_action.o \
					steering/sws/dr_definer.o \
					steering/sws/dr_ptrn.o \
					steering/sws/dr_arg.o \
					steering/sws/dr_dbg.o \
					steering/sws/fs_dr.o \
					lib/smfs.o

#
# HW Steering
#
mlx5_core-$(CONFIG_MLX5_HW_STEERING) += steering/hws/cmd.o \
					steering/hws/context.o \
					steering/hws/pat_arg.o \
					steering/hws/buddy.o \
					steering/hws/pool.o \
					steering/hws/table.o \
					steering/hws/action.o \
					steering/hws/rule.o \
					steering/hws/matcher.o \
					steering/hws/send.o \
					steering/hws/definer.o \
					steering/hws/bwc.o \
					steering/hws/debug.o \
					steering/hws/vport.o \
					steering/hws/bwc_complex.o \
					steering/hws/fs_hws_pools.o \
					steering/hws/fs_hws.o \
					steering/hws/action_ste_pool.o

#
# SF device
#
mlx5_core-$(CONFIG_MLX5_SF) += sf/vhca_event.o sf/dev/dev.o sf/dev/driver.o irq_affinity.o

#
# SF manager
#
mlx5_core-$(CONFIG_MLX5_SF_MANAGER) += sf/cmd.o sf/hw_table.o sf/devlink.o

#
# TPH support
#
mlx5_core-$(CONFIG_PCIE_TPH) += lib/st.o

obj-$(CONFIG_MLX5_DPLL) += mlx5_dpll.o
mlx5_dpll-y :=	dpll.o
