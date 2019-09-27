#include <math.h>

#include <cmath>
#include <cfloat>
#include <sstream>
#include <algorithm>

#include "lib_battery_capacity.h"

void capacity_state::init(double q, double SOC_init){
    q0 = 0.01*SOC_init*q;
    qmax = q;
    qmax_thermal = q;
    I = 0.;
    I_loss = 0.;

    // Initialize SOC, DOD
    SOC = SOC_init;
    DOD = 0;
    DOD_prev = 0;

    // Initialize charging states
    prev_charge = DISCHARGE;
    charge = DISCHARGE;
    chargeChange = false;
}

/*
Define Capacity Model
*/
capacity_interface::capacity_interface() { /* nothing to do */ }

void capacity_interface::check_charge_change(capacity_state& state)
{
    state.charge = capacity_state::NO_CHARGE;

    // charge state
    if (state.I < 0)
        state.charge = capacity_state::CHARGE;
    else if (state.I > 0)
        state.charge = capacity_state::DISCHARGE;

    // Check if charge changed
    state.chargeChange = false;
    if ((state.charge != state.prev_charge) && (state.charge != capacity_state::NO_CHARGE)
        && (state.prev_charge != capacity_state::NO_CHARGE))
    {
        state.chargeChange = true;
        state.prev_charge = state.charge;
    }
}

void capacity_interface::check_SOC(capacity_state &state, const battery_capacity_params &params)
{
    double q_upper = state.qmax * params.maximum_SOC * 0.01;
    double q_lower = state.qmax * params.minimum_SOC * 0.01;
    double I_orig = state.I;

    // set capacity to upper thermal limit
    if (q_upper > state.qmax_thermal * params.maximum_SOC * 0.01)
        q_upper = state.qmax_thermal * params.maximum_SOC * 0.01;

    // check if overcharged
    if (state.q0 > q_upper )
    {
        if (fabs(state.I) > tolerance)
        {
            state.I += (state.q0 - q_upper) / params.dt_hour;
            if (state.I / I_orig < 0)
                state.I = 0;
        }
        state.q0 = q_upper;
    }
        // check if undercharged
    else if (state.q0 < q_lower)
    {
        if (fabs(state.I) > tolerance)
        {
            state.I += (state.q0 - q_lower) / params.dt_hour;
            if (state.I / I_orig < 0)
                state.I = 0;
        }
        state.q0 = q_lower;
    }
}

void capacity_interface::update_SOC(capacity_state& state)
{
    if (state.qmax > 0)
        state.SOC = 100.*(state.q0 / state.qmax_thermal);
    else
        state.SOC = 0.;

    // due to dynamics, it's possible SOC could be slightly above 1 or below 0
    if (state.SOC > 100.0)
        state.SOC = 100.0;
    else if (state.SOC < 0.)
        state.SOC = 0.;

    state.DOD = 100. - state.SOC;
}

/*
Define KiBam Capacity Model
*/
capacity_kibam_t::capacity_kibam_t(const battery_capacity_params& p):
params(p),
state(capacity_state())
{
    state.init(params.qmax, params.initial_SOC);
    state.kibam.q10 = params.lead_acid.q10;
    state.kibam.q20 = params.lead_acid.q20;
    state.kibam.I20 = params.lead_acid.q20/20.;

    // parameters for c, k calculation
    q1 = params.lead_acid.q1;
    q2 = params.lead_acid.q10;
    t1 = params.lead_acid.t1;
    t2 = 10.;
    F1 = params.lead_acid.q1 / params.lead_acid.q20; // use t1, 20
    F2 = params.lead_acid.q1 / params.lead_acid.q10;  // use t1, 10

    // compute the parameters
    parameter_compute();

    // initializes to full battery
    replace_battery();
}

capacity_kibam_t::capacity_kibam_t(const capacity_kibam_t& capacity):
params(capacity.get_params())
{
    state = capacity.state;

    t1 = capacity.t1;
    t2 = capacity.t2;
    q1 = capacity.q1;
    q2 = capacity.q2;
    F1 = capacity.F1;
    F2 = capacity.F2;
    c = capacity.c;
    k = capacity.k;
}

