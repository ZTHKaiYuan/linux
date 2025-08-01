// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Freescale i.MX Frame Buffer device driver
 *
 *  Copyright (C) 2004 Sascha Hauer, Pengutronix
 *   Based on acornfb.c Copyright (C) Russell King.
 *
 * Please direct your questions and comments on this driver to the following
 * email address:
 *
 *	linux-arm-kernel@lists.arm.linux.org.uk
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/lcd.h>
#include <linux/math64.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/bitfield.h>

#include <linux/regulator/consumer.h>

#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

struct imx_fb_videomode {
	struct fb_videomode mode;
	u32 pcr;
	bool aus_mode;
	unsigned char	bpp;
};

/*
 * Complain if VAR is out of range.
 */
#define DEBUG_VAR 1

#define DRIVER_NAME "imx-fb"

#define LCDC_SSA	0x00

#define LCDC_SIZE	0x04
#define SIZE_XMAX_MASK	GENMASK(25, 20)

#define YMAX_MASK_IMX1	GENMASK(8, 0)
#define YMAX_MASK_IMX21	GENMASK(9, 0)

#define LCDC_VPW	0x08
#define VPW_VPW_MASK	GENMASK(9, 0)

#define LCDC_CPOS	0x0C
#define CPOS_CC1	BIT(31)
#define CPOS_CC0	BIT(30)
#define CPOS_OP		BIT(28)
#define CPOS_CXP_MASK	GENMASK(25, 16)

#define LCDC_LCWHB	0x10
#define LCWHB_BK_EN	BIT(31)
#define LCWHB_CW_MASK	GENMASK(28, 24)
#define LCWHB_CH_MASK	GENMASK(20, 16)
#define LCWHB_BD_MASK	GENMASK(7, 0)

#define LCDC_LCHCC	0x14

#define LCDC_PCR	0x18
#define PCR_TFT		BIT(31)
#define PCR_COLOR	BIT(30)
#define PCR_BPIX_MASK	GENMASK(27, 25)
#define PCR_BPIX_8	3
#define PCR_BPIX_12	4
#define PCR_BPIX_16	5
#define PCR_BPIX_18	6
#define PCR_PCD_MASK	GENMASK(5, 0)

#define LCDC_HCR	0x1C
#define HCR_H_WIDTH_MASK	GENMASK(31, 26)
#define HCR_H_WAIT_1_MASK	GENMASK(15, 8)
#define HCR_H_WAIT_2_MASK	GENMASK(7, 0)

#define LCDC_VCR	0x20
#define VCR_V_WIDTH_MASK	GENMASK(31, 26)
#define VCR_V_WAIT_1_MASK	GENMASK(15, 8)
#define VCR_V_WAIT_2_MASK	GENMASK(7, 0)

#define LCDC_POS	0x24
#define POS_POS_MASK	GENMASK(4, 0)

#define LCDC_LSCR1	0x28
/* bit fields in imxfb.h */

#define LCDC_PWMR	0x2C
/* bit fields in imxfb.h */

#define LCDC_DMACR	0x30
/* bit fields in imxfb.h */

#define LCDC_RMCR	0x34

#define RMCR_LCDC_EN_MX1	BIT(1)

#define RMCR_SELF_REF	BIT(0)

#define LCDC_LCDICR	0x38
#define LCDICR_INT_SYN	BIT(2)
#define LCDICR_INT_CON	BIT(0)

#define LCDC_LCDISR	0x40
#define LCDISR_UDR_ERR	BIT(3)
#define LCDISR_ERR_RES	BIT(2)
#define LCDISR_EOF	BIT(1)
#define LCDISR_BOF	BIT(0)

#define IMXFB_LSCR1_DEFAULT 0x00120300

#define LCDC_LAUSCR	0x80
#define LAUSCR_AUS_MODE	BIT(31)

/* Used fb-mode. Can be set on kernel command line, therefore file-static. */
static const char *fb_mode;

/*
 * These are the bitfields for each
 * display depth that we support.
 */
struct imxfb_rgb {
	struct fb_bitfield	red;
	struct fb_bitfield	green;
	struct fb_bitfield	blue;
	struct fb_bitfield	transp;
};

enum imxfb_type {
	IMX1_FB,
	IMX21_FB,
};

