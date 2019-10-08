#include <gtest/gtest.h>

//#include "lib_battery_capacity.h"
#include "lib_battery.h"
#include "lib_battery_voltage_test.h"

TEST_F(voltage_table_lib_battery_voltage_test, SetUpTest) {
    EXPECT_EQ(model->cell_voltage(), 3.6);
}

TEST_F(voltage_table_lib_battery_voltage_test, updateCapacityTest){
    double I = 2;
    cap->updateCapacity(I, dt_hour); // cap = 3
    model->updateVoltage(cap.get(), 0, dt_hour);
    EXPECT_NEAR(model->cell_voltage(), 1.65, tol);
}

TEST_F(voltage_dynamic_lib_battery_voltage_test, SetUpTest) {
    EXPECT_EQ(model->cell_voltage(), 4.1);
}

TEST_F(voltage_dynamic_lib_battery_voltage_test, NickelMetalHydrideFromPaperTest){
    // Figure 3 from A Generic Battery Model for the Dynamic Simulation of Hybrid Electric Vehicles
    cap = std::unique_ptr<capacity_lithium_ion_t>(new capacity_lithium_ion_t(6.5, 100, 100, 0));

    model = std::unique_ptr<voltage_t>(new voltage_dynamic_t(1, 1,1.2, 1.4,
            1.25, 1.2, 6.5, 1.3, 5.2, 0.2, 0.0046));
    std::vector<double> dt_hr = {1./6., 1./3., 1./3.};
    // testing with 1lt curve
    std::vector<double> voltages = {1.25, 1.22, 1.17};
    for (size_t i = 0; i < 3; i++){
        double I = 6.5;
        cap->updateCapacity(I, dt_hr[i]);
        model->updateVoltage(cap.get(), 0, dt_hr[i]);
        EXPECT_NEAR(model->battery_voltage(), voltages[i], 0.05) << "NickelMetalHydrideFromPaperTest: " + std::to_string(i);
    }
}

TEST_F(voltage_dynamic_lib_battery_voltage_test, updateCapacityTest){
    double I = 2;
    cap->updateCapacity(I, dt_hour); // qmx = 10, q0 = 3
    model->updateVoltage(cap.get(), 0, dt_hour);
    EXPECT_NEAR(model->cell_voltage(), 3.9, tol);

    I = -2;
    cap->updateCapacity(I, dt_hour); // qmx = 10, q0 = 5
    model->updateVoltage(cap.get(), 0, dt_hour);
    EXPECT_NEAR(model->cell_voltage(), 4.1, tol);

    I = 5;
    cap->updateCapacity(I, dt_hour); // qmx = 10, I = 4.5, q0 = 0.5
    model->updateVoltage(cap.get(), 0, dt_hour);
    EXPECT_NEAR(model->cell_voltage(), 2.49, tol);
}

TEST_F(voltage_vanadium_redox_lib_battery_voltage_test, SetUpTest) {
    EXPECT_EQ(model->cell_voltage(), 3.6);
}

TEST_F(voltage_vanadium_redox_lib_battery_voltage_test, updateCapacityTest){
    double I = 2;
    cap->updateCapacity(I, dt_hour); // qmx = 10, q0 = 3
    model->updateVoltage(cap.get(), 0, dt_hour);
    EXPECT_NEAR(model->cell_voltage(), 3.55, tol);

    I = -2;
    cap->updateCapacity(I, dt_hour); // qmx = 10, q0 = 5
    model->updateVoltage(cap.get(), 0, dt_hour);
    EXPECT_NEAR(model->cell_voltage(), 3.64, tol);

    I = 5;
    cap->updateCapacity(I, dt_hour); // qmx = 10,q0 = 0.5
    model->updateVoltage(cap.get(), 0, dt_hour);
    EXPECT_NEAR(model->cell_voltage(), 3.5, tol);
}