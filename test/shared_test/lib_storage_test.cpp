#include <gtest/gtest.h>

#include <lib_storage.h>

/**
* Storage Capacity Class test
*/

class Storage_lib_storage : public ::testing::Test {
protected:
    size_t n_rec = 10;
    double dt_hr = 1;
    storage* batt;
public:
    void SetUp() override {
    }
    void TearDown() override {
        delete batt;
    }
};

TEST_F(Storage_lib_storage, SetUp){
    batt = storage::Create(BATT_BTM_MAN);
}