/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <Library/LKEnvLib.h>
#include <Chipset/smem.h>
#include <Chipset/msm_panel.h>
#include <Chipset/mipi_dsi.h>
#include <Chipset/mdss_hdmi.h>
#include <Library/QcomPm8x41Lib.h>
#include <Chipset/board.h>
#include <Chipset/mdp5.h>
#include <Library/QcomScmLib.h>
#include <Chipset/regulator.h>
#include <Library/QcomGpioTlmmLib.h>
#include <Platform/clock.h>
#include <Platform/iomap.h>
#include <Target/display.h>
#include <Library/StrLib.h>
#include <Library/QcomQpnpWledLib.h>
#include <Library/PlatformIdLib.h>

#include <Device/panel.h>
#include <Device/display_resource.h>
#include <Device/gcdb_display.h>

#define HFPLL_LDO_ID 12

#define GPIO_STATE_LOW 0
#define GPIO_STATE_HIGH 2
#define RESET_GPIO_SEQ_LEN 3

#define PWM_DUTY_US 13
#define PWM_PERIOD_US 27
#define PMIC_WLED_SLAVE_ID 3
#define PMIC_MPP_SLAVE_ID 2

#define DSI0_BASE_ADJUST -0x4000
#define DSI1_BASE_ADJUST -0xA000

/*---------------------------------------------------------------------------*/
/* GPIO configuration                                                        */
/*---------------------------------------------------------------------------*/
static struct gpio_pin reset_gpio = {
  "msmgpio", 78, 3, 1, 0, 1
};

static struct gpio_pin lcd_reg_en = {	/* boost regulator */
  "pm8994_gpios", 14, 3, 1, 0, 1
};

static struct gpio_pin bklt_gpio = {	/* lcd_bklt_reg_en */
  "pmi8994_gpios", 2, 3, 1, 0, 1
};

/* gpio name, id, strength, direction, pull, state. */
static struct gpio_pin hdmi_cec_gpio = {        /* CEC */
  "msmgpio", 31, 0, 2, 3, 1
};

static struct gpio_pin hdmi_ddc_clk_gpio = {   /* DDC CLK */
  "msmgpio", 32, 0, 2, 3, 1
};

static struct gpio_pin hdmi_ddc_data_gpio = {  /* DDC DATA */
  "msmgpio", 33, 0, 2, 3, 1
};

static struct gpio_pin hdmi_hpd_gpio = {       /* HPD, input */
  "msmgpio", 34, 7, 0, 1, 1
};

int target_hdmi_gpio_ctrl(uint8_t enable)
{
	gGpioTlmm->gpio_tlmm_config(hdmi_cec_gpio.pin_id, 1,	/* gpio 31, CEC */
		hdmi_cec_gpio.pin_direction, hdmi_cec_gpio.pin_pull,
		hdmi_cec_gpio.pin_strength, hdmi_cec_gpio.pin_state);

	gGpioTlmm->gpio_tlmm_config(hdmi_ddc_clk_gpio.pin_id, 1,	/* gpio 32, DDC CLK */
		hdmi_ddc_clk_gpio.pin_direction, hdmi_ddc_clk_gpio.pin_pull,
		hdmi_ddc_clk_gpio.pin_strength, hdmi_ddc_clk_gpio.pin_state);


	gGpioTlmm->gpio_tlmm_config(hdmi_ddc_data_gpio.pin_id, 1,	/* gpio 33, DDC DATA */
		hdmi_ddc_data_gpio.pin_direction, hdmi_ddc_data_gpio.pin_pull,
		hdmi_ddc_data_gpio.pin_strength, hdmi_ddc_data_gpio.pin_state);

	gGpioTlmm->gpio_tlmm_config(hdmi_hpd_gpio.pin_id, 1,	/* gpio 34, HPD */
		hdmi_hpd_gpio.pin_direction, hdmi_hpd_gpio.pin_pull,
		hdmi_hpd_gpio.pin_strength, hdmi_hpd_gpio.pin_state);

	gGpioTlmm->gpio_set(hdmi_cec_gpio.pin_id,      hdmi_cec_gpio.pin_direction);
	gGpioTlmm->gpio_set(hdmi_ddc_clk_gpio.pin_id,  hdmi_ddc_clk_gpio.pin_direction);
	gGpioTlmm->gpio_set(hdmi_ddc_data_gpio.pin_id, hdmi_ddc_data_gpio.pin_direction);
	gGpioTlmm->gpio_set(hdmi_hpd_gpio.pin_id,      hdmi_hpd_gpio.pin_direction);

	return NO_ERROR;
}

