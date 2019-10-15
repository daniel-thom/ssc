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

    EXPECT_NEAR(batt_capacity_percent_total, 871497, 871497 * 1e-3) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_cycles_total, 1421754, 1421754 * 1e-3) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_power_total, -2397, 2397 * 1e-3) << "Ac-coupled look ahead";
    EXPECT_NEAR(grid_power_total, -5642022, 5642022 * 1e-3) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_to_load_total, 38598, 38598 * 1e-3) << "Ac-coupled look ahead";
    EXPECT_NEAR(pv_to_batt_total, 0, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled look ahead";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 94.37, 94.37 * 1e-3) << "Ac-coupled look ahead";
    EXPECT_NEAR(batt_pv_charge_percent, 0.07, m_error_tolerance_lo) << "Ac-coupled look ahead";
}

TEST_F(CMBattery_cmod_battery, LookBehind) {
    ssc_data_set_number(data, "batt_ac_or_dc", 1);

    // peak shaving look behind
    ssc_data_set_number(data, "batt_dispatch_choice", 1);
    int errors = run_module(data, "battery");

    ASSERT_FALSE(errors);
    GetBatteryOutputs();

    EXPECT_NEAR(batt_capacity_percent_total, 871484, 871484 * 1e-3) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_cycles_total, 1437225, 1437225 * 1e-3) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_power_total, -2403, 2403 * 1e-3) << "Ac-coupled look behind";
    EXPECT_NEAR(grid_power_total, -5642027, 5642027 * 1e-3) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_to_load_total, 38925, 38925 * 1e-3) << "Ac-coupled look behind";
    EXPECT_NEAR(pv_to_batt_total, 0, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled look behind";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 94.42, 94 * 1e-3) << "Ac-coupled look behind";
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

    EXPECT_NEAR(batt_capacity_percent_total, 875850, 875850 * 1e-3) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_cycles_total, 63751, 63751 * 1e-3) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_power_total, 23.94, 23 * 1e-3) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(grid_power_total, -5639600, 5639600 * 1e-3) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_to_load_total, 2787, 2787 * 1e-3) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(pv_to_batt_total, 2744, 2744 * 1e-3) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 93.38, 93 * 1e-3) << "Ac-coupled lnputForecast";
    EXPECT_NEAR(batt_pv_charge_percent, 99.301, 99 * 1e-3) << "Ac-coupled lnputForecast";
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

    EXPECT_NEAR(batt_capacity_percent_total, 873848.11, 873848 * 1e-3) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_cycles_total, 1588704, 1588704 * 1e-3) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_power_total, -381, 381 * 1e-2) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(grid_power_total, -5640006.57, 5640006 * 1e-3) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_to_load_total, 2475.31, 2475 * 1e-3) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(pv_to_batt_total,  5.77, 43 * 1e-3) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 95.64, 95 * 1e-3) << "Ac-coupled InputBatteryPower";
    EXPECT_NEAR(batt_pv_charge_percent, 0.012, m_error_tolerance_lo) << "Ac-coupled InputBatteryPower";
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

    EXPECT_NEAR(batt_capacity_percent_total, 875762, 875762 * 1e-3) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_cycles_total, 47609, 47609 * 1e-3) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_power_total, 26.60, 26 * 1e-3) << "Ac-coupled Manual";
    EXPECT_NEAR(grid_power_total, -5639597, 5639597 * 1e-3) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_to_load_total, 2769, 2769 * 1e-3) << "Ac-coupled Manual";
    EXPECT_NEAR(pv_to_batt_total, 2742, 2742 * 1e-3) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_system_loss, 0, m_error_tolerance_lo) << "Ac-coupled Manual";
    EXPECT_NEAR(average_battery_roundtrip_efficiency, 93.36, 93 * 1e-3) << "Ac-coupled Manual";
    EXPECT_NEAR(batt_pv_charge_percent, 99.99, m_error_tolerance_lo) << "Ac-coupled Manual";
}