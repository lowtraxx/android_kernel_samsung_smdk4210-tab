/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include "drmP.h"
#include "drm_backlight.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/cma.h>
#include <plat/map-base.h>

#include <drm/exynos_drm.h>
#include "exynos_drm_drv.h"
#include "exynos_drm_gem.h"
#include "exynos_drm_ipp.h"

/*
 * IPP is stand for Image Post Processing and
 * supports image scaler/rotator and input/output DMA operations.
 * using FIMC, GSC, Rotator, so on.
 * IPP is integration device driver of same attribute h/w
 */

/*
 * A structure of event.
 *
 * @base: base of event.
 * @event: ipp event.
 */
struct drm_exynos_ipp_send_event {
	struct drm_pending_event	base;
	struct drm_exynos_ipp_event	event;
};

/*
 * A structure of map node.
 *
 * @list: list head.
 * @ops_id: operations id;
 * @id: buffer id.
 * @buf_info: gem handle and dma address, size.
 */
struct drm_exynos_ipp_map_node {
	struct list_head	list;
	enum drm_exynos_ops_id	ops_id;
	u32	id;
	struct drm_exynos_ipp_buf_info buf_info;
};

/*
 * A structure of ipp context.
 *
 * @subdrv: prepare initialization using subdrv.
 * @lock: locking of operations.
 */
/* ToDo: Removed context */
struct ipp_context {
	struct exynos_drm_subdrv subdrv;
	struct mutex	lock;
};

static LIST_HEAD(exynos_drm_ippdrv_list);
static BLOCKING_NOTIFIER_HEAD(exynos_drm_ippnb_list);

int exynos_drm_ippdrv_register(struct exynos_drm_ippdrv *ippdrv)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ippdrv)
		return -EINVAL;

	list_add_tail(&ippdrv->list, &exynos_drm_ippdrv_list);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_drm_ippdrv_register);

int exynos_drm_ippdrv_unregister(struct exynos_drm_ippdrv *ippdrv)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ippdrv)
		return -EINVAL;

	list_del(&ippdrv->list);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_drm_ippdrv_unregister);

int exynos_drm_ipp_property(struct drm_device *drm_dev, void *data,
					struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_property *property = data;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!property) {
		DRM_ERROR("invalid property parameter.\n");
		return -EINVAL;
	}

	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, list) {
		/* in used status */
		if (ippdrv->used)
			continue;

		/* check property */
		if (ippdrv->check_property &&
		    ippdrv->check_property(ippdrv->dev, property)) {
			DRM_DEBUG_KMS("not support property.\n");
			continue;
		}

		/* stored property information and ippdrv in private data*/
		ippdrv->property = *property;
		ippdrv->used = true;
		priv->ippdrv = ippdrv;

		return 0;
	}

	if (!ippdrv)
		DRM_ERROR("failed to get ipp driver.\n");

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos_drm_ipp_property);

static int ipp_make_event(struct drm_device *drm_dev, struct drm_file *file,
	struct exynos_drm_ippdrv *ippdrv, struct drm_exynos_ipp_buf *buf)
{
	struct drm_exynos_ipp_send_event *e;
	unsigned long flags;

	DRM_DEBUG_KMS("%s:ops_id[%d]buf_idx[%d]\n", __func__,
		buf->ops_id, buf->id);

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e) {
		DRM_ERROR("failed to allocate event.\n");

		spin_lock_irqsave(&drm_dev->event_lock, flags);
		file->event_space += sizeof(e->event);
		spin_unlock_irqrestore(&drm_dev->event_lock, flags);
		return -ENOMEM;
	}

	DRM_DEBUG_KMS("%s:e[0x%x]\n", __func__, (int)e);

	e->event.base.type = DRM_EXYNOS_IPP_EVENT;
	e->event.base.length = sizeof(e->event);
	e->event.user_data = buf->user_data;
	e->event.buf_idx = buf->id;
	e->base.event = &e->event.base;
	e->base.file_priv = file;
	e->base.destroy = (void (*) (struct drm_pending_event *)) kfree;

	list_add_tail(&e->base.link, &ippdrv->event_list);

	return 0;
}

