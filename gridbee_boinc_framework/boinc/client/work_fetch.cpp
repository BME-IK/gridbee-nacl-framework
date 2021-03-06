// This file is part of BOINC.
// http://boinc.berkeley.edu
// Copyright (C) 2008 University of California
//
// BOINC is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// BOINC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with BOINC.  If not, see <http://www.gnu.org/licenses/>.

#include "cpp.h"

#ifdef _WIN32
#include "boinc_win.h"
#else
#include "config.h"
#endif

#include "util.h"

#include "client_state.h"
#include "client_msgs.h"

#include "scheduler_op.h"
#include "work_fetch.h"

using std::vector;

bool use_rec = true;
bool use_hyst_fetch = true;

RSC_WORK_FETCH rsc_work_fetch[MAX_RSC];
WORK_FETCH work_fetch;

#define FETCH_IF_IDLE_INSTANCE          0
    // If resource has an idle instance,
    // get work for it from the project with greatest LTD,
    // even if it's overworked.
#define FETCH_IF_MAJOR_SHORTFALL        1
    // If resource is saturated for less than work_buf_min(),
    // get work for it from the project with greatest LTD,
    // even if it's overworked.
#define FETCH_IF_MINOR_SHORTFALL        2
    // If resource is saturated for less than work_buf_total(),
    // get work for it from the non-overworked project with greatest LTD.
#define FETCH_IF_PROJECT_STARVED        3
    // If any project is not overworked and has too few jobs
    // to use its instance share,
    // get work from the one with greatest LTD.

static const char* criterion_name(int criterion) {
    switch (criterion) {
    case FETCH_IF_IDLE_INSTANCE: return "idle instance";
    case FETCH_IF_MAJOR_SHORTFALL: return "major shortfall";
    case FETCH_IF_MINOR_SHORTFALL: return "minor shortfall";
    case FETCH_IF_PROJECT_STARVED: return "starved";
    }
    return "unknown";
}

inline bool dont_fetch(PROJECT* p, int rsc_type) {
    if (p->no_rsc_pref[rsc_type]) return true;
    if (p->no_rsc_config[rsc_type]) return true;
    if (p->no_rsc_apps[rsc_type]) return true;
    if (p->no_rsc_ams[rsc_type]) return true;
    return false;
}

// if the configuration file disallows the use of a GPU type
// for a project, set a flag to that effect
//
void set_no_rsc_config() {
    for (unsigned int i=0; i<gstate.projects.size(); i++) {
        PROJECT& p = *gstate.projects[i];
        for (int j=1; j<coprocs.n_rsc; j++) {
            bool allowed[MAX_COPROC_INSTANCES];
            memset(allowed, 0, sizeof(allowed));
            COPROC& c = coprocs.coprocs[j];
            for (int k=0; k<c.count; k++) {
                allowed[c.device_nums[k]] = true;
            }
            for (unsigned int k=0; k<config.exclude_gpus.size(); k++) {
                EXCLUDE_GPU& e = config.exclude_gpus[k];
                if (strcmp(e.url.c_str(), p.master_url)) continue;
                if (!e.type.empty() && strcmp(e.type.c_str(), c.type)) continue;
                if (!e.appname.empty()) continue;
                if (e.device_num < 0) {
                    memset(allowed, 0, sizeof(allowed));
                    break;
                }
                allowed[e.device_num] = false;
            }
            p.no_rsc_config[j] = true;
            for (int k=0; k<c.count; k++) {
                if (allowed[k]) {
                    p.no_rsc_config[j] = false;
                    break;
                }
            }
        }
    }
}

// does the project have a downloading or runnable job?
//
static bool has_a_job(PROJECT* p) {
    for (unsigned int j=0; j<gstate.results.size(); j++) {
        RESULT* rp = gstate.results[j];
        if (rp->project != p) continue;
        if (rp->state() <= RESULT_FILES_DOWNLOADED) {
            return true;
        }
    }
    return false;
}

inline bool has_coproc_app(PROJECT* p, int rsc_type) {
    unsigned int i;
    for (i=0; i<gstate.app_versions.size(); i++) {
        APP_VERSION* avp = gstate.app_versions[i];
        if (avp->project != p) continue;
        if (avp->gpu_usage.rsc_type == rsc_type) return true;
    }
    return false;
}

///////////////  RSC_PROJECT_WORK_FETCH  ///////////////

bool RSC_PROJECT_WORK_FETCH::compute_may_have_work(PROJECT* p, int rsc_type) {
    if (dont_fetch(p, rsc_type)) return false;
    if (p->rsc_defer_sched[rsc_type]) return false;
    return (backoff_time < gstate.now);
}

void RSC_PROJECT_WORK_FETCH::rr_init(PROJECT* p, int rsc_type) {
    may_have_work = compute_may_have_work(p, rsc_type);
    runnable_share = 0;
    fetchable_share = 0;
    has_runnable_jobs = false;
    sim_nused = 0;
    nused_total = 0;
    deadlines_missed = 0;
}

//#ifndef USE_REC
// see if the project's debt is beyond what would normally happen;
// if so we conclude that it had a long job that ran in EDF mode;
// avoid asking it for work unless absolutely necessary.
//
bool RSC_PROJECT_WORK_FETCH::overworked() {
    double x = gstate.work_buf_total() + gstate.global_prefs.cpu_scheduling_period(); 
    if (x < 86400) x = 86400;
    return (long_term_debt < -x);
}

// should this project be accumulating LTD for this resource?
//
bool RSC_PROJECT_WORK_FETCH::debt_eligible(PROJECT* p, RSC_WORK_FETCH& rwf) {
    if (p->non_cpu_intensive) return false;
    if (p->suspended_via_gui) return false;
    if (p->some_result_suspended()) return false;
    if (has_runnable_jobs) return true;
        // must precede the done_request_more_work check
    if (p->dont_request_more_work) return false;
    if (backoff_time > gstate.now) return false;
    if (dont_fetch(p, rwf.rsc_type)) return false;

    // NOTE: it's critical that all conditions that might prevent
    // us from asking the project for work of this type
    // be included in the above list.
    // Otherwise we might get in a state where debt accumulates,
    // pushing other projects into overworked state

    // The last time we asked for work we didn't get any,
    // but it's been a while since we asked.
    // In this case, accumulate debt until we reach (around) zero, then stop.
    //
    if (backoff_interval == WF_MAX_BACKOFF_INTERVAL) {
        if (long_term_debt > -DEBT_ADJUST_PERIOD) {
            return false;
        }
    }
    if (p->min_rpc_time > gstate.now) return false;
    return true;
}
//#endif

