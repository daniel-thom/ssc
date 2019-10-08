#include <gtest/gtest.h>

#include "lib_battery_model_test.h"

TEST_F(lib_battery_thermal_test, SetUpTest){
    CreateModel(Cp);
    EXPECT_NEAR(model->get_T_battery(), 290, tol);
    EXPECT_NEAR(model->capacity_percent(), 100, tol);
}

TEST_F(lib_battery_thermal_test, updateTemperatureTest) {
    CreateModel(Cp);
    // battery which adjusts quickly to temp
    double I = 50;
    size_t idx = 0;
    model->updateTemperature(I, idx++);
    auto s = thermal_state({93.87, 290.495, 290});
    compareState(model, s, "updateTemperatureTest: 1");

    I = -50;
    model->updateTemperature(I, idx++);
    s = thermal_state({93.87, 290.495, 290});
    compareState(model, s, "updateTemperatureTest: 2");

    I = 50;
    model->updateTemperature(I, idx++);
    s = thermal_state({97.37, 294.86, 295});
    compareState(model, s, "updateTemperatureTest: 3");

    I = 10;
    model->updateTemperature(I, idx++);
    s = thermal_state({97.49, 295.02, 295});
    compareState(model, s, "updateTemperatureTest: 4");

    I = 10;
    model->updateTemperature(I, idx++);
    s = thermal_state({94.05, 290.72, 290});
    compareState(model, s, "updateTemperatureTest: 5");

    I = 10;
    model->updateTemperature(I, idx++);
    s = thermal_state({79.54, 272.92, 270});
    compareState(model, s, "updateTemperatureTest: 6");

    I = 100;
    model->updateTemperature(I, idx++);
    s = thermal_state({77.66, 271.98, 270});
    compareState(model, s, "updateTemperatureTest: 7");
}

TEST_F(lib_battery_thermal_test, updateTemperatureTest2){
    CreateModel(Cp * 2);
    // slower adjusting batt
    double I = 50;
    size_t idx = 0;
    model->updateTemperature(I, idx++);
    auto s = thermal_state({93.87, 290.495, 290});
    compareState(model, s, "updateTemperatureTest: 1");

    I = -50;
    model->updateTemperature(I, idx++);
    s = thermal_state({93.87, 290.495, 290});
    compareState(model, s, "updateTemperatureTest: 2");

    I = 50;
    model->updateTemperature(I, idx++);
    s = thermal_state({96.89, 294.26, 295});
    compareState(model, s, "updateTemperatureTest: 3");

    I = 10;
    model->updateTemperature(I, idx++);
    s = thermal_state({97.49, 295.02, 295});
    compareState(model, s, "updateTemperatureTest: 4");

    I = 10;
    model->updateTemperature(I, idx++);
    s = thermal_state({94.58, 291.38, 290});
    compareState(model, s, "updateTemperatureTest: 5");

    I = 10;
    model->updateTemperature(I, idx++);
    s = thermal_state({82.15, 275.84, 270});
    compareState(model, s, "updateTemperatureTest: 6");

    I = 100;
    model->updateTemperature(I, idx++);
    s = thermal_state({77.69, 271.99, 270});
    compareState(model, s, "updateTemperatureTest: 7");
}

TEST_F(lib_battery_losses_test, MonthlyLossesTest){
    model = std::unique_ptr<losses_t>(new losses_t(dt_hour, 0, chargingLosses,
                                                   dischargingLosses, chargingLosses, fullLosses));

    // losses for charging and idling are the same
    int charge_mode = capacity_t::CHARGE;

    size_t idx = 0;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 0, tol) << "MonthlyLossesTest: 1";

    idx = 40 * 24;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 1, tol) << "MonthlyLossesTest: 2";

    idx = 70 * 24;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 2, tol) << "MonthlyLossesTest: 3";

    // discharging
    charge_mode = capacity_t::DISCHARGE;

    idx = 0;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 1, tol) << "MonthlyLossesTest: 4";

    idx = 40 * 24;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 2, tol) << "MonthlyLossesTest: 5";

    idx = 70 * 24;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 3, tol) << "MonthlyLossesTest: 6";

}

TEST_F(lib_battery_losses_test, TimeSeriesLossesTest){
    model = std::unique_ptr<losses_t>(new losses_t(dt_hour, 1, chargingLosses,
                                                   dischargingLosses, chargingLosses, fullLosses));

    int charge_mode = -1;       // not used

    size_t idx = 0;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 0, tol) << "TimeSeriesLossesTest: 1";

    idx = 40;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 40, tol) << "TimeSeriesLossesTest: 2";

    idx = 70;
    model->run_losses(idx, charge_mode);
    EXPECT_NEAR(model->getLoss(idx), 70, tol) << "TimeSeriesLossesTest: 3";

}

TEST_F(lib_battery_test, SetUpTest){
    ASSERT_TRUE(1);
}