int exynos_drm_ipp_buf(struct drm_device *drm_dev, void *data,
					struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct exynos_drm_ippdrv *ippdrv = priv->ippdrv;
	struct drm_exynos_ipp_buf *buf = data;
	struct exynos_drm_ipp_ops *ops = NULL;
	struct drm_exynos_ipp_send_event *e, *te;
	struct drm_exynos_ipp_map_node *node = NULL, *tnode;
	struct drm_exynos_ipp_buf_info buf_info;
	void *addr;
	unsigned long size;
	int ret, i;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!buf) {
		DRM_ERROR("invalid buf parameter.\n");
		return -EINVAL;
	}

	if (!ippdrv) {
		DRM_ERROR("failed to get ipp driver.\n");
		return -EINVAL;
	}

	if (buf->ops_id >= EXYNOS_DRM_OPS_MAX) {
		DRM_ERROR("invalid ops parameter.\n");
		return -EINVAL;
	}

	ops = ippdrv->ops[buf->ops_id];
	if (!ops) {
		DRM_ERROR("failed to get ops.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("%s:ops_id[%s]buf_idx[%d]buf_ctrl[%d]\n",
		__func__, buf->ops_id ? "dst" : "src",
		buf->id, buf->buf_ctrl);

	/* clear base address for error handling */
	memset(&buf_info, 0x0, sizeof(buf_info));

	/* buffer control */
	switch (buf->buf_ctrl) {
	case IPP_BUF_CTRL_MAP:
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			DRM_ERROR("failed to allocate map node.\n");
			return -ENOMEM;
		}

		/* operations, buffer id */
		node->ops_id = buf->ops_id;
		node->id = buf->id;

		DRM_DEBUG_KMS("%s:node[0x%x]\n", __func__, (int)node);

		for (i = 0; i < EXYNOS_DRM_PLANAR_MAX; i++) {
			DRM_DEBUG_KMS("%s:i[%d]handle[0x%x]\n", __func__,
				i, buf->handle[i]);

			if (buf->handle[i] != 0) {
				addr = exynos_drm_gem_get_dma_addr(drm_dev,
							buf->handle[i], file);
				if (!addr) {
					DRM_ERROR("failed to get addr.\n");
					ret = -EFAULT;
					goto err_clear;
				}

				size = exynos_drm_gem_get_size(drm_dev,
							buf->handle[i], file);
				if (!size) {
					DRM_ERROR("failed to get size.\n");
					ret = -EFAULT;
					goto err_clear;
				}

				buf_info.handle[i] = buf->handle[i];
				buf_info.base[i] = *(dma_addr_t *) addr;
				buf_info.size[i] = (uint64_t) size;
			}
		}

		node->buf_info = buf_info;
		list_add_tail(&node->list, &ippdrv->map_list);
		break;
	case IPP_BUF_CTRL_UNMAP:
		/* free node */
		list_for_each_entry_safe(node, tnode,
			&ippdrv->map_list, list) {
			if (node->id == buf->id &&
				node->ops_id == buf->ops_id) {
				list_del(&node->list);
				kfree(node);
			}
		}

		for (i = 0; i < EXYNOS_DRM_PLANAR_MAX; i++) {
			DRM_DEBUG_KMS("%s:i[%d]handle[0x%x]\n", __func__,
				i, buf->handle[i]);

			if (buf->handle[i] != 0)
				exynos_drm_gem_put_dma_addr(drm_dev,
							buf->handle[i], file);
		}

		if (pm_runtime_suspended(ippdrv->dev)) {
			DRM_ERROR("suspended:invalid operations.\n");
			return -EINVAL;
		}

		/* clear address */
		if (ops->set_addr) {
			ret = ops->set_addr(ippdrv->dev, &buf_info, buf->id,
				buf->buf_ctrl);
			if (ret) {
				DRM_ERROR("failed to set addr.\n");
				goto err_clear;
			}
		}
		break;
	case IPP_BUF_CTRL_QUEUE:
	case IPP_BUF_CTRL_DEQUEUE:
		if (pm_runtime_suspended(ippdrv->dev)) {
			DRM_ERROR("suspended:invalid operations.\n");
			return -EINVAL;
		}

		/* set address sequence and enable irq */
		if (ops->set_addr) {
			ret = ops->set_addr(ippdrv->dev, NULL, buf->id,
				buf->buf_ctrl);
			if (ret) {
				DRM_ERROR("failed to set addr.\n");
				goto err_clear;
			}
		}
		break;
	default:
		DRM_ERROR("invalid buffer control.\n");
		return -EINVAL;
	}

	/* event control */
	if (buf->ops_id == EXYNOS_DRM_OPS_DST) {
		switch (buf->buf_ctrl) {
		case IPP_BUF_CTRL_MAP:
		case IPP_BUF_CTRL_QUEUE:
			/* make event */
			ret = ipp_make_event(drm_dev, file, ippdrv, buf);
			if (ret) {
				DRM_ERROR("failed to make event.\n");
				goto err_clear;
			}
			break;
		case IPP_BUF_CTRL_UNMAP:
		case IPP_BUF_CTRL_DEQUEUE:
			/* free event */
			list_for_each_entry_safe(e, te,
				&ippdrv->event_list, base.link) {
				if (e->event.buf_idx == buf->id) {
					/* delete list */
					list_del(&e->base.link);
					kfree(e);
				}
			}
			break;
		default:
			/* no action */
			break;
		}
	}

	return 0;