static uint32_t dsi_pll_20nm_enable_seq(uint32_t pll_base)
{
	uint32_t pll_locked;
	/* MDSS_DSI_0_PHY_DSIPHY_CTRL_1 */
	writel(0x00, pll_base + 0x374);
	dmb();
	/* MDSS_DSI_0_PHY_DSIPHY_CTRL_0 */
	writel(0x7f, pll_base + 0x370);
	dmb();
	pll_locked = mdss_dsi_pll_20nm_lock_status(pll_base);
	if (!pll_locked)
		dprintf(INFO, "%s: DSI PLL lock failed\n", __func__);
	else
		dprintf(INFO, "%s: DSI PLL lock Success\n", __func__);

	return  pll_locked;
}

static int msm8994_wled_backlight_ctrl(uint8_t enable)
{
	uint8_t slave_id = 3;	/* pmi */

	gPm8x41->pm8x41_wled_config_slave_id(slave_id);
	gQpnpWled->qpnp_wled_enable_backlight(enable);
	gQpnpWled->qpnp_ibb_enable(enable);
	return NO_ERROR;
}

static int msm8994_pwm_backlight_ctrl(uint8_t enable)
{
	uint8_t slave_id = 3; /* lpg at pmi */

        if (enable) {
		/* mpp-1 had been configured already */
                /* lpg channel 4 */

		 /* LPG_ENABLE_CONTROL */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x46, 0x0);
		mdelay(100);

		 /* LPG_VALUE_LSB, duty cycle = 0x80/0x200 = 1/4 */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x44, 0x80);
		/* LPG_VALUE_MSB */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x45, 0x00);
		/* LPG_PWM_SYNC */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x47, 0x01);

		 /* LPG_PWM_SIZE_CLK, */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x41, 0x13);
		 /* LPG_PWM_FREQ_PREDIV */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x42, 0x02);
		 /* LPG_PWM_TYPE_CONFIG */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x43, 0x20);
		 /* LPG_ENABLE_CONTROL */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x46, 0x04);

		 /* SEC_ACCESS */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0xD0, 0xA5);
		 /* DTEST4, OUT_HI */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0xE5, 0x01);
		 /* LPG_ENABLE_CONTROL */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x46, 0xA4);
        } else {
		 /* LPG_ENABLE_CONTROL */
                gPm8x41->pm8x41_lpg_write_sid(slave_id, PWM_BL_LPG_CHAN_ID, 0x46, 0x0);
        }

        return NO_ERROR;
}

void lcd_bklt_reg_enable(void)
{
	uint8_t slave_id = 2;	/* gpio at pmi */

	struct pm8x41_gpio gpio = {
                .direction = PM_GPIO_DIR_OUT,
                .function = PM_GPIO_FUNC_HIGH,
                .vin_sel = 2,   /* VIN_2 */
                .output_buffer = PM_GPIO_OUT_CMOS,
                .out_strength = PM_GPIO_OUT_DRIVE_LOW,
        };

        gPm8x41->pm8x41_gpio_config_sid(slave_id, bklt_gpio.pin_id, &gpio);
	gPm8x41->pm8x41_gpio_set_sid(slave_id, bklt_gpio.pin_id, 1);
}

void lcd_bklt_reg_disable(void)
{
	uint8_t slave_id = 2;	/* gpio at pmi */

	gPm8x41->pm8x41_gpio_set_sid(slave_id, bklt_gpio.pin_id, 0);
}

void lcd_reg_enable(void)
{
       struct pm8x41_gpio gpio = {
                .direction = PM_GPIO_DIR_OUT,
                .function = PM_GPIO_FUNC_HIGH,
                .vin_sel = 2,   /* VIN_2 */
                .output_buffer = PM_GPIO_OUT_CMOS,
                .out_strength = PM_GPIO_OUT_DRIVE_MED,
        };

        gPm8x41->pm8x41_gpio_config(lcd_reg_en.pin_id, &gpio);
	gPm8x41->pm8x41_gpio_set(lcd_reg_en.pin_id, 1);
}