void RSC_PROJECT_WORK_FETCH::backoff(PROJECT* p, const char* name) {
    if (backoff_interval) {
        backoff_interval *= 2;
        if (backoff_interval > WF_MAX_BACKOFF_INTERVAL) backoff_interval = WF_MAX_BACKOFF_INTERVAL;
    } else {
        backoff_interval = WF_MIN_BACKOFF_INTERVAL;
    }
    double x = (.5 + drand())*backoff_interval;
    backoff_time = gstate.now + x;
    if (log_flags.work_fetch_debug) {
        msg_printf(p, MSG_INFO,
            "[work_fetch] backing off %s %.0f sec", name, x
        );
    }
}

///////////////  RSC_WORK_FETCH  ///////////////

RSC_PROJECT_WORK_FETCH& RSC_WORK_FETCH::project_state(PROJECT* p) {
    return p->rsc_pwf[rsc_type];
}

bool RSC_WORK_FETCH::may_have_work(PROJECT* p) {
    if (dont_fetch(p, rsc_type)) return false;
    RSC_PROJECT_WORK_FETCH& w = project_state(p);
    return (w.backoff_time < gstate.now);
}

void RSC_WORK_FETCH::rr_init() {
    shortfall = 0;
    nidle_now = 0;
    sim_nused = 0;
    total_fetchable_share = 0;
    total_runnable_share = 0;
    deadline_missed_instances = 0;
    saturated_time = 0;
    pending.clear();
    busy_time_estimator.reset();
}

void RSC_WORK_FETCH::accumulate_shortfall(double d_time) {
    double idle = ninstances - sim_nused;
    if (idle > 1e-6) {
        //msg_printf(0, MSG_INFO, "adding shortfall %d %f", rsc_type, idle*d_time);
        shortfall += idle*d_time;
    }
#if 0
    msg_printf(0, MSG_INFO, "accum shortf (%s): idle %f dt %f sf %f",
        rsc_name(rsc_type), idle, d_time, shortfall
    );
#endif
}

void RSC_WORK_FETCH::update_saturated_time(double dt) {
    double idle = ninstances - sim_nused;
    if (idle < 1e-6) {
        saturated_time = dt;
    }
}

void RSC_WORK_FETCH::update_busy_time(double dur, double nused) {
    busy_time_estimator.update(dur, nused);
}

static bool wacky_dcf(PROJECT* p) {
    double dcf = p->duration_correction_factor;
    return (dcf < 0.02 || dcf > 80.0);
}

PROJECT* RSC_WORK_FETCH::choose_project_hyst() {
    PROJECT* pbest = NULL;
    if (saturated_time > gstate.work_buf_min()) return NULL;
    for (unsigned i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        if (!p->pwf.can_fetch_work) continue;
        if (!project_state(p).may_have_work) continue;
        RSC_PROJECT_WORK_FETCH& rpwf = project_state(p);
        if (rpwf.anon_skip) continue;
        if (pbest) {
if (use_rec) {
            if (project_priority(pbest) > project_priority(p)) {
                continue;
            }
} else {
            if (pbest->pwf.overall_debt > p->pwf.overall_debt) {
                continue;
            }
}
        }
        pbest = p;
    }
    if (!pbest) return NULL;
    work_fetch.clear_request();
    work_fetch.set_all_requests_hyst(pbest, rsc_type);
    return pbest;
}

// Choose the best project to ask for work for this resource,
// given the specific criterion
//
PROJECT* RSC_WORK_FETCH::choose_project(int criterion) {
    PROJECT* pbest = NULL;

    switch (criterion) {
    case FETCH_IF_IDLE_INSTANCE:
        if (nidle_now == 0) return NULL;
        break;
    case FETCH_IF_MAJOR_SHORTFALL:
        if (saturated_time > gstate.work_buf_min()) return NULL;
        break;
    case FETCH_IF_MINOR_SHORTFALL:
        if (saturated_time > gstate.work_buf_total()) return NULL;
        break;
    case FETCH_IF_PROJECT_STARVED:
        if (deadline_missed_instances >= ninstances) return NULL;
        break;
    }

    for (unsigned i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        if (!p->pwf.can_fetch_work) continue;
        if (!project_state(p).may_have_work) continue;
        RSC_PROJECT_WORK_FETCH& rpwf = project_state(p);
        if (rpwf.anon_skip) continue;
        switch (criterion) {
        case FETCH_IF_MINOR_SHORTFALL:
if (!use_rec) {
            if (rpwf.overworked()) continue;
}
            if (wacky_dcf(p)) continue;
            if (!p->resource_share) continue;
            break;
        case FETCH_IF_MAJOR_SHORTFALL:
            if (wacky_dcf(p)) continue;
            if (!p->resource_share) continue;
            break;
        case FETCH_IF_PROJECT_STARVED:
if (!use_rec) {
            if (rpwf.overworked()) continue;
            if (rpwf.nused_total >= ninstances*rpwf.fetchable_share) continue;
} else {
            if (project_priority(p) < 0) continue;
            if (rpwf.nused_total >= ninstances) continue;
}
            if (!p->resource_share) continue;
            break;
        }

        if (pbest) {
            if (!p->resource_share) {
                continue;
            }
if (use_rec) {
            if (project_priority(pbest) > project_priority(p)) {
                continue;
            }
} else {
            if (pbest->pwf.overall_debt > p->pwf.overall_debt) {
                continue;
            }
}
        }
        pbest = p;
    }
    if (!pbest) return NULL;

    // decide how much work to request from each resource
    //
    work_fetch.clear_request();
    switch (criterion) {
    case FETCH_IF_IDLE_INSTANCE:
    case FETCH_IF_MAJOR_SHORTFALL:
        set_request(pbest, true);
        break;
    case FETCH_IF_PROJECT_STARVED:
        set_request(pbest, false);
        break;
    case FETCH_IF_MINOR_SHORTFALL:
        // in this case, potentially request work for all resources
        //
        if (use_rec && (project_priority(pbest) < 0)) {
            set_request(pbest, true);
        } else {
            work_fetch.set_all_requests(pbest);
        }
        break;
    }
    // in principle there should be a nonzero request.
    // check, just in case
    //
    if (!req_secs && !req_instances) {
        if (log_flags.work_fetch_debug) {
            msg_printf(pbest, MSG_INFO,
                "[work_fetch] error: project chosen but zero request"
            );
        }
        return 0;
    }

    if (log_flags.work_fetch_debug) {
        msg_printf(pbest, MSG_INFO,
            "[work_fetch] chosen: %s %s: %.2f inst, %.2f sec",
            criterion_name(criterion), rsc_name(rsc_type),
            req_instances, req_secs
        );
    }

    return pbest;
}