err_clear:
	DRM_ERROR("%s:failed to set buf.\n", __func__);

	/* delete list */
	list_for_each_entry_safe(node, tnode, &ippdrv->map_list, list) {
		if (node->id == buf->id &&
			node->ops_id == buf->ops_id) {
			list_del(&node->list);
			kfree(node);
		}
	}

	/* put gem buffer */
	for (i = 0; i < EXYNOS_DRM_PLANAR_MAX; i++)
		if (buf_info.base[i] != 0)
			exynos_drm_gem_put_dma_addr(drm_dev,
						buf->handle[i], file);

	/* free address */
	switch (buf->buf_ctrl) {
	case IPP_BUF_CTRL_UNMAP:
	case IPP_BUF_CTRL_QUEUE:
	case IPP_BUF_CTRL_DEQUEUE:
		if (pm_runtime_suspended(ippdrv->dev)) {
			DRM_ERROR("suspended:invalid error operations.\n");
			return -EINVAL;
		}

		/* clear base address for error handling */
		memset(&buf_info, 0x0, sizeof(buf_info));

		/* don't need check error case */
		if (ops->set_addr)
			ops->set_addr(ippdrv->dev, &buf_info,
				buf->id, IPP_BUF_CTRL_UNMAP);
		break;
	default:
		/* no action */
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_drm_ipp_buf);

static int ipp_set_property(struct exynos_drm_ippdrv *ippdrv)
{
	struct drm_exynos_ipp_property *property =
		&ippdrv->property;
	struct exynos_drm_ipp_ops *ops = NULL;
	int ret, i, swap = 0;

	/* reset h/w block */
	if (ippdrv->reset &&
		ippdrv->reset(ippdrv->dev)) {
		DRM_ERROR("failed to reset.\n");
		return -EINVAL;
	}

	/* set source,destination operations */
	for (i = 0; i < EXYNOS_DRM_OPS_MAX; i++) {
		/* ToDo: integrate property and config */
		struct drm_exynos_ipp_config *config =
			&property->config[i];

		ops = ippdrv->ops[i];
		if (!ops || !config) {
			DRM_ERROR("not support ops and config.\n");
			return -EINVAL;
		}

		/* set format */
		if (ops->set_fmt) {
			ret = ops->set_fmt(ippdrv->dev, config->fmt);
			if (ret) {
				DRM_ERROR("not support format.\n");
				return ret;
			}
		}

		/* set transform for rotation, flip */
		if (ops->set_transf) {
			swap = ops->set_transf(ippdrv->dev,
				config->degree, config->flip);
			if (swap < 0) {
				DRM_ERROR("not support tranf.\n");
				return -EINVAL;
			}
		}

		/* set size */
		if (ops->set_size) {
			ret = ops->set_size(ippdrv->dev, swap,
				&config->pos, &config->sz);
			if (ret) {
				DRM_ERROR("not support size.\n");
				return ret;
			}
		}
	}

	return 0;
}