enum imxfb_panel_type {
	PANEL_TYPE_MONOCHROME,
	PANEL_TYPE_CSTN,
	PANEL_TYPE_TFT,
};

struct imxfb_info {
	struct platform_device  *pdev;
	void __iomem		*regs;
	struct clk		*clk_ipg;
	struct clk		*clk_ahb;
	struct clk		*clk_per;
	enum imxfb_type		devtype;
	enum imxfb_panel_type	panel_type;
	bool			enabled;

	/*
	 * These are the addresses we mapped
	 * the framebuffer memory region to.
	 */
	dma_addr_t		map_dma;
	u_int			map_size;

	u_int			palette_size;

	dma_addr_t		dbar1;
	dma_addr_t		dbar2;

	u_int			pcr;
	u_int			lauscr;
	u_int			pwmr;
	u_int			lscr1;
	u_int			dmacr;
	bool			cmap_inverse;
	bool			cmap_static;

	struct imx_fb_videomode *mode;
	int			num_modes;

	struct regulator	*lcd_pwr;
	int			lcd_pwr_enabled;
};

static const struct platform_device_id imxfb_devtype[] = {
	{
		.name = "imx1-fb",
		.driver_data = IMX1_FB,
	}, {
		.name = "imx21-fb",
		.driver_data = IMX21_FB,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(platform, imxfb_devtype);

static const struct of_device_id imxfb_of_dev_id[] = {
	{
		.compatible = "fsl,imx1-fb",
		.data = &imxfb_devtype[IMX1_FB],
	}, {
		.compatible = "fsl,imx21-fb",
		.data = &imxfb_devtype[IMX21_FB],
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, imxfb_of_dev_id);

static inline int is_imx1_fb(struct imxfb_info *fbi)
{
	return fbi->devtype == IMX1_FB;
}

#define IMX_NAME	"IMX"

/*
 * Minimum X and Y resolutions
 */
#define MIN_XRES	64
#define MIN_YRES	64

/* Actually this really is 18bit support, the lowest 2 bits of each colour
 * are unused in hardware. We claim to have 24bit support to make software
 * like X work, which does not support 18bit.
 */
static struct imxfb_rgb def_rgb_18 = {
	.red	= {.offset = 16, .length = 8,},
	.green	= {.offset = 8, .length = 8,},
	.blue	= {.offset = 0, .length = 8,},
	.transp = {.offset = 0, .length = 0,},
};

static struct imxfb_rgb def_rgb_16_tft = {
	.red	= {.offset = 11, .length = 5,},
	.green	= {.offset = 5, .length = 6,},
	.blue	= {.offset = 0, .length = 5,},
	.transp = {.offset = 0, .length = 0,},
};

static struct imxfb_rgb def_rgb_16_stn = {
	.red	= {.offset = 8, .length = 4,},
	.green	= {.offset = 4, .length = 4,},
	.blue	= {.offset = 0, .length = 4,},
	.transp = {.offset = 0, .length = 0,},
};

static struct imxfb_rgb def_rgb_8 = {
	.red	= {.offset = 0, .length = 8,},
	.green	= {.offset = 0, .length = 8,},
	.blue	= {.offset = 0, .length = 8,},
	.transp = {.offset = 0, .length = 0,},
};

static int imxfb_activate_var(struct fb_var_screeninfo *var,
		struct fb_info *info);

static inline u_int chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int imxfb_setpalettereg(u_int regno, u_int red, u_int green, u_int blue,
		u_int trans, struct fb_info *info)
{
	struct imxfb_info *fbi = info->par;
	u_int val, ret = 1;

#define CNVT_TOHW(val, width) ((((val)<<(width))+0x7FFF-(val))>>16)
	if (regno < fbi->palette_size) {
		val = (CNVT_TOHW(red, 4) << 8) |
		      (CNVT_TOHW(green, 4) << 4) |
		      CNVT_TOHW(blue,  4);

		writel(val, fbi->regs + 0x800 + (regno << 2));
		ret = 0;
	}
	return ret;
}

static int imxfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		   u_int trans, struct fb_info *info)
{
	struct imxfb_info *fbi = info->par;
	unsigned int val;
	int ret = 1;

	/*
	 * If inverse mode was selected, invert all the colours
	 * rather than the register number.  The register number
	 * is what you poke into the framebuffer to produce the
	 * colour you requested.
	 */
	if (fbi->cmap_inverse) {
		red   = 0xffff - red;
		green = 0xffff - green;
		blue  = 0xffff - blue;
	}

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no mater what visual we are using.
	 */
	if (info->var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;

	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 12 or 16-bit True Colour.  We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u32 *pal = info->pseudo_palette;

			val  = chan_to_field(red, &info->var.red);
			val |= chan_to_field(green, &info->var.green);
			val |= chan_to_field(blue, &info->var.blue);

			pal[regno] = val;
			ret = 0;
		}
		break;

	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		ret = imxfb_setpalettereg(regno, red, green, blue, trans, info);
		break;
	}

	return ret;
}

static const struct imx_fb_videomode *imxfb_find_mode(struct imxfb_info *fbi)
{
	struct imx_fb_videomode *m;
	int i;

	if (!fb_mode)
		return &fbi->mode[0];

	for (i = 0, m = &fbi->mode[0]; i < fbi->num_modes; i++, m++) {
		if (!strcmp(m->mode.name, fb_mode))
			return m;
	}
	return NULL;
}

/*
 *  imxfb_check_var():
 *    Round up in the following order: bits_per_pixel, xres,
 *    yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *    bitfields, horizontal timing, vertical timing.
 */
static int imxfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct imxfb_info *fbi = info->par;
	struct imxfb_rgb *rgb;
	const struct imx_fb_videomode *imxfb_mode;
	unsigned long lcd_clk;
	unsigned long long tmp;
	u32 pcr = 0;

	if (var->xres < MIN_XRES)
		var->xres = MIN_XRES;
	if (var->yres < MIN_YRES)
		var->yres = MIN_YRES;

	imxfb_mode = imxfb_find_mode(fbi);
	if (!imxfb_mode)
		return -EINVAL;

	var->xres		= imxfb_mode->mode.xres;
	var->yres		= imxfb_mode->mode.yres;
	var->bits_per_pixel	= imxfb_mode->bpp;
	var->pixclock		= imxfb_mode->mode.pixclock;
	var->hsync_len		= imxfb_mode->mode.hsync_len;
	var->left_margin	= imxfb_mode->mode.left_margin;
	var->right_margin	= imxfb_mode->mode.right_margin;
	var->vsync_len		= imxfb_mode->mode.vsync_len;
	var->upper_margin	= imxfb_mode->mode.upper_margin;
	var->lower_margin	= imxfb_mode->mode.lower_margin;
	var->sync		= imxfb_mode->mode.sync;
	var->xres_virtual	= max(var->xres_virtual, var->xres);
	var->yres_virtual	= max(var->yres_virtual, var->yres);

	pr_debug("var->bits_per_pixel=%d\n", var->bits_per_pixel);

	lcd_clk = clk_get_rate(fbi->clk_per);

	tmp = var->pixclock * (unsigned long long)lcd_clk;

	do_div(tmp, 1000000);

	if (do_div(tmp, 1000000) > 500000)
		tmp++;

	pcr = (unsigned int)tmp;

	if (--pcr > PCR_PCD_MASK) {
		pcr = PCR_PCD_MASK;
		dev_warn(&fbi->pdev->dev, "Must limit pixel clock to %luHz\n",
			 lcd_clk / pcr);
	}

	switch (var->bits_per_pixel) {
	case 32:
		pcr |= FIELD_PREP(PCR_BPIX_MASK, PCR_BPIX_18);
		rgb = &def_rgb_18;
		break;
	case 16:
	default:
		if (is_imx1_fb(fbi))
			pcr |= FIELD_PREP(PCR_BPIX_MASK, PCR_BPIX_12);
		else
			pcr |= FIELD_PREP(PCR_BPIX_MASK, PCR_BPIX_16);

		if (imxfb_mode->pcr & PCR_TFT)
			rgb = &def_rgb_16_tft;
		else
			rgb = &def_rgb_16_stn;
		break;
	case 8:
		pcr |= FIELD_PREP(PCR_BPIX_MASK, PCR_BPIX_8);
		rgb = &def_rgb_8;
		break;
	}

	/* add sync polarities */
	pcr |= imxfb_mode->pcr & ~(PCR_PCD_MASK | PCR_BPIX_MASK);

	fbi->pcr = pcr;
	/*
	 * The LCDC AUS Mode Control Register does not exist on imx1.
	 */
	if (!is_imx1_fb(fbi) && imxfb_mode->aus_mode)
		fbi->lauscr = LAUSCR_AUS_MODE;

	if (imxfb_mode->pcr & PCR_TFT)
		fbi->panel_type = PANEL_TYPE_TFT;
	else if (imxfb_mode->pcr & PCR_COLOR)
		fbi->panel_type = PANEL_TYPE_CSTN;
	else
		fbi->panel_type = PANEL_TYPE_MONOCHROME;

	/*
	 * Copy the RGB parameters for this display
	 * from the machine specific parameters.
	 */
	var->red    = rgb->red;
	var->green  = rgb->green;
	var->blue   = rgb->blue;
	var->transp = rgb->transp;

	pr_debug("RGBT length = %d:%d:%d:%d\n",
		var->red.length, var->green.length, var->blue.length,
		var->transp.length);

	pr_debug("RGBT offset = %d:%d:%d:%d\n",
		var->red.offset, var->green.offset, var->blue.offset,
		var->transp.offset);

	return 0;
}