// request this project's share of shortfall and instances.
// don't request anything if project is overworked or backed off.
//
void RSC_WORK_FETCH::set_request(PROJECT* p, bool allow_overworked) {
    if (dont_fetch(p, rsc_type)) return;

    // if backup project, fetch 1 job per idle instance
    //
    if (p->resource_share == 0) {
        req_instances = nidle_now;
        req_secs = 1;
        return;
    }
    if (config.fetch_minimal_work) {
        req_instances = ninstances;
        req_secs = 1;
        return;
    }
    RSC_PROJECT_WORK_FETCH& w = project_state(p);
    if (!w.may_have_work) return;
    if (w.anon_skip) return;
if (!use_rec) {
    if (!allow_overworked && w.overworked()) return;
}
    if (shortfall) {
        if (wacky_dcf(p)) {
            // if project's DCF is too big or small,
            // its completion time estimates are useless; just ask for 1 second
            //
            req_secs = 1;
        } else {
            if (use_rec) {
                req_secs = shortfall;
            } else {
                req_secs = shortfall * w.fetchable_share;
            }
        }
    }

    // the number of additional instances needed to have our share
    //
    double x1;
    if (use_rec) {
        x1 = ninstances - w.nused_total;
    } else {
        x1 = (ninstances * w.fetchable_share) - w.nused_total;
    }

    // our share of the idle instances
    //
    double x2 = nidle_now * w.fetchable_share;

    req_instances = std::max(x1, x2);
    if (req_instances && !req_secs) {
        req_secs = 1;
    }
}

void RSC_WORK_FETCH::print_state(const char* name) {
    msg_printf(0, MSG_INFO,
        "[work_fetch] %s: shortfall %.2f nidle %.2f saturated %.2f busy %.2f RS fetchable %.2f runnable %.2f",
        name,
        shortfall, nidle_now, saturated_time, busy_time_estimator.get_busy_time(),
        total_fetchable_share, total_runnable_share
    );
    for (unsigned int i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        if (p->non_cpu_intensive) continue;
        RSC_PROJECT_WORK_FETCH& pwf = project_state(p);
        bool no_rsc_pref = p->no_rsc_pref[rsc_type];
        bool no_rsc_config = p->no_rsc_config[rsc_type];
        bool no_rsc_apps = p->no_rsc_apps[rsc_type];
        bool no_rsc_ams = p->no_rsc_ams[rsc_type];
        double bt = pwf.backoff_time>gstate.now?pwf.backoff_time-gstate.now:0;
if (use_rec) {
        msg_printf(p, MSG_INFO,
            "[work_fetch] %s: fetch share %.2f rec %.5f prio %.5f backoff dt %.2f int %.2f%s%s%s%s%s%s%s%s%s",
            name,
            pwf.fetchable_share, p->pwf.rec, project_priority(p), bt, pwf.backoff_interval,
            p->suspended_via_gui?" (susp via GUI)":"",
            p->master_url_fetch_pending?" (master fetch pending)":"",
            p->min_rpc_time > gstate.now?" (comm deferred)":"",
            p->dont_request_more_work?" (no new tasks)":"",
            p->too_many_uploading_results?" (too many uploads)":"",
            no_rsc_pref?" (blocked by prefs)":"",
            no_rsc_apps?" (no apps)":"",
            no_rsc_ams?" (blocked by account manager)":"",
            no_rsc_config?" (blocked by configuration file)":""
        );
} else {
        msg_printf(p, MSG_INFO,
            "[work_fetch] %s: fetch share %.2f LTD %.2f backoff dt %.2f int %.2f%s%s%s%s%s%s%s%s",
            name,
            pwf.fetchable_share, pwf.long_term_debt, bt, pwf.backoff_interval,
            p->suspended_via_gui?" (susp via GUI)":"",
            p->master_url_fetch_pending?" (master fetch pending)":"",
            p->min_rpc_time > gstate.now?" (comm deferred)":"",
            p->dont_request_more_work?" (no new tasks)":"",
            pwf.overworked()?" (overworked)":"",
            p->too_many_uploading_results?" (too many uploads)":"",
            no_rsc_pref?" (blocked by prefs)":"",
            no_rsc_config?" (blocked by configuration file)":""
        );
}
    }
}

void RSC_WORK_FETCH::clear_request() {
    req_secs = 0;
    req_instances = 0;
}