void lcd_reg_disable(void)
{
	gPm8x41->pm8x41_gpio_set(lcd_reg_en.pin_id, 0);
}

int target_backlight_ctrl(struct backlight *bl, uint8_t enable)
{
	uint32_t ret = NO_ERROR;
	struct pm8x41_mpp mpp;
	int rc;

	if (!bl) {
		dprintf(CRITICAL, "backlight structure is not available\n");
		return ERR_INVALID_ARGS;
	}

	switch (bl->bl_interface_type) {
	case BL_WLED:
		/* Enable MPP4 */
		gPm8x41->pmi8994_config_mpp_slave_id(PMIC_MPP_SLAVE_ID);
	        mpp.base = PM8x41_MMP4_BASE;
		mpp.vin = MPP_VIN2;
		if (enable) {
			gPm8x41->pm_pwm_enable(false);
			rc = gPm8x41->pm_pwm_config(PWM_DUTY_US, PWM_PERIOD_US);
			if (rc < 0) {
				mpp.mode = MPP_HIGH;
			} else {
				mpp.mode = MPP_DTEST1;
				gPm8x41->pm_pwm_enable(true);
			}
			gPm8x41->pm8x41_config_output_mpp(&mpp);
			gPm8x41->pm8x41_enable_mpp(&mpp, MPP_ENABLE);
		} else {
			gPm8x41->pm_pwm_enable(false);
			gPm8x41->pm8x41_enable_mpp(&mpp, MPP_DISABLE);
		}
		/* Need delay before power on regulators */
		mdelay(20);
		/* Enable WLED backlight control */
		ret = msm8994_wled_backlight_ctrl(enable);
		break;
	case BL_PWM:
		/* Enable MPP1 */
		gPm8x41->pmi8994_config_mpp_slave_id(PMIC_MPP_SLAVE_ID);
	        mpp.base = PM8x41_MMP1_BASE;
		mpp.vin = MPP_VIN2;
		mpp.mode = MPP_DTEST4;
		if (enable) {
			gPm8x41->pm8x41_config_output_mpp(&mpp);
			gPm8x41->pm8x41_enable_mpp(&mpp, MPP_ENABLE);
		} else {
			gPm8x41->pm8x41_enable_mpp(&mpp, MPP_DISABLE);
		}
		/* Need delay before power on regulators */
		mdelay(20);
		ret = msm8994_pwm_backlight_ctrl(enable);
		break;
	default:
		dprintf(CRITICAL, "backlight type:%d not supported\n",
						bl->bl_interface_type);
		return ERR_NOT_SUPPORTED;
	}

	return ret;
}

int target_hdmi_pll_clock(uint8_t enable, struct msm_panel_info *pinfo)
{
	if (enable) {
		hdmi_phy_reset();
		hdmi_pll_config(pinfo->clk_rate);
		hdmi_vco_enable();
		hdmi_pixel_clk_enable(pinfo->clk_rate);
	} else if(!target_cont_splash_screen()) {
		/* Disable clocks if continuous splash off */
		hdmi_pixel_clk_disable();
		hdmi_vco_disable();
	}

	return NO_ERROR;
}

int target_panel_clock(uint8_t enable, struct msm_panel_info *pinfo)
{
	uint32_t ret = NO_ERROR;
	struct mdss_dsi_pll_config *pll_data;
	uint32_t flags;
	struct dfps_pll_codes *pll_codes = &pinfo->mipi.pll_codes;

	if (pinfo->dest == DISPLAY_2) {
		flags = MMSS_DSI_CLKS_FLAG_DSI1;
		if (pinfo->mipi.dual_dsi)
			flags |= MMSS_DSI_CLKS_FLAG_DSI0;
	} else {
		flags = MMSS_DSI_CLKS_FLAG_DSI0;
		if (pinfo->mipi.dual_dsi)
			flags |= MMSS_DSI_CLKS_FLAG_DSI1;
	}

	pll_data = pinfo->mipi.dsi_pll_config;

	if (!enable) {
		mmss_dsi_clock_disable(flags);
		goto clks_disable;
	}

	mdp_gdsc_ctrl(enable);
	mmss_bus_clock_enable();
	mdp_clock_enable();

	ret = gScm->restore_secure_cfg(SECURE_DEVICE_MDSS);
	if (ret) {
		dprintf(CRITICAL,
			"%s: Failed to restore MDP security configs",
			__func__);
		goto clks_disable;
	}

	mdss_dsi_auto_pll_20nm_config(pinfo);

	if (!dsi_pll_20nm_enable_seq(pinfo->mipi.pll_0_base)) {
		ret = ERROR;
		dprintf(CRITICAL, "PLL failed to lock!\n");
		goto clks_disable;
	}

	pll_codes->codes[0] = readl_relaxed(pinfo->mipi.pll_0_base +
		MMSS_DSI_PHY_PLL_CORE_KVCO_CODE);
	pll_codes->codes[1] = readl_relaxed(pinfo->mipi.pll_0_base +
		MMSS_DSI_PHY_PLL_CORE_VCO_TUNE);
	dprintf(SPEW, "codes %d %d\n", pll_codes->codes[0],
		pll_codes->codes[1]);

	mmss_dsi_clock_enable(DSI0_PHY_PLL_OUT, flags,
		pll_data->pclk_m,
		pll_data->pclk_n,
		pll_data->pclk_d);

	return NO_ERROR;

clks_disable:
	mdp_clock_disable();
	mmss_bus_clock_disable();
	mdp_gdsc_ctrl(0);

	return ret;
}

