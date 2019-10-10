#include <gtest/gtest.h>

//#include "lib_battery_capacity.h"
#include "lib_battery.h"
#include "lib_battery_voltage_test.h"

TEST_F(voltage_table_lib_battery_voltage_test, SetUpTest) {
    EXPECT_EQ(model->get_cell_voltage(), 3.6) << "SetUpTest";
}

TEST_F(voltage_table_lib_battery_voltage_test, updateCapacityTest){
    double I = 2;
    cap->updateCapacity(I); // cap = 3
    model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
    EXPECT_NEAR(model->get_cell_voltage(), 1.65, tol) << "UpdateCapacityTest";
}

TEST_F(voltage_dynamic_lib_battery_voltage_test, SetUpTest) {
    EXPECT_EQ(model->get_cell_voltage(), 4.1);
}

TEST_F(voltage_dynamic_lib_battery_voltage_test, NickelMetalHydrideFromPaperTest){
    // Figure 3 from A Generic Battery Model for the Dynamic Simulation of Hybrid Electric Vehicles
    auto cap_params_Ni = std::shared_ptr<battery_capacity_params>(new battery_capacity_params({cap_params->time, 6.5, 100, 0, 100}));
    cap = std::unique_ptr<capacity_lithium_ion>(new capacity_lithium_ion(cap_params_Ni));

    auto vol_params_Ni = std::shared_ptr<battery_voltage_params>(new battery_voltage_params({1, 1, 1.2, 0.0046, battery_voltage_params::MODEL,
                                                 1.4, 1.25, 1.2, 6.5, 1.3, 5.2, 0.2}));
    model = std::unique_ptr<battery_voltage_interface>(new voltage_dynamic(vol_params_Ni));

    // testing with 1lt curve
    std::vector<double> dt_hr = {1./6., 1./3., 1./3.};
    std::vector<double> voltages = {1.25, 1.22, 1.17};
    for (size_t i = 0; i < 3; i++){
        double I = 6.5;
        time->dt_hour = dt_hr[i];           // other time parameters are unused by capacity model
        cap->updateCapacity(I);
        model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
        EXPECT_NEAR(model->get_battery_voltage(), voltages[i], 0.05) << "NickelMetalHydrideFromPaperTest: " + std::to_string(i);
    }
}

TEST_F(voltage_dynamic_lib_battery_voltage_test, updateCapacityTest){
    double I = 2;
    cap->updateCapacity(I); // qmx = 10, q0 = 3
    model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
    EXPECT_NEAR(model->get_cell_voltage(), 3.9, tol);

    I = -2;
    cap->updateCapacity(I); // qmx = 10, q0 = 5
    model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
    EXPECT_NEAR(model->get_cell_voltage(), 4.1, tol);

    I = 5;
    cap->updateCapacity(I); // qmx = 10, I = 4.5, q0 = 0.5
    model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
    EXPECT_NEAR(model->get_cell_voltage(), 2.49, tol);
}

TEST_F(voltage_vanadium_redox_lib_battery_voltage_test, SetUpTest) {
    EXPECT_EQ(model->get_cell_voltage(), 3.6);
}

TEST_F(voltage_vanadium_redox_lib_battery_voltage_test, updateCapacityTest){
    double I = 2;
    cap->updateCapacity(I); // qmx = 10, q0 = 3
    model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
    EXPECT_NEAR(model->get_cell_voltage(), 3.55, tol);

    I = -2;
    cap->updateCapacity(I); // qmx = 10, q0 = 5
    model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
    EXPECT_NEAR(model->get_cell_voltage(), 3.64, tol);

    I = 5;
    cap->updateCapacity(I); // qmx = 10,q0 = 0.5
    model->updateVoltage(cap->get_I(), cap->get_q1(), cap->get_q10(), 0);
    EXPECT_NEAR(model->get_cell_voltage(), 3.5, tol);
}