/*
 * imxfb_set_par():
 *	Set the user defined part of the display for the specified console
 */
static int imxfb_set_par(struct fb_info *info)
{
	struct imxfb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;

	if (var->bits_per_pixel == 16 || var->bits_per_pixel == 32)
		info->fix.visual = FB_VISUAL_TRUECOLOR;
	else if (!fbi->cmap_static)
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else {
		/*
		 * Some people have weird ideas about wanting static
		 * pseudocolor maps.  I suspect their user space
		 * applications are broken.
		 */
		info->fix.visual = FB_VISUAL_STATIC_PSEUDOCOLOR;
	}

	info->fix.line_length = var->xres_virtual * var->bits_per_pixel / 8;
	fbi->palette_size = var->bits_per_pixel == 8 ? 256 : 16;

	imxfb_activate_var(var, info);

	return 0;
}

static int imxfb_enable_controller(struct imxfb_info *fbi)
{
	int ret;

	if (fbi->enabled)
		return 0;

	pr_debug("Enabling LCD controller\n");

	writel(fbi->map_dma, fbi->regs + LCDC_SSA);

	/* panning offset 0 (0 pixel offset)        */
	writel(FIELD_PREP(POS_POS_MASK, 0), fbi->regs + LCDC_POS);

	/* disable hardware cursor */
	writel(readl(fbi->regs + LCDC_CPOS) & ~(CPOS_CC0 | CPOS_CC1),
		fbi->regs + LCDC_CPOS);

	/*
	 * RMCR_LCDC_EN_MX1 is present on i.MX1 only, but doesn't hurt
	 * on other SoCs
	 */
	writel(RMCR_LCDC_EN_MX1, fbi->regs + LCDC_RMCR);

	ret = clk_prepare_enable(fbi->clk_ipg);
	if (ret)
		goto err_enable_ipg;

	ret = clk_prepare_enable(fbi->clk_ahb);
	if (ret)
		goto err_enable_ahb;

	ret = clk_prepare_enable(fbi->clk_per);
	if (ret)
		goto err_enable_per;

	fbi->enabled = true;
	return 0;

err_enable_per:
	clk_disable_unprepare(fbi->clk_ahb);
err_enable_ahb:
	clk_disable_unprepare(fbi->clk_ipg);
err_enable_ipg:
	writel(0, fbi->regs + LCDC_RMCR);

	return ret;
}