int target_panel_reset(uint8_t enable, struct panel_reset_sequence *resetseq,
					struct msm_panel_info *pinfo)
{
	uint32_t i = 0;

	if (enable) {
		gGpioTlmm->gpio_tlmm_config(reset_gpio.pin_id, 0,
				reset_gpio.pin_direction, reset_gpio.pin_pull,
				reset_gpio.pin_strength, reset_gpio.pin_state);
		/* reset */
		for (i = 0; i < RESET_GPIO_SEQ_LEN; i++) {
			if (resetseq->pin_state[i] == GPIO_STATE_LOW)
				gGpioTlmm->gpio_set(reset_gpio.pin_id, GPIO_STATE_LOW);
			else
				gGpioTlmm->gpio_set(reset_gpio.pin_id, GPIO_STATE_HIGH);
			mdelay(resetseq->sleep[i]);
		}
		lcd_bklt_reg_enable();
	} else {
		lcd_bklt_reg_disable();
		gGpioTlmm->gpio_set(reset_gpio.pin_id, 0);
	}

	return NO_ERROR;
}

static void wled_init(struct msm_panel_info *pinfo)
{
	struct qpnp_wled_config_data config = {0};
	struct labibb_desc *labibb;
	int display_type = 0;

	labibb = pinfo->labibb;

	if (labibb)
		display_type = labibb->amoled_panel;

	config.display_type = display_type;
	config.lab_init_volt = 4600000;	/* fixed, see pmi register */
	config.ibb_init_volt = 1400000;	/* fixed, see pmi register */

	if (labibb && labibb->force_config) {
		config.lab_min_volt = labibb->lab_min_volt;
		config.lab_max_volt = labibb->lab_max_volt;
		config.ibb_min_volt = labibb->ibb_min_volt;
		config.ibb_max_volt = labibb->ibb_max_volt;
		config.pwr_up_delay = labibb->pwr_up_delay;
		config.pwr_down_delay = labibb->pwr_down_delay;
		config.ibb_discharge_en = labibb->ibb_discharge_en;
	} else {
		/* default */
		config.pwr_up_delay = 3;
		config.pwr_down_delay =  3;
		config.ibb_discharge_en = 1;
		if (display_type) {	/* amoled */
			config.lab_min_volt = 4600000;
			config.lab_max_volt = 4600000;
			config.ibb_min_volt = 4000000;
			config.ibb_max_volt = 4000000;
		} else { /* lcd */
			config.lab_min_volt = 5500000;
			config.lab_max_volt = 5500000;
			config.ibb_min_volt = 5500000;
			config.ibb_max_volt = 5500000;
		}
	}

	dprintf(SPEW, "%s: %d %d %d %d %d %d %d %d %d %d\n", __func__,
		config.display_type,
		config.lab_min_volt, config.lab_max_volt,
		config.ibb_min_volt, config.ibb_max_volt,
		config.lab_init_volt, config.ibb_init_volt,
		config.pwr_up_delay, config.pwr_down_delay,
		config.ibb_discharge_en);


	/* QPNP WLED init for display backlight */
	gPm8x41->pm8x41_wled_config_slave_id(PMIC_WLED_SLAVE_ID);

	gQpnpWled->qpnp_wled_init(&config);
}

