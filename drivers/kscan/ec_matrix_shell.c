/*
 * Copyright (c) 2018 Prevas A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include "ec_matrix_settings.h"
#include "zmk_kscan_ec_matrix.h"

#define DT_DRV_COMPAT zmk_kscan_ec_matrix

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ec_matrix_shell);

#define CMD_HELP_SCAN_RATE "Print EC Scan Rate.\n"
#define CMD_HELP_CALIBRATE "EC Calibration Utilities.\n"
#define CMD_HELP_CALIBRATION_START "Calibrate the EC Martix.\n"

#define CMD_HELP_CALIBRATION_SAVE "Save the EC Martix Calibration To Flash.\n"

#define CMD_HELP_CALIBRATION_LOAD "Load the EC Martix Calibration From Flash.\n"

#define DEVICES(n) DEVICE_DT_INST_GET(n),

#define INIT_MACRO() DT_INST_FOREACH_STATUS_OKAY(DEVICES) NULL

#define EC_MATRIX_ENTRY(dev_)                                                                      \
    { .dev = dev_ }

/* This table size is = ADC devices count + 1 (NA). */
static struct matrix_hdl {
    const struct device *dev;
} matrix_hdl_list[] = {FOR_EACH(EC_MATRIX_ENTRY, (, ), INIT_MACRO())};

static struct matrix_hdl *get_matrix(const char *device_label) {
    for (int i = 0; i < ARRAY_SIZE(matrix_hdl_list); i++) {
        if (!strcmp(device_label, matrix_hdl_list[i].dev->name)) {
            return &matrix_hdl_list[i];
        }
    }

    /* This will never happen because ADC was prompted by shell */
    __ASSERT_NO_MSG(false);
    return NULL;
}

static void calibrate_cb(const struct zmk_kscan_ec_matrix_calibration_event *ev,
                         const void *user_data) {
    const struct shell *sh = (const struct shell *)user_data;

    switch (ev->type) {
    case CALIBRATION_EV_LOW_SAMPLING_START:
        shell_prompt_change(sh, "-");
        shell_print(sh, "Low value sampling begins. Please do not press any keys");
        k_sleep(K_SECONDS(1));
        break;
    case CALIBRATION_EV_HIGH_SAMPLING_START:
        shell_prompt_change(sh, "-");
        shell_print(sh, "\nHigh value sampling begins. Please slowly press each key in sequence, releasing once an asterisk appears");
        break;
    case CALIBRATION_EV_POSITION_LOW_DETERMINED:
		shell_fprintf(sh, SHELL_NORMAL, "*");
        // shell_print(sh, "Key at (%d,%d) is calibrated with avg low %d, noise: %d",
        //             ev->data.position_low_determined.strobe, ev->data.position_low_determined.input,
        //             ev->data.position_low_determined.low_avg,
        //             ev->data.position_low_determined.noise);
        break;
    case CALIBRATION_EV_POSITION_COMPLETE:
		shell_fprintf(sh, SHELL_NORMAL, "*");
        // shell_print(sh,
        //             "Key at (%d,%d) is calibrated with avg low %d, avg high %d, noise: %d, SNR: %d",
        //             ev->data.position_complete.strobe,
        //             ev->data.position_complete.input, ev->data.position_complete.low_avg,
        //             ev->data.position_complete.high_avg, ev->data.position_complete.noise,
        //             ev->data.position_complete.snr);
        break;
    case CALIBRATION_EV_COMPLETE:
        shell_prompt_change(sh, CONFIG_SHELL_PROMPT_UART);
        shell_print(sh, "\nCalibration complete!");
        break;
    }
}

static int cmd_matrix_calibration_start(const struct shell *shell, size_t argc, char **argv,
                                        void *data) {
    /* -2: index of ADC label name */
    struct matrix_hdl *matrix = get_matrix(argv[-2]);

    int ret = zmk_kscan_ec_matrix_calibrate(matrix->dev, &calibrate_cb, shell);
    if (ret < 0) {
        shell_print(shell, "Failed to start calibration (%d)", ret);
    }

    return ret;
}