void capacity_kibam_t::replace_battery()
{
    // Assume initial charge is max capacity
    state.q0 = params.qmax * params.initial_SOC * 0.01;
    state.kibam.q1_0 = state.q0 * c;
    state.kibam.q2_0 = state.q0 - state.kibam.q1_0;
    state.qmax = params.qmax;
    state.SOC = params.initial_SOC;
}

double capacity_kibam_t::c_compute(double F, double t2_, double k_guess)
{
    double num = F*(1 - exp(-k_guess*t1))*t2_ - (1 - exp(-k_guess*t2_))*t1;
    double denom = F*(1 - exp(-k_guess*t1))*t2_ - (1 - exp(-k_guess*t2_))*t1 - k_guess*F*t1*t2_ + k_guess*t1*t2_;
    return (num / denom);
}

double capacity_kibam_t::q1_compute(double q10, double q0, double I)
{
    double A = q10*exp(-k * params.dt_hour);
    double B = (q0 * k * c - I) * (1 - exp(-k * params.dt_hour)) / k;
    double C = I * c * (k * params.dt_hour - 1 + exp(-k * params.dt_hour)) / k;
    return (A + B - C);
}

double capacity_kibam_t::q2_compute(double q20, double q0, double I)
{
    double A = q20*exp(-k * params.dt_hour);
    double B = q0 * (1 - c) * (1 - exp(-k * params.dt_hour));
    double C = I * (1 - c) * (k * params.dt_hour - 1 + exp(-k * params.dt_hour)) / k;
    return (A + B - C);
}

double capacity_kibam_t::Icmax_compute(double q10, double q0)
{
    double num = -k * c * state.qmax + k * q10 * exp(-k * params.dt_hour) + q0 * k * c * (1 - exp(-k * params.dt_hour));
    double denom = 1 - exp(-k * params.dt_hour) + c * (k * params.dt_hour - 1 + exp(-k * params.dt_hour));
    return (num / denom);
}

double capacity_kibam_t::Idmax_compute(double q10, double q0)
{
    double num = k * q10 * exp(-k * params.dt_hour) + q0 * k * c * (1 - exp(-k * params.dt_hour));
    double denom = 1 - exp(-k * params.dt_hour) + c * (k * params.dt_hour - 1 + exp(-k * params.dt_hour));
    return (num / denom);
}

double capacity_kibam_t::qmax_compute()
{
    double num = state.kibam.q20*((1 - exp(-k * 20)) * (1 - c) + k * c * 20);
    double denom = k * c * 20;
    return (num / denom);
}

void capacity_kibam_t::parameter_compute()
{
    double k_guess = 0.;
    double c1 = 0.;
    double c2 = 0.;
    double minRes = 10000.;

    for (int i = 0; i < 5000; i++)
    {
        k_guess = i*0.001;
        c1 = c_compute(F1, 20, k_guess);
        c2 = c_compute(F2, t2, k_guess);

        if (fabs(c1 - c2) < minRes)
        {
            minRes = fabs(c1 - c2);
            k = k_guess;
            c = 0.5 * (c1 + c2);
        }
    }
    state.qmax = qmax_compute();
}