static void imxfb_disable_controller(struct imxfb_info *fbi)
{
	if (!fbi->enabled)
		return;

	pr_debug("Disabling LCD controller\n");

	clk_disable_unprepare(fbi->clk_per);
	clk_disable_unprepare(fbi->clk_ahb);
	clk_disable_unprepare(fbi->clk_ipg);
	fbi->enabled = false;

	writel(0, fbi->regs + LCDC_RMCR);
}

static int imxfb_blank(int blank, struct fb_info *info)
{
	struct imxfb_info *fbi = info->par;

	pr_debug("%s: blank=%d\n", __func__, blank);

	switch (blank) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
		imxfb_disable_controller(fbi);
		break;

	case FB_BLANK_UNBLANK:
		return imxfb_enable_controller(fbi);
	}
	return 0;
}

static const struct fb_ops imxfb_ops = {
	.owner		= THIS_MODULE,
	FB_DEFAULT_IOMEM_OPS,
	.fb_check_var	= imxfb_check_var,
	.fb_set_par	= imxfb_set_par,
	.fb_setcolreg	= imxfb_setcolreg,
	.fb_blank	= imxfb_blank,
};

/*
 * imxfb_activate_var():
 *	Configures LCD Controller based on entries in var parameter.  Settings are
 *	only written to the controller if changes were made.
 */
