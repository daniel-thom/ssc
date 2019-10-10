#include <gtest/gtest.h>

#include "lib_battery_model_test.h"

TEST_F(lib_battery_thermal_test, testTimeIncrement){
    storage_time_state time_state(step_hour);
    while (time_state.get_lifetime_index() < 8759)
        time_state.increment();
    EXPECT_EQ(time_state.get_lifetime_index(), time_state.get_index());
    EXPECT_EQ(time_state.get_lifetime_index(), time_state.get_hour_year());
    time_state.increment();
    EXPECT_EQ(time_state.get_hour_year(), 0);
    EXPECT_EQ(time_state.get_hour_lifetime(), 8760);

    storage_time_state time_state2(2);
    while (time_state2.get_lifetime_index() < 17519)
        time_state2.increment();
    EXPECT_EQ(time_state2.get_lifetime_index(), time_state2.get_index());
    EXPECT_EQ(8759, time_state2.get_hour_year());
    time_state2.increment();
    EXPECT_EQ(time_state2.get_hour_year(), 0);
    EXPECT_EQ(time_state2.get_hour_lifetime(), 8760);
}

TEST_F(lib_battery_thermal_test, SetUpTest){
    CreateModel(Cp);
    EXPECT_NEAR(model->get_T_battery(), 290, tol);
    EXPECT_NEAR(model->get_capacity_percent(), 100, tol);
}

