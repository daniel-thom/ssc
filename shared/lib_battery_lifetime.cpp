#include "lib_util.h"

#include "lib_storage_params.h"
#include "lib_battery_capacity.h"
#include "lib_battery_lifetime.h"

void cycle_lifetime_state::init(double rel_q){
    nCycles = 0;
    relative_q = rel_q;
    jlt = 0;
    Xlt = 0;
    Ylt = 0;
    average_range = 0;
}

lifetime_cycle::lifetime_cycle(const std::shared_ptr<const battery_lifetime_params> &params):
params_p(params){
    for (int i = 0; i <(int)params_p->cycle_matrix.nrows(); i++)
    {
        _DOD_vect.push_back(params_p->cycle_matrix.at(i,0));
        _cycles_vect.push_back(params_p->cycle_matrix.at(i,1));
        _capacities_vect.push_back(params_p->cycle_matrix.at(i, 2));
    }
    // initialize other member variables
    state.Range = 0;
    state.init(bilinear(0, 0));
}

lifetime_cycle::lifetime_cycle(const lifetime_cycle& lifetime_cycle):
params_p(lifetime_cycle.params_p)
{
    _cycles_vs_DOD = lifetime_cycle._cycles_vs_DOD;
    _DOD_vect = lifetime_cycle._DOD_vect;
    _cycles_vect = lifetime_cycle._cycles_vect;
    _capacities_vect = lifetime_cycle._capacities_vect;

    state = lifetime_cycle.state;
    state.Range = lifetime_cycle.state.Range;
}
double lifetime_cycle::estimateCycleDamage()
{
    // Initialize assuming 50% DOD
    double DOD = 50;
    if (state.average_range > 0){
        DOD = state.average_range;
    }
    return(bilinear(DOD, state.nCycles + 1) - bilinear(DOD, state.nCycles + 2));
}
void lifetime_cycle::runCycleLifetime(double DOD)
{
    rainflow(DOD);
}

void lifetime_cycle::rainflow(double DOD)
{
    // initialize return code
    int retCode = LT_GET_DATA;

    // Begin algorithm
    state.Peaks.push_back(DOD);
    bool atStepTwo = true;

    // Loop until break
    while (atStepTwo)
    {
        // Rainflow: Step 2: Form ranges X,Y
        if (state.jlt >= 2)
            rainflow_ranges();
        else
        {
            // Get more data (Step 1)
            retCode = LT_GET_DATA;
            break;
        }

        // Rainflow: Step 3: Compare ranges
        retCode = rainflow_compareRanges();

        // We break to get more data, or if we are done with step 5
        if (retCode == LT_GET_DATA)
            break;
    }

    if (retCode == LT_GET_DATA)
        state.jlt++;
}

void lifetime_cycle::rainflow_ranges()
{
    state.Ylt = fabs(state.Peaks[state.jlt - 1] - state.Peaks[state.jlt - 2]);
    state.Xlt = fabs(state.Peaks[state.jlt] - state.Peaks[state.jlt - 1]);
}
void lifetime_cycle::rainflow_ranges_circular(int index)
{
    size_t end = state.Peaks.size() - 1;
    if (index == 0)
    {
        state.Xlt = fabs(state.Peaks[0] - state.Peaks[end]);
        state.Ylt = fabs(state.Peaks[end] - state.Peaks[end - 1]);
    }
    else if (index == 1)
    {
        state.Xlt = fabs(state.Peaks[1] - state.Peaks[0]);
        state.Ylt = fabs(state.Peaks[0] - state.Peaks[end]);
    }
    else
        rainflow_ranges();
}