int exynos_drm_ipp_ctrl(struct drm_device *drm_dev, void *data,
					struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct exynos_drm_ippdrv *ippdrv = priv->ippdrv;
	struct drm_exynos_ipp_ctrl *ctrl = data;
	struct exynos_drm_ipp_ops *ops = NULL;
	int ret;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ctrl) {
		DRM_ERROR("invalid control parameter.\n");
		return -EINVAL;
	}

	if (!ippdrv) {
		DRM_ERROR("failed to get ipp driver.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("%s:use[%d]cmd[%d]\n", __func__,
		ctrl->use, ctrl->cmd);

	/*
	 * start/stop operations,
	 * set use to 1, you can use start operations
	 * other case is stop opertions
	 */
	if (ctrl->use) {
		struct drm_exynos_ipp_map_node *node, *t_node;
		int count;

		if (pm_runtime_suspended(ippdrv->dev))
			pm_runtime_get_sync(ippdrv->dev);

		ret = ipp_set_property(ippdrv);
		if (ret) {
			DRM_ERROR("failed to set property.\n");
			goto err_clear;
		}

		count = 0;
		list_for_each_entry_safe(node, t_node,
			&ippdrv->map_list, list) {
			if (node) {
				DRM_DEBUG_KMS("%s:count[%d]node[0x%x]\n",
					__func__, count++, (int)node);

				ops = ippdrv->ops[node->ops_id];
				if (!ops) {
					DRM_DEBUG_KMS("not support ops.\n");
					goto err_clear;
				}

				/* set address and enable irq */
				if (ops->set_addr) {
					ret = ops->set_addr(ippdrv->dev,
						&node->buf_info, node->id,
						IPP_BUF_CTRL_MAP);
					if (ret) {
						DRM_ERROR("failed set addr.\n");
						goto err_clear;
					}
				}
			}
		}

		/* start operations */
		if (ippdrv->start) {
			ret = ippdrv->start(ippdrv->dev, ctrl->cmd);
			if (ret) {
				DRM_ERROR("failed to start operations.\n");
				ret = -EIO;
				goto err_clear;
			}
		}
	} else {
		ippdrv->used = false;

		/* stop operations */
		if (ippdrv->stop)
			ippdrv->stop(ippdrv->dev, ctrl->cmd);

		if (!pm_runtime_suspended(ippdrv->dev))
			pm_runtime_put_sync(ippdrv->dev);
	}

	return 0;

err_clear:
	/*
	 * ToDo: register clear if needed
	 * If failed choose device using property. then
	 * revert register clearing if needed
	 */

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_drm_ipp_ctrl);

int exynos_drm_ippnb_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
			&exynos_drm_ippnb_list, nb);
}
EXPORT_SYMBOL_GPL(exynos_drm_ippnb_register);

int exynos_drm_ippnb_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
			&exynos_drm_ippnb_list, nb);
}
EXPORT_SYMBOL_GPL(exynos_drm_ippnb_unregister);

int exynos_drm_ippnb_send_event(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(
			&exynos_drm_ippnb_list, val, v);
}
EXPORT_SYMBOL_GPL(exynos_drm_ippnb_send_event);

void ipp_send_event_handler(struct exynos_drm_ippdrv *ippdrv,
	int buf_idx)
{
	struct drm_device *drm_dev = ippdrv->drm_dev;
	struct drm_exynos_ipp_send_event *e;
	struct timeval now;
	unsigned long flags;

	DRM_DEBUG_KMS("%s:buf_idx[%d]\n", __func__, buf_idx);

	if (!drm_dev) {
		DRM_ERROR("failed to get drm_dev.\n");
		return;
	}

	if (list_empty(&ippdrv->event_list)) {
		DRM_ERROR("event list is empty.\n");
		return;
	}

	e = list_first_entry(&ippdrv->event_list,
			     struct drm_exynos_ipp_send_event, base.link);

	do_gettimeofday(&now);
	e->event.tv_sec = now.tv_sec;
	e->event.tv_usec = now.tv_usec;
	/* ToDo: compare buf index. If needed */
	e->event.buf_idx = buf_idx;

	spin_lock_irqsave(&drm_dev->event_lock, flags);
	list_move_tail(&e->base.link, &e->base.file_priv->event_list);
	wake_up_interruptible(&e->base.file_priv->event_wait);
	spin_unlock_irqrestore(&drm_dev->event_lock, flags);
}

static int ipp_subdrv_probe(struct drm_device *drm_dev, struct device *dev)
{
	struct exynos_drm_ippdrv *ippdrv;

	DRM_DEBUG_KMS("%s\n", __func__);

	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, list)
		ippdrv->drm_dev = drm_dev;

	return 0;
}

static void ipp_subdrv_remove(struct drm_device *drm_dev)
{
	struct exynos_drm_ippdrv *ippdrv;

	DRM_DEBUG_KMS("%s\n", __func__);

	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, list) {
		ippdrv->drm_dev = NULL;
		exynos_drm_ippdrv_unregister(ippdrv);
	}

	/* ToDo: free notifier callback list if needed */
}