static int imxfb_activate_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct imxfb_info *fbi = info->par;
	u32 ymax_mask = is_imx1_fb(fbi) ? YMAX_MASK_IMX1 : YMAX_MASK_IMX21;
	u8 left_margin_low;

	pr_debug("var: xres=%d hslen=%d lm=%d rm=%d\n",
		var->xres, var->hsync_len,
		var->left_margin, var->right_margin);
	pr_debug("var: yres=%d vslen=%d um=%d bm=%d\n",
		var->yres, var->vsync_len,
		var->upper_margin, var->lower_margin);

	if (fbi->panel_type == PANEL_TYPE_TFT)
		left_margin_low = 3;
	else if (fbi->panel_type == PANEL_TYPE_CSTN)
		left_margin_low = 2;
	else
		left_margin_low = 0;

#if DEBUG_VAR
	if (var->xres < 16        || var->xres > 1024)
		dev_err(&fbi->pdev->dev, "%s: invalid xres %d\n",
			info->fix.id, var->xres);
	if (var->hsync_len < 1    || var->hsync_len > 64)
		dev_err(&fbi->pdev->dev, "%s: invalid hsync_len %d\n",
			info->fix.id, var->hsync_len);
	if (var->left_margin < left_margin_low  || var->left_margin > 255)
		dev_err(&fbi->pdev->dev, "%s: invalid left_margin %d\n",
			info->fix.id, var->left_margin);
	if (var->right_margin < 1 || var->right_margin > 255)
		dev_err(&fbi->pdev->dev, "%s: invalid right_margin %d\n",
			info->fix.id, var->right_margin);
	if (var->yres < 1 || var->yres > ymax_mask)
		dev_err(&fbi->pdev->dev, "%s: invalid yres %d\n",
			info->fix.id, var->yres);
	if (var->vsync_len > 100)
		dev_err(&fbi->pdev->dev, "%s: invalid vsync_len %d\n",
			info->fix.id, var->vsync_len);
	if (var->upper_margin > 63)
		dev_err(&fbi->pdev->dev, "%s: invalid upper_margin %d\n",
			info->fix.id, var->upper_margin);
	if (var->lower_margin > 255)
		dev_err(&fbi->pdev->dev, "%s: invalid lower_margin %d\n",
			info->fix.id, var->lower_margin);
#endif

	/* physical screen start address	    */
	writel(FIELD_PREP(VPW_VPW_MASK,
			  var->xres * var->bits_per_pixel / 8 / 4),
	       fbi->regs + LCDC_VPW);

	writel(FIELD_PREP(HCR_H_WIDTH_MASK, var->hsync_len - 1) |
	       FIELD_PREP(HCR_H_WAIT_1_MASK, var->right_margin - 1) |
	       FIELD_PREP(HCR_H_WAIT_2_MASK,
			  var->left_margin - left_margin_low),
	       fbi->regs + LCDC_HCR);

	writel(FIELD_PREP(VCR_V_WIDTH_MASK, var->vsync_len) |
	       FIELD_PREP(VCR_V_WAIT_1_MASK, var->lower_margin) |
	       FIELD_PREP(VCR_V_WAIT_2_MASK, var->upper_margin),
	       fbi->regs + LCDC_VCR);

	writel(FIELD_PREP(SIZE_XMAX_MASK, var->xres >> 4) |
	       (var->yres & ymax_mask),
	       fbi->regs + LCDC_SIZE);

	writel(fbi->pcr, fbi->regs + LCDC_PCR);
	if (fbi->pwmr)
		writel(fbi->pwmr, fbi->regs + LCDC_PWMR);
	writel(fbi->lscr1, fbi->regs + LCDC_LSCR1);

	/* dmacr = 0 is no valid value, as we need DMA control marks. */
	if (fbi->dmacr)
		writel(fbi->dmacr, fbi->regs + LCDC_DMACR);

	if (fbi->lauscr)
		writel(fbi->lauscr, fbi->regs + LCDC_LAUSCR);

	return 0;
}

