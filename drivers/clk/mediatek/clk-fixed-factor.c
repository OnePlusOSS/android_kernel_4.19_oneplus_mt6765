/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>

/*
 * DOC: basic fixed multiplier and divider clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is fixed.  clk->rate = parent->rate / div * mult
 * parent - fixed parent.  No clk_set_parent support
 */
#ifndef BIT
#define BIT(_bit_)		(u32)(1U << (_bit_))
#endif

struct mtk_clk_fixed_factor {
	struct clk_hw	hw;
	unsigned int	mult;
	unsigned int	div;
	unsigned int	shift;
	unsigned int	pd_reg;
	void __iomem	*base;
};

static inline struct mtk_clk_fixed_factor *to_mtk_fixed_factor_data(
	struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_fixed_factor, hw);
}

static unsigned long clk_factor_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{


	struct mtk_clk_fixed_factor *fix = to_mtk_fixed_factor_data(hw);
	unsigned long long int rate;

	rate = (unsigned long long int)parent_rate * fix->mult;
	do_div(rate, fix->div);
	return (unsigned long)rate;
}

static long clk_factor_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	struct mtk_clk_fixed_factor *fix = to_mtk_fixed_factor_data(hw);

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT) {
		unsigned long best_parent;

		best_parent = (rate / fix->mult) * fix->div;
		*prate = clk_hw_round_rate(clk_hw_get_parent(hw), best_parent);
	}

	return (*prate / fix->div) * fix->mult;
}

static int clk_factor_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	/*
	 * We must report success but we can do so unconditionally because
	 * clk_factor_round_rate returns values that ensure this call is a
	 * nop.
	 */
	return 0;
}

static int clk_fixed_factor_enable(struct clk_hw *hw)
{
	struct mtk_clk_fixed_factor *ff = to_mtk_fixed_factor_data(hw);
	u32 val, orig;

	val = clk_readl(ff->base + ff->pd_reg);
	orig = val;
	val |= BIT(ff->shift);

	if (val != orig)
		clk_writel(val, ff->base + ff->pd_reg);

	return 0;
}

static void clk_fixed_factor_disable(struct clk_hw *hw)
{
	struct mtk_clk_fixed_factor *ff = to_mtk_fixed_factor_data(hw);
	u32 val, orig;

	val = clk_readl(ff->base + ff->pd_reg);
	orig = val;
	val &= ~BIT(ff->shift);

	if (val != orig)
		clk_writel(val, ff->base + ff->pd_reg);
}

static int clk_fixed_factor_prepare(struct clk_hw *hw)
{
	return 0;
}

static void clk_fixed_factor_unprepare(struct clk_hw *hw)
{
}

const struct clk_ops clk_fixed_factor_pdn_ops = {
	.round_rate = clk_factor_round_rate,
	.set_rate = clk_factor_set_rate,
	.recalc_rate = clk_factor_recalc_rate,
	.prepare	= clk_fixed_factor_prepare,
	.unprepare	= clk_fixed_factor_unprepare,
	.enable	= clk_fixed_factor_enable,
	.disable	= clk_fixed_factor_disable,
};

const struct clk_ops clk_fixed_factor_npdn_ops = {
	.round_rate = clk_factor_round_rate,
	.set_rate = clk_factor_set_rate,
	.recalc_rate = clk_factor_recalc_rate,
};
/*EXPORT_SYMBOL_GPL(clk_fixed_factor_pdn_ops);*/

struct clk *mtk_clk_register_fixed_factor_pdn(struct device *dev,
	const char *name,
	const char *parent_name, unsigned long flags,
	unsigned int mult, unsigned int div, unsigned int shift,
	unsigned int pd_reg, void __iomem *base)
{
	struct mtk_clk_fixed_factor *fix;
	struct clk_init_data init = {};
	struct clk *clk;

	fix = kmalloc(sizeof(*fix), GFP_KERNEL);
	if (!fix)
		return ERR_PTR(-ENOMEM);

	/* struct clk_fixed_factor assignments */
	fix->mult = mult;
	fix->div = div;
	fix->shift = shift;
	fix->base = base;
	fix->pd_reg = pd_reg;
	fix->hw.init = &init;

	init.name = name;
	if (pd_reg == -1)
		init.ops = &clk_fixed_factor_npdn_ops;
	else
		init.ops = &clk_fixed_factor_pdn_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(dev, &fix->hw);

	if (IS_ERR(clk))
		kfree(fix);

	return clk;
}
EXPORT_SYMBOL_GPL(mtk_clk_register_fixed_factor_pdn);

#ifdef CONFIG_OF
/**
 * of_fixed_factor_clk_setup() - Setup function for simple fixed factor clock
 */
 #if 0
void __init mtk_of_fixed_factor_clk_setup(struct device_node *node)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *parent_name;
	u32 div, mult;

	if (of_property_read_u32(node, "clock-div", &div)) {
		pr_debug("%s Fixed factor clock <%s> must have a clock-div property\n",
			__func__, node->name);
		return;
	}

	if (of_property_read_u32(node, "clock-mult", &mult)) {
		pr_debug("%s Fixed factor clock <%s> must have a clock-mult property\n",
			__func__, node->name);
		return;
	}

	of_property_read_string(node, "clock-output-names", &clk_name);
	parent_name = of_clk_get_parent_name(node, 0);

	clk = clk_register_fixed_factor(NULL, clk_name, parent_name, 0,
					mult, div);
	if (!IS_ERR(clk))
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
}
EXPORT_SYMBOL_GPL(mtk_of_fixed_factor_clk_setup);
CLK_OF_DECLARE(fixed_factor_clk, "mtk_fixed-factor-clock",
		mtk_of_fixed_factor_clk_setup);
#endif
#endif
