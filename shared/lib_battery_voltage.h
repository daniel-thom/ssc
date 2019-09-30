/**
BSD-3-Clause
Copyright 2019 Alliance for Sustainable Energy, LLC
Redistribution and use in source and binary forms, with or without modification, are permitted provided
that the following conditions are met :
1.	Redistributions of source code must retain the above copyright notice, this list of conditions
and the following disclaimer.
2.	Redistributions in binary form must reproduce the above copyright notice, this list of conditions
and the following disclaimer in the documentation and/or other materials provided with the distribution.
3.	Neither the name of the copyright holder nor the names of its contributors may be used to endorse
or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER, CONTRIBUTORS, UNITED STATES GOVERNMENT OR UNITED STATES
DEPARTMENT OF ENERGY, NOR ANY OF THEIR EMPLOYEES, BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SYSTEM_ADVISOR_MODEL_LIB_BATTERY_VOLTAGE_H
#define SYSTEM_ADVISOR_MODEL_LIB_BATTERY_VOLTAGE_H


#include <vector>
#include <map>
#include <string>
#include <stdio.h>
#include <algorithm>

#include "lib_util.h"
#include "lib_storage_params.h"

/*
Voltage Base class.
All voltage models are based on one-cell, but return the voltage for one battery
*/
class thermal_t;
struct capacity_state;
class battery_voltage_interface
{
public:
    explicit battery_voltage_interface() {}

    virtual ~battery_voltage_interface(){};

    virtual void updateVoltage(const capacity_state &capacity, double T_battery_K = 0) = 0;

    virtual void set_batt_voltage(double) = 0;

    virtual double get_battery_voltage() = 0; // voltage of one battery
    virtual double get_cell_voltage() = 0; // voltage of one cell
    virtual double get_battery_voltage_nominal() = 0; // nominal voltage of battery
    virtual double get_R_battery() = 0; // computed battery resistance

    virtual battery_voltage_params get_params() const = 0;

};

// A row in the table
class table_point
{
public:
    table_point(double DOD = 0., double V = 0.) :
            _DOD(DOD), _V(V){}
    double DOD() const{ return _DOD; }
    double V() const{ return _V; }

private:
    double _DOD;
    double _V;
};

struct byDOD
{
    bool operator()(table_point const &a, table_point const &b){ return a.DOD() < b.DOD(); }
};


class voltage_table : public battery_voltage_interface
{
public:
    voltage_table(const battery_voltage_params& p);

    voltage_table(const voltage_table&);

    void updateVoltage(const capacity_state &capacity, double T_battery_K = 0) override;

    void set_batt_voltage(double v) override { cell_voltage_state = v / params.num_cells_series;}

    double get_battery_voltage() override {return params.num_cells_series * cell_voltage_state;};
    double get_cell_voltage() override { return cell_voltage_state; };
    double get_battery_voltage_nominal() {return params.num_cells_series * params.Vnom_default;};
    double get_R_battery() override { return params.resistance * params.num_cells_series / params.num_strings; }

    battery_voltage_params get_params() const override {return params;};

private:
    double cell_voltage_state;         // closed circuit voltage per cell [V]

    const battery_voltage_params params;

    std::vector<table_point> v_table;

    bool exactVoltageFound(double DOD, double &V);
    void prepareInterpolation(double & DOD_lo, double & V_lo, double & DOD_hi, double & V_hi, double DOD);

};

// Shepard + Tremblay Model
class voltage_dynamic : public battery_voltage_interface
{
public:
    voltage_dynamic(const battery_voltage_params& p);

    // copy from voltage to this
    voltage_dynamic(const voltage_dynamic&);

    void parameter_compute();
    void updateVoltage(const capacity_state &capacity, double T_battery_K) override;

    void set_batt_voltage(double v) override { cell_voltage_state = v / params.num_cells_series;}

    double get_battery_voltage() override {return params.num_cells_series * cell_voltage_state;};
    double get_cell_voltage() override { return cell_voltage_state; };
    double get_battery_voltage_nominal() override {return params.num_cells_series * params.Vnom_default;};
    double get_R_battery() override { return params.resistance * params.num_cells_series / params.num_strings; }

    battery_voltage_params get_params() const override {return params;};

protected:
    double cell_voltage_state;         // closed circuit voltage per cell [V]

    const battery_voltage_params params;

    double A;
    double B0;
    double E0;
    double K;

    double voltage_model_tremblay_hybrid(double capacity, double current, double q0);

};

// D'Agostino Vanadium Redox Flow Model
class voltage_vanadium_redox : public battery_voltage_interface
{
public:
    explicit voltage_vanadium_redox(const battery_voltage_params& p);

    // copy from voltage to this
    voltage_vanadium_redox(const voltage_vanadium_redox &);

    void updateVoltage(const capacity_state &capacity, double T_battery_K) override;

    void set_batt_voltage(double v) override { cell_voltage_state = v / params.num_cells_series;}

    double get_battery_voltage() override {return params.num_cells_series * cell_voltage_state;};
    double get_cell_voltage() override { return cell_voltage_state; };
    double get_battery_voltage_nominal() override {return params.num_cells_series * params.Vnom_default;};
    double get_R_battery() override { return params.resistance * params.num_cells_series / params.num_strings; }

    battery_voltage_params get_params() const override {return params;};

private:
    double cell_voltage_state;         // closed circuit voltage per cell [V]

    const battery_voltage_params params;

    constexpr const static double R_molar = 8.314;            // Molar gas constant [J/mol/K]^M
    constexpr const static double F = 26.801 * 3600;          // Faraday constant [As/mol]^M
    constexpr const static double C0 = 1.38;                  // model correction factor^M

    double voltage_model(double q0, double qmax, double I_string, double T);
};


#endif //SYSTEM_ADVISOR_MODEL_LIB_BATTERY_VOLTAGE_H
