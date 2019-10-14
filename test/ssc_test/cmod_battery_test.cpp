#include <numeric>

#include <gtest/gtest.h>

#include "cmod_battery_test.h"


/// Test standalone battery compute modeule with a input lifetime generation and commercial load
TEST_F(CMBattery_cmod_battery, CommercialLifetimePeakShaving) {

	// Run with fixed output
	ssc_number_t n_years;
	ssc_data_get_number(data, "analysis_period", &n_years);
	size_t n_lifetime = (size_t)(n_years) * 8760;

	int errors = run_module(data, "battery");
	ASSERT_FALSE(errors);

	if (!errors)
	{
		// roundtrip efficiency test will ensure that the battery cycled
		ssc_number_t roundtripEfficiency;
		ssc_data_get_number(data, "average_battery_roundtrip_efficiency", &roundtripEfficiency);
		EXPECT_NEAR(roundtripEfficiency, 90.06, 2);

		// test that lifetime output is achieved
		int n;
		calculated_array = ssc_data_get_array(data, "gen", &n);
		EXPECT_EQ(n_lifetime, (size_t)n);

		// test that battery was replaced at some point
		calculated_array = ssc_data_get_array(data, "batt_bank_replacement", &n);
		int replacements = std::accumulate(calculated_array, calculated_array + n, 0);
		
		EXPECT_GT(replacements, 0);
	}
}

TEST_F(CMBattery_cmod_battery, LookAhead) {
    ssc_data_set_number(data, "batt_ac_or_dc", 1);

    // peak shaving look ahead
    ssc_data_set_number(data, "batt_dispatch_choice", 0);
    int errors = run_module(data, "battery");

    ASSERT_FALSE(errors);
    GetBatteryOutputs();

    EXPECT_NEAR(batt_capacity_percent_total, 871476.15, m_error_tolerance_hi) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_cycles_total, 1411373, m_error_tolerance_hi) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_power_total, -5748.64, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(grid_power_total, -5645373.38, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_to_load_total, 38609.58, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(pv_to_batt_total, 0, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 90.01, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_pv_charge_percent, 0.07, m_error_tolerance_lo) << "Ac-coupled look ahead";
}

TEST_F(CMBattery_cmod_battery, LookBehind) {
    ssc_data_set_number(data, "batt_ac_or_dc", 1);

    // peak shaving look behind
    ssc_data_set_number(data, "batt_dispatch_choice", 1);
    int errors = run_module(data, "battery");

    ASSERT_FALSE(errors);
    GetBatteryOutputs();

    EXPECT_NEAR(batt_capacity_percent_total, 871484.00, m_error_tolerance_hi) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_cycles_total, 1426945, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_power_total, -4533.94, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(grid_power_total, -5644158.68, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_to_load_total, 38937.54, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(pv_to_batt_total, 0, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 90.80, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_pv_charge_percent, 0.06, m_error_tolerance_lo) << "Ac-coupled look behind";
}

TEST_F(CMBattery_cmod_battery, InputForecast) {
    // passing
    ssc_data_set_number(data, "batt_ac_or_dc", 1);

    // InputGridTarget
    ssc_data_set_number(data, "batt_dispatch_choice", 2);
    ssc_data_set_number(data, "batt_target_choice", 0);
    double target_power[12] = {1};
    ssc_data_set_array(data, "batt_target_power_monthly", target_power, 12);
    int errors = run_module(data, "battery");

    ASSERT_FALSE(errors);
    GetBatteryOutputs();

    EXPECT_NEAR(batt_capacity_percent_total, 875850.74, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_cycles_total, 63751, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_power_total, 20.72, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(grid_power_total, -5639604.02, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_to_load_total, 2788.70, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(pv_to_batt_total, 2749.47, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 93.38, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_pv_charge_percent, 99.301, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
}

TEST_F(CMBattery_cmod_battery, PowerTarget) {
    ssc_data_set_number(data, "batt_ac_or_dc", 1);

    // InputBatteryPower
    ssc_data_set_number(data, "batt_dispatch_choice", 3);
    double dispatch[8760];
    for (size_t i = 0; i < 8760; i++){
        if (i % 24 > 10 && i % 24 < 18)
            dispatch[i] = 1.;
        else
            dispatch[i] = -0.5;
    }
    ssc_data_set_array(data, "batt_custom_dispatch", dispatch, 8760);
    int errors = run_module(data, "battery");

    ASSERT_FALSE(errors);
    GetBatteryOutputs();

    EXPECT_NEAR(batt_capacity_percent_total, 873848.11, m_error_tolerance_hi) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_cycles_total, 1588704, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_power_total, -381.82, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(grid_power_total, -5640006.57, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_to_load_total, 2475.31, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(pv_to_batt_total,  43.82, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 95.64, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_pv_charge_percent, 0.15, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
}

TEST_F(CMBattery_cmod_battery, Manual) {
    ssc_data_set_number(data, "batt_ac_or_dc", 1);

    // Manual
    ssc_data_set_number(data, "batt_dispatch_choice", 4);
    double dispatch_manual_sched [12*24] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1,
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1};
    ssc_data_set_matrix(data, "dispatch_manual_sched", dispatch_manual_sched, 12, 24);
    ssc_data_set_matrix(data, "dispatch_manual_sched_weekend", dispatch_manual_sched, 12, 24);
    int errors = run_module(data, "battery");

    ASSERT_FALSE(errors);
    GetBatteryOutputs();

    EXPECT_NEAR(batt_capacity_percent_total, 875762.55, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_cycles_total, 47609, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_power_total, 26.60, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(grid_power_total, -5639598.14, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_to_load_total, 2767.81, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(pv_to_batt_total, 2741.21, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 93.36, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_pv_charge_percent, 99.99, m_error_tolerance_lo) << "Ac-coupled Manual";
}