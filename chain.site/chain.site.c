/**
 @file
 chain.test - a simple chain_site object
 
 @ingroup    maxchain
 */

#include "ext.h"
#include "ext_common.h"
#include "ext_obex.h"
#include "ext_time.h"
#include "ext_itm.h"

typedef struct chain_site
{
    t_object s_obj;
    t_systhread s_systhread;
    t_systhread_mutex s_mutex;
    int s_systhread_cancel;
    void *s_qelem;
    void *s_outlet;
    int s_sleeptime;
    int s_value;
} t_chain_site;

void *chain_site_new(t_symbol *s, long argc, t_atom *argv);
void chain_site_free(t_chain_site *x);
void chain_site_int(t_chain_site *x, long n);
void chain_site_bang(t_chain_site *x);
void chain_site_sleeptime(t_chain_site *x, long sleeptime);
void chain_site_stop(t_chain_site *x);
void *chain_site_threadproc(t_chain_site *x);
void chain_site_qfn(t_chain_site *x);

static t_class *s_chain_site_class = NULL;

int C74_EXPORT main(void)
{
    t_class *c;
    
    c = class_new("chain.site", (method)chain_site_new, (method)chain_site_free, sizeof(t_chain_site), (method)0L, A_GIMME, 0);
    
    class_addmethod(c, (method)chain_site_bang, "bang", 0);
    class_addmethod(c, (method)chain_site_int, "int", A_LONG, 0);
    class_addmethod(c, (method)chain_site_sleeptime, "sleeptime", A_DEFLONG, 0);
    class_addmethod(c, (method)chain_site_stop, "cancel", 0);
    
    class_register(CLASS_BOX, c);
    
    s_chain_site_class = c;
    
    return 0;
}

void *chain_site_new(t_symbol *s, long argc, t_atom *argv)
{
    t_chain_site *x = (t_chain_site *)object_alloc(s_chain_site_class);
    
    x->s_outlet = outlet_new(x, NULL);
    x->s_qelem = qelem_new(x, (method)chain_site_qfn);
    x->s_systhread = NULL;
    systhread_mutex_new(&x->s_mutex,0);
    x->s_sleeptime = 1000;
    x->s_value = 0;
    
    return x;
}

void chain_site_free(t_chain_site *x)
{
    chain_site_stop(x);

    if (x->s_qelem)
        qelem_free(x->s_qelem);

    if (x->s_mutex)
        systhread_mutex_free(x->s_mutex);
}

void chain_site_sleeptime(t_chain_site *x, long sleeptime)
{
    if (sleeptime<10)
        sleeptime = 10;
    x->s_sleeptime = sleeptime;
}

void chain_site_int(t_chain_site *x, long n)
{
    systhread_mutex_lock(x->s_mutex);
    x->s_value = (int)n;
    systhread_mutex_unlock(x->s_mutex);
}

void chain_site_bang(t_chain_site *x)
{
    chain_site_stop(x);

    if (x->s_systhread == NULL) {
        post("Starting a new thread");
        systhread_create((method) chain_site_threadproc, x, 0, 0, 0, &x->s_systhread);
    }

}

void chain_site_stop(t_chain_site *x)
{
    unsigned int ret;

    if (x->s_systhread) {
        post("stopping our thread");
        x->s_systhread_cancel = true;
        systhread_join(x->s_systhread, &ret);
        x->s_systhread = NULL;
    }
}

void chain_site_qfn(t_chain_site *x){

    int myValue;

    systhread_mutex_lock(x->s_mutex);
    myValue = x->s_value;
    systhread_mutex_unlock(x->s_mutex);

    outlet_int(x->s_outlet, myValue);
}

void *chain_site_threadproc(t_chain_site *x)
{
    while(1){
        if (x->s_systhread_cancel)
            break;

        systhread_mutex_lock(x->s_mutex);
        x->s_value++;
        systhread_mutex_unlock(x->s_mutex);

        qelem_set(x->s_qelem);

        systhread_sleep(x->s_sleeptime);
    }

    x->s_systhread_cancel = false;

    systhread_exit(0);
    return NULL;
}
