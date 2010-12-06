/*
 * PLT utility for wireless chip supported by TI's driver wl12xx
 *
 * See README and COPYING for more details.
 */

#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <linux/wireless.h>
#include "nl80211.h"

#include "calibrator.h"
#include "plt.h"

SECTION(plt);

static void str2mac(unsigned char *pmac, char *pch)
{
	int i;

	for (i = 0; i < MAC_ADDR_LEN; i++) {
		pmac[i] = (unsigned char)strtoul(pch, &pch, 16);
		pch++;
	}
}

static int plt_power_mode(struct nl80211_state *state, struct nl_cb *cb,
			  struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	unsigned int pmode;

	if (argc != 1) {
		fprintf(stderr, "%s> Missing arguments\n", __func__);
		return 2;
	}

	if (strcmp(argv[0], "on") == 0)
		pmode = 1;
	else if (strcmp(argv[0], "off") == 0)
		pmode = 0;
	else {
		fprintf(stderr, "%s> Invalid parameter: %s\n",
			__func__, argv[0]);
		return 2;
	}

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_SET_PLT_MODE);
	NLA_PUT_U32(msg, WL1271_TM_ATTR_PLT_MODE, pmode);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, power_mode, "<on|off>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_power_mode,
	"Set PLT power mode.\n");

static int plt_tune_channel(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_cal_channel_tune prms;

	if (argc < 1 || argc > 2)
		return 1;

	prms.test.id = TEST_CMD_CHANNEL_TUNE;
	prms.band = (unsigned char)atoi(argv[0]);
	prms.channel = (unsigned char)atoi(argv[1]);

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA,
		sizeof(struct wl1271_cmd_cal_channel_tune),
		&prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tune_channel, "<band> <channel>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tune_channel,
	"Set band and channel for PLT.\n");

static int plt_ref_point(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_cal_update_ref_point prms;

	if (argc < 1 || argc > 3)
		return 1;

	prms.test.id = TEST_CMD_UPDATE_PD_REFERENCE_POINT;
	prms.ref_detector = atoi(argv[0]);
	prms.ref_power = atoi(argv[1]);
	prms.sub_band = atoi(argv[2]);

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, ref_point, "<voltage> <power> <subband>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_ref_point,
	"Set reference point for PLT.\n");

static int calib_valid_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[WL1271_TM_ATTR_MAX + 1];
	struct wl1271_cmd_cal_p2g *prms;
	int i; unsigned char *pc;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		fprintf(stderr, "no data!\n");
		return NL_SKIP;
	}

	nla_parse(td, WL1271_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	prms = (struct wl1271_cmd_cal_p2g *)nla_data(td[WL1271_TM_ATTR_DATA]);
#if 0
	printf("%s> id %04x status %04x\ntest id %02x ver %08x len %04x=%d\n",
		__func__,
		prms->header.id, prms->header.status, prms->test.id,
		prms->ver, prms->len, prms->len);

		pc = (unsigned char *)prms->buf;
		printf("++++++++++++++++++++++++\n");
		for (i = 0; i < prms->len; i++) {
			if (i%0xf == 0)
				printf("\n");

			printf("%02x ", *(unsigned char *)pc);
			pc += 1;
		}
		printf("++++++++++++++++++++++++\n");
#endif
	if (prepare_nvs_file(prms)) {
		fprintf(stderr, "Fail to prepare new NVS file\n");
		return 2;
	}

	printf("\n\tThe NVS file (%s) is ready\n\tCopy it to %s and "
		"reboot the system\n\n",
		NEW_NVS_NAME, CURRENT_NVS_NAME);

	return NL_SKIP;
}

static int plt_tx_bip(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_cal_p2g prms;
	int i; unsigned char *pc;

	if (argc != 8)
		return 1;

	memset(&prms, 0, sizeof(struct wl1271_cmd_cal_p2g));

	prms.test.id = TEST_CMD_P2G_CAL;
	for (i = 0; i < argc; i++)
		prms.sub_band_mask |= (atoi(argv[i]) & 0x1)<<i;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);
	NLA_PUT_U8(msg, WL1271_TM_ATTR_ANSWER, 1);

	nla_nest_end(msg, key);

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, calib_valid_handler, NULL);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tx_bip, "<0|1> <0|1> <0|1> <0|1> <0|1> <0|1> <0|1> <0|1>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tx_bip,
	"Do calibrate.\n");

