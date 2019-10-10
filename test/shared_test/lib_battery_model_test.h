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


#ifndef SYSTEM_ADVISOR_MODEL_LIB_BATTERY_THERMAL_TEST_H
#define SYSTEM_ADVISOR_MODEL_LIB_BATTERY_THERMAL_TEST_H


#include <gtest/gtest.h>

#include "lib_battery_model.h"
#include "lib_battery.h"
#include "lib_battery_capacity_test.h"
#include "lib_battery_lifetime_test.h"

static void compareState(const thermal_state& thermal_model, const thermal_state& state, const std::string& msg){
    double tol = 0.01;
    EXPECT_NEAR(thermal_model.capacity_percent, state.capacity_percent, tol) << msg;
    EXPECT_NEAR(thermal_model.T_batt_avg, state.T_batt_avg, tol) << msg;
    EXPECT_NEAR(thermal_model.T_batt_init, state.T_batt_init, tol) << msg;
    EXPECT_NEAR(thermal_model.T_room_init, state.T_room_init, tol) << msg;
    EXPECT_NEAR(thermal_model.next_time_at_current_T_room, state.next_time_at_current_T_room, tol) << msg;

}

class lib_battery_thermal_test : public ::testing::Test
{
protected:
    std::unique_ptr<battery_thermal> model;
    std::shared_ptr<storage_time_params> time;

    std::shared_ptr<battery_thermal_params> params;
    double tol = 0.01;
    double error;

    double mass = 507;
    double surface_area = 0.58*0.58 * 6;
    double batt_R = 0.2;
    double Cp = 1004;
    double h = 500;
    std::vector<double> T_room = {290,290,295,295,290,270,270};
    util::matrix_t<double> capacityVsTemperature;

    double step_hour = 1;
    int nyears = 1;

public:

    void SetUp() override {
        double vals3[] = { 263.15, 60, 273.15, 80, 298.15, 100, 318.15, 100 };
        capacityVsTemperature.assign(vals3, 4, 2);
        time = std::make_shared<storage_time_params>(step_hour, nyears);
    }
    void CreateModel(double Cp_){
        params = std::shared_ptr<battery_thermal_params>(new battery_thermal_params({time, mass, surface_area, batt_R, Cp_,
                                         capacityVsTemperature, h, T_room}));
        model = std::unique_ptr<battery_thermal>(new battery_thermal(params));
    }
};

class lib_battery_losses_test : public ::testing::Test
{
protected:
    std::unique_ptr<battery_losses> model;
    std::shared_ptr<storage_time_params> time;

    std::shared_ptr<battery_losses_params> params;

    double tol = 0.01;
    double error;

    std::vector<double> chargingLosses;
    std::vector<double> dischargingLosses;

    std::vector<double> fullLosses;

    double dt_hour = 1;
    int nyears = 1;

public:

    void SetUp() override {
        // losses
        for (size_t m = 0; m < 12; m++) {
            chargingLosses.push_back((double)m);
            dischargingLosses.push_back((double)m + 1.);
        }
        for (size_t i = 0; i < 8760; i++) {
            fullLosses.push_back(i);
        }
    }
};


static void compareState(const battery_state& batt_state, const battery_state& state, const std::string& msg){
    compareState(batt_state.capacity, state.capacity, msg);
    EXPECT_NEAR(batt_state.batt_voltage, state.batt_voltage, 0.01);
    compareState(batt_state.lifetime, state.lifetime, msg);
    compareState(batt_state.thermal, state.thermal, msg);
}

// entire suite of tests runs in 177ms - 10/7
class lib_battery_test : public ::testing::Test
{
public:

    // capacity
    double q = 1000;
    double SOC_init = 50;
    double SOC_min = 5;
    double SOC_max = 95;

    // voltage
    int n_series = 139;
    int n_strings = 9;
    double Vnom_default = 3.6;
    double resistance = 0.2;
    double Vfull = 4.1;
    double Vexp = 4.05;
    double Vnom = 3.4;
    double Qfull = 2.25;
    double Qexp = 0.04;
    double Qnom = 2.0;
    double C_rate = 0.2;

    // lifetime
    util::matrix_t<double> cycleLifeMatrix;
    util::matrix_t<double> calendarLifeMatrix;
    double calendar_q0 = 1.02;
    double calendar_a = 2.66e-3;
    double calendar_b = -7280;
    double calendar_c = 930;

    // thermal
    double mass = 507;
    double surface_area = 0.58*0.58 * 6;
    double Cp = 1004;
    double h = 500;
    std::vector<double> T_room;
    util::matrix_t<double> capacityVsTemperature;

    // losses
    std::vector<double> monthlyLosses;
    std::vector<double> fullLosses;
    std::vector<double> fullLossesMinute;
    int lossChoice;

    // battery
    double dt_hour = 1;

    // models
    double tol = 0.01;

    std::shared_ptr<battery> batteryModel;

    std::shared_ptr<battery_properties_params> params;
    std::shared_ptr<storage_time_params> time;

    void SetUp() override
    {
        // lifetime
        double vals[] = { 20, 0, 100, 20, 5000, 80, 20, 10000, 60, 80, 0, 100, 80, 1000, 80, 80, 2000, 60 };
        cycleLifeMatrix.assign(vals, 6, 3);

        T_room.emplace_back(293.15);
        double vals3[] = { 263.15, 60, 273.15, 80, 298.15, 100, 318.15, 100 };
        capacityVsTemperature.assign(vals3, 4, 2);

        // losses
        for (size_t m = 0; m < 12; m++) {
            monthlyLosses.push_back((double)m);
        }
        for (size_t i = 0; i < 8760; i++) {
            fullLosses.push_back(0);
        }
        for (size_t i = 0; i < 8760 * 60; i++) {
            fullLossesMinute.push_back(0);
        }

        time = std::make_shared<storage_time_params>(dt_hour, 1);

        auto cap_params = std::shared_ptr<const battery_capacity_params>(new battery_capacity_params(
                {time, q, SOC_init, SOC_min, SOC_max}));
        auto vol_params = std::shared_ptr<battery_voltage_params>(new battery_voltage_params(
                {n_series, n_strings, Vnom_default, resistance, battery_voltage_params::MODEL,
                                                  Vfull, Vexp, Vnom, Qfull, Qexp, Qnom, C_rate}));
        auto life_params = std::shared_ptr<const battery_lifetime_params>(new battery_lifetime_params(
                {time, cycleLifeMatrix,battery_lifetime_params::MODEL, calendar_q0,calendar_a,calendar_b,calendar_c}));

        auto temp_params = std::shared_ptr<const battery_thermal_params>(new battery_thermal_params(
                {time, mass, surface_area, resistance, Cp, capacityVsTemperature, h, T_room}));

        auto loss_params = std::shared_ptr<battery_losses_params>(new battery_losses_params(
                {time, battery_losses_params::MONTHLY, monthlyLosses, monthlyLosses, monthlyLosses, fullLosses}));

        params = std::shared_ptr<battery_properties_params>(new battery_properties_params({battery_properties_params::LITHIUM_ION,
                                            cap_params,
                                            vol_params,
                                            temp_params,
                                            life_params,
                                            loss_params}));
        batteryModel = std::shared_ptr<battery>(new battery(params));
    }

};

#endif //SYSTEM_ADVISOR_MODEL_LIB_BATTERY_THERMAL_TEST_H