//#ifndef USE_REC
// update long-term debts for a resource.
//
void RSC_WORK_FETCH::update_long_term_debts() {
    unsigned int i;
    int neligible = 0;
    double ders = 0;
    PROJECT* p;

    // find the total resource share of eligible projects
    //
    for (i=0; i<gstate.projects.size(); i++) {
        p = gstate.projects[i];
        RSC_PROJECT_WORK_FETCH& w = project_state(p);
        if (w.debt_eligible(p, *this)) {
            ders += p->resource_share;
            neligible++;
        }
    }
    if (!neligible) {
        if (log_flags.debt_debug) {
            msg_printf(0, MSG_INFO,
                "[debt] %s: no eligible projects", rsc_name(rsc_type)
            );
        }
        return;
    }

    double max_debt=0;
    bool first = true;
    for (i=0; i<gstate.projects.size(); i++) {
        p = gstate.projects[i];
        if (p->non_cpu_intensive) continue;
        if (!p->resource_share) continue;
        RSC_PROJECT_WORK_FETCH& w = project_state(p);
        if (w.debt_eligible(p, *this)) {
            double share_frac = p->resource_share/ders;

            // the change to a project's debt is:
            // (how much it's owed) - (how much it got)
            //
            double delta = share_frac*secs_this_debt_interval - w.secs_this_debt_interval;
            delta /= ninstances;
            w.long_term_debt += delta;
            if (log_flags.debt_debug) {
                msg_printf(p, MSG_INFO,
                    "[debt] %s LTD %.2f delta %.2f (%.2f*%.2f - %.2f)/%d",
                    rsc_name(rsc_type),
                    w.long_term_debt, delta, share_frac,
                    secs_this_debt_interval,
                    w.secs_this_debt_interval,
                    ninstances
                );
            }
            if (first) {
                max_debt = w.long_term_debt;
                first = false;
            } else {
                if (w.long_term_debt > max_debt) {
                    max_debt = w.long_term_debt;
                }
            }
        } else {
            if (log_flags.debt_debug) {
                msg_printf(p, MSG_INFO,
                    "[debt] %s ineligible; LTD %.2f",
                    rsc_name(rsc_type), w.long_term_debt
                );
            }
        }
    }

    // The net change may be
    // - positive if the resource wasn't fully utilized during the debt interval
    // - negative it was overcommitted (e.g., CPU)
    // We need to keep eligible projects from diverging from non-eligible ones;
    // also, if all the debts are large negative we need to gradually
    // shift them towards zero.
    // To do this, we add an offset as follows:
    // delta_limit is the largest rate at which any project's debt
    // could increase or decrease.
    // If the largest debt is close to zero (relative to delta_limit)
    // than add an offset that will bring it exactly to zero.
    // Otherwise add an offset of 2*delta_limit,
    // which will gradually bring all the debts towards zero
    //
    // The policy of keeping the max debt at zero is important;
    // it means that new projects will begin in parity with high-debt project,
    // and won't wait for months to get work.
    //
    double offset;
    double delta_limit = secs_this_debt_interval;
    if (max_debt > -2*delta_limit) {
        if (fabs(max_debt) < 1e-6) max_debt = 0;
        offset = max_debt?-max_debt:0;  // avoid -0
    } else {
        offset = 2*delta_limit;
    }
    if (log_flags.debt_debug) {
        msg_printf(0, MSG_INFO, "[debt] %s LTD: adding offset %f",
            rsc_name(rsc_type), offset
        );
    }
    for (i=0; i<gstate.projects.size(); i++) {
        p = gstate.projects[i];
        if (p->non_cpu_intensive) continue;
        if (!p->resource_share) continue;
        RSC_PROJECT_WORK_FETCH& w = project_state(p);
        if (w.debt_eligible(p, *this)) {
            w.long_term_debt += offset;
        } else {
            if (offset > 0) {
                w.long_term_debt += offset;
            }
        }
        if (w.long_term_debt > 0) w.long_term_debt = 0;
    }
}


// update short-term debts for a resource.
//
void RSC_WORK_FETCH::update_short_term_debts() {
    unsigned int i;
    PROJECT* p;
    int nprojects=0, nrprojects=0;
    double share_frac;
    double total_short_term_debt = 0;
    double rrs = gstate.runnable_resource_share(rsc_type);

    // for projects with no runnable jobs,
    // STD decays by a factor of e every day
    //
    double decay_factor = exp(-secs_this_debt_interval/86400);

    for (i=0; i<gstate.projects.size(); i++) {
        double delta;
        p = gstate.projects[i];
        if (p->non_cpu_intensive) continue;
        if (!p->resource_share) continue;
        RSC_PROJECT_WORK_FETCH& rpwf = project_state(p);
        nprojects++;

        if (p->runnable(rsc_type)) {
            nrprojects++;
            share_frac = p->resource_share/rrs;
            delta = share_frac*secs_this_debt_interval
                - rpwf.secs_this_debt_interval;
            delta /= ninstances;
            if (log_flags.std_debug) {
                msg_printf(p, MSG_INFO,
                    "[std] %s STD delta %.2f (%.2f*%.2f - %.2f)/%d",
                    rsc_name(rsc_type),
                    delta,
                    share_frac,
                    secs_this_debt_interval,
                    rpwf.secs_this_debt_interval,
                    ninstances
                );
            }
            rpwf.short_term_debt += delta;
        } else {
            rpwf.short_term_debt *= decay_factor;
        }
        total_short_term_debt += rpwf.short_term_debt;
    }

    //  normalize so mean is zero, and limit abs value to MAX_STD
    //
    if (nrprojects) {
        double avg_short_term_debt = total_short_term_debt / nprojects;
        for (i=0; i<gstate.projects.size(); i++) {
            p = gstate.projects[i];
            if (p->non_cpu_intensive) continue;
            if (!p->resource_share) continue;
            if (p->runnable(rsc_type)) {
                RSC_PROJECT_WORK_FETCH& rpwf = project_state(p);
                rpwf.short_term_debt -= avg_short_term_debt;
                if (rpwf.short_term_debt > MAX_STD) {
                    rpwf.short_term_debt = MAX_STD;
                }
                if (rpwf.short_term_debt < -MAX_STD) {
                    rpwf.short_term_debt = -MAX_STD;
                }
                if (log_flags.std_debug) {
                    msg_printf(p, MSG_INFO,
                        "[std] %s STD %.2f",
                        rsc_name(rsc_type), rpwf.short_term_debt
                    );
                }
            }
        }
    }
}
//#endif

///////////////  PROJECT_WORK_FETCH  ///////////////