int lifetime_cycle::rainflow_compareRanges()
{
    int retCode = LT_SUCCESS;
    bool contained = true;

    // modified to disregard some of algorithm which doesn't work well
    if (state.Xlt < state.Ylt)
        retCode = LT_GET_DATA;
    else if (state.Xlt >= state.Ylt)
        contained = false;

    // Step 5: Count range Y, discard peak & valley of Y, go to Step 2
    if (!contained)
    {
        state.Range = state.Ylt;
        state.average_range = (state.average_range * state.nCycles + state.Range) / (state.nCycles + 1);
        state.nCycles++;

        // the capacity percent cannot increase
        double dq = bilinear(state.average_range, state.nCycles) - bilinear(state.average_range, state.nCycles + 1);
        if (dq > 0)
            state.relative_q -= dq;

        if (state.relative_q < 0)
            state.relative_q = 0.;

        // discard peak & valley of Y
        double save = state.Peaks[state.jlt];
        state.Peaks.pop_back();
        state.Peaks.pop_back();
        state.Peaks.pop_back();
        state.Peaks.push_back(save);
        state.jlt -= 2;
        // stay in while loop
        retCode = LT_RERANGE;
    }
    return retCode;
}

void lifetime_cycle::replaceBattery(double replacement_percent)
{
    state.relative_q += replacement_percent;
    state.relative_q = fmin(bilinear(0., 0), state.relative_q);

    // More work to figure out degradation of multiple-aged battery units
    if (replacement_percent == 100) {
        state.nCycles = 0;
    }

    state.jlt = 0;
    state.Xlt = 0;
    state.Ylt = 0;
    state.Range = 0;
    state.Peaks.clear();
}

double lifetime_cycle::bilinear(double DOD, int cycle_number)
{
    /*
    Work could be done to make this simpler
    Current idea is to interpolate first along the C = f(n) curves for each DOD to get C_DOD_, C_DOD_+
    Then interpolate C_, C+ to get C at the DOD of interest
    */

    std::vector<double> D_unique_vect;
    std::vector<double> C_n_low_vect;
    std::vector<double> D_high_vect;
    std::vector<double> C_n_high_vect;
    std::vector<int> low_indices;
    std::vector<int> high_indices;
    double D = 0.;
    size_t n = 0;
    double C = 100;

    // get unique values of D
    D_unique_vect.push_back(_DOD_vect[0]);
    for (int i = 0; i < (int)_DOD_vect.size(); i++){
        bool contained = false;
        for (int j = 0; j < (int)D_unique_vect.size(); j++){
            if (_DOD_vect[i] == D_unique_vect[j]){
                contained = true;
                break;
            }
        }
        if (!contained){
            D_unique_vect.push_back(_DOD_vect[i]);
        }
    }
    n = D_unique_vect.size();

    if (n > 1)
    {
        // get where DOD is bracketed [D_lo, DOD, D_hi]
        double D_lo = 0;
        double D_hi = 100;

        for (int i = 0; i < (int)_DOD_vect.size(); i++)
        {
            D = _DOD_vect[i];
            if (D < DOD && D > D_lo)
                D_lo = D;
            else if (D >= DOD && D < D_hi)
                D_hi = D;
        }

        // Seperate table into bins
        double D_min = 100.;
        double D_max = 0.;

        for (int i = 0; i < (int)_DOD_vect.size(); i++)
        {
            D = _DOD_vect[i];
            if (D == D_lo)
                low_indices.push_back(i);
            else if (D == D_hi)
                high_indices.push_back(i);

            if (D < D_min){ D_min = D; }
            else if (D > D_max){ D_max = D; }
        }

        // if we're out of the bounds, just make the upper bound equal to the highest input
        if (high_indices.size() == 0)
        {
            for (int i = 0; i != (int)_DOD_vect.size(); i++)
            {
                if (_DOD_vect[i] == D_max)
                    high_indices.push_back(i);
            }
        }

        size_t n_rows_lo = low_indices.size();
        size_t n_rows_hi = high_indices.size();
        size_t n_cols = 2;

        // If we aren't bounded, fill in values
        if (n_rows_lo == 0)
        {
            // Assumes 0% DOD
            for (int i = 0; i < (int)n_rows_hi; i++)
            {
                C_n_low_vect.push_back(0. + i * 500); // cycles
                C_n_low_vect.push_back(100.); // 100 % capacity
            }
        }

        if (n_rows_lo != 0)
        {
            for (int i = 0; i < (int)n_rows_lo; i++)
            {
                C_n_low_vect.push_back(_cycles_vect[low_indices[i]]);
                C_n_low_vect.push_back(_capacities_vect[low_indices[i]]);
            }
        }
        if (n_rows_hi != 0)
        {
            for (int i = 0; i < (int)n_rows_hi; i++)
            {
                C_n_high_vect.push_back(_cycles_vect[high_indices[i]]);
                C_n_high_vect.push_back(_capacities_vect[high_indices[i]]);
            }
        }
        n_rows_lo = C_n_low_vect.size() / n_cols;
        n_rows_hi = C_n_high_vect.size() / n_cols;

        if (n_rows_lo == 0 || n_rows_hi == 0)
        {
            // need a safeguard here
        }

        util::matrix_t<double> C_n_low(n_rows_lo, n_cols, &C_n_low_vect);
        util::matrix_t<double> C_n_high(n_rows_lo, n_cols, &C_n_high_vect);

        // Compute C(D_lo, n), C(D_hi, n)
        double C_Dlo = util::linterp_col(C_n_low, 0, cycle_number, 1);
        double C_Dhi = util::linterp_col(C_n_high, 0, cycle_number, 1);

        if (C_Dlo < 0.)
            C_Dlo = 0.;
        if (C_Dhi > 100.)
            C_Dhi = 100.;

        // Interpolate to get C(D, n)
        C = util::interpolate(D_lo, C_Dlo, D_hi, C_Dhi, DOD);
    }
        // just have one row, single level interpolation
    else
    {
        C = util::linterp_col(params_p->cycle_matrix, 1, cycle_number, 2);
    }

    return C;
}