void capacity_kibam_t::updateCapacity(double &I)
{
    if (fabs(I) < low_tolerance)
        I = 0;

    state.DOD_prev = state.DOD;
    state.I_loss = 0.;
    state.I = I;

    double Idmax = 0.;
    double Icmax = 0.;
    double Id = 0.;
    double Ic = 0.;
    double q1_ = 0.;
    double q2_ = 0.;

    if (state.I > 0)
    {
        Idmax = Idmax_compute(state.kibam.q1_0, state.q0);
        Id = fmin(state.I, Idmax);
        state.I = Id;
    }
    else if (state.I < 0)
    {
        Icmax = Icmax_compute(state.kibam.q1_0, state.q0);
        Ic = -fmin(fabs(state.I), fabs(Icmax));
        state.I = Ic;
    }

    // new charge levels
    q1_ = q1_compute(state.kibam.q1_0, state.q0, state.I);
    q2_ = q2_compute(state.kibam.q2_0, state.q0, state.I);

    // Check for thermal effects
    if (q1_ + q2_ > state.qmax_thermal)
    {
        double q0 = q1_ + q2_;
        double p1 = q1_ / q0;
        double p2 = q2_ / q0;
        state.q0 = state.qmax_thermal;
        q1_ = state.q0 * p1;
        q2_ = state.q0 * p2;
    }

    // update internal variables
    state.kibam.q1_0 = q1_;
    state.kibam.q2_0 = q2_;
    state.q0 = q1_ + q2_;

    update_SOC(state);
    check_charge_change(state);

    // Pass current out
    I = state.I;
}
void capacity_kibam_t::updateCapacityForThermal(double capacity_percent)
{
    // Modify the lifetime degraded capacity by the thermal effect
    state.qmax_thermal = state.qmax*capacity_percent*0.01;
}
void capacity_kibam_t::updateCapacityForLifetime(double capacity_percent)
{

    if (params.qmax * capacity_percent * 0.01 <= state.qmax)
        state.qmax = params.qmax * capacity_percent * 0.01;

    // scale to q0 = qmax if q0 > qmax
    if (state.q0 > state.qmax)
    {
        double q0_orig = state.q0;
        double p = state.qmax / state.q0;
        state.q0 *= p;
        q1 *= p;
        q2 *= p;
        state.I_loss += (q0_orig - state.q0) / params.dt_hour;
    }
    update_SOC(state);
}

double capacity_kibam_t::get_q1(){ return state.kibam.q1_0; }
double capacity_kibam_t::get_q2(){ return state.kibam.q2_0; }
double capacity_kibam_t::get_q10(){ return state.kibam.q10; }


/*
Define Lithium Ion capacity model
*/
capacity_lithium_ion_t::capacity_lithium_ion_t(const battery_capacity_params& p):
params(p),
state(capacity_state())
{
    state.init(params.qmax, params.initial_SOC);
}

capacity_lithium_ion_t::capacity_lithium_ion_t(const capacity_lithium_ion_t& capacity):
params(capacity.get_params()){
    state = capacity.state;
}

void capacity_lithium_ion_t::replace_battery()
{
    state.q0 = params.qmax * params.initial_SOC * 0.01;
    state.qmax = params.qmax;
    state.qmax_thermal = params.qmax;
    state.SOC = params.initial_SOC;
}

void capacity_lithium_ion_t::updateCapacity(double &I)
{
    state.DOD_prev = state.DOD;
    state.I_loss = 0.;
    state.I = I;

    // compute charge change ( I > 0 discharging, I < 0 charging)
    state.q0 -= state.I*params.dt_hour;

    // check if SOC constraints violated, update q0, I if so
    check_SOC(state, params);

    // update SOC, DOD
    update_SOC(state);
    check_charge_change(state);

    // Pass current out
    I = state.I;
}
void capacity_lithium_ion_t::updateCapacityForThermal(double capacity_percent)
{
    // Modify the lifetime degraded capacity by the thermal effect
    state.qmax_thermal = state.qmax*capacity_percent*0.01;
}
void capacity_lithium_ion_t::updateCapacityForLifetime(double capacity_percent)
{

    if (params.qmax * capacity_percent * 0.01 <= state.qmax)
        state.qmax = params.qmax * capacity_percent * 0.01;

    if (state.q0 > state.qmax)
    {
        state.I_loss += (state.q0 - state.qmax) / params.dt_hour;
        state.q0 = state.qmax;
    }

    update_SOC(state);
}
double capacity_lithium_ion_t::get_q1(){return state.q0;}
double capacity_lithium_ion_t::get_q10(){return state.qmax;}

