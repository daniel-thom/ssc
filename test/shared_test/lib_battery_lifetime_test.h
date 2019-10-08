#ifndef SAM_SIMULATION_CORE_LIB_BATTERY_LIFETIME_TEST_H
#define SAM_SIMULATION_CORE_LIB_BATTERY_LIFETIME_TEST_H

#include <gtest/gtest.h>

#include "lib_util.h"
#include "lib_battery_lifetime.h"
#include "lib_battery.h"

static void compareState(const cycle_lifetime_state& cycle_state, const cycle_lifetime_state& state, const std::string& msg){
    double tol = 0.01;
    EXPECT_NEAR(cycle_state.relative_q, state.relative_q, tol) << msg;
    EXPECT_NEAR(cycle_state.Xlt, state.Xlt, tol) << msg;
    EXPECT_NEAR(cycle_state.Ylt, state.Ylt, tol) << msg;
    EXPECT_NEAR(cycle_state.Range, state.Range, tol) << msg;
    EXPECT_NEAR(cycle_state.average_range, state.average_range, tol) << msg;
    EXPECT_NEAR(cycle_state.nCycles, state.nCycles, tol) << msg;
    EXPECT_NEAR(cycle_state.jlt, state.jlt, tol) << msg;
}

class lib_battery_lifetime_test : public ::testing::Test{
protected:
    std::unique_ptr<battery_lifetime> model;
    std::shared_ptr<storage_time_params> time;

    std::shared_ptr<const battery_lifetime_params> params;

    double calendar_q0 = 1.02;
    double calendar_a = 2.66e-3;
    double calendar_b = -7280;
    double calendar_c = 930;

    util::matrix_t<double> cycles_vs_DOD;

    double dt_hour = 1;
public:

    void SetUp() override {
        time = std::make_shared<storage_time_params>(dt_hour, 1);

        double table_vals[18] = {20, 0, 100, 20, 5000, 80, 20, 10000, 60, 80, 0, 100, 80, 1000, 80, 80, 2000, 60};
        cycles_vs_DOD.assign(table_vals, 6, 3);
        params = std::shared_ptr<battery_lifetime_params>(new battery_lifetime_params({time, cycles_vs_DOD, battery_lifetime_params::MODEL,
                                                                                       calendar_q0, calendar_a, calendar_b, calendar_c}));

        model = std::unique_ptr<battery_lifetime>(new battery_lifetime(params));
    }
};

class lib_battery_lifetime_cycle_test : public lib_battery_lifetime_test
{
protected:
    std::unique_ptr<lifetime_cycle> cycle_model;

public:
    void SetUp() override {
        time = std::make_shared<storage_time_params>(dt_hour, 1);
        double table_vals[18] = {20, 0, 100, 20, 5000, 80, 20, 10000, 60, 80, 0, 100, 80, 1000, 80, 80, 2000, 60};
        cycles_vs_DOD.assign(table_vals, 6, 3);

        params = std::shared_ptr<battery_lifetime_params>(new battery_lifetime_params({time, cycles_vs_DOD}));
        cycle_model = std::unique_ptr<lifetime_cycle>(new lifetime_cycle(params));
    }
};

static void compareState(const calendar_lifetime_state& calendar_state, const calendar_lifetime_state& state, const std::string& msg){
    double tol = 0.01;
//        EXPECT_NEAR(model->, state.day_age_of_battery, tol) << msg;
    EXPECT_NEAR(calendar_state.q, state.q, tol) << msg;
//        EXPECT_NEAR(model->capacity_percent(), state.last_idx, tol) << msg;
//        EXPECT_NEAR(model->average_range(), state.dq_old, tol) << msg;
//        EXPECT_NEAR(model->cycle_range(), state.dq_new, tol) << msg;
}

class lib_battery_lifetime_calendar_matrix_test : public lib_battery_lifetime_test
{
protected:
    std::unique_ptr<lifetime_calendar> cal_model;

    util::matrix_t<double> calendar_matrix;

public:
    void SetUp() override {
        time = std::make_shared<storage_time_params>(dt_hour, 1);

        double table_vals[18] = {0, 100, 3650, 80, 7300, 50};
        calendar_matrix.assign(table_vals, 3, 2);
        params = std::shared_ptr<battery_lifetime_params>(new  battery_lifetime_params({time, util::matrix_t<double>(),
                battery_lifetime_params::TABLE, 0, 0, 0, 0, calendar_matrix}));
        cal_model = std::unique_ptr<lifetime_calendar>(new lifetime_calendar(params));
    }
};


class lib_battery_lifetime_calendar_model_test : public lib_battery_lifetime_test
{
protected:
    std::unique_ptr<lifetime_calendar> model;

public:
    void SetUp() override {
        time = std::make_shared<storage_time_params>(dt_hour, 1);

        params = std::shared_ptr<battery_lifetime_params>(new battery_lifetime_params({time, util::matrix_t<double>(),
                battery_lifetime_params::MODEL, calendar_q0, calendar_a, calendar_b, calendar_c,
                                                                                       util::matrix_t<double>()}));
        model = std::unique_ptr<lifetime_calendar>(new lifetime_calendar(params));
    }
};


static void compareState(const lifetime_state& model_state, const lifetime_state& s, const std::string& msg){
    auto cycle = model_state.cycle;
    compareState(cycle, s.cycle, msg);
    auto cal = model_state.calendar;
    compareState(cal, s.calendar, msg);
}


#endif //SAM_SIMULATION_CORE_LIB_BATTERY_LIFETIME_TEST_H