void calendar_lifetime_state::init(double q0){
    day_age_of_battery = 0;
    last_idx = 0;

    // coefficients based on fractional capacity (0 - 1)
    dq_old = 0;
    dq_new = 0;
    q = q0 * 100;
}

/*
Lifetime Calendar Model
*/
lifetime_calendar::lifetime_calendar(){};

lifetime_calendar::lifetime_calendar(const std::shared_ptr<const battery_lifetime_params> &params):
params_p(params) {
    if (params_p->choice == battery_lifetime_params::MODEL)
        state.init(params_p->calendar_q0);
    else
        state.init(1);

    dt_day = params_p->time->dt_hour / util::hours_per_day;
    if (params_p->choice == battery_lifetime_params::TABLE) {
        // extract and sort calendar life info from table
        for (size_t i = 0; i != params_p->calendar_matrix.nrows(); i++) {
            table_days.emplace_back((int) params_p->calendar_matrix.at(i, 0));
            table_capacity.emplace_back(params_p->calendar_matrix.at(i, 1));
        }
    }
}

lifetime_calendar::lifetime_calendar(const lifetime_calendar& lifetime_calendar):
params_p(lifetime_calendar.params_p)
{
    table_days = lifetime_calendar.table_days;
    table_capacity = lifetime_calendar.table_capacity;
    dt_day = lifetime_calendar.dt_day;
    state = lifetime_calendar.state;
}

void lifetime_calendar::runLifetimeCalendarModel(size_t idx, double T, double SOC)
{
    if (params_p->choice != battery_lifetime_params::NONE)
    {
        // only run once per iteration (need to make the last iteration)
        if (idx > state.last_idx)
        {

            if (idx % util::hours_per_day / params_p->time->dt_hour == 0)
                state.day_age_of_battery++;

            if (params_p->choice == battery_lifetime_params::MODEL)
                runLithiumIonModel(T, SOC);
            else if (params_p->choice == battery_lifetime_params::TABLE)
                runTableModel();

            state.last_idx = idx;
        }
    }
}
void lifetime_calendar::runLithiumIonModel(double T, double SOC_ratio)
{
    double k_cal = params_p->calendar_a * exp(params_p->calendar_b * (1. / T - 1. / 296)) * exp(params_p->calendar_c * (SOC_ratio / T - 1. / 296));
    if (state.dq_old == 0)
        state.dq_new = k_cal * sqrt(dt_day);
    else
        state.dq_new = (0.5 * pow(k_cal, 2) / state.dq_old) * dt_day + state.dq_old;
    state.dq_old = state.dq_new;
    state.q = (params_p->calendar_q0 - (state.dq_new)) * 100;

}
void lifetime_calendar::runTableModel()
{
    size_t n = table_days.size() - 1;
    int day_lo = 0;
    int day_hi = table_days[n];
    double capacity_lo = 100;
    double capacity_hi = 0;

    // interpolation mode
    for (int i = 0; i != (int)table_days.size(); i++)
    {
        int day = table_days[i];
        double capacity = table_capacity[i];
        if (day <= state.day_age_of_battery)
        {
            day_lo = day;
            capacity_lo = capacity;
        }
        if (day > state.day_age_of_battery)
        {
            day_hi = day;
            capacity_hi = capacity;
            break;
        }
    }
    if (day_lo == day_hi)
    {
        day_lo = table_days[n - 1];
        day_hi = table_days[n];
        capacity_lo = table_capacity[n - 1];
        capacity_hi = table_capacity[n];
    }

    state.q = util::interpolate(day_lo, capacity_lo, day_hi, capacity_hi, state.day_age_of_battery);
}

