/*
 * QEMU IRQ/GPIO common code.
 *
 * Copyright (c) 2007 CodeSourcery.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu-common.h"
#include "irq.h"
#include "typeinfo.h"

#define IRQ(obj) obj

struct IRQState {
    VeertuType parent;

    vmx_irq_handler handler;
    void *opaque;
    int n;
};

void vmx_set_irq(vmx_irq irq, int level)
{
    if (!irq)
        return;

    irq->handler(irq->opaque, irq->n, level);
}

vmx_irq *vmx_extend_irqs(vmx_irq *old, int n_old, vmx_irq_handler handler,
                           void *opaque, int n)
{
    vmx_irq *s;
    int i;

    if (!old) {
        n_old = 0;
    }
    s = old ? g_renew(vmx_irq, old, n + n_old) : g_new(vmx_irq, n);
    for (i = n_old; i < n + n_old; i++) {
        s[i] = vmx_allocate_irq(handler, opaque, i);
    }
    return s;
}

vmx_irq *vmx_allocate_irqs(vmx_irq_handler handler, void *opaque, int n)
{
    return vmx_extend_irqs(NULL, 0, handler, opaque, n);
}

vmx_irq vmx_allocate_irq(vmx_irq_handler handler, void *opaque, int n)
{
    struct IRQState *irq;

    irq = IRQ(vtype_new(TYPE_IRQ));
    irq->handler = handler;
    irq->opaque = opaque;
    irq->n = n;

    return irq;
}

void vmx_free_irqs(vmx_irq *s, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        vmx_free_irq(s[i]);
    }
    g_free(s);
}

void vmx_free_irq(vmx_irq irq)
{
}

static void vmx_notirq(void *opaque, int line, int level)
{
    struct IRQState *irq = opaque;

    irq->handler(irq->opaque, irq->n, !level);
}

vmx_irq vmx_irq_invert(vmx_irq irq)
{
    /* The default state for IRQs is low, so raise the output now.  */
    vmx_irq_raise(irq);
    return vmx_allocate_irq(vmx_notirq, irq, 0);
}

static void vmx_splitirq(void *opaque, int line, int level)
{
    struct IRQState **irq = opaque;
    irq[0]->handler(irq[0]->opaque, irq[0]->n, level);
    irq[1]->handler(irq[1]->opaque, irq[1]->n, level);
}

vmx_irq vmx_irq_split(vmx_irq irq1, vmx_irq irq2)
{
    vmx_irq *s = g_malloc0(2 * sizeof(vmx_irq));
    s[0] = irq1;
    s[1] = irq2;
    return vmx_allocate_irq(vmx_splitirq, s, 0);
}

static void proxy_irq_handler(void *opaque, int n, int level)
{
    vmx_irq **target = opaque;

    if (*target) {
        vmx_set_irq((*target)[n], level);
    }
}

vmx_irq *vmx_irq_proxy(vmx_irq **target, int n)
{
    return vmx_allocate_irqs(proxy_irq_handler, target, n);
}

void vmx_irq_intercept_in(vmx_irq *gpio_in, vmx_irq_handler handler, int n)
{
    int i;
    vmx_irq *old_irqs = vmx_allocate_irqs(NULL, NULL, n);
    for (i = 0; i < n; i++) {
        *old_irqs[i] = *gpio_in[i];
        gpio_in[i]->handler = handler;
        gpio_in[i]->opaque = &old_irqs[i];
    }
}

static const VeertuTypeInfo irq_type_info = {
   .name = TYPE_IRQ,
   .parent = VtypeBase,
   .instance_size = sizeof(struct IRQState),
};

void irq_register_types(void)
{
    register_type_internal(&irq_type_info);
}