bool PROJECT_WORK_FETCH::compute_can_fetch_work(PROJECT* p) {
    if (p->non_cpu_intensive) return false;
    if (p->suspended_via_gui) return false;
    if (p->master_url_fetch_pending) return false;
    if (p->min_rpc_time > gstate.now) return false;
    if (p->dont_request_more_work) return false;
    if (p->some_download_stalled()) return false;
    if (p->some_result_suspended()) return false;
    if (p->too_many_uploading_results) return false;
    return true;
}

void PROJECT_WORK_FETCH::reset(PROJECT* p) {
    for (int i=0; i<coprocs.n_rsc; i++) {
        p->rsc_pwf[i].reset();
    }
}

///////////////  WORK_FETCH  ///////////////

void WORK_FETCH::rr_init() {
    for (int i=0; i<coprocs.n_rsc; i++) {
        rsc_work_fetch[i].rr_init();
    }
    for (unsigned int i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        p->pwf.can_fetch_work = p->pwf.compute_can_fetch_work(p);
        p->pwf.has_runnable_jobs = false;
        for (int j=0; j<coprocs.n_rsc; j++) {
            p->rsc_pwf[j].rr_init(p, j);
        }
    }
}

// if the given project is highest-priority among the projects
// eligible for the resource, set request fields
//
void RSC_WORK_FETCH::supplement(PROJECT* pp) {
    double x = project_priority(pp);
    for (unsigned i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        if (p == pp) continue;
        if (!p->pwf.can_fetch_work) continue;
        if (!project_state(p).may_have_work) continue;
        RSC_PROJECT_WORK_FETCH& rpwf = project_state(p);
        if (rpwf.anon_skip) continue;
        if (project_priority(p) > x) {
            return;
        }
    }
    // didn't find a better project; ask for work
    set_request(pp, true);
}

void WORK_FETCH::set_all_requests_hyst(PROJECT* p, int rsc_type) {
    for (int i=0; i<coprocs.n_rsc; i++) {
        if (i == rsc_type) {
            rsc_work_fetch[i].set_request(p, true);
        } else {
            if (i==0 || gpus_usable) {
                rsc_work_fetch[i].supplement(p);
            }
        }
    }
}

void WORK_FETCH::set_all_requests(PROJECT* p) {
    for (int i=0; i<coprocs.n_rsc; i++) {
        if (i==0 || gpus_usable) {
            rsc_work_fetch[i].set_request(p, false);
        }
    }
}

//#ifndef USE_REC
// Compute an "overall long-term debt" for each project.
// This is a sum of per-resource terms, scaled by the relative speed of the resource.
// The term for a resource is its LTD plus an estimate of queued work.
//
void WORK_FETCH::set_overall_debts() {
    unsigned int i;
    PROJECT* p;
    RESULT* rp;
    APP_VERSION* avp;

    for (i=0; i<gstate.projects.size(); i++) {
        p = gstate.projects[i];
        for (int j=0; j<coprocs.n_rsc; j++) {
            p->rsc_pwf[j].queue_est = 0;
        }
    }
    for (i=0; i<gstate.results.size(); i++) {
        rp = gstate.results[i];
        p = rp->project;
        if (!rp->nearly_runnable()) continue;
        if (p->non_cpu_intensive) continue;
        double dt = rp->estimated_time_remaining();
        avp = rp->avp;
        p->rsc_pwf[0].queue_est += dt*avp->avg_ncpus;
        int rt = avp->gpu_usage.rsc_type;
        if (rt) {
            p->rsc_pwf[rt].queue_est += dt*avp->gpu_usage.usage;
        }
    }
    for (i=0; i<gstate.projects.size(); i++) {
        p = gstate.projects[i];
        double queue_debt = p->rsc_pwf[0].queue_est/gstate.ncpus;
        p->pwf.overall_debt = p->rsc_pwf[0].long_term_debt - queue_debt;
        for (int j=1; j<coprocs.n_rsc; j++) {
            p->pwf.overall_debt += rsc_work_fetch[j].relative_speed*
                (p->rsc_pwf[j].long_term_debt - p->rsc_pwf[j].queue_est/coprocs.coprocs[j].count);
        }
    }
}

void WORK_FETCH::zero_debts() {
    for (unsigned i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        for (int j=0; j<coprocs.n_rsc; j++) {
            p->rsc_pwf[j].zero_debt();
        }
    }
}
//#endif

void WORK_FETCH::print_state() {
    msg_printf(0, MSG_INFO, "[work_fetch] ------- start work fetch state -------");
    msg_printf(0, MSG_INFO, "[work_fetch] target work buffer: %.2f + %.2f sec",
        gstate.work_buf_min(), gstate.work_buf_additional()
    );
    for (int i=0; i<coprocs.n_rsc; i++) {
        rsc_work_fetch[i].print_state(rsc_name(i));
    }
    for (unsigned int i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        if (p->non_cpu_intensive) continue;
if (use_rec) {
        msg_printf(p, MSG_INFO, "[work_fetch] REC %f", p->pwf.rec);
} else {
        msg_printf(p, MSG_INFO, "[work_fetch] overall LTD %.2f",
            p->pwf.overall_debt
        );
}
    }
    msg_printf(0, MSG_INFO, "[work_fetch] ------- end work fetch state -------");
}

void WORK_FETCH::clear_request() {
    for (int i=0; i<coprocs.n_rsc; i++) {
        rsc_work_fetch[i].clear_request();
    }
}

// we're going to contact this project for reasons other than work fetch;
// decide if we should piggy-back a work fetch request.
//
void WORK_FETCH::compute_work_request(PROJECT* p) {
    clear_request();
    if (config.fetch_minimal_work && gstate.had_or_requested_work) return;
    if (p->dont_request_more_work) return;
    if (p->resource_share  == 0) return;
    if (p->non_cpu_intensive) {
        if (!has_a_job(p)) {
            rsc_work_fetch[0].req_secs = 1;
        }
        return;
    }

    // See if this is the project we'd ask for work anyway.
    // Temporarily clear resource backoffs,
    // since we're going to contact this project in any case.
    //
    double backoff_save[MAX_RSC];
    for (int i=0; i<coprocs.n_rsc; i++) {
        backoff_save[i] = p->rsc_pwf[i].backoff_time;
        p->rsc_pwf[i].backoff_time = 0;
    }
    PROJECT* pbest = choose_project();
    for (int i=0; i<coprocs.n_rsc; i++) {
        p->rsc_pwf[i].backoff_time = backoff_save[i];
    }
    if (p == pbest) {
        // Ask for work for all devices w/ a shortfall.
        // Otherwise we can have a situation where a CPU is idle,
        // we ask only for GPU work, and the project never has any
        //
        work_fetch.set_all_requests(pbest);
        return;
    }

    // if not, don't request any work
    //
    clear_request();
}