static int plt_tx_cont(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms = {
		.src_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	};

	if (argc != 15)
		return 1;
#if 0
	printf("%s> delay (%d) rate (%08x) size (%d) amount (%d) power (%d) "
		"seed (%d) pkt_mode (%d) DCF (%d) GI (%d) preamble (%d) type "
		"(%d) scramble (%d) CLPC (%d), SeqNbrMode (%d) DestMAC (%s)\n",
		__func__,
		atoi(argv[0]),  atoi(argv[1]),  atoi(argv[2]),  atoi(argv[3]),
		atoi(argv[4]),  atoi(argv[5]),  atoi(argv[6]),  atoi(argv[7]),
		atoi(argv[8]),  atoi(argv[9]),  atoi(argv[10]), atoi(argv[11]),
		atoi(argv[12]), atoi(argv[13]), argv[14]
	);
#endif
	memset((void *)&prms, 0, sizeof(struct wl1271_cmd_pkt_params));

	prms.test.id = TEST_CMD_FCC;
	prms.delay = atoi(argv[0]);
	prms.rate = atoi(argv[1]);
	prms.size = (unsigned short)atoi(argv[2]);
	prms.amount = (unsigned short)atoi(argv[3]);
	prms.power = atoi(argv[4]);
	prms.seed = (unsigned short)atoi(argv[5]);
	prms.pkt_mode = (unsigned char)atoi(argv[6]);
	prms.dcf_enable = (unsigned char)atoi(argv[7]);
	prms.g_interval = (unsigned char)atoi(argv[8]);
	prms.preamble = (unsigned char)atoi(argv[9]);
	prms.type = (unsigned char)atoi(argv[10]);
	prms.scramble = (unsigned char)atoi(argv[11]);
	prms.clpc_enable = (unsigned char)atoi(argv[12]);
	prms.seq_nbr_mode = (unsigned char)atoi(argv[13]);
	str2mac(prms.dst_mac, argv[14]);

	if (get_mac_addr(0, prms.src_mac))
		fprintf(stderr, "fail to get MAC addr\n");

	printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
		prms.src_mac[0], prms.src_mac[1], prms.src_mac[2],
		prms.src_mac[3], prms.src_mac[4], prms.src_mac[5]);

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tx_cont, "<delay> <rate> <size> <amount> <power>\n\t\t<seed> "
	"<pkt mode> <DC on/off> <gi> <preamble>\n\t\t<type> <scramble> "
	"<clpc> <seq nbr mode> <dest mac>",
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tx_cont,
	"Do calibrate.\n");

static int plt_tx_stop(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_STOP_TX;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "fail to nla_nest_start()\n");
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, tx_stop, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_tx_stop,
	"Do calibrate.\n");

static int plt_start_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_RX_STAT_START;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, start_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_start_rx_statcs,
	"Do calibrate.\n");

static int plt_stop_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_RX_STAT_STOP;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, stop_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_stop_rx_statcs,
	"Do calibrate.\n");

static int plt_reset_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_RX_STAT_RESET;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);

	nla_nest_end(msg, key);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, reset_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_reset_rx_statcs,
	"Do calibrate.\n");

static int display_rx_statcs(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *td[WL1271_TM_ATTR_MAX + 1];
	struct wl1271_radio_rx_statcs *prms;
	int i; unsigned char *pc;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_TESTDATA]) {
		fprintf(stderr, "no data!\n");
		return NL_SKIP;
	}

	nla_parse(td, WL1271_TM_ATTR_MAX, nla_data(tb[NL80211_ATTR_TESTDATA]),
		nla_len(tb[NL80211_ATTR_TESTDATA]), NULL);

	prms =
		(struct wl1271_radio_rx_statcs *)
			nla_data(td[WL1271_TM_ATTR_DATA]);

	printf("\tNbr of rx valid pkts = (%d)\n\tNbr of rx FCS err pkts = (%d)"
		"\n\tNbr of addr miss pkts = (%d)\n\tSeq nbr miss cnt = (%d)"
		"\n\tAverage SNR = (%d)\n\tAverage RSSI = (%d)"
		"\n\tBase pkt Id = (%d)\n\tNbr of pkts = (%d)"
		"\n\tNbr of missed pkts = (%d)\n",
		prms->rx_path_statcs.nbr_rx_valid_pkts,
		prms->rx_path_statcs.nbr_rx_fcs_err_pkts,
		prms->rx_path_statcs.nbr_rx_plcp_err_pkts,
		prms->rx_path_statcs.seq_nbr_miss_cnt,
		prms->rx_path_statcs.ave_snr/8,
		prms->rx_path_statcs.ave_rssi/8,
		prms->base_pkt_id, prms->nbr_pkts, prms->nbr_miss_pkts
	);

	return NL_SKIP;
}