static int imxfb_init_fbinfo(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct imxfb_info *fbi = info->par;
	struct device_node *np;

	info->pseudo_palette = devm_kmalloc_array(&pdev->dev, 16,
						  sizeof(u32), GFP_KERNEL);
	if (!info->pseudo_palette)
		return -ENOMEM;

	memset(fbi, 0, sizeof(struct imxfb_info));

	fbi->pdev = pdev;
	fbi->devtype = pdev->id_entry->driver_data;

	strscpy(info->fix.id, IMX_NAME, sizeof(info->fix.id));

	info->fix.type			= FB_TYPE_PACKED_PIXELS;
	info->fix.type_aux		= 0;
	info->fix.xpanstep		= 0;
	info->fix.ypanstep		= 0;
	info->fix.ywrapstep		= 0;
	info->fix.accel			= FB_ACCEL_NONE;

	info->var.nonstd		= 0;
	info->var.activate		= FB_ACTIVATE_NOW;
	info->var.height		= -1;
	info->var.width	= -1;
	info->var.accel_flags		= 0;
	info->var.vmode			= FB_VMODE_NONINTERLACED;

	info->fbops			= &imxfb_ops;
	info->flags			= FBINFO_READS_FAST;

	np = pdev->dev.of_node;
	info->var.grayscale = of_property_read_bool(np,
					"cmap-greyscale");
	fbi->cmap_inverse = of_property_read_bool(np, "cmap-inverse");
	fbi->cmap_static = of_property_read_bool(np, "cmap-static");

	fbi->lscr1 = IMXFB_LSCR1_DEFAULT;

	of_property_read_u32(np, "fsl,lpccr", &fbi->pwmr);

	of_property_read_u32(np, "fsl,lscr1", &fbi->lscr1);

	of_property_read_u32(np, "fsl,dmacr", &fbi->dmacr);

	return 0;
}

static int imxfb_of_read_mode(struct device *dev, struct device_node *np,
		struct imx_fb_videomode *imxfb_mode)
{
	int ret;
	struct fb_videomode *of_mode = &imxfb_mode->mode;
	u32 bpp;
	u32 pcr;

	ret = of_property_read_string(np, "model", &of_mode->name);
	if (ret)
		of_mode->name = NULL;

	ret = of_get_fb_videomode(np, of_mode, OF_USE_NATIVE_MODE);
	if (ret) {
		dev_err(dev, "Failed to get videomode from DT\n");
		return ret;
	}

	ret = of_property_read_u32(np, "bits-per-pixel", &bpp);
	ret |= of_property_read_u32(np, "fsl,pcr", &pcr);

	if (ret) {
		dev_err(dev, "Failed to read bpp and pcr from DT\n");
		return -EINVAL;
	}

	if (bpp < 1 || bpp > 255) {
		dev_err(dev, "Bits per pixel have to be between 1 and 255\n");
		return -EINVAL;
	}

	imxfb_mode->bpp = bpp;
	imxfb_mode->pcr = pcr;

	/*
	 * fsl,aus-mode is optional
	 */
	imxfb_mode->aus_mode = of_property_read_bool(np, "fsl,aus-mode");

	return 0;
}

static int imxfb_lcd_get_contrast(struct lcd_device *lcddev)
{
	struct imxfb_info *fbi = dev_get_drvdata(&lcddev->dev);

	return fbi->pwmr & 0xff;
}

static int imxfb_lcd_set_contrast(struct lcd_device *lcddev, int contrast)
{
	struct imxfb_info *fbi = dev_get_drvdata(&lcddev->dev);

	if (fbi->pwmr && fbi->enabled) {
		if (contrast > 255)
			contrast = 255;
		else if (contrast < 0)
			contrast = 0;

		fbi->pwmr &= ~0xff;
		fbi->pwmr |= contrast;

		writel(fbi->pwmr, fbi->regs + LCDC_PWMR);
	}

	return 0;
}

static int imxfb_lcd_get_power(struct lcd_device *lcddev)
{
	struct imxfb_info *fbi = dev_get_drvdata(&lcddev->dev);

	if (!IS_ERR(fbi->lcd_pwr) &&
	    !regulator_is_enabled(fbi->lcd_pwr))
		return LCD_POWER_OFF;

	return LCD_POWER_ON;
}

