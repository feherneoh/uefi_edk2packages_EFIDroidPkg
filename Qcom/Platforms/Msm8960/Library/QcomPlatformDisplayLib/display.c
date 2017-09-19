/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Library/LKEnvLib.h>
#include <Library/MallocLib.h>
#include <Chipset/msm_panel.h>
#include <Chipset/mdp4.h>
#include <Chipset/mipi_dsi.h>
#include <Platform/display.h>

static struct msm_fb_panel_data *panel;

static int msm_fb_alloc(struct fbcon_config *fb)
{
	if (fb == NULL)
		return ERROR;

	if (fb->base == NULL)
		fb->base = memalign(4096, fb->width
							* fb->height
							* (fb->bpp / 8));

	if (fb->base == NULL)
		return ERROR;

	return NO_ERROR;
}

int msm_display_config(void)
{
	int ret = NO_ERROR;
	struct msm_panel_info *pinfo;

	if (!panel)
		return ERR_INVALID_ARGS;

	pinfo = &(panel->panel_info);

	/* Set MDP revision */
	mdp_set_revision(panel->mdp_rev);

	switch (pinfo->type) {
	case MIPI_VIDEO_PANEL:
		dprintf(INFO, "Config MIPI_VIDEO_PANEL.\n");
		ret = mipi_config(panel);
		if (ret)
			goto msm_display_config_out;

		if (pinfo->early_config)
			ret = pinfo->early_config((void *)pinfo);

		ret = mdp_dsi_video_config(pinfo, &(panel->fb));
		if (ret)
			goto msm_display_config_out;
		break;
	case MIPI_CMD_PANEL:
		dprintf(INFO, "Config MIPI_CMD_PANEL.\n");
		ret = mipi_config(panel);
		if (ret)
			goto msm_display_config_out;
		ret = mdp_dsi_cmd_config(pinfo, &(panel->fb));
		if (ret)
			goto msm_display_config_out;
		break;
	default:
		return ERR_INVALID_ARGS;
	};

	if (pinfo->config)
		ret = pinfo->config((void *)pinfo);

msm_display_config_out:
	return ret;
}

int msm_display_on(void)
{
	int ret = NO_ERROR;
	struct msm_panel_info *pinfo;

	if (!panel)
		return ERR_INVALID_ARGS;

	pinfo = &(panel->panel_info);

	switch (pinfo->type) {
	case MIPI_VIDEO_PANEL:
		dprintf(INFO, "Turn on MIPI_VIDEO_PANEL.\n");
		ret = mdp_dsi_video_on();
		if (ret)
			goto msm_display_on_out;
		ret = mipi_dsi_on();
		if (ret)
			goto msm_display_on_out;
		break;
	case MIPI_CMD_PANEL:
		dprintf(INFO, "Turn on MIPI_CMD_PANEL.\n");
		ret = mdp_dma_on();
		if (ret)
			goto msm_display_on_out;
		ret = mipi_cmd_trigger();
		if (ret)
			goto msm_display_on_out;
		break;
	default:
		return ERR_INVALID_ARGS;
	};

	if (pinfo->on)
		ret = pinfo->on();

msm_display_on_out:
	return ret;
}

int msm_display_init(struct msm_fb_panel_data *pdata)
{
	int ret = NO_ERROR;

	panel = pdata;
	if (!panel) {
		ret = ERR_INVALID_ARGS;
		goto msm_display_init_out;
	}

	/* Enable clock */
	if (pdata->clk_func)
		ret = pdata->clk_func(1);

	if (ret)
		goto msm_display_init_out;

	/* Turn on panel */
	if (pdata->power_func)
		ret = pdata->power_func(1);

	if (ret)
		goto msm_display_init_out;

	ret = msm_fb_alloc(&(panel->fb));
	if (ret)
		goto msm_display_init_out;

	if (panel->panel_info.type==MIPI_CMD_PANEL && panel->panel_info.mipi.mdp_trigger==DSI_CMD_TRIGGER_SW) {
		panel->fb.update_start = mipi_update_sw_trigger;
	}

	fbcon_setup(&(panel->fb));
	ret = msm_display_config();
	if (ret)
		goto msm_display_init_out;

	ret = msm_display_on();
	if (ret)
		goto msm_display_init_out;

msm_display_init_out:
	return ret;
}

int msm_display_off(void)
{
	int ret = NO_ERROR;
	struct msm_panel_info *pinfo;

	if (!panel)
		return ERR_INVALID_ARGS;

	pinfo = &(panel->panel_info);

	switch (pinfo->type) {
	case LVDS_PANEL:
		dprintf(INFO, "Turn off LVDS PANEL.\n");
		mdp_lcdc_off();
		break;
	case MIPI_VIDEO_PANEL:
		dprintf(INFO, "Turn off MIPI_VIDEO_PANEL.\n");
		ret = mdp_dsi_video_off();
		if (ret)
			goto msm_display_off_out;
		ret = mipi_dsi_off();
		if (ret)
			goto msm_display_off_out;
		break;
	case MIPI_CMD_PANEL:
		dprintf(INFO, "Turn off MIPI_CMD_PANEL.\n");
		ret = mdp_dsi_cmd_off();
		if (ret)
			goto msm_display_off_out;
		ret = mipi_dsi_off();
		if (ret)
			goto msm_display_off_out;
		break;
	case LCDC_PANEL:
		dprintf(INFO, "Turn off LCDC PANEL.\n");
		mdp_lcdc_off();
		break;
	default:
		return ERR_INVALID_ARGS;
	};

	if (pinfo->off)
		ret = pinfo->off();

	/* Disable clock */
	if (panel->clk_func)
		ret = panel->clk_func(0);

	if (ret)
		goto msm_display_off_out;

	/* Disable panel */
	if (panel->power_func)
		ret = panel->power_func(0);

msm_display_off_out:
	return ret;
}
