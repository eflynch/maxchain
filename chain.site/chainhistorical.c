#include "chainhistorical.h"

#include "jansson.h"

#include "chainevent.h"
#include "messages.h"
#include "requests.h"
#include "chainquery.h"
#include "queries.h"


#define LOOKAHEAD_TIME 1000
#define CHUNK_LENGTH 2000
#define PROCESS_SLEEP_TIME 100
#define LOOKAHEAD_SLEEP_TIME 100


int chain_historical_process(t_chain_site *x){
    t_pseudo_clk *clk = x->s_historical_clk;
    pri_queue q = x->s_historical_pq;
    int pri;
    t_chain_event *e;
    while(1){
        if (x->s_play_cancel){
           return 0; 
        }
        systhread_mutex_lock(x->s_historical_mutex);
        e = (t_chain_event *)priq_pop(q, &pri);
        systhread_mutex_unlock(x->s_historical_mutex);
        if(!e){
            systhread_sleep(PROCESS_SLEEP_TIME);
            continue;
        }
        if (pseudo_now(clk) < e->s_time){
            systhread_mutex_lock(x->s_historical_mutex);
            priq_push(q, (void *)e, pri);
            systhread_mutex_unlock(x->s_historical_mutex);
            systhread_sleep(PROCESS_SLEEP_TIME);
            continue;
        } else {
            chain_info("Popped: %d", e->s_time);
            int err = chain_site_update_sensors(x, e->s_href, e->s_timestamp, e->s_value);
            chain_free_event(e);
            if (err){
                return 1;
            }
        }
    }
}

void chain_historical_lookahead(t_chain_site *x){
    time_t highest_time_checked = x->s_historical_start;
    t_pseudo_clk *clk = x->s_historical_clk;

    while(1)
    {
        if (pseudo_now(clk) >= local_now()){
            break;
        }
        if (pseudo_now(clk) < highest_time_checked - LOOKAHEAD_TIME){
            systhread_sleep(LOOKAHEAD_SLEEP_TIME);
            continue;
        }
        if(x->s_historical_cancel){
            return;
        }
        time_t start = highest_time_checked;
        time_t end = highest_time_checked + CHUNK_LENGTH;

        t_db_result *db_result = NULL;
        query_list_sensors(x->s_db, &db_result);
        long num_records = db_result_numrecords(db_result);

        // query list of sensors
        for (int i=0; i < num_records; i++){
            const char *url = db_result_string(db_result, i, 0);
            long data_len;
            long *s_times;
            double *data;
            chain_get_data(url, (long) start, (long) end, &data, &s_times, &data_len);
            for (int j=0; j<data_len; j++){
                long datum = data[j];
                long s_time = s_times[j];
                t_chain_event *e = chain_new_event(s_time, url, "NOT IMPLEMENTED", datum);
                chain_info("Pushed: %d", s_time);
                systhread_mutex_lock(x->s_historical_mutex);
                priq_push(x->s_historical_pq, (void *)e, (int) s_time);
                systhread_mutex_unlock(x->s_historical_mutex);
            }

            free(s_times);
            free(data);
        }
            
        highest_time_checked = end;
    }
}