#if IS_ENABLED(CONFIG_ZMK_KSCAN_EC_MATRIX_SCAN_RATE_CALC)

static int cmd_matrix_scan_rate(const struct shell *shell, size_t argc, char **argv,
                                       void *data) {
    struct matrix_hdl *matrix = get_matrix(argv[-1]);
	uint64_t duration_ns = zmk_kscan_ec_matrix_max_scan_duration_ns(matrix->dev);

	if (duration_ns > 0) {
		uint64_t scan_rate = 1000000000 / duration_ns;
		shell_info(shell, "Matrix scan rate: %lluHz", scan_rate);
	}

    return 0;
}

#endif // IS_ENABLED(CONFIG_ZMK_KSCAN_EC_MATRIX_SCAN_RATE_CALC)

#if IS_ENABLED(CONFIG_ZMK_KSCAN_EC_MATRIX_SETTINGS)

static int cmd_matrix_calibration_save(const struct shell *shell, size_t argc, char **argv,
                                       void *data) {
    struct matrix_hdl *matrix = get_matrix(argv[-2]);
    int ret = zmk_kscan_ec_matrix_settings_save_calibration(matrix->dev);
    if (ret < 0) {
        shell_print(shell, "Failed to initiate save calibration (%d)", ret);
    }

    return ret;
}

static int cmd_matrix_calibration_load(const struct shell *shell, size_t argc, char **argv,
                                       void *data) {
    struct matrix_hdl *matrix = get_matrix(argv[-2]);
    int ret = zmk_kscan_ec_matrix_settings_load_calibration(matrix->dev);
    if (ret < 0) {
        shell_print(shell, "Failed to initiate save calibration (%d)", ret);
    }

    return ret;
}

#endif // IS_ENABLED(CONFIG_ZMK_KSCAN_EC_MATRIX_SETTINGS)

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_matrix_calibration_cmds,
    /* Alphabetically sorted. */
    SHELL_CMD(start, NULL, CMD_HELP_CALIBRATION_START, cmd_matrix_calibration_start),
#if IS_ENABLED(CONFIG_SETTINGS)
    SHELL_CMD(save, NULL, CMD_HELP_CALIBRATION_SAVE, cmd_matrix_calibration_save),
    SHELL_CMD(load, NULL, CMD_HELP_CALIBRATION_LOAD, cmd_matrix_calibration_load),
#endif                   // IS_ENABLED(CONFIG_SETTINGS)
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_matrix_cmds,
                               /* Alphabetically sorted. */
                               SHELL_CMD(calibration, &sub_matrix_calibration_cmds,
                                         CMD_HELP_CALIBRATE, NULL),
#if IS_ENABLED(CONFIG_ZMK_KSCAN_EC_MATRIX_SCAN_RATE_CALC)
	SHELL_CMD(scan_rate, NULL, CMD_HELP_SCAN_RATE, cmd_matrix_scan_rate),
#endif // IS_ENABLED(CONFIG_ZMK_KSCAN_EC_MATRIX_SCAN_RATE_CALC)
                               SHELL_SUBCMD_SET_END /* Array terminated. */
);

static void cmd_matrix_dev_get(size_t idx, struct shell_static_entry *entry) {
    /* -1 because the last element in the list is a "list terminator" */
    if (idx < ARRAY_SIZE(matrix_hdl_list) - 1) {
        entry->syntax = matrix_hdl_list[idx].dev->name;
        entry->handler = NULL;
        entry->subcmd = &sub_matrix_cmds;
        entry->help = "Select subcommand for matrix property label.\n";
    } else {
        entry->syntax = NULL;
    }
}
SHELL_DYNAMIC_CMD_CREATE(sub_ec_matrix_dev, cmd_matrix_dev_get);

SHELL_CMD_REGISTER(ec, &sub_ec_matrix_dev, "EC Matrix commands", NULL);