TEST_F(lib_battery_thermal_test, updateTemperatureTest) {
    CreateModel(Cp);
    // battery which adjusts quickly to temp
    double I = 50;
    storage_time_state time_state(step_hour);
    model->updateTemperature(I, time_state);
    auto s = thermal_state({93.87, 290.495, 290, 290, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 1");

    I = -50;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({93.87, 290.495, 290, 290, 3600 * 3});
    compareState(model->get_state(), s, "updateTemperatureTest: 2");

    I = 50;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({97.37, 294.86, 290.49, 295, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 3");

    I = 10;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({97.49, 295.02, 290.49, 295, 3600 * 3});
    compareState(model->get_state(), s, "updateTemperatureTest: 4");

    I = 10;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({94.05, 290.72, 295.019, 290, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 5");

    I = 10;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({79.54, 272.92, 290.72, 270, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 6");

    I = 100;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({77.66, 271.98, 290.72, 270, 3600 * 3});
    compareState(model->get_state(), s, "updateTemperatureTest: 7");
}

TEST_F(lib_battery_thermal_test, updateTemperatureTest2){
    CreateModel(Cp * 2);
    // slower adjusting batt
    double I = 50;
    storage_time_state time_state(step_hour);
    model->updateTemperature(I, time_state);
    auto s = thermal_state({93.87, 290.495, 290, 290, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 1");

    I = -50;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({93.87, 290.495, 290, 290, 3600 * 3});
    compareState(model->get_state(), s, "updateTemperatureTest: 2");

    I = 50;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({96.89, 294.26, 290.49, 295, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 3");

    I = 10;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({97.49, 295.02, 290.49, 295, 3600 * 3});
    compareState(model->get_state(), s, "updateTemperatureTest: 4");

    I = 10;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({94.58, 291.38, 295.016, 290, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 5");

    I = 10;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({82.15, 275.84, 291.385, 270, 3600 * 2});
    compareState(model->get_state(), s, "updateTemperatureTest: 6");

    I = 100;
    model->updateTemperature(I, time_state.increment());
    s = thermal_state({77.69, 271.99, 291.385, 270, 3600 * 3});
    compareState(model->get_state(), s, "updateTemperatureTest: 7");
}

TEST_F(lib_battery_losses_test, MonthlyLossesTest){
    params = std::shared_ptr<battery_losses_params>(new battery_losses_params({time, battery_losses_params::MONTHLY, chargingLosses,
                                    dischargingLosses, chargingLosses, fullLosses}));
    model = std::unique_ptr<battery_losses>(new battery_losses(params));

    // losses for charging and idling are the same
    int charge_mode = capacity_t::CHARGE;

    storage_time_state time_state(1);

    EXPECT_NEAR(model->getLoss(time_state, charge_mode), 0, tol) << "MonthlyLossesTest: 1";

    EXPECT_NEAR(model->getLoss(time_state.increment(40 * 24), charge_mode), 1, tol) << "MonthlyLossesTest: 2";

    EXPECT_NEAR(model->getLoss(time_state.increment(30 * 24), charge_mode), 2, tol) << "MonthlyLossesTest: 3";

    // discharging
    charge_mode = capacity_t::DISCHARGE;

    EXPECT_NEAR(model->getLoss(time_state.reset_storage_time(), charge_mode), 1, tol) << "MonthlyLossesTest: 4";

    EXPECT_NEAR(model->getLoss(time_state.increment(40 * 24), charge_mode), 2, tol) << "MonthlyLossesTest: 5";

    EXPECT_NEAR(model->getLoss(time_state.increment(30 * 24), charge_mode), 3, tol) << "MonthlyLossesTest: 6";

}

TEST_F(lib_battery_losses_test, TimeSeriesLossesTest){
    params = std::shared_ptr<battery_losses_params>(new battery_losses_params({time, battery_losses_params::TIMESERIES, chargingLosses,
                                                                               dischargingLosses, chargingLosses, fullLosses}));
    model = std::unique_ptr<battery_losses>(new battery_losses(params));

    int charge_mode = -1;       // not used

    storage_time_state time_state(1);

    EXPECT_NEAR(model->getLoss(time_state, charge_mode), 0, tol) << "TimeSeriesLossesTest: 1";

    EXPECT_NEAR(model->getLoss(time_state.increment(40), charge_mode), 40, tol) << "TimeSeriesLossesTest: 2";

    EXPECT_NEAR(model->getLoss(time_state.increment(30), charge_mode), 70, tol) << "TimeSeriesLossesTest: 3";

}

TEST_F(lib_battery_test, SetUpTest){
    ASSERT_TRUE(1);
}

TEST_F(lib_battery_test, runTestCycleAt1C){
    double capacity_passed = 0.;
    double I = Qfull * n_strings;
    storage_time_state time(1);
    batteryModel->run(time, I);
    model->run(time.get_lifetime_index(), I);
    capacity_passed += batteryModel->get_I() * batteryModel->get_V() / 1000.;

    auto s = battery_state({{479.75, 1000, 960.65, 20.25, 0, 49.94, 50.059, 0, 2}, // cap
                            500.66, // voltage
                           {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                            {0, 102, 0}, 100}, // calendar
                           {96.065, 293.23, 293.15, 293.15, 3600*2}}); // thermal
    compareState(batteryModel->get_state(), s, "runTestCycleAt1C: 1");

    while (batteryModel->get_SOC() > SOC_min + 1){
        batteryModel->run(time.increment(), I);
        model->run(time.get_lifetime_index(), I);
        capacity_passed += batteryModel->get_I() * batteryModel->get_V() / 1000.;
    }
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{54.5, 1000, 960.65, 20.25, 0, 5.67, 94.32, 0, 2}, // cap
                       316.979, // voltage
                       {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                        {0, 101.976, 21}, 100}, // calendar
                       {96.06, 293.23, 293.15, 293.15, 82800}}); // thermal
    compareState(batteryModel->get_state(), s, "runTestCycleAt1C: 2");

    size_t n_cycles = 400;

    while (n_cycles-- > 0){
        while (batteryModel->get_SOC() < SOC_max - 1){
            batteryModel->run(time.increment(), -I);
            model->run(time.get_lifetime_index(), -I);
            capacity_passed += -batteryModel->get_I() * batteryModel->get_V() / 1000.;
            auto batt = batteryModel->get_state();
            assert(model->capacity_model()->I() == batteryModel->get_I());
            assert(model->thermal_model()->capacity_percent() == batt.thermal.capacity_percent);
            assert(abs(model->lifetime_model()->capacity_percent_calendar()- batt.lifetime.calendar.q) < 1e-2);
            assert(model->lifetime_model()->capacity_percent_cycle() == batt.lifetime.cycle.relative_q);
            assert(model->voltage_model()->battery_voltage() == batt.batt_voltage);
        }
        while (batteryModel->get_SOC() > SOC_min + 1){
            if (time.get_lifetime_index() == 6068)
                int x = 0;
            batteryModel->run(time.increment(), I);
            model->run(time.get_lifetime_index(), I);
            capacity_passed += batteryModel->get_I() * batteryModel->get_V() / 1000.;
            auto batt = batteryModel->get_state();
            assert(model->capacity_model()->I() == batteryModel->get_I());
            assert(model->thermal_model()->capacity_percent() == batt.thermal.capacity_percent);
            assert(abs(model->lifetime_model()->capacity_percent_calendar()- batt.lifetime.calendar.q) < 1e-2);

            assert(model->lifetime_model()->capacity_percent_cycle() == batt.lifetime.cycle.relative_q);
            assert(model->voltage_model()->battery_voltage() == batt.batt_voltage);


        }
    }
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{44.71, 930.8, 893.89, 14.925, 0, 5.00, 95, 93.328, 2}, // cap
                       305.52, // voltage
                       {{93.079, 85.442, 85.445, 85.44, 85.311, 346, 108, std::vector<double>()}, // cycle
                        {1374, 99.127, 32991, 0.0287, 0.0287}, 93.08}, // calendar
                       {96.03, 293.194, 293.15, 293.15, 118774800}}); // thermal

    compareState(batteryModel->get_state(), s, "runTestCycleAt1C: 3");

    std::cerr << "\n"<< capacity_passed << " " << time.get_lifetime_index() << "\n";

    EXPECT_NEAR(capacity_passed, 365045, 1000) << "Current passing through cell";
    double qmax = fmax(s.capacity.qmax, s.capacity.qmax_thermal);
    EXPECT_NEAR(qmax/q, .93, 0.01) << "capacity relative to max capacity";
}

TEST_F(lib_battery_test, runTestCycleAt3C){
    storage_time_state time(1);
    double capacity_passed = 0.;
    double I = Qfull * n_strings * 3;
    batteryModel->run(time, I);
    capacity_passed += batteryModel->get_I() * batteryModel->get_V() / 1000.;

    auto s = battery_state({{439.25, 1000, 965.85, 60.75, 0, 45.47, 54.52, 0, 2}, // cap
                            373.39, // voltage
                            {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                             {0, 102}, 100}, // calendar
                            {96.58, 293.88, 293.15, 293.15, 7200}}); // thermal
    compareState(batteryModel->get_state(), s, "runTest: 1");

    while (batteryModel->get_SOC() > SOC_min + 1){
        batteryModel->run(time.increment(), I);
        capacity_passed += batteryModel->get_I() * batteryModel->get_V() / 1000.;
    }
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{48.29, 1000, 961.10, 26.45, 0, 5.02, 94.97, 92.22, 2}, // cap
                       248.01, // voltage
                       {{100, 0, 0, 0, 0, 0, 0, std::vector<double>()}, // cycle
                        {0, 101.98, 7}, 101.98}, // calendar
                       {96.11, 293.288, 293.15, 293.15, 32400}}); // thermal
    compareState(batteryModel->get_state(), s, "runTest: 2");

    size_t n_cycles = 400;

    double neg_I = -I;

    while (n_cycles-- > 0){
        while (batteryModel->get_SOC() < SOC_max - 1){
            batteryModel->run(time.increment(), neg_I);
            capacity_passed += -batteryModel->get_I() * batteryModel->get_V() / 1000.;
        }
        while (batteryModel->get_SOC() > SOC_min + 1){
            batteryModel->run(time.increment(), I);
            capacity_passed += batteryModel->get_I() * batteryModel->get_V() / 1000.;
        }
    }
    // the SOC isn't at 5 so it means the controller is not able to calculate a current/voltage at which to discharge to 5
    s = battery_state({{45.49, 942.07, 905.31, 24.80, 0, 5.02, 94.98, 93.328, 2}, // cap
                       247.76, // voltage
                       {{94.20, 76.16, 76.17, 76.17, 76.62, 303, 194, std::vector<double>()}, // cycle
                        {469, 100.21, 11278, 0.0178, 0.0178}, 93.08}, // calendar
                       {96.097, 293.27, 293.15, 293.15, 40608000}}); // thermal
    compareState(batteryModel->get_state(), s, "runTest: 3");


    EXPECT_NEAR(capacity_passed, 316781, 100) << "Current passing through cell";
    double qmax = fmax(s.capacity.qmax, s.capacity.qmax_thermal);
    EXPECT_NEAR(qmax/q, .94, 0.01) << "capacity relative to max capacity";
}