static int plt_get_rx_statcs(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	struct nlattr *key;
	struct wl1271_cmd_pkt_params prms;

	prms.test.id = TEST_CMD_RX_STAT_GET;

	key = nla_nest_start(msg, NL80211_ATTR_TESTDATA);
	if (!key) {
		fprintf(stderr, "%s> fail to nla_nest_start()\n", __func__);
		return 1;
	}

	NLA_PUT_U32(msg, WL1271_TM_ATTR_CMD_ID, WL1271_TM_CMD_TEST);
	NLA_PUT(msg, WL1271_TM_ATTR_DATA, sizeof(prms), &prms);
	NLA_PUT_U8(msg, WL1271_TM_ATTR_ANSWER, 1);

	nla_nest_end(msg, key);

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, display_rx_statcs, NULL);

	/* Important: needed gap between tx_start and tx_get */
	sleep(2);

	return 0;

nla_put_failure:
	fprintf(stderr, "%s> building message failed\n", __func__);
	return 2;
}

COMMAND(plt, get_rx_statcs, NULL,
	NL80211_CMD_TESTMODE, 0, CIB_NETDEV, plt_get_rx_statcs,
	"Do calibrate.\n");

static int plt_rx_statistics(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	int ret;

	/* power mode on */
	{
		char *prms[4] = { "wlan0", "plt", "power_mode", "on" };

		ret = handle_cmd(state, II_NETDEV, 4, prms);
		if (ret < 0) {
			fprintf(stderr, "Fail to set PLT power mode on\n");
			return 1;
		}
	}

	/* start_rx_statcs */
	{
		char *prms[3] = { "wlan0", "plt", "start_rx_statcs" };

		ret = handle_cmd(state, II_NETDEV, 3, prms);
		if (ret < 0) {
			fprintf(stderr, "Fail to start Rx statistics\n");
			goto fail_out;
		}
	}

	/* get_rx_statcs */
	{
		int err;
		char *prms[3] = { "wlan0", "plt", "get_rx_statcs" };

		err = handle_cmd(state, II_NETDEV, 3, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to get Rx statistics\n");
			ret = err;
		}
	}


	/* stop_rx_statcs */
	{
		int err;
		char *prms[3] = { "wlan0", "plt", "stop_rx_statcs" };

		err = handle_cmd(state, II_NETDEV, 3, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to stop Rx statistics\n");
			ret = err;
		}
	}

fail_out:
	/* power mode off */
	{
		int err;
		char *prms[4] = { "wlan0", "plt", "power_mode", "off"};

		err = handle_cmd(state, II_NETDEV, 4, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to set PLT power mode on\n");
			return 1;
		}
	}

	if (ret < 0)
		return 1;

	return 0;
}

COMMAND(plt, rx_statistics, NULL, 0, 0, CIB_NONE, plt_rx_statistics,
	"To get Rx statistics.\n");

static int plt_calibrate(struct nl80211_state *state, struct nl_cb *cb,
			struct nl_msg *msg, int argc, char **argv)
{
	int ret;

	/* power mode on */
	{
		int err;
		char *pm_on[4] = { "wlan0", "plt", "power_mode", "on"};

		err = handle_cmd(state, II_NETDEV, 4, pm_on);
		if (err < 0) {
			fprintf(stderr, "Fail to set PLT power mode on\n");
			return 1;
		}
	}

	/* reference point */
	{
		int err;
		char *pm_on[6] = {
			"wlan0", "plt", "ref_point", "375", "128", "0"
		};

		err = handle_cmd(state, II_NETDEV, 6, pm_on);
		if (err < 0) {
			fprintf(stderr, "Fail to set PLT reference point\n");
			ret = err;
			goto fail_out;
		}
	}

	/* calibrate off */
	{
		int err;
		char *prms[11] = {
			"wlan0", "plt", "tx_bip", "1", "0", "0", "0",
			"0", "0", "0", "0"
		};

		err = handle_cmd(state, II_NETDEV, 11, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to calibrate\n");
			ret = err;
		}
	}

fail_out:
	/* power mode off */
	{
		int err;
		char *prms[4] = { "wlan0", "plt", "power_mode", "off"};

		err = handle_cmd(state, II_NETDEV, 4, prms);
		if (err < 0) {
			fprintf(stderr, "Fail to set PLT power mode on\n");
			return 1;
		}
	}

	if (ret < 0)
		return 1;

	return 0;
}

COMMAND(plt, calibrate, NULL, 0, 0, CIB_NONE, plt_calibrate, "Do calibrate.\n");