// see if there's a fetchable non-CPU-intensive project without work
//
PROJECT* WORK_FETCH::non_cpu_intensive_project_needing_work() {
    for (unsigned int i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        if (!p->non_cpu_intensive) continue;
        if (!p->can_request_work()) continue;
        if (p->rsc_pwf[0].backoff_time > gstate.now) continue;
        if (has_a_job(p)) continue;
        clear_request();
        rsc_work_fetch[0].req_secs = 1;
        return p;
    }
    return 0;
}

// choose a project to fetch work from,
// and set the request fields of resource objects
//
PROJECT* WORK_FETCH::choose_project() {
    PROJECT* p = 0;

    if (log_flags.work_fetch_debug) {
        msg_printf(0, MSG_INFO, "[work_fetch] work fetch start");
    }

    p = non_cpu_intensive_project_needing_work();
    if (p) return p;

    gstate.compute_nuploading_results();

    gstate.rr_simulation();
    compute_shares();
if (use_rec) {
    project_priority_init();
    for (unsigned int i=0; i<gstate.results.size(); i++) {
        RESULT* rp = gstate.results[i];
        adjust_rec_work_fetch(rp);
    }
} else {
    set_overall_debts();
}

if (use_hyst_fetch) {
    if (gpus_usable) {
        for (int i=1; i<coprocs.n_rsc; i++) {
            p = rsc_work_fetch[i].choose_project_hyst();
            if (p) break;
        }
    }
    if (!p) {
        p = rsc_work_fetch[0].choose_project_hyst();
    }
} else {
    if (gpus_usable) {
        for (int i=1; i<coprocs.n_rsc; i++) {
            p = rsc_work_fetch[i].choose_project(FETCH_IF_IDLE_INSTANCE);
            if (p) break;
        }
    }
    if (!p) {
        p = rsc_work_fetch[0].choose_project(FETCH_IF_IDLE_INSTANCE);
    }

    if (!p && gpus_usable) {
        for (int i=1; i<coprocs.n_rsc; i++) {
            p = rsc_work_fetch[i].choose_project(FETCH_IF_MAJOR_SHORTFALL);
            if (p) break;
        }
    }
    if (!p) {
        p = rsc_work_fetch[0].choose_project(FETCH_IF_MAJOR_SHORTFALL);
    }
    
    if (!p && gpus_usable) {
        for (int i=1; i<coprocs.n_rsc; i++) {
            p = rsc_work_fetch[i].choose_project(FETCH_IF_MINOR_SHORTFALL);
            if (p) break;
        }
    }
    if (!p) {
        p = rsc_work_fetch[0].choose_project(FETCH_IF_MINOR_SHORTFALL);
    }

    if (!p && gpus_usable) {
        for (int i=1; i<coprocs.n_rsc; i++) {
            p = rsc_work_fetch[i].choose_project(FETCH_IF_PROJECT_STARVED);
            if (p) break;
        }
    }
    if (!p) {
        p = rsc_work_fetch[0].choose_project(FETCH_IF_PROJECT_STARVED);
    }
}

    if (log_flags.work_fetch_debug) {
        print_state();
        if (!p) {
            msg_printf(0, MSG_INFO, "[work_fetch] No project chosen for work fetch");
        }
    }

    return p;
}

void WORK_FETCH::accumulate_inst_sec(ACTIVE_TASK* atp, double dt) {
    APP_VERSION* avp = atp->result->avp;
    PROJECT* p = atp->result->project;
    double x = dt*avp->avg_ncpus;
    p->rsc_pwf[0].secs_this_debt_interval += x;
    rsc_work_fetch[0].secs_this_debt_interval += x;
    int rt = avp->gpu_usage.rsc_type;
    if (rt) {
        x = dt*avp->gpu_usage.usage;
        p->rsc_pwf[rt].secs_this_debt_interval += x;
        rsc_work_fetch[rt].secs_this_debt_interval += x;
    }
}

// find total and per-project resource shares for each resource
//
void WORK_FETCH::compute_shares() {
    unsigned int i;
    PROJECT* p;
    for (i=0; i<gstate.projects.size(); i++) {
        p = gstate.projects[i];
        if (p->non_cpu_intensive) continue;
        for (int j=0; j<coprocs.n_rsc; j++) {
            if (p->rsc_pwf[j].has_runnable_jobs) {
                rsc_work_fetch[j].total_runnable_share += p->resource_share;
            }
        }
        if (!p->pwf.can_fetch_work) continue;
        for (int j=0; j<coprocs.n_rsc; j++) {
            if (p->rsc_pwf[j].may_have_work) {
                rsc_work_fetch[j].total_fetchable_share += p->resource_share;
            }
        }
    }
    for (i=0; i<gstate.projects.size(); i++) {
        p = gstate.projects[i];
        if (p->non_cpu_intensive) continue;
        for (int j=0; j<coprocs.n_rsc; j++) {
            if (p->rsc_pwf[j].has_runnable_jobs) {
                p->rsc_pwf[j].runnable_share = p->resource_share/rsc_work_fetch[j].total_runnable_share;
            }
        }
        if (!p->pwf.can_fetch_work) continue;
        for (int j=0; j<coprocs.n_rsc; j++) {
            if (p->rsc_pwf[j].may_have_work) {
                p->rsc_pwf[j].fetchable_share = rsc_work_fetch[j].total_fetchable_share?p->resource_share/rsc_work_fetch[j].total_fetchable_share:1;
            }
        }
    }
}