TEST_F(lib_battery_test, runTestCycleAt1C){
    size_t idx = 0;
    double capacity_passed = 0.;
    double I = Qfull * n_strings;
    batteryModel->run(idx++, I);
    capacity_passed += batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
    std::cerr << "\n" << idx << ": " << capacity_passed << "\n";

    auto s = battery_state({{479.75, 1000, 960.65, 20.25, 0, 49.94, 50.059, 0, 2}, // cap
                            500.66, // voltage
                           {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                            {0, 102}, 100}, // calendar
                           {96.065, 293.23}, // thermal
                           0});
    compareState(batteryModel, s, "runTestCycleAt1C: 1");

    while (batteryModel->capacity_model()->SOC() > SOC_min + 1){
        batteryModel->run(idx++, I);
        capacity_passed += batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
    }
    std::cerr <<  idx << ": soc " << batteryModel->capacity_model()->SOC() << ", cap " << capacity_passed << "\n";
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{54.5, 1000, 960.65, 20.25, 0, 5.67, 94.32, 0, 2}, // cap
                       316.979, // voltage
                       {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                        {0, 101.976}, 100}, // calendar
                       {96.06, 293.23}, // thermal
                       0});
    compareState(batteryModel, s, "runTestCycleAt1C: 2");

    size_t n_cycles = 400;

    while (n_cycles-- > 0){
        while (batteryModel->capacity_model()->SOC() < SOC_max - 1){
            batteryModel->run(idx++, -I);
            capacity_passed += -batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
        }
        while (batteryModel->capacity_model()->SOC() > SOC_min + 1){
            batteryModel->run(idx++, I);
            capacity_passed += batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
        }
    }
    std::cerr <<  idx << ": soc " << batteryModel->capacity_model()->SOC() << ", cap " << capacity_passed << "\n";
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{44.71, 930.8, 893.89, 14.925, 0, 5.00, 95, 93.328, 2}, // cap
                       305.52, // voltage
                       {{93.079, 85.442, 85.445, 85.44, 85.311, 346, 0, std::vector<double>()}, // cycle
                        {1374, 99.127}, 93.08}, // calendar
                       {96.03, 293.194, 118774800}, // thermal
                       32991});
    compareState(batteryModel, s, "runTestCycleAt1C: 3");

    EXPECT_NEAR(capacity_passed, 365045, 1000) << "Current passing through cell";
    double qmax = fmax(s.capacity.qmax, s.capacity.qmax_thermal);
    EXPECT_NEAR(qmax/q, .93, 0.01) << "capacity relative to max capacity";
}

TEST_F(lib_battery_test, runTestCycleAt3C){
    size_t idx = 0;
    double capacity_passed = 0.;
    double I = Qfull * n_strings * 3;
    batteryModel->run(idx++, I);
    capacity_passed += batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
    std::cerr << "\n" << idx << ": " << capacity_passed << "\n";

    auto s = battery_state({{439.25, 1000, 965.85, 60.75, 0, 45.47, 54.52, 0, 2}, // cap
                            373.39, // voltage
                            {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                             {0, 102}, 100}, // calendar
                            {96.58, 293.88}, // thermal
                            0});
    compareState(batteryModel, s, "runTest: 1");

    while (batteryModel->capacity_model()->SOC() > SOC_min + 1){
        batteryModel->run(idx++, I);
        capacity_passed += batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
    }
    std::cerr <<  idx << ": soc " << batteryModel->capacity_model()->SOC() << ", cap " << capacity_passed << "\n";
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{48.29, 1000, 961.10, 26.45, 0, 5.02, 94.97, 92.22, 2}, // cap
                       248.01, // voltage
                       {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                        {0, 101.98}, 101.98}, // calendar
                       {96.11, 293.288}, // thermal
                       0});
    compareState(batteryModel, s, "runTest: 2");

    size_t n_cycles = 400;

    while (n_cycles-- > 0){
        while (batteryModel->capacity_model()->SOC() < SOC_max - 1){
            batteryModel->run(idx++, -I);
            capacity_passed += -batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
        }
        while (batteryModel->capacity_model()->SOC() > SOC_min + 1){
            batteryModel->run(idx++, I);
            capacity_passed += batteryModel->capacity_model()->I() * batteryModel->voltage_model()->battery_voltage() / 1000.;
        }
    }
    std::cerr <<  idx << ": soc " << batteryModel->capacity_model()->SOC() << ", cap " << capacity_passed << "\n";
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{45.49, 942.07, 905.31, 24.80, 0, 5.02, 94.98, 93.328, 2}, // cap
                       247.76, // voltage
                       {{94.20, 76.16, 76.17, 76.17, 76.62, 303, 194, std::vector<double>()}, // cycle
                        {1374, 100.21}, 93.08}, // calendar
                       {96.097, 293.27, 118774800}, // thermal
                       32991});
    compareState(batteryModel, s, "runTest: 3");


    EXPECT_NEAR(capacity_passed, 316781, 100) << "Current passing through cell";
    double qmax = fmax(s.capacity.qmax, s.capacity.qmax_thermal);
    EXPECT_NEAR(qmax/q, .94, 0.01) << "capacity relative to max capacity";
}