static int imxfb_regulator_set(struct imxfb_info *fbi, int enable)
{
	int ret;

	if (enable == fbi->lcd_pwr_enabled)
		return 0;

	if (enable)
		ret = regulator_enable(fbi->lcd_pwr);
	else
		ret = regulator_disable(fbi->lcd_pwr);

	if (ret == 0)
		fbi->lcd_pwr_enabled = enable;

	return ret;
}

static int imxfb_lcd_set_power(struct lcd_device *lcddev, int power)
{
	struct imxfb_info *fbi = dev_get_drvdata(&lcddev->dev);

	if (!IS_ERR(fbi->lcd_pwr))
		return imxfb_regulator_set(fbi, power == LCD_POWER_ON);

	return 0;
}

static const struct lcd_ops imxfb_lcd_ops = {
	.get_contrast	= imxfb_lcd_get_contrast,
	.set_contrast	= imxfb_lcd_set_contrast,
	.get_power	= imxfb_lcd_get_power,
	.set_power	= imxfb_lcd_set_power,
};

static int imxfb_setup(void)
{
	char *opt, *options = NULL;

	if (fb_get_options("imxfb", &options))
		return -ENODEV;

	if (!options || !*options)
		return 0;

	while ((opt = strsep(&options, ",")) != NULL) {
		if (!*opt)
			continue;
		else
			fb_mode = opt;
	}

	return 0;
}

static int imxfb_probe(struct platform_device *pdev)
{
	struct imxfb_info *fbi;
	struct lcd_device *lcd;
	struct fb_info *info;
	struct imx_fb_videomode *m;
	const struct of_device_id *of_id;
	struct device_node *display_np;
	int ret, i;
	int bytes_per_pixel;

	dev_info(&pdev->dev, "i.MX Framebuffer driver\n");

	ret = imxfb_setup();
	if (ret < 0)
		return ret;

	of_id = of_match_device(imxfb_of_dev_id, &pdev->dev);
	if (of_id)
		pdev->id_entry = of_id->data;

	info = framebuffer_alloc(sizeof(struct imxfb_info), &pdev->dev);
	if (!info)
		return -ENOMEM;

	fbi = info->par;

	platform_set_drvdata(pdev, info);

	ret = imxfb_init_fbinfo(pdev);
	if (ret < 0)
		goto failed_init;

	fb_mode = NULL;

	display_np = of_parse_phandle(pdev->dev.of_node, "display", 0);
	if (!display_np) {
		dev_err(&pdev->dev, "No display defined in devicetree\n");
		ret = -EINVAL;
		goto failed_init;
	}

	/*
	 * imxfb does not support more modes, we choose only the native
	 * mode.
	 */
	fbi->num_modes = 1;

	fbi->mode = devm_kzalloc(&pdev->dev,
			sizeof(struct imx_fb_videomode), GFP_KERNEL);
	if (!fbi->mode) {
		ret = -ENOMEM;
		of_node_put(display_np);
		goto failed_init;
	}

	ret = imxfb_of_read_mode(&pdev->dev, display_np, fbi->mode);
	of_node_put(display_np);
	if (ret)
		goto failed_init;

	/*
	 * Calculate maximum bytes used per pixel. In most cases this should
	 * be the same as m->bpp/8
	 */
	m = &fbi->mode[0];
	bytes_per_pixel = (m->bpp + 7) / 8;
	for (i = 0; i < fbi->num_modes; i++, m++)
		info->fix.smem_len = max_t(size_t, info->fix.smem_len,
				m->mode.xres * m->mode.yres * bytes_per_pixel);

	fbi->clk_ipg = devm_clk_get(&pdev->dev, "ipg");
	if (IS_ERR(fbi->clk_ipg)) {
		ret = PTR_ERR(fbi->clk_ipg);
		goto failed_init;
	}

	/*
	 * The LCDC controller does not have an enable bit. The
	 * controller starts directly when the clocks are enabled.
	 * If the clocks are enabled when the controller is not yet
	 * programmed with proper register values (enabled at the
	 * bootloader, for example) then it just goes into some undefined
	 * state.
	 * To avoid this issue, let's enable and disable LCDC IPG clock
	 * so that we force some kind of 'reset' to the LCDC block.
	 */
	ret = clk_prepare_enable(fbi->clk_ipg);
	if (ret)
		goto failed_init;
	clk_disable_unprepare(fbi->clk_ipg);

	fbi->clk_ahb = devm_clk_get(&pdev->dev, "ahb");
	if (IS_ERR(fbi->clk_ahb)) {
		ret = PTR_ERR(fbi->clk_ahb);
		goto failed_init;
	}

	fbi->clk_per = devm_clk_get(&pdev->dev, "per");
	if (IS_ERR(fbi->clk_per)) {
		ret = PTR_ERR(fbi->clk_per);
		goto failed_init;
	}

	fbi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(fbi->regs)) {
		ret = PTR_ERR(fbi->regs);
		goto failed_init;
	}

	fbi->map_size = PAGE_ALIGN(info->fix.smem_len);
	info->screen_buffer = dma_alloc_wc(&pdev->dev, fbi->map_size,
					   &fbi->map_dma, GFP_KERNEL);
	if (!info->screen_buffer) {
		dev_err(&pdev->dev, "Failed to allocate video RAM\n");
		ret = -ENOMEM;
		goto failed_init;
	}

	info->fix.smem_start = fbi->map_dma;

	INIT_LIST_HEAD(&info->modelist);
	for (i = 0; i < fbi->num_modes; i++) {
		ret = fb_add_videomode(&fbi->mode[i].mode, &info->modelist);
		if (ret) {
			dev_err(&pdev->dev, "Failed to add videomode\n");
			goto failed_cmap;
		}
	}

	/*
	 * This makes sure that our colour bitfield
	 * descriptors are correctly initialised.
	 */
	imxfb_check_var(&info->var, info);

	/*
	 * For modes > 8bpp, the color map is bypassed.
	 * Therefore, 256 entries are enough.
	 */
	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret < 0)
		goto failed_cmap;

	imxfb_set_par(info);

	fbi->lcd_pwr = devm_regulator_get(&pdev->dev, "lcd");
	if (PTR_ERR(fbi->lcd_pwr) == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto failed_lcd;
	}

	lcd = devm_lcd_device_register(&pdev->dev, "imxfb-lcd", &pdev->dev, fbi,
				       &imxfb_lcd_ops);
	if (IS_ERR(lcd)) {
		ret = PTR_ERR(lcd);
		goto failed_lcd;
	}

	lcd->props.max_contrast = 0xff;

	info->lcd_dev = lcd;

	ret = register_framebuffer(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register framebuffer\n");
		goto failed_lcd;
	}

	imxfb_enable_controller(fbi);

	return 0;