void lifetime_calendar::replaceBattery(double replacement_percent)
{
    state.day_age_of_battery = 0;
    state.q += replacement_percent;
    if (params_p->choice == battery_lifetime_params::MODEL)
        state.q = fmin(params_p->calendar_q0 * 100, state.q);
    state.dq_new = 0;
    state.dq_old = 0;
}

/*
Define Lifetime Model
*/

battery_lifetime::battery_lifetime(const std::shared_ptr<const battery_lifetime_params> &p):
        cycle_model(new lifetime_cycle(p)),
        calendar_model(new lifetime_calendar(p)),
        params(p),
        relative_q(100)
{
}


battery_lifetime::battery_lifetime(const battery_lifetime& lifetime):
        params(lifetime.params),
        cycle_model(new lifetime_cycle(lifetime.params)),
        calendar_model(new lifetime_calendar(lifetime.params)),
        relative_q(lifetime.relative_q)
{
}

void
battery_lifetime::runLifetimeModels(const size_t &lifetime_index, double SOC, bool charge_changed, double T_battery)
{
    double q_last = relative_q;

    if (relative_q > 0)
    {
        if (charge_changed)
            cycle_model->runCycleLifetime(100. - SOC);

        calendar_model->runLifetimeCalendarModel(lifetime_index, T_battery, SOC * 0.01);

        // total capacity is min of cycle (Q_neg) and calendar (Q_li) capacity
        relative_q = fmin(cycle_model->get_relative_q(), calendar_model->get_relative_q());
    }
    if (relative_q < 0)
        relative_q = 0;

    // capacity cannot increase
    if (relative_q > q_last)
        relative_q = q_last;
}

void battery_lifetime::replaceBattery(double replacement_percent)
{
//    auto& rep = params.replacement;
//    bool replace = false;
//    if (time.get_year() < rep.replacement_per_yr_schedule.size())
//    {
//        auto num_repl = (size_t)rep.replacement_per_yr_schedule[time.get_year()];
//        for (size_t j_repl = 0; j_repl < num_repl; j_repl++)
//        {
//            if ((time.get_hour() == (j_repl * 8760 / num_repl)) && time.get_step() == 0)
//            {
//                replace = true;
//                break;
//            }
//        }
//    }
//    bool replaced = false;
//    if ((rep.replacement_option == 1 && (state.q - tolerance) <= rep.replacement_capacity) || replace)
//    {
//        state.replacements++;
//
//        double replacement_percent = rep.replacement_percent_per_yr_schedule[time.get_year()];
//        state.q += replacement_percent;
//
//        // for now, only allow augmenting up to original installed capacity
//        state.q = fmin(100., state.q);
//
//        replaced = true;
//
//        cycle_model->replaceBattery(replacement_percent);
//        calendar_model->replaceBattery(replacement_percent);
//    }
//    return replaced;

    cycle_model->replaceBattery(replacement_percent);
    calendar_model->replaceBattery(replacement_percent);
    relative_q = fmin(cycle_model->get_relative_q(), calendar_model->get_relative_q());

}