void WORK_FETCH::request_string(char* buf) {
    char buf2[256];
    sprintf(buf,
        "[work_fetch] request: CPU (%.2f sec, %.2f inst)",
        rsc_work_fetch[0].req_secs, rsc_work_fetch[0].req_instances
    );
    for (int i=1; i<coprocs.n_rsc; i++) {
        sprintf(buf2, " %s (%.2f sec, %.2f inst)",
            rsc_name(i), rsc_work_fetch[i].req_secs, rsc_work_fetch[i].req_instances
        );
        strcat(buf, buf2);
    }
}

void WORK_FETCH::write_request(FILE* f, PROJECT* p) {
    double work_req = rsc_work_fetch[0].req_secs;

    // if project is anonymous platform, set the overall work req
    // to the max of the requests of resource types for which we have versions.
    // Otherwise projects with old schedulers won't send us work.
    // THIS CAN BE REMOVED AT SOME POINT
    //
    if (p->anonymous_platform) {
        for (int i=1; i<coprocs.n_rsc; i++) {
            if (has_coproc_app(p, i)) {
                if (rsc_work_fetch[i].req_secs > work_req) {
                    work_req = rsc_work_fetch[i].req_secs;
                }
            }
        }
    }
    fprintf(f,
        "    <work_req_seconds>%f</work_req_seconds>\n"
        "    <cpu_req_secs>%f</cpu_req_secs>\n"
        "    <cpu_req_instances>%f</cpu_req_instances>\n"
        "    <estimated_delay>%f</estimated_delay>\n",
        work_req,
        rsc_work_fetch[0].req_secs,
        rsc_work_fetch[0].req_instances,
        rsc_work_fetch[0].req_secs?rsc_work_fetch[0].busy_time_estimator.get_busy_time():0
    );
    if (log_flags.work_fetch_debug) {
        char buf[256];
        request_string(buf);
        msg_printf(p, MSG_INFO, buf);
    }
}

// we just got a scheduler reply with the given jobs; update backoffs
//
void WORK_FETCH::handle_reply(
    PROJECT* p, SCHEDULER_REPLY*, vector<RESULT*> new_results
) {
    bool got_rsc[MAX_RSC];
    for (int i=0; i<coprocs.n_rsc; i++) {
        got_rsc[i] = false;
    }

    // if didn't get any jobs, back off on requested resource types
    //
    if (!new_results.size()) {
        // but not if RPC was requested by project
        //
        if (p->sched_rpc_pending != RPC_REASON_PROJECT_REQ) {
            for (int i=0; i<coprocs.n_rsc; i++) {
                if (rsc_work_fetch[i].req_secs) {
                    p->rsc_pwf[i].backoff(p, rsc_name(i));
                }
            }
        }
        return;
    }

    // if we did get jobs, clear backoff on resource types
    //
    for (unsigned int i=0; i<new_results.size(); i++) {
        RESULT* rp = new_results[i];
        got_rsc[rp->avp->gpu_usage.rsc_type] = true;
    }
    for (int i=0; i<coprocs.n_rsc; i++) {
        if (got_rsc[i]) p->rsc_pwf[i].clear_backoff();
    }
}

// set up for initial RPC.
// arrange to always get one job, even if we don't need it or can't handle it.
// (this is probably what user wants)
//
void WORK_FETCH::set_initial_work_request() {
    for (int i=0; i<coprocs.n_rsc; i++) {
        rsc_work_fetch[i].req_secs = 1;
        rsc_work_fetch[i].req_instances = 0;
        rsc_work_fetch[i].busy_time_estimator.reset();
    }
}

// called once, at client startup
//
void WORK_FETCH::init() {
    rsc_work_fetch[0].init(0, gstate.ncpus, 1);
    double cpu_flops = gstate.host_info.p_fpops;

    // use 20% as a rough estimate of GPU efficiency

    for (int i=1; i<coprocs.n_rsc; i++) {
        rsc_work_fetch[i].init(
            i, coprocs.coprocs[i].count,
            coprocs.coprocs[i].count*0.2*coprocs.coprocs[i].peak_flops/cpu_flops
        );
    }

if (!use_rec) {
    if (config.zero_debts) {
        zero_debts();
    }
}

    // see what resources anon platform projects can use
    //
    unsigned int i, j;
    for (i=0; i<gstate.projects.size(); i++) {
        PROJECT* p = gstate.projects[i];
        if (!p->anonymous_platform) continue;
        for (int k=0; k<coprocs.n_rsc; k++) {
            p->rsc_pwf[k].anon_skip = true;
        }
        for (j=0; j<gstate.app_versions.size(); j++) {
            APP_VERSION* avp = gstate.app_versions[j];
            if (avp->project != p) continue;
            p->rsc_pwf[avp->gpu_usage.rsc_type].anon_skip = false;
        }
    }
}

// clear backoff for app's resource
//
void WORK_FETCH::clear_backoffs(APP_VERSION& av) {
    av.project->rsc_pwf[av.gpu_usage.rsc_type].clear_backoff();
}

////////////////////////

void CLIENT_STATE::compute_nuploading_results() {
    unsigned int i;

    for (i=0; i<projects.size(); i++) {
        projects[i]->nuploading_results = 0;
        projects[i]->too_many_uploading_results = false;
    }
    for (i=0; i<results.size(); i++) {
        RESULT* rp = results[i];
        if (rp->state() == RESULT_FILES_UPLOADING) {
            rp->project->nuploading_results++;
        }
    }
    int n = gstate.ncpus;
    for (int j=1; j<coprocs.n_rsc; j++) {
        if (coprocs.coprocs[j].count > n) {
            n = coprocs.coprocs[j].count;
        }
    }
    n *= 2;
    for (i=0; i<projects.size(); i++) {
        if (projects[i]->nuploading_results > n) {
            projects[i]->too_many_uploading_results = true;
        }
    }
}

bool PROJECT::runnable(int rsc_type) {
    if (suspended_via_gui) return false;
    for (unsigned int i=0; i<gstate.results.size(); i++) {
        RESULT* rp = gstate.results[i];
        if (rp->project != this) continue;
        if (rsc_type != RSC_TYPE_ANY) {
            if (rp->avp->gpu_usage.rsc_type != rsc_type) {
                continue;
            }
        }
        if (rp->runnable()) return true;
    }
    return false;
}