failed_lcd:
	fb_dealloc_cmap(&info->cmap);
failed_cmap:
	dma_free_wc(&pdev->dev, fbi->map_size, info->screen_buffer,
		    fbi->map_dma);
failed_init:
	framebuffer_release(info);
	return ret;
}

static void imxfb_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	struct imxfb_info *fbi = info->par;

	imxfb_disable_controller(fbi);

	unregister_framebuffer(info);
	fb_dealloc_cmap(&info->cmap);
	dma_free_wc(&pdev->dev, fbi->map_size, info->screen_buffer,
		    fbi->map_dma);
	framebuffer_release(info);
}

static int imxfb_suspend(struct device *dev)
{
	struct fb_info *info = dev_get_drvdata(dev);
	struct imxfb_info *fbi = info->par;

	imxfb_disable_controller(fbi);

	return 0;
}

static int imxfb_resume(struct device *dev)
{
	struct fb_info *info = dev_get_drvdata(dev);
	struct imxfb_info *fbi = info->par;

	imxfb_enable_controller(fbi);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(imxfb_pm_ops, imxfb_suspend, imxfb_resume);

static struct platform_driver imxfb_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table = imxfb_of_dev_id,
		.pm	= pm_sleep_ptr(&imxfb_pm_ops),
	},
	.probe		= imxfb_probe,
	.remove		= imxfb_remove,
	.id_table	= imxfb_devtype,
};
module_platform_driver(imxfb_driver);

MODULE_DESCRIPTION("Freescale i.MX framebuffer driver");
MODULE_AUTHOR("Sascha Hauer, Pengutronix");
MODULE_LICENSE("GPL");