static int ipp_subdrv_open(struct drm_device *drm_dev, struct device *dev,
							struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv;
	struct exynos_drm_ippdrv *ippdrv;
	int ret, count = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* ToDo: multi device open */
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		DRM_ERROR("failed to allocate priv.\n");
		return -ENOMEM;
	}

	file_priv->ipp_priv = priv;
	INIT_LIST_HEAD(&priv->event_list);
	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, list) {
		DRM_DEBUG_KMS("%s:count[%d]ippdrv[0x%x]\n", __func__,
			count++, (int)ippdrv);

		/* in used status */
		if (ippdrv->used)
			continue;

		INIT_LIST_HEAD(&ippdrv->event_list);
		INIT_LIST_HEAD(&ippdrv->map_list);
		list_splice_init(&priv->event_list, &ippdrv->event_list);

		if (ippdrv->open) {
			ret = ippdrv->open(drm_dev, ippdrv->dev, file);
			if (ret)
				goto err_clear;
		}
	}

	return 0;

err_clear:
	list_for_each_entry_reverse(ippdrv, &ippdrv->list, list) {
		/* in used status */
		if (ippdrv->used)
			continue;

		if (ippdrv->close)
			ippdrv->close(drm_dev, ippdrv->dev, file);
	}

	return ret;
}

static void ipp_subdrv_close(struct drm_device *drm_dev, struct device *dev,
							struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct exynos_drm_ippdrv *ippdrv_cur = priv->ippdrv;
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_send_event *e, *te;
	struct drm_exynos_ipp_map_node *node, *tnode;
	int i, count = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* ToDo: for multi device close */
	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, list) {
		DRM_DEBUG_KMS("%s:ippdrv[0x%x]\n", __func__,
			(int)ippdrv);

		/* current used ippdrv stop needed */
		if (ippdrv_cur && ippdrv_cur == ippdrv &&
			ippdrv->used) {

			if (ippdrv->stop)
				ippdrv->stop(ippdrv->dev, ippdrv->cmd);

			if (!pm_runtime_suspended(ippdrv->dev))
				pm_runtime_put_sync(ippdrv->dev);

			ippdrv->used = false;
		}

		/* in used status */
		if (ippdrv->used)
			continue;

		if (ippdrv->close)
			ippdrv->close(drm_dev, ippdrv->dev, file);

		/* free event */
		count = 0;
		list_for_each_entry_safe(e, te,
			&ippdrv->event_list, base.link) {
			DRM_DEBUG_KMS("%s:count[%d]e[0x%x]\n",
				__func__, count++, (int)e);

			/* delete list */
			list_del(&e->base.link);
			kfree(e);
		}

		/* free node */
		count = 0;
		list_for_each_entry_safe(node, tnode,
			&ippdrv->map_list, list) {
			DRM_DEBUG_KMS("%s:count[%d]node[0x%x]\n",
				__func__, count++, (int)node);

			/* put gem buffer */
			for (i = 0; i < EXYNOS_DRM_PLANAR_MAX; i++)
				if (node->buf_info.handle[i] != 0)
					exynos_drm_gem_put_dma_addr(drm_dev,
						node->buf_info.handle[i], file);

			/* delete list */
			list_del(&node->list);
			kfree(node);
		}
	}

	kfree(file_priv->ipp_priv);

	return;
}

int exynos_drm_ipp_init(struct drm_device *dev)
{
	struct ipp_context *ctx;
	struct exynos_drm_subdrv *subdrv;
	int ret = -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* ToDo: Removed mutex if possible */
	mutex_init(&ctx->lock);

	subdrv = &ctx->subdrv;
	subdrv->dev = dev->dev;
	subdrv->probe = ipp_subdrv_probe;
	subdrv->remove = ipp_subdrv_remove;
	subdrv->open = ipp_subdrv_open;
	subdrv->close = ipp_subdrv_close;

	ret = exynos_drm_subdrv_register(subdrv);
	if (ret < 0) {
		DRM_ERROR("failed to register drm ipp device.\n");
		goto err_clear;
	}

	return 0;

err_clear:
	kfree(ctx);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_drm_ipp_init);

/* ToDo: Remove this api */
void exynos_drm_ipp_fini(struct drm_device *dev)
{
	DRM_DEBUG_KMS("%s\n", __func__);
}
EXPORT_SYMBOL_GPL(exynos_drm_ipp_fini);