bool PROJECT::uploading() {
    for (unsigned int i=0; i<gstate.file_xfers->file_xfers.size(); i++) {
        FILE_XFER& fx = *gstate.file_xfers->file_xfers[i];
        if (fx.fip->project == this && fx.is_upload) {
            return true;
        }
    }
    return false;
}

bool PROJECT::downloading() {
    if (suspended_via_gui) return false;
    for (unsigned int i=0; i<gstate.results.size(); i++) {
        RESULT* rp = gstate.results[i];
        if (rp->project != this) continue;
        if (rp->downloading()) return true;
    }
    return false;
}

bool PROJECT::some_result_suspended() {
    unsigned int i;
    for (i=0; i<gstate.results.size(); i++) {
         RESULT *rp = gstate.results[i];
         if (rp->project != this) continue;
         if (rp->suspended_via_gui) return true;
     }
    return false;
}

bool PROJECT::can_request_work() {
    if (suspended_via_gui) return false;
    if (master_url_fetch_pending) return false;
    if (min_rpc_time > gstate.now) return false;
    if (dont_request_more_work) return false;
    if (gstate.in_abort_sequence) return false;
    return true;
}

bool PROJECT::potentially_runnable() {
    if (runnable(RSC_TYPE_ANY)) return true;
    if (can_request_work()) return true;
    if (downloading()) return true;
    return false;
}

bool PROJECT::nearly_runnable() {
    if (runnable(RSC_TYPE_ANY)) return true;
    if (downloading()) return true;
    return false;
}

// whether this task can be run right now
//
bool RESULT::runnable() {
    if (suspended_via_gui) return false;
    if (project->suspended_via_gui) return false;
    if (state() != RESULT_FILES_DOWNLOADED) return false;
    if (coproc_missing) return false;
    if (schedule_backoff > gstate.now) return false;
    return true;
}

// whether this task should be included in RR simulation
// Like runnable, except downloading backoff is OK
// Schedule-backoff is not OK;
// we should be able to get GPU jobs from project A
// even if project B has backed-off jobs.
//
bool RESULT::nearly_runnable() {
    if (suspended_via_gui) return false;
    if (project->suspended_via_gui) return false;
    switch (state()) {
    case RESULT_FILES_DOWNLOADED:
    case RESULT_FILES_DOWNLOADING:
        break;
    default:
        return false;
    }
    if (coproc_missing) return false;
    if (schedule_backoff > gstate.now) return false;
    return true;
}

// Return true if the result is waiting for its files to download,
// and nothing prevents this from happening soon
//
bool RESULT::downloading() {
    if (suspended_via_gui) return false;
    if (project->suspended_via_gui) return false;
    if (state() > RESULT_FILES_DOWNLOADING) return false;
    if (some_download_stalled()) return false;
    return true;
}

double RESULT::estimated_duration_uncorrected() {
    return wup->rsc_fpops_est/avp->flops;
}

// estimate how long a result will take on this host
//
double RESULT::estimated_duration() {
    return estimated_duration_uncorrected()*project->duration_correction_factor;
}

double RESULT::estimated_time_remaining() {
    if (computing_done()) return 0;
    ACTIVE_TASK* atp = gstate.lookup_active_task_by_result(this);
    if (atp) {
        return atp->est_dur() - atp->elapsed_time;
    }
    return estimated_duration();
}

// Returns the estimated total elapsed time of this task.
// Compute this as a weighted average of estimates based on
// 1) the workunit's flops count (static estimate)
// 2) the current elapsed time and fraction done (dynamic estimate)
//
double ACTIVE_TASK::est_dur() {
    if (fraction_done >= 1) return elapsed_time;
    double wu_est = result->estimated_duration();
    if (fraction_done <= 0) return wu_est;
    if (wu_est < elapsed_time) wu_est = elapsed_time;
    double frac_est = fraction_done_elapsed_time / fraction_done;
    double fraction_left = 1-fraction_done;
    double wu_weight = fraction_left * fraction_left * fraction_left;
    double fd_weight = 1 - wu_weight;
    double x = fd_weight*frac_est + wu_weight*wu_est;
#if 0
    if (log_flags.rr_simulation) {
        msg_printf(result->project, MSG_INFO,
            "[rr_sim] %s frac_est %f = %f/%f",
            result->name, frac_est, fraction_done_elapsed_time, fraction_done
        );
        msg_printf(result->project, MSG_INFO,
            "[rr_sim] %s dur: %.2f = %.3f*%.2f + %.3f*%.2f",
            result->name, x, fd_weight, frac_est, wu_weight, wu_est
        );
    }
#endif
    return x;
}

// the fraction of time BOINC is processing
//
double CLIENT_STATE::overall_cpu_frac() {
    double x = time_stats.on_frac * time_stats.active_frac;
    if (x < 0.01) x = 0.01;
    if (x > 1) x = 1;
    return x;
}
double CLIENT_STATE::overall_gpu_frac() {
    double x = time_stats.on_frac * time_stats.gpu_active_frac;
    if (x < 0.01) x = 0.01;
    if (x > 1) x = 1;
    return x;
}

// called when benchmarks change
//
void CLIENT_STATE::scale_duration_correction_factors(double factor) {
    if (factor <= 0) return;
    for (unsigned int i=0; i<projects.size(); i++) {
        PROJECT* p = projects[i];
        p->duration_correction_factor *= factor;
    }
    if (log_flags.dcf_debug) {
        msg_printf(NULL, MSG_INFO,
            "[dcf] scaling all duration correction factors by %f",
            factor
        );
    }
}

// Choose a new host CPID.
// If using account manager, do scheduler RPCs
// to all acct-mgr-attached projects to propagate the CPID
//
void CLIENT_STATE::generate_new_host_cpid() {
    host_info.generate_host_cpid();
    for (unsigned int i=0; i<projects.size(); i++) {
        if (projects[i]->attached_via_acct_mgr) {
            projects[i]->sched_rpc_pending = RPC_REASON_ACCT_MGR_REQ;
            projects[i]->set_min_rpc_time(now + 15, "Sending new host CPID");
        }
    }
}

