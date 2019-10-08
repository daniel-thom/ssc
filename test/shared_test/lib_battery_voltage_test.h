#ifndef SAM_SIMULATION_CORE_LIB_BATTERY_VOLTAGE_TEST_H
#define SAM_SIMULATION_CORE_LIB_BATTERY_VOLTAGE_TEST_H

#include <gtest/gtest.h>

#include "lib_util.h"
#include "lib_battery_voltage.h"
#include "lib_battery_capacity_test.h"
#include "lib_battery.h"

class lib_battery_voltage_test : public ::testing::Test
{
protected:
    std::unique_ptr<battery_voltage_interface> model;
    std::unique_ptr<battery_capacity_interface> cap;

    std::shared_ptr<storage_time_params> time;

    battery_voltage_params params;
    std::shared_ptr<const battery_capacity_params> cap_params;
    double tol = 0.01;
    double error;

    int n_cells_series = 139;
    int n_strings = 9;
    double voltage_nom = 3.6;
    double R = 0.2;

    double dt_hour = 1;
    int nyears = 1;

public:
    void SetUp() override {
        time = std::make_shared<storage_time_params>(dt_hour, nyears);
        cap_params = std::shared_ptr<battery_capacity_params>(new battery_capacity_params({time, 10, 50, 5, 95}));
        cap = std::unique_ptr<capacity_lithium_ion>(new capacity_lithium_ion(cap_params));
    }
};

class voltage_table_lib_battery_voltage_test : public lib_battery_voltage_test
{
protected:
    std::vector<double> table_vals = {75, 1.5, 25, 3.5};

    void SetUp() override {
        lib_battery_voltage_test::SetUp();
        params = battery_voltage_params({n_cells_series, n_strings, voltage_nom, R, battery_voltage_params::TABLE});
        util::matrix_t<double> table = util::matrix_t<double>(2, 2, &table_vals);
        params.voltage_matrix = table;

        model = std::unique_ptr<battery_voltage_interface>(new voltage_table(params));
    };
};

class voltage_dynamic_lib_battery_voltage_test : public lib_battery_voltage_test
{
protected:
    double Vfull = 4.1;
    double Vexp = 4.05;
    double Vnom = 3.4;
    double Qfull = 2.25;
    double Qexp = 0.04;
    double Qnom = 2.0;
    double C_rate = 0.2;

    void SetUp() override {
        lib_battery_voltage_test::SetUp();
        params = battery_voltage_params({n_cells_series, n_strings, voltage_nom, R,
                                         battery_voltage_params::TABLE, Vfull, Vexp, Vnom,
                                         Qfull, Qexp, Qnom, C_rate});

        model = std::unique_ptr<battery_voltage_interface>(new voltage_dynamic(params));
    }
};

class voltage_vanadium_redox_lib_battery_voltage_test : public lib_battery_voltage_test
{
protected:
    void SetUp(){
        lib_battery_voltage_test::SetUp();
        params = battery_voltage_params({n_cells_series, n_strings, voltage_nom, R, battery_voltage_params::MODEL});
        params.resistance = R;

        model = std::unique_ptr<battery_voltage_interface>(new voltage_vanadium_redox(params));
    }
};

#endif //SAM_SIMULATION_CORE_LIB_BATTERY_VOLTAGE_TEST_H
