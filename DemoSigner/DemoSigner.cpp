// DemoSigner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "SignManager.h"
#include "DeviceManager.hpp"
#include "AnisetteDataManager.h"
#include "AppleAPI.hpp"
#include <WinSock2.h>
#include "AnisetteData.h"
std::string strIpaName;
std::string strAppleId;
std::string strApplePwd;
std::string strDeviceId;

bool ParseArguments(int argc, char* args[])
{
    if (argc < 4) 
    {
        std::cout << "Argument missing\n";
        std::cout << "Usage: DemoSigner.exe IPANAME APPLEID APPLEPWD DEVICEUDID\n";
        return false;
    }

    strIpaName = args[0];
    strAppleId = args[1];
    strApplePwd = args[2];
    strDeviceId = args[3];

    return true;
}


void _convert_filetime(struct timeval* out_tv, const FILETIME* filetime)
{
    // Microseconds between 1601-01-01 00:00:00 UTC and 1970-01-01 00:00:00 UTC
    static const uint64_t EPOCH_DIFFERENCE_MICROS = 11644473600000000ull;

    // First convert 100-ns intervals to microseconds, then adjust for the
    // epoch difference
    uint64_t total_us = (((uint64_t)filetime->dwHighDateTime << 32) | (uint64_t)filetime->dwLowDateTime) / 10;
    total_us -= EPOCH_DIFFERENCE_MICROS;

    // Convert to (seconds, microseconds)
    out_tv->tv_sec = (time_t)(total_us / 1000000);
    out_tv->tv_usec = (long)(total_us % 1000000);
}

int main(int argc, char* args[])
{
    //std::cout << "Hello World!\n";
    std::cout << "IPA demo signer\n";
    std::cout << "Usage: DemoSigner.exe IPANAME APPLEID APPLEPWD DEVICEUDID\n";
    /*

    if (ParseArguments(argc, args)) {
        auto devices = DeviceManager::instance()->connectedDevices();

        bool deviceFound = false;
        std::shared_ptr<Device> targetDevice = NULL;
        
        for (int i = 0; i < devices.size(); i++)
        {
            auto _dev = devices[i];
            if (_dev->identifier() == strDeviceId)
            {
                // Duplicate.
                deviceFound = true;
                targetDevice = _dev;
                break;
            }
        }

        if (deviceFound == false) {
            std::cout << "Current Device not connected\n";
            return 0;
        }

        auto task = SignManager::instance()->InstallApplication(strIpaName, targetDevice, strAppleId, strApplePwd);
    }
    */
    std::string device_name("Sol's iPhone");
    //std::string udid("604E359E1E3307B9A333593BD95FC03335D2C6FB");
    std::string udid("BCE88EA1C399F444A56931F2110E8DF3EBCA2FCB");
    Device::Type deviceType = Device::iPhone;

    strIpaName = "E:\\WORK\\WORK_IOS\\AltServer-Windows\\Release\\plenixclash-v13.369.29.ipa";
    strAppleId = "khj20130703@hotmail.com";
    strApplePwd = "Jinxue11!!";
    

    //AnisetteDataManager::instance()->FetchAnisetteData();

    auto verificationHandler = [=](void)->pplx::task<std::optional<std::string>> {
        return pplx::create_task([=]() -> std::optional<std::string> {

            /*int result = DialogBox(NULL, MAKEINTRESOURCE(ID_TWOFACTOR), NULL, TwoFactorDlgProc);
            if (result == IDCANCEL)
            {
                return std::nullopt;
            }

            auto verificationCode = std::make_optional<std::string>(_verificationCode);
            _verificationCode = "";

            return verificationCode;*/
            std::cout << "Please input TwoFactor Verfication Code:\n";
            std::string verificationCode;
            std::cin >> verificationCode;
            return verificationCode;
            });
    };

    std::string deviceSerialNumber = "C02LKHBBFD57";


    FILETIME systemTime;
    GetSystemTimeAsFileTime(&systemTime);

    TIMEVAL date;
    _convert_filetime(&date, &systemTime);
    auto description = SignManager::instance()->serverID();
    std::vector<unsigned char> deviceIDData(description.begin(), description.end());
    auto encodedDeviceID = StringFromWideString(utility::conversions::to_base64(deviceIDData));

    auto anisetteData = std::make_shared<AnisetteData>(
        "4bSBDHiAyF3TuysylztsYKTlZkw6/cFdh4T62Or2F0/K1XMB0XKdKv08TxPHIt6g6jCnseMM0WSgPUkR",
        "AAAABQAAABBOjsL6L/k9WibU+xw1trpwAAAAAg==",
        encodedDeviceID,
        17106176,
        description,
        deviceSerialNumber,
        "<MacBookPro15,1> <Mac OS X;10.15.2;19C57> <com.apple.AuthKit/1 (com.apple.dt.Xcode/3594.4.19)>",
        date,
        "en_US",
        "PST");

    

    try
    {
        AppleAPI::getInstance()->Authenticate(strAppleId, strApplePwd, anisetteData, verificationHandler).wait();
    }
    catch (const std::exception& ex)
    {
       std::cout << ex.what();
        // Swallow the exception.
    }


    //##########
    //auto targetDevice = std::make_shared<Device>(device_name, udid, deviceType);

    auto devices = DeviceManager::instance()->connectedDevices();

    bool deviceFound = false;
    std::shared_ptr<Device> targetDevice = NULL;

    if (devices.size() == 0) {
        std::cout << "Device Not Connected\n";
        return 0;
    }

    if (deviceFound == false) {
        std::cout << "Current Device not connected\n";
        //return 0;
    }
    targetDevice = devices[0];

    auto task_status = SignManager::instance()->InstallApplication(strIpaName, targetDevice, strAppleId, strApplePwd).wait();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
