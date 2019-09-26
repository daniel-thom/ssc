#include <gtest/gtest.h>

#include <lib_storage.h>

#include "battery_common_data.h"

/**
* Storage Capacity Class test
*/

class Storage_lib_storage : public ::testing::Test {
protected:
    size_t n_rec = 10;
    double dt_hr = 1;
    storage* batt;
    var_table* vt;
public:
    void SetUp() override {
        vt = new var_table();
        vt->assign("dt_hr", 1);
        auto ssc_data = static_cast<ssc_data_t>(vt);
        battery_commercial_peak_shaving_lifetime(ssc_data);
    }
    void TearDown() override {
        delete batt;
        delete vt;
    }
};

TEST_F(Storage_lib_storage, SetUp){
    batt = storage::Create(BATT_BTM_AUTO);
    batt->from_data(*vt);
}