int target_ldo_ctrl(uint8_t enable, struct msm_panel_info *pinfo)
{
	if (enable) {
		regulator_enable();	/* L2, L12, L14, and L28 */
		mdelay(10);
		wled_init(pinfo);
		gQpnpWled->qpnp_ibb_enable(true);	/* +5V and -5V */
		mdelay(50);

		if (pinfo->lcd_reg_en)
			lcd_reg_enable();
	} else {
		if (pinfo->lcd_reg_en)
			lcd_reg_disable();

		regulator_disable();
	}

	return NO_ERROR;
}

int target_display_pre_on(void)
{
	writel(0xC0000CCC, MDP_CLK_CTRL0);
	writel(0xC0000CCC, MDP_CLK_CTRL1);
	writel(0x00CCCCCC, MDP_CLK_CTRL2);
	writel(0x000000CC, MDP_CLK_CTRL6);
	writel(0x0CCCC0C0, MDP_CLK_CTRL3);
	writel(0xCCCCC0C0, MDP_CLK_CTRL4);
	writel(0xCCCCC0C0, MDP_CLK_CTRL5);
	writel(0x00CCC000, MDP_CLK_CTRL7);

	return NO_ERROR;
}

int target_display_get_base_offset(uint32_t base)
{
	if(platform_is_msm8992()) {
		if (base == MIPI_DSI0_BASE)
			return DSI0_BASE_ADJUST;
		else if (base == MIPI_DSI1_BASE)
			return DSI1_BASE_ADJUST;
	}

	return 0;
}

bool target_display_panel_node(char *pbuf, uint16_t buf_size)
{
	int prefix_string_len = strlen(DISPLAY_CMDLINE_PREFIX);
	bool ret = true;
	char vic_buf[HDMI_VIC_LEN] = "0";
	struct oem_panel_data oem = mdss_dsi_get_oem_data();

	if (!strcmp(oem.panel, HDMI_PANEL_NAME)) {
		if (buf_size < (prefix_string_len + LK_OVERRIDE_PANEL_LEN +
				strlen(HDMI_CONTROLLER_STRING))) {
			dprintf(CRITICAL, "command line argument is greater than buffer size\n");
			return false;
		}

		strlcpy(pbuf, DISPLAY_CMDLINE_PREFIX, buf_size);
		buf_size -= prefix_string_len;
		strlcat(pbuf, LK_OVERRIDE_PANEL, buf_size);
		buf_size -= LK_OVERRIDE_PANEL_LEN;
		strlcat(pbuf, HDMI_CONTROLLER_STRING, buf_size);
		buf_size -= strlen(HDMI_CONTROLLER_STRING);
		mdss_hdmi_get_vic(vic_buf);
		strlcat(pbuf, vic_buf, buf_size);
	} else {
		ret = gcdb_display_cmdline_arg(pbuf, buf_size);
	}

	return ret;
}

void target_display_init(const char *panel_name)
{
	struct oem_panel_data oem;

	set_panel_cmd_string(panel_name);
	oem = mdss_dsi_get_oem_data();

	if (!strcmp(oem.panel, NO_PANEL_CONFIG)
		|| !strcmp(oem.panel, SIM_VIDEO_PANEL)
		|| !strcmp(oem.panel, SIM_DUALDSI_VIDEO_PANEL)
		|| !strcmp(oem.panel, SIM_CMD_PANEL)
		|| !strcmp(oem.panel, SIM_DUALDSI_CMD_PANEL)
		|| oem.skip) {
		dprintf(INFO, "Selected panel: %s\nSkip panel configuration\n",
			oem.panel);
		return;
	} else if (!strcmp(oem.panel, HDMI_PANEL_NAME)) {
		dprintf(INFO, "%s: HDMI is primary\n", __func__);
		return;
	}

	if (gcdb_display_init(oem.panel, MDP_REV_50, (void *)MIPI_FB_ADDR)) {
		target_force_cont_splash_disable(true);
		msm_display_off();
	}

	if (!oem.cont_splash) {
		dprintf(INFO, "Forcing continuous splash disable\n");
		target_force_cont_splash_disable(true);
	}
}

void target_display_shutdown(void)
{
	gcdb_display_shutdown();
}
