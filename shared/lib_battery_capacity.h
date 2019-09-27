#ifndef SYSTEM_ADVISOR_MODEL_LIB_BATTERY_CAPACITY_H
#define SYSTEM_ADVISOR_MODEL_LIB_BATTERY_CAPACITY_H

#include "lib_storage_params.h"

struct capacity_state{
    enum { CHARGE, NO_CHARGE, DISCHARGE };

    double q0;  // [Ah] - Total capacity at timestep
    double qmax; // [Ah] - maximum possible capacity
    double qmax_thermal; // [Ah] - maximum capacity adjusted for temperature affects
    double I;   // [A]  - Current draw during last step
    double I_loss; // [A] - Lifetime and thermal losses
    double SOC; // [%] - State of Charge
    double DOD; // [%] - Depth of Discharge
    double DOD_prev; // [%] - Depth of Discharge of previous step
    bool chargeChange; // [true/false] - indicates if charging state has changed since last step
    int prev_charge; // {CHARGE, NO_CHARGE, DISCHARGE}
    int charge; // {CHARGE, NO_CHARGE, DISCHARGE}

    struct {
        double q1_0; // [Ah] - charge available
        double q2_0; // [Ah] - charge bound
        double q10; //  [Ah] - Capacity at 10 hour discharge rate
        double q20; // [Ah] - Capacity at 20 hour discharge rate
        double I20; // [A]  - Current at 20 hour discharge rate
    } kibam;

    void init(double q, double SOC_init);
};

/*
Base class from which capacity models derive
Note, all capacity models are based on the capacity of one battery
*/
class capacity_interface {
public:

    capacity_interface();

    // virtual destructor
    virtual ~capacity_interface() {};

    // pure virtual functions (abstract) which need to be defined in derived classes
    virtual void updateCapacity(double &I) = 0;
    virtual void updateCapacityForThermal(double capacity_percent) = 0;
    virtual void updateCapacityForLifetime(double capacity_percent) = 0;

    virtual void replace_battery() = 0;

    virtual double get_q1() = 0; // available charge
    virtual double get_q10() = 0; // capacity at 10 hour discharge rate

    virtual capacity_state get_state() = 0;
    virtual void set_state(const capacity_state &state) = 0;

    virtual battery_capacity_params get_params() const = 0;

protected:
    static void check_charge_change(capacity_state &state);

    static void check_SOC(capacity_state &state, const battery_capacity_params &params);

    static void update_SOC(capacity_state &state);
};

/*
KiBaM specific capacity model
*/

class capacity_kibam_t : public capacity_interface
{
public:

    // Public APIs
    capacity_kibam_t(const battery_capacity_params& p);
    ~capacity_kibam_t(){}

    // copy from capacity to this
    capacity_kibam_t(const capacity_kibam_t&);

    void updateCapacity(double &I);
    void updateCapacityForThermal(double capacity_percent);
    void updateCapacityForLifetime(double capacity_percent);

    void replace_battery();

    double get_q1() override; // Available charge
    double get_q2(); // Bound charge
    double get_q10() override; // Capacity at 10 hour discharge rate

    capacity_state get_state() override { return state; }
    void set_state(const capacity_state& new_state) override { state = new_state; }

    battery_capacity_params get_params() const override {return params;};

protected:
    capacity_state state;

    const battery_capacity_params params;

    // parameters for finding c, k, qmax
    double t1;  // [h] - discharge rate for capacity at _q1
    double t2;  // [h] - discharge rate for capacity at q2
    double q1;  // [Ah]- capacity at discharge rate t1
    double q2;  // [Ah] - capacity at discharge rate t2
    double F1;  // [unitless] - internal ratio computation
    double F2;  // [unitless] - internal ratio computation

    // model parameters
    double c;  // [0-1] - capacity fraction
    double k;  // [1/hour] - rate constant

    // unique to kibam
    double c_compute(double F, double t2, double k_guess);
    double q1_compute(double q10, double q0, double I); // may remove some inputs, use class variables
    double q2_compute(double q20, double q0, double I); // may remove some inputs, use class variables
    double Icmax_compute(double q10, double q0);
    double Idmax_compute(double q10, double q0);
    double qmax_compute();
    void parameter_compute();
};

/*
Lithium Ion specific capacity model
*/
class capacity_lithium_ion_t : public capacity_interface
{
public:
    capacity_lithium_ion_t(const battery_capacity_params& p);
    ~capacity_lithium_ion_t(){};

    // copy from capacity to this
    capacity_lithium_ion_t(const capacity_lithium_ion_t&);

    // override public api
    void updateCapacity(double &I) override;
    void updateCapacityForThermal(double capacity_percent) override;
    void updateCapacityForLifetime(double capacity_percent) override;

    void replace_battery() override;

    double get_q1() override;  // Available charge
    double get_q10() override; // Capacity at 10 hour discharge rate

    capacity_state get_state() override { return state; }
    void set_state(const capacity_state& new_state) override { state = new_state; }

    battery_capacity_params get_params() const override {return params;};

protected:
    capacity_state state;

    battery_capacity_params params;
};


#endif //SYSTEM_ADVISOR_MODEL_LIB_BATTERY_CAPACITY_H
