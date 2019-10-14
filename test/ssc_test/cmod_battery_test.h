#ifndef _CMOD_BATTERY_TEST_H_
#define _CMOD_BATTERY_TEST_H_

#include <gtest/gtest.h>
#include <memory>

#include "core.h"
#include "sscapi.h"

#include "vartab.h"
#include "../ssc/common.h"
#include "../input_cases/code_generator_utilities.h"
#include "../input_cases/battery_common_data.h"

/**
 * CMBattery tests the cmod_battery using the SAM code generator to generate data
 * Eventually a method can be written to write this data to a vartable so that lower-level methods of pvsamv1 can be tested
 * For now, this uses the SSCAPI interfaces to run the compute module and compare results
 */
class CMBattery_cmod_battery : public ::testing::Test {

public:

	ssc_data_t data;
	ssc_number_t calculated_value;
	ssc_number_t * calculated_array;
	double m_error_tolerance_hi = 1.0;
	double m_error_tolerance_lo = 0.1;

	void SetUp()
	{
		data = ssc_data_create();
        battery_commercial_peak_shaving_lifetime(data);

	}
	void TearDown() {
		if (data) {
			ssc_data_free(data);
			data = nullptr;
		}
	}
	void SetCalculated(std::string name)
	{
		ssc_data_get_number(data, const_cast<char *>(name.c_str()), &calculated_value);
	}
	// memory of the array is managed internally to the sscapi.
	void SetCalculatedArray(std::string name)
	{
		int n;
		calculated_array = ssc_data_get_array(data, const_cast<char *>(name.c_str()), &n);
	}

    // battery outputs
    double batt_capacity_percent_total;
    double batt_cycles_total;
    double batt_power_total;
    double grid_power_total;
    double batt_to_load_total;
    double pv_to_batt_total;
    double batt_system_loss;
    double average_battery_roundtrip_efficiency;
    double batt_pv_charge_percent;

    void GetBatteryOutputs(){
        double* arr = ssc_data_get_array(data, "batt_capacity_percent", nullptr);
        double* arr2 = ssc_data_get_array(data, "batt_cycles", nullptr);
        double* arr3 = ssc_data_get_array(data, "batt_power", nullptr);
        double* arr4 = ssc_data_get_array(data, "grid_power", nullptr);
        double* arr5 = ssc_data_get_array(data, "batt_to_load", nullptr);
        double* arr6 = ssc_data_get_array(data, "pv_to_batt", nullptr);
        double* arr7 = ssc_data_get_array(data, "batt_system_loss", nullptr);
        batt_capacity_percent_total = std::accumulate(arr, arr+8760, 0.);
        batt_cycles_total = std::accumulate(arr2, arr2+8760, 0.);
        batt_power_total = std::accumulate(arr3, arr3+8760, 0.);
        grid_power_total = std::accumulate(arr4, arr4+8760, 0.);
        batt_to_load_total = std::accumulate(arr5, arr5+8760, 0.);
        pv_to_batt_total = std::accumulate(arr6, arr6+8760, 0.);
        batt_system_loss = std::accumulate(arr7, arr7+8760, 0.);
        ssc_data_get_number(data, "average_battery_roundtrip_efficiency", &average_battery_roundtrip_efficiency);
        ssc_data_get_number(data, "batt_pv_charge_percent", &batt_pv_charge_percent);
    }
};

#endif 
