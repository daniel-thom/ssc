#include <math.h>

#include <cmath>
#include <cfloat>
#include <sstream>
#include <algorithm>

#include "lib_storage_capacity.h"

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

void capacity_interface::check_SOC(capacity_state& state, double SOC_min, double SOC_max, double dt_hour)
{
    double q_upper = state.qmax * SOC_max * 0.01;
    double q_lower = state.qmax * SOC_min * 0.01;
    double I_orig = state.I;

    // set capacity to upper thermal limit
    if (q_upper > state.qmax_thermal * SOC_max * 0.01)
        q_upper = state.qmax_thermal * SOC_max * 0.01;

    // check if overcharged
    if (state.q0 > q_upper )
    {
        if (fabs(state.I) > tolerance)
        {
            state.I += (state.q0 - q_upper) / dt_hour;
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
            state.I += (state.q0 - q_lower) / dt_hour;
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
capacity_kibam_t::capacity_kibam_t(){ /* nothing to do */}

capacity_kibam_t::capacity_kibam_t(double q20, double t1, double q1, double q10, double SOC_in, double SOC_mx, double SOC_mi)
{
    qmax0 = q20;
    SOC_init = SOC_in;
    SOC_min = SOC_mi;
    SOC_max = SOC_mx;
    state.kibam.q10 = q10;
    state.kibam.q20 = q20;
    state.kibam.I20 = q20/20.;

    // parameters for c, k calculation
    q1 = q1;
    q2 = q10;
    t1 = t1;
    t2 = 10.;
    F1 = q1 / q20; // use t1, 20
    F2 = q1 / q10;  // use t1, 10

    // compute the parameters
    parameter_compute();
    qmax0 = state.qmax;

    // initializes to full battery
    replace_battery();
}
capacity_kibam_t * capacity_kibam_t::clone(){ return new capacity_kibam_t(*this); }
void capacity_kibam_t::copy(capacity_interface * capacity)
{
    if (auto* tmp = dynamic_cast<capacity_kibam_t*>(capacity)){
        state = tmp->state;
        qmax0 = tmp->qmax0;
        SOC_init = tmp->SOC_init;
        SOC_max = tmp->SOC_max;
        SOC_min = tmp->SOC_min;
        dt_hour = tmp->dt_hour;

        t1 = tmp->t1;
        t2 = tmp->t2;
        q1 = tmp->q1;
        q2 = tmp->q2;
        F1 = tmp->F1;
        F2 = tmp->F2;
        c = tmp->c;
        k = tmp->k;

    }

}

void capacity_kibam_t::replace_battery()
{
    // Assume initial charge is max capacity
    state.q0 = qmax0 * SOC_init * 0.01;
    state.kibam.q1_0 = state.q0 * c;
    state.kibam.q2_0 = state.q0 - state.kibam.q1_0;
    state.qmax = qmax0;
    state.SOC = SOC_init;
}

double capacity_kibam_t::c_compute(double F, double t1, double t2, double k_guess)
{
    double num = F*(1 - exp(-k_guess*t1))*t2 - (1 - exp(-k_guess*t2))*t1;
    double denom = F*(1 - exp(-k_guess*t1))*t2 - (1 - exp(-k_guess*t2))*t1 - k_guess*F*t1*t2 + k_guess*t1*t2;
    return (num / denom);
}

double capacity_kibam_t::q1_compute(double q10, double q0, double dt, double I)
{
    double A = q10*exp(-k * dt);
    double B = (q0 * k * c - I) * (1 - exp(-k * dt)) / k;
    double C = I * c * (k * dt - 1 + exp(-k * dt)) / k;
    return (A + B - C);
}

double capacity_kibam_t::q2_compute(double q20, double q0, double dt, double I)
{
    double A = q20*exp(-k * dt);
    double B = q0 * (1 - c) * (1 - exp(-k * dt));
    double C = I * (1 - c) * (k * dt - 1 + exp(-k * dt)) / k;
    return (A + B - C);
}

double capacity_kibam_t::Icmax_compute(double q10, double q0, double dt)
{
    double num = -k * c * state.qmax + k * q10 * exp(-k * dt) + q0 * k * c * (1 - exp(-k * dt));
    double denom = 1 - exp(-k * dt) + c * (k * dt - 1 + exp(-k * dt));
    return (num / denom);
}

double capacity_kibam_t::Idmax_compute(double q10, double q0, double dt)
{
    double num = k * q10 * exp(-k * dt) + q0 * k * c * (1 - exp(-k * dt));
    double denom = 1 - exp(-k * dt) + c * (k * dt - 1 + exp(-k * dt));
    return (num / denom);
}

double capacity_kibam_t::qmax_compute()
{
    double num = state.kibam.q20*((1 - exp(-k * 20)) * (1 - c) + k * c * 20);
    double denom = k * c * 20;
    return (num / denom);
}

double capacity_kibam_t::qmax_of_i_compute(double T)
{
    return ((state.qmax * k * c * T) / (1 - exp(-k * T) + c * (k * T - 1 + exp(-k * T))));
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
        c1 = c_compute(F1, t1, 20, k_guess);
        c2 = c_compute(F2, t1, t2, k_guess);

        if (fabs(c1 - c2) < minRes)
        {
            minRes = fabs(c1 - c2);
            k = k_guess;
            c = 0.5 * (c1 + c2);
        }
    }
    state.qmax = qmax_compute();
}

void capacity_kibam_t::updateCapacity(double &I, double dt_hour)
{
    if (fabs(I) < low_tolerance)
        I = 0;

    state.DOD_prev = state.DOD;
    state.I_loss = 0.;
    state.I = I;
    dt_hour = dt_hour;

    double Idmax = 0.;
    double Icmax = 0.;
    double Id = 0.;
    double Ic = 0.;
    double q1 = 0.;
    double q2 = 0.;

    if (state.I > 0)
    {
        Idmax = Idmax_compute(state.kibam.q1_0, state.q0, dt_hour);
        Id = fmin(state.I, Idmax);
        state.I = Id;
    }
    else if (state.I < 0)
    {
        Icmax = Icmax_compute(state.kibam.q1_0, state.q0, dt_hour);
        Ic = -fmin(fabs(state.I), fabs(Icmax));
        state.I = Ic;
    }

    // new charge levels
    q1 = q1_compute(state.kibam.q1_0, state.q0, dt_hour, state.I);
    q2 = q2_compute(state.kibam.q2_0, state.q0, dt_hour, state.I);

    // Check for thermal effects
    if (q1 + q2 > state.qmax_thermal)
    {
        double q0 = q1 + q2;
        double p1 = q1 / q0;
        double p2 = q2 / q0;
        state.q0 = state.qmax_thermal;
        q1 = state.q0*p1;
        q2 = state.q0*p2;
    }

    // update internal variables
    state.kibam.q1_0 = q1;
    state.kibam.q2_0 = q2;
    state.q0 = q1 + q2;

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

    if (qmax0 * capacity_percent * 0.01 <= state.qmax)
        state.qmax = qmax0 * capacity_percent * 0.01;

    // scale to q0 = qmax if q0 > qmax
    if (state.q0 > state.qmax)
    {
        double q0_orig = state.q0;
        double p = state.qmax / state.q0;
        state.q0 *= p;
        q1 *= p;
        q2 *= p;
        state.I_loss += (q0_orig - state.q0) / dt_hour;
    }
    update_SOC(state);
}

double capacity_kibam_t::get_q1(){ return state.kibam.q1_0; }
double capacity_kibam_t::get_q2(){ return state.kibam.q2_0; }
double capacity_kibam_t::get_q10(){ return state.kibam.q10; }


/*
Define Lithium Ion capacity model
*/
capacity_lithium_ion_t::capacity_lithium_ion_t() {
    /* nothing to do */ }

capacity_lithium_ion_t::capacity_lithium_ion_t(double q, double SOC_init, double SOC_max, double SOC_min) {
    qmax0 = q;
    SOC_init = SOC_init;
    SOC_max = SOC_max;
    SOC_min = SOC_min;
    dt_hour = 0.;

    state.init(q, SOC_init);
}

capacity_lithium_ion_t * capacity_lithium_ion_t::clone(){
    auto clone = new capacity_lithium_ion_t();
}

void capacity_lithium_ion_t::copy(capacity_interface * capacity){
    if (auto* tmp = dynamic_cast<capacity_lithium_ion_t*>(capacity)) {

        state = tmp->state;

        qmax0 = tmp->qmax0;
        SOC_init = tmp->SOC_init;
        SOC_min = tmp->SOC_min;
        SOC_max = tmp->SOC_max;
        dt_hour = tmp->dt_hour;
    }
}

void capacity_lithium_ion_t::replace_battery()
{
    state.q0 = qmax0 * SOC_init * 0.01;
    state.qmax = qmax0;
    state.qmax_thermal = qmax0;
    state.SOC = SOC_init;
}

void capacity_lithium_ion_t::updateCapacity(double &I, double dt)
{
    state.DOD_prev = state.DOD;
    state.I_loss = 0.;
    dt_hour = dt;
    state.I = I;

    // compute charge change ( I > 0 discharging, I < 0 charging)
    state.q0 -= state.I*dt;

    // check if SOC constraints violated, update q0, I if so
    check_SOC(state, SOC_min, SOC_max, dt);

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

    if (qmax0 * capacity_percent * 0.01 <= state.qmax)
        state.qmax = qmax0 * capacity_percent * 0.01;

    if (state.q0 > state.qmax)
    {
        state.I_loss += (state.q0 - state.qmax) / dt_hour;
        state.q0 = state.qmax;
    }

    update_SOC(state);
}
double capacity_lithium_ion_t::get_q1(){return state.q0;}
double capacity_lithium_ion_t::get_q10(){return state.qmax;}

